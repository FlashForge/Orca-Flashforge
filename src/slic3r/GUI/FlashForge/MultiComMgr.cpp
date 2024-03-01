#include "MultiComMgr.hpp"
#include <memory>
#include "FreeInDestructor.h"

namespace Slic3r { namespace GUI {

MultiComMgr::MultiComMgr()
    : m_idNum(ComInvalidId + 1)
{
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_LAN;
    devData.devDetail = nullptr;
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    m_datMap.emplace(ComInvalidId, devData);
}

bool MultiComMgr::initalize(const std::string &newtworkDllPath, const std::string &logFileDir)
{
    if (networkIntfc() != nullptr) {
        return false;
    }
    m_networkIntfc.reset(new fnet::FlashNetworkIntfc(newtworkDllPath.c_str(), logFileDir.c_str(), 72));
    if (!m_networkIntfc->isOk()) {
        m_networkIntfc.reset(nullptr);
        return false;
    }
    m_userDataUpdateThd.reset(new UserDataUpdateThd(m_networkIntfc.get()));
    m_userDataUpdateThd->Bind(COM_GET_USER_PROFILE_EVENT, &MultiComMgr::onGetUserProfile, this);
    m_userDataUpdateThd->Bind(GET_WAN_DEV_EVENT, &MultiComMgr::onGetWanDev, this);
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
    com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum, checkCode, devInfo, networkIntfc());
    com_dev_data_t devData = { COM_CONNECT_LAN, devInfo, com_wan_dev_info_t(), nullptr };
    initConnection(comPtr, devData);
    return m_idNum++;
}

void MultiComMgr::removeLanDev(com_id_t id)
{
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return;
    }
    it->second->disconnect(0);
}

ComErrno MultiComMgr::addWanDev(const std::string &accessToken)
{
    if (networkIntfc() == nullptr) {
        return COM_UNINITIALIZED;
    }
    if (m_wanAsyncConn.get() != nullptr) {
        return COM_ERROR;
    }
    m_wanAsyncConn.reset(new ComWanAsyncConn(m_networkIntfc.get()));
    ComErrno ret = m_wanAsyncConn->createConn(accessToken);
    if (ret != COM_OK) {
        m_wanAsyncConn.reset(nullptr);
        return ret;
    }
    m_wanAsyncConn->Bind(COM_WAN_DEV_MAINTAIN_EVENT, &MultiComMgr::onWanDevMaintian, this);
    m_wanAsyncConn->Bind(WAN_CONN_READ_DATA_EVENT, &MultiComMgr::onWanConnReadData, this);
    m_wanAsyncConn->Bind(WAN_CONN_RECONNECT_EVENT, &MultiComMgr::onWanConnReconnect, this);
    m_userDataUpdateThd->setToken(accessToken);
    m_userDataUpdateThd->setUpdateUserProfile();
    m_userDataUpdateThd->setUpdateWanDev();
    return ret;
}

void MultiComMgr::removeWanDev()
{
    if (networkIntfc() == nullptr || m_wanAsyncConn.get() == nullptr) {
        return;
    }
    m_wanAsyncConn->freeConn();
    m_wanAsyncConn.reset(nullptr);
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN) {
            comPtr.get()->disconnect(0);
        }
    }
}

void MultiComMgr::setWanDevToken(const std::string &accessToken)
{
    if (networkIntfc() == nullptr) {
        return;
    }
    m_userDataUpdateThd->setToken(accessToken);
}

ComErrno MultiComMgr::bindWanDev(const std::string &serialNumber, unsigned short pid,
    const std::string &name)
{
    if (networkIntfc() == nullptr) {
        return COM_UNINITIALIZED;
    }
    std::string accessToken = m_userDataUpdateThd->getToken();
    if (accessToken.empty() || m_userId.empty() || m_wanAsyncConn.get() == nullptr) {
        return COM_ERROR;
    }
    fnet_wan_dev_bind_data_t *bindData;
    int ret = m_networkIntfc->bindWanDev(
        accessToken.c_str(), serialNumber.c_str(), pid, name.c_str(), &bindData, ComTimeoutWan);
    fnet::FreeInDestructor freeBinData(bindData, m_networkIntfc->freeBindData);
    if (ret == FNET_OK) {
        m_userDataUpdateThd->setUpdateWanDev();
        m_wanAsyncConn->postSyncBindDev(m_userId, bindData->devId);
    }
    return MultiComUtils::fnetRet2ComErrno(ret);
}

