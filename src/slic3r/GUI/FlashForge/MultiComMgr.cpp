#include "MultiComMgr.hpp"
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem.hpp>
#include "FreeInDestructor.h"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

MultiComMgr::MultiComMgr()
    : m_idNum(ComInvalidId + 1)
    , m_login(false)
    , m_procPendingWanDevTimer(this)
{
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_LAN;
    devData.devProduct = nullptr;
    devData.devDetail = nullptr;
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    m_datMap.emplace(ComInvalidId, devData);
    Bind(wxEVT_TIMER, &MultiComMgr::onTimer, this);
}

bool MultiComMgr::initalize(const std::string &dllPath, const std::string &logFileDir)
{
    if (networkIntfc() != nullptr) {
        return false;
    }
    std::string dirPath = boost::dll::program_location().parent_path().string();
    bool debug = boost::filesystem::exists(dirPath + "/FLASHNETWORK_DEBUG");

    fnet_log_settings_t logSettings;
    logSettings.fileDir = logFileDir.c_str();
    logSettings.expireHours = 72;
    logSettings.level = debug ? FNET_LOG_LEVEL_DEBUG : FNET_LOG_LEVEL_INFO;

    std::string serverSettingsPath = dirPath + "/FLASHNETWORK.DAT";
    m_networkIntfc.reset(new fnet::FlashNetworkIntfc(
        dllPath.c_str(), serverSettingsPath.c_str(), logSettings));
    if (!m_networkIntfc->isOk()) {
        m_networkIntfc.reset(nullptr);
        return false;
    }
    m_wanDevMaintainThd.reset(new WanDevMaintainThd(m_networkIntfc.get()));
    m_wanDevMaintainThd->Bind(RELOGIN_EVENT, &MultiComMgr::onRelogin, this);
    m_wanDevMaintainThd->Bind(GET_WAN_DEV_EVENT, &MultiComMgr::onUpdateWanDev, this);
    m_wanDevMaintainThd->Bind(COM_GET_USER_PROFILE_EVENT, &MultiComMgr::onUpdateUserProfile, this);
    m_sendGcodeThd.reset(new WanDevSendGcodeThd(m_networkIntfc.get()));
    m_sendGcodeThd->Bind(COM_SEND_GCODE_PROGRESS_EVENT, &MultiComMgr::onWanSendGcodeProgress, this);
    m_sendGcodeThd->Bind(COM_SEND_GCODE_FINISH_EVENT, &MultiComMgr::onWanSendGcodeFinish, this);
    return true;
}

