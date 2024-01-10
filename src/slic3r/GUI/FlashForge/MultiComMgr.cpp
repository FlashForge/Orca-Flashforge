#include "MultiComMgr.hpp"
#include "FreeInDestructor.h"

namespace Slic3r { namespace GUI {

MultiComMgr::MultiComMgr()
    : m_idNum(0)
{
    m_datMap.emplace(ComInvalidId, com_dev_data_t{COM_CONNECT_LAN, nullptr});
}

bool MultiComMgr::initalize(const std::string &newtworkDllPath)
{
    if (networkIntfc() != nullptr) {
        return false;
    }
    m_networkIntfc.reset(new fnet::FlashNetworkIntfc(newtworkDllPath.c_str()));
    if (!m_networkIntfc->isOk()) {
        m_networkIntfc.reset(nullptr);
        return false;
    }
    m_userDataUpdateThd.reset(new UserDataUpdateThd(m_networkIntfc.get()));
    m_userDataUpdateThd->Bind(WAN_DEV_UPDATE_EVENT, &MultiComMgr::onWanDevUpdated, this);
    m_userDataUpdateThd->Bind(COM_WAN_DEV_MAINTAIN_EVENT, &MultiComMgr::onWanDevMaintian, this);
    m_userDataUpdateThd->Bind(COM_GET_USER_PROFILE_EVENT, [this](const ComGetUserProfileEvent &event) {
        QueueEvent(event.Clone());
    });
    return true;
}

void MultiComMgr::uninitalize()
{
    if (networkIntfc() == nullptr) {
        return;
    }
    m_userDataUpdateThd->exit();
    m_userDataUpdateThd.reset(nullptr);
    m_networkIntfc.reset(nullptr);
}

fnet::FlashNetworkIntfc *MultiComMgr::networkIntfc()
{
    return m_networkIntfc.get();
}

com_id_t MultiComMgr::addLanDev(const fnet_lan_dev_info &devInfo, const std::string &checkCode)
{
    if (networkIntfc() == nullptr) {
        return ComInvalidId;
    }
    initConnection(com_ptr_t(new ComConnection(m_idNum++, checkCode, devInfo, networkIntfc())));
    return m_idNum;
}

void MultiComMgr::removeLanDev(com_id_t id)
{
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return;
    }
    it->second->disconnect(0);
}

void MultiComMgr::setWanDevToken(const std::string &userName, const std::string &accessToken)
{
    if (networkIntfc() == nullptr) {
        return;
    }
    m_userDataUpdateThd->setToken(userName, accessToken);
}

void MultiComMgr::removeWanDev()
{
    if (networkIntfc() == nullptr) {
        return;
    }
    m_userDataUpdateThd->clearToken();
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN) {
            comPtr.get()->disconnect(0);
        }
    }
}

ComErrno MultiComMgr::bindWanDev(const std::string &serialNumber, unsigned short pid,
    const std::string &name)
{
    if (networkIntfc() == nullptr) {
        return COM_UNINITIALIZED;
    }
    std::string userName, accessToken;
    m_userDataUpdateThd->getToken(userName, accessToken);
    if (accessToken.empty()) {
        return COM_ERROR;
    }
    fnet_wan_dev_bind_data_t *bindData;
    int ret = m_networkIntfc->bindWanDev(
        accessToken.c_str(), serialNumber.c_str(), pid, name.c_str(), &bindData);
    if (ret == FNET_OK) {
        initConnection(com_ptr_t(new ComConnection(
            m_idNum++, accessToken, bindData->serialNumber, bindData->devId, m_networkIntfc.get())));
    }
    return MultiComUtils::fnetRet2ComErrno(ret);
}

ComErrno MultiComMgr::unbindWanDev(const std::string &serialNumber, const std::string &devId)
{
    if (networkIntfc() == nullptr) {
        return COM_UNINITIALIZED;
    }
    std::string userName, accessToken;
    m_userDataUpdateThd->getToken(userName, accessToken);
    if (accessToken.empty()) {
        return COM_ERROR;
    }
    int ret = m_networkIntfc->unbindWanDev(accessToken.c_str(), devId.c_str());
    if (ret == FNET_OK) {
        for (auto &comPtr : m_comPtrs) {
            if (comPtr->deviceId() == devId) {
                comPtr->disconnect(0);
            }
        }
    }
    return MultiComUtils::fnetRet2ComErrno(ret);
}