ComErrno MultiComMgr::unbindWanDev(const std::string &serialNumber, const std::string &devId)
{
    if (networkIntfc() == nullptr) {
        return COM_UNINITIALIZED;
    }
    std::string accessToken = m_userDataUpdateThd->getToken();
    if (accessToken.empty() || m_userId.empty() || m_wanAsyncConn.get() == nullptr) {
        return COM_ERROR;
    }
    int ret = m_networkIntfc->unbindWanDev(accessToken.c_str(), devId.c_str(), ComTimeoutWan);
    if (ret == FNET_OK) {
        m_wanAsyncConn->postSyncUnbindDev(m_userId, devId);
        for (auto &comPtr : m_comPtrs) {
            if (comPtr->deviceId() == devId) {
                comPtr->disconnect(0);
                break;
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

void MultiComMgr::putCommand(com_id_t id, ComCommand *command)
{
    ComCommandPtr commandPtr(command);
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return;
    }
    ComWanAsyncCommand *wanAsyncCommand = dynamic_cast<ComWanAsyncCommand *>(command);
    if (it->second->connectMode() != COM_CONNECT_WAN || wanAsyncCommand == nullptr) {
        m_ptrMap.left.at(id)->putCommand(commandPtr);
    } else {
        wanAsyncCommand->asyncExec(m_wanAsyncConn.get(), it->second->deviceId());
    }
}

void MultiComMgr::abortSendGcode(com_id_t id, int commandId)
{
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return;
    }
    m_ptrMap.left.at(id)->abortSendGcode(commandId);
}

void MultiComMgr::initConnection(const com_ptr_t &comPtr, const com_dev_data_t &devData)
{
    m_comPtrs.push_back(comPtr);
    m_ptrMap.insert(com_ptr_map_val_t(comPtr->id(), comPtr.get()));
    m_datMap.emplace(comPtr->id(), devData);
    if (devData.connectMode == COM_CONNECT_WAN) {
        m_devIdMap.emplace(devData.wanDevInfo.devId, comPtr->id());
    }
    comPtr->Bind(COM_SEND_GCODE_PROGRESS_EVENT, [this](const ComSendGcodeProgressEvent &event) {
        QueueEvent(event.Clone());
    });
    comPtr->Bind(COM_SEND_GCODE_FINISH_EVENT, [this](const ComSendGcodeFinishEvent &event) {
        QueueEvent(event.Clone());
    });
    comPtr->Bind(COM_CONNECTION_READY_EVENT, &MultiComMgr::onConnectionReady, this);
    comPtr->Bind(COM_CONNECTION_EXIT_EVENT, &MultiComMgr::onConnectionExit, this);
    comPtr->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &MultiComMgr::onDevDetailUpdate, this);
    comPtr->connect();
}

void MultiComMgr::onWanDevMaintian(const ComWanDevMaintainEvent &event)
{
    if (event.ret != COM_OK) {
        removeWanDev();
        m_userId.clear();
    }
    QueueEvent(event.Clone());
}

void MultiComMgr::onGetUserProfile(const ComGetUserProfileEvent &event)
{
    if (event.ret != COM_OK) {
        onWanDevMaintian(ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, event.ret));
        return;
    }
    m_userId = event.userProfile.uid;
    m_wanAsyncConn->postSubscribeApp(m_userId);
    QueueEvent(event.Clone());
}

void MultiComMgr::onGetWanDev(const GetWanDevEvent &event)
{
    if (event.ret != COM_OK) {
        onWanDevMaintian(ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, event.ret));
        return;
    }
    fnet::FreeInDestructorArg freeDevInfos(event.devInfos, m_networkIntfc->freeWanDevList, event.devCnt);
    if (m_userDataUpdateThd->getToken() != event.accessToken || m_wanAsyncConn.get() == nullptr) {
        return;
    }
    std::set<std::string> devIdSet;
    for (int i = 0; i < event.devCnt; ++i) {
        devIdSet.insert(event.devInfos[i].devId);
    }
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN
         && devIdSet.find(comPtr->deviceId()) == devIdSet.end()) {
            comPtr.get()->disconnect(0);
        } else {
            comPtr->setAccessToken(event.accessToken);
        }
    }
    std::vector<std::string> addDevIds;
    for (int i = 0; i < event.devCnt; ++i) {
        if (m_devIdMap.find(event.devInfos[i].devId) == m_devIdMap.end()) {
            com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, event.accessToken,
                event.devInfos[i].serialNumber, event.devInfos[i].devId, networkIntfc());
            initConnection(comPtr, makeDevData(&event.devInfos[i]));
            addDevIds.push_back(event.devInfos[i].devId);
        }
    }
    m_wanAsyncConn->postSubscribeDev(addDevIds);
}