void MultiComMgr::uninitalize()
{
    if (networkIntfc() == nullptr) {
        return;
    }
    m_sendGcodeThd->exit();
    m_sendGcodeThd.reset(nullptr);
    m_wanDevMaintainThd->exit();
    m_wanDevMaintainThd.reset(nullptr);
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

ComErrno MultiComMgr::addWanDev(const com_token_data_t &tokenData, int tryCnt, int tryMsInterval)
{
    auto tryDo = [tryCnt, tryMsInterval](const std::function<ComErrno()> &func) {
        ComErrno ret = COM_ERROR;
        for (int i = 0; i < tryCnt; ++i) {
            ret = func();
            if (ret == COM_OK) {
                return ret;
            } else if (i + 1 < tryCnt) {
                boost::this_thread::sleep_for(boost::chrono::milliseconds(tryMsInterval));
            }
        }
        return ret;
    };
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::addWanDev";
    if (networkIntfc() == nullptr || m_login) {
        return COM_ERROR;
    }
    com_user_profile_t userProfile;
    ComErrno ret = tryDo([&]() {
        return MultiComUtils::getUserProfile(tokenData.accessToken, userProfile);
    });
    if (ret != COM_OK) {
        return ret;
    }
    m_wanAsyncConn.reset(new ComWanAsyncConn(m_networkIntfc.get()));
    ret = tryDo([&]() {
        return m_wanAsyncConn->createConn(userProfile.uid, tokenData.accessToken);
    });
    if (ret != COM_OK) {
        m_wanAsyncConn.reset(nullptr);
        return ret;
    }
    m_login = true;
    m_uid = userProfile.uid;
    WanDevTokenMgr::inst()->Bind(COM_REFRESH_TOKEN_EVENT, &MultiComMgr::onRefreshToken, this);
    WanDevTokenMgr::inst()->start(tokenData, networkIntfc()); // initialize global token
    m_wanAsyncConn->Bind(WAN_CONN_READ_DATA_EVENT, &MultiComMgr::onWanConnReadData, this);
    m_wanAsyncConn->Bind(WAN_CONN_RECONNECT_EVENT, &MultiComMgr::onWanConnReconnect, this);
    m_wanAsyncConn->Bind(WAN_CONN_EXIT_EVENT, &MultiComMgr::onWanConnExit, this);
    m_wanAsyncConn->postSubscribeAppSlicer(userProfile.uid);
    m_wanAsyncConn->postSyncSlicerLogin(userProfile.uid);
    m_wanDevMaintainThd->setUid(userProfile.uid);
    m_wanDevMaintainThd->setUpdateWanDev();
    m_procPendingWanDevTimer.Start(3000);
    onUpdateUserProfile(ComGetUserProfileEvent(COM_GET_USER_PROFILE_EVENT, userProfile, COM_OK));
    return ret;
}

void MultiComMgr::removeWanDev()
{
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::removeWanDev";
    if (!m_login) {
        return;
    }
    m_login = false;
    WanDevTokenMgr::inst()->exit();
    m_procPendingWanDevTimer.Stop();
    m_wanDevMaintainThd->stop();
    if (m_wanAsyncConn.get() != nullptr) {
        m_wanAsyncConn->freeConn();
        m_wanAsyncConn.reset(nullptr);
    }
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN) {
            comPtr.get()->disconnect(0);
        }
    }
}