com_id_list_t MultiComMgr::getReadyDevList()
{
    com_id_list_t idList;
    for (auto &id : m_readyIdSet) {
        idList.push_back(id);
    }
    return idList;
}

const com_dev_data_t &MultiComMgr::devData(com_id_t id, bool *valid /* = nullptr */)
{
    auto it = m_ptrMap.left.find(id);
    if (valid != nullptr) {
        *valid = (it != m_ptrMap.left.end());
    }
    if (it == m_ptrMap.left.end()) {
        return m_datMap.at(ComInvalidId);
    } else {
        return m_datMap.at(it->get_left());
    }
}

void MultiComMgr::putCommand(com_id_t id, const ComCommandPtr &command)
{
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return;
    }
    m_ptrMap.left.at(id)->putCommand(command);
}

void MultiComMgr::initConnection(const com_ptr_t &comPtr)
{
    m_comPtrs.push_back(comPtr);
    m_ptrMap.insert(com_ptr_map_val_t(comPtr->id(), comPtr.get()));
    m_datMap.emplace(comPtr->id(), com_dev_data_t{comPtr->connectMode(), nullptr});
    m_serialNumberSet.insert(comPtr->serialNumber());

    comPtr->Bind(COM_CONNECTION_READY_EVENT, [this](const ComConnectionReadyEvent &event) {
        m_readyIdSet.insert(event.id);
        QueueEvent(event.Clone());
    });
    comPtr->Bind(COM_SEND_GCODE_PROGRESS_EVENT, [this](const ComSendGcodeProgressEvent &event) {
        QueueEvent(event.Clone());
    });
    comPtr->Bind(COM_SEND_GCODE_FINISH_EVENT, [this](const ComSendGcodeFinishEvent &event) {
        QueueEvent(event.Clone());
    });
    comPtr->Bind(COM_CONNECTION_EXIT_EVENT, &MultiComMgr::onConnectionExit, this);
    comPtr->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &MultiComMgr::onDevDetailUpdate, this);
    comPtr->connect();
}

void MultiComMgr::onWanDevMaintian(const ComWanDevMaintainEvent &event)
{
    if (event.ret != COM_OK) {
        for (auto &comPtr : m_comPtrs) {
            if (comPtr->connectMode() == COM_CONNECT_WAN) {
                comPtr.get()->disconnect(0);
            }
        }
    }
    QueueEvent(event.Clone());
}

void MultiComMgr::onWanDevUpdated(const WanDevUpdateEvent &event)
{
    std::string userName, accessToken;
    m_userDataUpdateThd->getToken(userName, accessToken);
    if (accessToken != event.accessToken) {
        return;
    }
    fnet::FreeInDestructorArg freeDevInfos(event.devInfos, m_networkIntfc->freeWanDevList, event.devCnt);
    std::set<std::string> devIdSet;
    for (int i = 0; i < event.devCnt; ++i) {
        devIdSet.insert(event.devInfos[i].serialNumber);
    }
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN
         && devIdSet.find(comPtr->serialNumber()) == devIdSet.end()) {
            comPtr.get()->disconnect(0);
        } else {
            comPtr->setAccessToken(event.accessToken);
        }
    }
    for (int i = 0; i < event.devCnt; ++i) {
        if (m_serialNumberSet.find(event.devInfos[i].serialNumber) == m_serialNumberSet.end()) {
            initConnection(com_ptr_t(new ComConnection(m_idNum++, event.accessToken,
                event.devInfos[i].serialNumber, event.devInfos[i].devId, m_networkIntfc.get())));
        }
    }
}

void MultiComMgr::onConnectionExit(const ComConnectionExitEvent &event)
{
    ComConnection *comConnection = m_ptrMap.left.at(event.id);
    comConnection->joinThread();
    m_networkIntfc->freeDevDetail(m_datMap.at(event.id).devDetail);
    m_readyIdSet.erase(event.id);
    m_serialNumberSet.erase(comConnection->serialNumber());
    m_datMap.erase(event.id);
    m_ptrMap.left.erase(event.id);
    m_comPtrs.remove_if([comConnection](auto &ptr) { return ptr.get() == comConnection; });
    QueueEvent(event.Clone());
}

void MultiComMgr::onDevDetailUpdate(const ComDevDetailUpdateEvent &event)
{
    fnet_dev_detail_t *&devDetail = m_datMap.at(event.id).devDetail;
    m_networkIntfc->freeDevDetail(devDetail);
    devDetail = event.devDetail;
    QueueEvent(event.Clone());
}

}} // namespace Slic3r::GUI