void MultiComMgr::onConnectionReady(const ComConnectionReadyEvent &event)
{
    fnet_dev_detail_t *&devDetail = m_datMap.at(event.id).devDetail;
    m_networkIntfc->freeDevDetail(devDetail);
    devDetail = event.devDetail;
    m_readyIdSet.insert(event.id);
    QueueEvent(event.Clone());
}

void MultiComMgr::onConnectionExit(const ComConnectionExitEvent &event)
{
    ComConnection *comConnection = m_ptrMap.left.at(event.id);
    comConnection->joinThread();
    m_networkIntfc->freeDevDetail(m_datMap.at(event.id).devDetail);
    m_readyIdSet.erase(event.id);
    if (comConnection->connectMode() == COM_CONNECT_WAN) {
        m_devIdMap.erase(m_datMap.at(comConnection->id()).wanDevInfo.devId);
    }
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

void MultiComMgr::onWanConnReadData(const WanConnReadDataEvent &event)
{
    auto procDevDetailUpdateEvent = [this](const fnet_conn_read_data_t &readData) {
        auto it = m_devIdMap.find(readData.devId);
        if (it != m_devIdMap.end()) {
            ComDevDetailUpdateEvent devDetailUpdateEvent(COM_DEV_DETAIL_UPDATE_EVENT,
                it->second, ComInvalidCommandId, (fnet_dev_detail_t *)readData.data);
            onDevDetailUpdate(devDetailUpdateEvent);
        }
    };
    auto procDevOfflineEvent = [this](const fnet_conn_read_data_t &readData) {
        auto it = m_devIdMap.find(readData.devId);
        if (it != m_devIdMap.end()) {
            QueueEvent(new ComDevOfflineEvent(COM_DEV_OFFLINE_EVENT, it->second));
        }
    };
    switch (event.readData.type) {
    case FNET_CONN_READ_SYNC_BIND_DEVICE:
    case FNET_CONN_READ_SYNC_UNBIND_DEVICE:
        m_userDataUpdateThd->setUpdateWanDev();
        break;
    case FNET_CONN_READ_DEVICE_DETAIL:
        procDevDetailUpdateEvent(event.readData);
        break;
    case FNET_CONN_READ_DEVICE_OFFLINE:
        procDevOfflineEvent(event.readData);
        break;
    }
    m_networkIntfc->freeString(event.readData.devId);
}

void MultiComMgr::onWanConnReconnect(const wxCommandEvent &)
{
    if (m_wanAsyncConn.get() == nullptr) {
        return;
    }
    std::vector<std::string> devIds;
    for (auto &item : m_devIdMap) {
        devIds.push_back(item.first);
    }
    m_wanAsyncConn->postSubscribeApp(m_userId);
    m_wanAsyncConn->postSubscribeDev(devIds);
    m_userDataUpdateThd->setUpdateWanDev();
}

com_dev_data_t MultiComMgr::makeDevData(const fnet_wan_dev_info_t *wanDevInfo)
{
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_WAN;
    devData.wanDevInfo.devId = wanDevInfo->devId;
    devData.wanDevInfo.name = wanDevInfo->name;
    devData.wanDevInfo.model = wanDevInfo->model;
    devData.wanDevInfo.imageUrl = wanDevInfo->imageUrl;
    devData.wanDevInfo.status = wanDevInfo->status;
    devData.wanDevInfo.location = wanDevInfo->location;
    devData.wanDevInfo.serialNumber = wanDevInfo->serialNumber;
    devData.devDetail = nullptr;
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    return devData;
}

}} // namespace Slic3r::GUI