ComErrno MultiComMgr::bindWanDev(const std::string &serialNumber, unsigned short pid,
    const std::string &name)
{
    if (m_wanAsyncConn.get() == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_wan_dev_bind_data_t *bindData;
    int ret = m_networkIntfc->bindWanDev(m_uid.c_str(), token.accessToken().c_str(),
        serialNumber.c_str(), pid, name.c_str(), &bindData, ComTimeoutWan);
    fnet::FreeInDestructor freeBinData(bindData, m_networkIntfc->freeBindData);
    if (ret == FNET_OK) {
        m_wanAsyncConn->postSyncBindDev(m_uid, bindData->devId);
        m_wanDevMaintainThd->setUpdateWanDev();
    }
    return MultiComUtils::fnetRet2ComErrno(ret);
}

ComErrno MultiComMgr::unbindWanDev(const std::string &serialNumber, const std::string &devId)
{
    if (m_wanAsyncConn.get() == nullptr) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    int ret = m_networkIntfc->unbindWanDev(
        m_uid.c_str(), token.accessToken().c_str(), devId.c_str(), ComTimeoutWan);
    if (ret == FNET_OK) {
        m_wanAsyncConn->postSyncUnbindDev(m_uid, devId);
        for (auto &comPtr : m_comPtrs) {
            if (comPtr->deviceId() == devId) {
                if (m_readyIdSet.find(comPtr->id()) != m_readyIdSet.end()) {
                    const char *name = m_datMap.at(comPtr->id()).devDetail->name;
                    BOOST_LOG_TRIVIAL(info) << name << ", " << serialNumber << ", unbind_disconnect";
                }
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

bool MultiComMgr::putCommand(com_id_t id, ComCommand *command)
{
    ComCommandPtr commandPtr(command);
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return false;
    }
    ComWanAsyncCommand *wanAsyncCommand = dynamic_cast<ComWanAsyncCommand *>(command);
    if (it->second->connectMode() != COM_CONNECT_WAN || wanAsyncCommand == nullptr) {
        m_ptrMap.left.at(id)->putCommand(commandPtr);
        return true;
    } else if (m_wanAsyncConn.get() != nullptr) {
        wanAsyncCommand->asyncExec(m_wanAsyncConn.get(), it->second->deviceId());
        return true;
    }
    return false;
}

bool MultiComMgr::abortSendGcode(com_id_t id, int commandId)
{
    auto it = m_ptrMap.left.find(id);
    if (it == m_ptrMap.left.end()) {
        return false;
    }
    m_ptrMap.left.at(id)->abortSendGcode(commandId);
    return true;
}

bool MultiComMgr::wanSendGcode(const std::vector<std::string> &devIds, const std::string &gcodeFilePath,
    const std::string &thumbFilePath, const std::string &gcodeDstName, bool printNow,
    bool levelingBeforePrint)
{
    return m_sendGcodeThd->startSendGcode(
        m_uid, devIds, gcodeFilePath, thumbFilePath, gcodeDstName, printNow, levelingBeforePrint);
}

bool MultiComMgr::abortWanSendGcode()
{
    return m_sendGcodeThd->abortSendGcode();
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
    comPtr->Bind(COMMAND_FAILED_EVENT, &MultiComMgr::onCommandFailed, this);
    comPtr->connect();
}

void MultiComMgr::onTimer(const wxTimerEvent &event)
{
    if (m_wanAsyncConn.get() == nullptr) {
        m_pendingWanDevDatas.clear();
        return;
    }
    std::vector<std::string> addDevIds;
    for (auto it = m_pendingWanDevDatas.begin(); it != m_pendingWanDevDatas.end();) {
        if (m_devIdMap.find(it->wanDevInfo.devId) == m_devIdMap.end()) {
            com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, m_uid,
                it->wanDevInfo.serialNumber, it->wanDevInfo.devId, networkIntfc());
            initConnection(comPtr, *it);
            addDevIds.push_back(it->wanDevInfo.devId);
            it = m_pendingWanDevDatas.erase(it);
        } else {
            ++it;
        }
    }
    m_wanAsyncConn->postSubscribeDev(addDevIds);
}

void MultiComMgr::onRelogin(ReloginEvent &event)
{
    if (!m_login || m_wanAsyncConn.get() != nullptr
     || event.ret != COM_OK && event.ret != COM_UNAUTHORIZED) {
        m_networkIntfc->freeWanDevList(event.devInfos, event.devCnt);
        event.wanAsyncConn->freeConn();
        return;
    }
    if (event.ret == COM_UNAUTHORIZED && !WanDevTokenMgr::inst()->tokenExpired(event.accessToken)) {
        m_networkIntfc->freeWanDevList(event.devInfos, event.devCnt);
        event.wanAsyncConn->freeConn();
        removeWanDev();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, false, false, event.ret));
        return;
    }
    m_wanAsyncConn.swap(event.wanAsyncConn);
    m_wanAsyncConn->Bind(WAN_CONN_READ_DATA_EVENT, &MultiComMgr::onWanConnReadData, this);
    m_wanAsyncConn->Bind(WAN_CONN_RECONNECT_EVENT, &MultiComMgr::onWanConnReconnect, this);
    m_wanAsyncConn->Bind(WAN_CONN_EXIT_EVENT, &MultiComMgr::onWanConnExit, this);

    std::vector<std::string> devIds;
    for (auto &item : m_devIdMap) {
        devIds.push_back(item.first);
    }
    m_wanAsyncConn->postSubscribeAppSlicer(event.uid);
    m_wanAsyncConn->postSubscribeDev(devIds);
    m_wanDevMaintainThd->setUpdateUserProfile();

    GetWanDevEvent updateWanDevEvent;
    updateWanDevEvent.SetEventType(GET_WAN_DEV_EVENT);
    updateWanDevEvent.ret = event.ret;
    updateWanDevEvent.uid = event.uid;
    updateWanDevEvent.devInfos = event.devInfos;
    updateWanDevEvent.devCnt = event.devCnt;
    onUpdateWanDev(updateWanDevEvent);

    QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, true, COM_OK));
}

void MultiComMgr::onUpdateWanDev(const GetWanDevEvent &event)
{
    fnet::FreeInDestructorArg freeDevInfos(event.devInfos, m_networkIntfc->freeWanDevList, event.devCnt);
    if (m_uid != event.uid || m_wanAsyncConn.get() == nullptr) {
        return;
    }
    if (event.ret != COM_OK) {
        maintianWanDev(event.ret);
        return;
    }
    std::map<std::string, fnet_wan_dev_info_t *> devInfoMap;
    for (int i = 0; i < event.devCnt; ++i) {
        const char *devId = event.devInfos[i].devId;
        if (devInfoMap.find(devId) != devInfoMap.end()) {
            BOOST_LOG_TRIVIAL(fatal) << devId << ", duplicated_devId";
        }
        devInfoMap.emplace(event.devInfos[i].devId, &event.devInfos[i]);
    }
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN) {
            auto it = devInfoMap.find(comPtr->deviceId());
            if (it == devInfoMap.end()) {
                comPtr.get()->disconnect(0);
            } else {
                updateWanDevInfo(comPtr->id(), it->second->name, it->second->status,
                    it->second->location);
            }
        }
    }
    std::vector<std::string> addDevIds;
    m_pendingWanDevDatas.clear();
    for (int i = 0; i < event.devCnt; ++i) {
        auto it = m_devIdMap.find(event.devInfos[i].devId);
        if (it == m_devIdMap.end()) {
            com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, m_uid,
                event.devInfos[i].serialNumber, event.devInfos[i].devId, networkIntfc());
            initConnection(comPtr, makeDevData(&event.devInfos[i]));
            addDevIds.push_back(event.devInfos[i].devId);
        } else if (m_ptrMap.left.at(it->second)->isDisconnect()) {
            m_pendingWanDevDatas.push_back(makeDevData(&event.devInfos[i]));
        }
    }
    m_wanAsyncConn->postSubscribeDev(addDevIds);
}

void MultiComMgr::onUpdateUserProfile(const ComGetUserProfileEvent &event)
{
    if (m_wanAsyncConn.get() == nullptr) {
        return;
    }
    if (event.ret == COM_UNAUTHORIZED) {
        maintianWanDev(event.ret);
    } else if (event.ret != COM_OK) {
        m_wanDevMaintainThd->setUpdateUserProfile();
    } else {
        QueueEvent(event.Clone());
    }
}

void MultiComMgr::onConnectionReady(const ComConnectionReadyEvent &event)
{
    com_dev_data_t &devData = m_datMap.at(event.id);
    devData.devProduct = event.devProduct;
    devData.devDetail = event.devDetail;
    m_readyIdSet.insert(event.id);
    QueueEvent(event.Clone());

    const char *name = m_datMap.at(event.id).devDetail->name;
    const std::string &serialNumber = m_ptrMap.left.at(event.id)->serialNumber();
    BOOST_LOG_TRIVIAL(info) << name << ", " << serialNumber << ", connection_ready";
}

void MultiComMgr::onConnectionExit(const ComConnectionExitEvent &event)
{
    if (m_readyIdSet.find(event.id) != m_readyIdSet.end()) {
        const char *name = m_datMap.at(event.id).devDetail->name;
        const std::string &serialNumber = m_ptrMap.left.at(event.id)->serialNumber();
        BOOST_LOG_TRIVIAL(info) << name << ", " << serialNumber << ", connection_exit";
    }
    ComConnection *comConnection = m_ptrMap.left.at(event.id);
    comConnection->joinThread();
    com_dev_data_t &devData = m_datMap.at(event.id);
    m_networkIntfc->freeDevProduct(devData.devProduct);
    m_networkIntfc->freeDevDetail(devData.devDetail);
    m_readyIdSet.erase(event.id);
    if (comConnection->connectMode() == COM_CONNECT_WAN) {
        m_devIdMap.erase(devData.wanDevInfo.devId);
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
    if (m_readyIdSet.find(event.id) != m_readyIdSet.end()) {
        QueueEvent(event.Clone());
    }
    updateWanDevInfo(event.id, devDetail->name, devDetail->status, devDetail->location);
}

void MultiComMgr::onCommandFailed(const CommandFailedEvent &event)
{
    if (m_wanAsyncConn.get() == nullptr) {
        return;
    }
    if (event.fatalError || event.ret == COM_UNAUTHORIZED) {
        maintianWanDev(event.ret);
    } else {
        m_wanDevMaintainThd->setUpdateWanDev();
    }
}

void MultiComMgr::onWanConnReadData(const WanConnReadDataEvent &event)
{
    auto procDevDetailUpdate = [this](const fnet_conn_read_data_t &readData) {
        auto it = m_devIdMap.find(readData.devId);
        if (it != m_devIdMap.end()) {
            ComDevDetailUpdateEvent devDetailUpdateEvent(COM_DEV_DETAIL_UPDATE_EVENT,
                it->second, ComInvalidCommandId, (fnet_dev_detail_t *)readData.data);
            onDevDetailUpdate(devDetailUpdateEvent);
        }
    };
    auto procDevOffline = [this](const fnet_conn_read_data_t &readData) {
        auto it = m_devIdMap.find(readData.devId);
        if (it != m_devIdMap.end()) {
            m_datMap.at(it->second).wanDevInfo.status = "offline";
            if (m_readyIdSet.find(it->second) != m_readyIdSet.end()) {
                QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, it->second));
            }
        }
    };
    switch (event.readData.type) {
    case FNET_CONN_READ_SYNC_SLICER_LOGIN:
        maintianWanDev(COM_REPEAT_LOGIN);
        break;
    case FNET_CONN_READ_SYNC_USER_PROFILE:
        m_wanDevMaintainThd->setUpdateUserProfile();
        break;
    case FNET_CONN_READ_SYNC_BIND_DEVICE:
    case FNET_CONN_READ_SYNC_UNBIND_DEVICE:
        m_wanDevMaintainThd->setUpdateWanDev();
        break;
    case FNET_CONN_READ_UNREGISTER_USER:
        maintianWanDev(COM_UNREGISTER_USER);
        break;
    case FNET_CONN_READ_DEVICE_DETAIL:
        procDevDetailUpdate(event.readData);
        break;
    case FNET_CONN_READ_DEVICE_OFFLINE:
        procDevOffline(event.readData);
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
    m_wanAsyncConn->postSubscribeAppSlicer(m_uid);
    m_wanAsyncConn->postSubscribeDev(devIds);
    m_wanDevMaintainThd->setUpdateWanDev();
}

void MultiComMgr::onWanConnExit(const WanConnExitEvent &event)
{
    if (m_wanAsyncConn.get() == nullptr) {
        return;
    }
    maintianWanDev(event.ret);
}

void MultiComMgr::onRefreshToken(const ComRefreshTokenEvent &event)
{
    if (!m_login || event.ret != COM_OK) {
        return;
    }
    QueueEvent(event.Clone());
}

void MultiComMgr::onWanSendGcodeProgress(const ComSendGcodeProgressEvent &event)
{
    QueueEvent(event.Clone());
}

void MultiComMgr::onWanSendGcodeFinish(const ComSendGcodeFinishEvent &event)
{
    QueueEvent(event.Clone());
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
    devData.devProduct = nullptr;
    devData.devDetail = nullptr;
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    return devData;
}

void MultiComMgr::maintianWanDev(ComErrno ret)
{
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::maintianWanDev " << (int)ret;
    if (ret == COM_UNREGISTER_USER || ret == COM_REPEAT_LOGIN) {
        removeWanDev();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, false, false, ret));
        return;
    }
    if (ret != COM_OK) {
        m_wanAsyncConn->freeConn();
        m_wanAsyncConn.reset(nullptr);
        m_wanDevMaintainThd->setRelogin();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, false, ret));
    }
}

void MultiComMgr::updateWanDevInfo(com_id_t id, const std::string &name, const std::string &status,
    const std::string &location)
{
    com_dev_data_t &devData = m_datMap.at(id);
    if (devData.connectMode != COM_CONNECT_WAN) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << name << " status---" << status;
    devData.wanDevInfo.name = name;
    devData.wanDevInfo.status = status;
    devData.wanDevInfo.location = location;
    if (m_readyIdSet.find(id) != m_readyIdSet.end()) {
        QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, id));
    }
}

}} // namespace Slic3r::GUI
