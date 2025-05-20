#include "MultiComMgr.hpp"
#include <boost/interprocess/sync/file_lock.hpp>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include "CleanNimData.hpp"
#include "FreeInDestructor.h"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

MultiComMgr::MultiComMgr()
    : m_idNum(ComInvalidId + 1)
    , m_login(false)
    , m_httpOnline(false)
    , m_nimOnline(false)
    , m_nimFirstLogined(true)
    , m_loopCheckTimer(this)
    , m_nimDataFileLockName("ff_file_lock")
{
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_LAN;
    devData.devProduct = nullptr;
    devData.devDetail = nullptr;
    devData.lanGcodeList.gcodeCnt = 0;
    devData.lanGcodeList.gcodeDatas = nullptr;
    devData.wanGcodeList.gcodeCnt = 0;
    devData.wanGcodeList.gcodeDatas = nullptr;
    devData.wanTimeLapseVideoList.videoCnt = 0;
    devData.wanTimeLapseVideoList.videoDatas = nullptr;
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    m_datMap.emplace(ComInvalidId, devData);
    Bind(wxEVT_TIMER, &MultiComMgr::onTimer, this);
}

bool MultiComMgr::initalize(const std::string &dllPath, const std::string &dataDir)
{
    if (networkIntfc() != nullptr) {
        return false;
    }
    wxFileName appFileName(wxStandardPaths::Get().GetExecutablePath());
    wxString appPathWithSep = appFileName.GetPathWithSep();
    std::string logFileDir = dataDir + "/FlashNetwork";
    bool debug = wxFileName::FileExists(appPathWithSep + "FLASHNETWORK_DEBUG");

    fnet_log_settings_t logSettings;
    logSettings.fileDir = logFileDir.c_str();
    logSettings.expireHours = 72;
    logSettings.level = debug ? FNET_LOG_LEVEL_DEBUG : FNET_LOG_LEVEL_INFO;

    std::string serverSettingsPath = (appPathWithSep + "FLASHNETWORK2.DAT").ToUTF8().data();
    m_networkIntfc.reset(new fnet::FlashNetworkIntfc(
        dllPath.c_str(), serverSettingsPath.c_str(), logSettings));
    if (!m_networkIntfc->isOk()) {
        BOOST_LOG_TRIVIAL(error) << "initalize FlashNetwork failed: " << dllPath;
        m_networkIntfc.reset();
        return false;
    }
    m_wanDevMaintainThd.reset(new WanDevMaintainThd(m_networkIntfc.get()));
    m_wanDevMaintainThd->Bind(RELOGIN_HTTP_EVENT, &MultiComMgr::onReloginHttp, this);
    m_wanDevMaintainThd->Bind(GET_WAN_DEV_EVENT, &MultiComMgr::onUpdateWanDev, this);
    m_wanDevMaintainThd->Bind(COM_GET_USER_PROFILE_EVENT, &MultiComMgr::onUpdateUserProfile, this);

    auto queueEvent = [this](auto &event) { QueueEvent(event.Clone()); };
    m_sendGcodeThd.reset(new WanDevSendGcodeThd(m_networkIntfc.get()));
    m_sendGcodeThd->Bind(COM_SEND_GCODE_PROGRESS_EVENT, queueEvent);
    m_sendGcodeThd->Bind(COM_SEND_GCODE_FINISH_EVENT, queueEvent);

    m_threadPool.reset(new ComThreadPool(5, 30000));
    m_threadExitEvent.set(false);
    m_loopCheckTimer.Start(1000);

    std::string nimAppDir = getNimAppDir(dataDir);
    CleanNimData::inst()->run(nimAppDir, m_nimDataFileLockName);
    ComWanNimConn::inst()->initalize(networkIntfc(), nimAppDir.c_str());
    ComWanNimConn::inst()->Bind(WAN_CONN_STATUS_EVENT, &MultiComMgr::onWanConnStatus, this);
    ComWanNimConn::inst()->Bind(WAN_CONN_READ_EVENT, &MultiComMgr::onWanConnRead, this);
    ComWanNimConn::inst()->Bind(WAN_CONN_SUBSCRIBE_EVENT, &MultiComMgr::onWanConnSubscribe, this);
    WanDevTokenMgr::inst()->Bind(COM_REFRESH_TOKEN_EVENT, &MultiComMgr::onRefreshToken, this);
    return true;
}

void MultiComMgr::uninitalize()
{
    if (networkIntfc() == nullptr) {
        return;
    }
    m_loopCheckTimer.Stop();
    m_threadExitEvent.set(true);
    removeWanDev();
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_LAN) {
            comPtr->disconnect(0);
        }
        comPtr->joinThread();
    }
    WanDevTokenMgr::inst()->Unbind(COM_REFRESH_TOKEN_EVENT, &MultiComMgr::onRefreshToken, this);
    ComWanNimConn::inst()->Unbind(WAN_CONN_STATUS_EVENT, &MultiComMgr::onWanConnStatus, this);
    ComWanNimConn::inst()->Unbind(WAN_CONN_READ_EVENT, &MultiComMgr::onWanConnRead, this);
    ComWanNimConn::inst()->Unbind(WAN_CONN_SUBSCRIBE_EVENT, &MultiComMgr::onWanConnSubscribe, this);
    ComWanNimConn::inst()->uninitalize();
    m_threadPool.reset();
    m_sendGcodeThd->exit();
    m_sendGcodeThd.reset();
    m_wanDevMaintainThd->exit();
    m_wanDevMaintainThd.reset();
    m_networkIntfc.reset();
}

fnet::FlashNetworkIntfc *MultiComMgr::networkIntfc()
{
    return m_networkIntfc.get();
}

com_id_t MultiComMgr::addLanDev(const fnet_lan_dev_info_t &devInfo, const std::string &checkCode)
{
    if (networkIntfc() == nullptr) {
        return ComInvalidId;
    }
    com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum, checkCode, devInfo, networkIntfc());
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_LAN;
    devData.lanDevInfo = devInfo;
    devData.devProduct = nullptr;
    devData.devDetail = nullptr;
    devData.lanGcodeList.gcodeCnt = 0;
    devData.lanGcodeList.gcodeDatas = nullptr;
    devData.wanGcodeList.gcodeCnt = 0;
    devData.wanGcodeList.gcodeDatas = nullptr;
    devData.wanTimeLapseVideoList.videoCnt = 0;
    devData.wanTimeLapseVideoList.videoDatas = nullptr;
    devData.devDetailUpdated = false;
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
            if (ret == COM_OK || ret == COM_UNAUTHORIZED) {
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
        return MultiComUtils::getUserProfile(tokenData.accessToken, userProfile, ComTimeoutWanA);
    });
    if (ret != COM_OK) {
        return ret;
    }
    com_nim_data_t nimData;
    ret = tryDo([&]() {
        return MultiComUtils::getNimData(userProfile.uid, tokenData.accessToken, nimData, ComTimeoutWanA);
    });
    if (ret != COM_OK) {
        return ret;
    }
    m_login = true;
    m_httpOnline = true;
    m_nimOnline = true;
    m_nimFirstLogined = true;
    m_uid = userProfile.uid;
    m_nimData = nimData;
    m_subscribeTime = std_precise_clock::now();
    m_blockCommandFailedUpdate = false;
    m_commandFailedUpdateTime = std_precise_clock::time_point::min();
    m_wanDevMaintainThd->setUid(userProfile.uid);
    WanDevTokenMgr::inst()->start(tokenData, networkIntfc()); // initialize global token
    //
    ret = ComWanNimConn::inst()->createConn(nimData.nimDataId.c_str());
    if (ret != COM_OK) {
        m_login = false;
        m_httpOnline = false;
        m_nimOnline = false;
        return ret;
    }
    m_wanDevMaintainThd->setUpdateWanDev();
    QueueEvent(new ComGetUserProfileEvent(COM_GET_USER_PROFILE_EVENT, userProfile, ret));
    return ret;
}

void MultiComMgr::removeWanDev()
{
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::removeWanDev";
    if (!m_login) {
        return;
    }
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN) {
            comPtr->disconnect(0);
        }
    }
    m_login = false;
    m_httpOnline = false;
    m_nimOnline = false;
    m_wanDevMaintainThd->stop();
    WanDevTokenMgr::inst()->exit();
    ComWanNimConn::inst()->freeConn();
}

ComErrno MultiComMgr::bindWanDev(const std::string &ip, unsigned short port,
    const std::string &serialNumber, unsigned short pid, const std::string &name)
{
    if (!m_httpOnline || !m_nimOnline) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    fnet_wan_dev_bind_data_t *bindData;
    int ret = m_networkIntfc->bindWanDev(m_uid.c_str(), token.accessToken().c_str(),
        serialNumber.c_str(), pid, name.c_str(), &bindData, ComTimeoutWanA);
    fnet::FreeInDestructor freeBinData(bindData, m_networkIntfc->freeBindData);
    if (ret == FNET_OK) {
        m_threadPool->post([this, ip, port, serialNumber]() {
            for (int i = 0; i < 3 && !m_threadExitEvent.get(); ++i) {
                int ret = m_networkIntfc->notifyLanDevWanBind(
                    ip.c_str(), port, serialNumber.c_str(), ComTimeoutLanA);
                if (ret == FNET_OK) {
                    break;
                }
                m_threadExitEvent.waitTrue(3000);
            }
        });
        ComWanNimConn::inst()->syncBindDev(m_nimData.appNimAccountId, bindData->devId);
        m_wanDevMaintainThd->setUpdateWanDev();
    }
    return MultiComUtils::fnetRet2ComErrno(ret);
}

ComErrno MultiComMgr::unbindWanDev(const std::string &serialNumber, const std::string &devId,
    const std::string &nimAccountId)
{
    if (!m_httpOnline || !m_nimOnline) {
        return COM_ERROR;
    }
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    int ret = m_networkIntfc->unbindWanDev(
        m_uid.c_str(), token.accessToken().c_str(), devId.c_str(), ComTimeoutWanA);
    if (ret == FNET_OK) {
        ComWanNimConn::inst()->syncUnbindDev(m_nimData.appNimAccountId, devId);
        if (!nimAccountId.empty()) {
            ComWanNimConn::inst()->syncDevUnregister(nimAccountId);
            ComWanNimConn::inst()->unsubscribeDevStatus(std::vector<std::string>(1, nimAccountId));
        }
        for (auto &comPtr : m_comPtrs) {
            if (comPtr->deviceId() == devId) {
                if (m_readyIdSet.find(comPtr->id()) != m_readyIdSet.end()) {
                    BOOST_LOG_TRIVIAL(info) << serialNumber << ", unbind disconnect";
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
    if (it->second->connectMode() == COM_CONNECT_WAN && (!m_httpOnline || !m_nimOnline)) {
        return false;
    }
    m_ptrMap.left.at(id)->putCommand(commandPtr);
    return true;
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

bool MultiComMgr::wanSendGcode(const std::vector<std::string> &devIds,
    const std::vector<std::string> &devSerialNumbers, const std::vector<std::string> &nimAccountIds,
    const com_send_gcode_data_t &sendGocdeData)
{
    if (!m_httpOnline || !m_nimOnline) {
        return false;
    }
    return m_sendGcodeThd->startSendGcode(
        m_uid, devIds, devSerialNumbers, m_nimData.nimTeamId, nimAccountIds, sendGocdeData);
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
        if (!devData.wanDevInfo.nimAccountId.empty()) {
            m_nimAccountIdMap.emplace(devData.wanDevInfo.nimAccountId, comPtr->id());
        }
    }
    auto queueEvent = [this](auto &event) { QueueEvent(event.Clone()); };
    comPtr->Bind(COM_CONNECTION_READY_EVENT, &MultiComMgr::onConnectionReady, this);
    comPtr->Bind(COM_CONNECTION_EXIT_EVENT, &MultiComMgr::onConnectionExit, this);
    comPtr->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &MultiComMgr::onDevDetailUpdate, this);
    comPtr->Bind(COM_GET_DEV_GCODE_LIST_EVENT, &MultiComMgr::onGetDevGcodeList, this);
    comPtr->Bind(COM_GET_GCODE_THUMB_EVENT, [this](auto &event){ QueueEvent(event.MoveClone()); });
    comPtr->Bind(COM_GET_TIME_LAPSE_VIDEO_LIST_EVENT, &MultiComMgr::onGetDevTimeLapseVideoList, this);
    comPtr->Bind(COM_DELETE_TIME_LAPSE_VIDEO_EVENT, queueEvent);
    comPtr->Bind(COM_START_JOB_EVENT, queueEvent);
    comPtr->Bind(COM_SEND_GCODE_PROGRESS_EVENT, queueEvent);
    comPtr->Bind(COM_SEND_GCODE_FINISH_EVENT, queueEvent);
    comPtr->Bind(COMMAND_FAILED_EVENT, &MultiComMgr::onCommandFailed, this);
    comPtr->connect();
}

void MultiComMgr::onTimer(const wxTimerEvent &event)
{
    if (event.GetId() != m_loopCheckTimer.GetId()) {
        return;
    }
    if (!m_pendingWanDevDatas.empty()) {
        if (!m_httpOnline || !m_nimOnline) {
            m_pendingWanDevDatas.clear();
            return;
        }
        std::vector<std::string> nimAccountIds;
        for (auto it = m_pendingWanDevDatas.begin(); it != m_pendingWanDevDatas.end();) {
            const com_wan_dev_info_t &wanDevInfo = it->wanDevInfo;
            if (m_devIdMap.find(wanDevInfo.devId) == m_devIdMap.end()) {
                com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, m_uid,
                    wanDevInfo.serialNumber, wanDevInfo.devId, wanDevInfo.nimAccountId, networkIntfc());
                initConnection(comPtr, *it);
                if (!wanDevInfo.nimAccountId.empty()) {
                    nimAccountIds.push_back(wanDevInfo.nimAccountId);
                }
                it = m_pendingWanDevDatas.erase(it);
            } else {
                ++it;
            }
        }
        if (!nimAccountIds.empty()) {
            ComWanNimConn::inst()->updateDetail(nimAccountIds, m_nimData.nimTeamId);
            ComWanNimConn::inst()->subscribeDevStatus(nimAccountIds, SubscribeDevStatusSecond);
        }
    }
    if (m_nimOnline && m_httpOnline) {
        for (auto comId : m_readyIdSet) {
            com_dev_data_t &devData = m_datMap.at(comId);
            if (devData.connectMode == COM_CONNECT_WAN) {
                std::chrono::duration<double> duration = std_precise_clock::now() - m_devAliveTimeMap.at(comId);
                if (duration.count() > 20 && devData.wanDevInfo.status != "offline") {
                    devData.wanDevInfo.status = "offline";
                    QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, comId));
                }
            }
        }
    }
    if (m_nimOnline) {
        std::chrono::duration<double> duration = std_precise_clock::now() - m_subscribeTime;
        if (duration.count() > SubscribeDevStatusSecond - 30) {
            subscribeWanDevNimStatus();
            m_subscribeTime = std_precise_clock::now();
        }
    }
}

void MultiComMgr::onReloginHttp(ReloginHttpEvent &event)
{
    if (!m_login || event.ret != COM_OK && event.ret != COM_UNAUTHORIZED) {
        m_networkIntfc->freeWanDevList(event.devInfos, event.devCnt);
        return;
    }
    if (event.ret == COM_UNAUTHORIZED) {
        m_networkIntfc->freeWanDevList(event.devInfos, event.devCnt);
        removeWanDev();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, false, false, event.ret));
        return;
    }
    m_httpOnline = true;
    updateWanDevDetail();

    GetWanDevEvent updateWanDevEvent;
    updateWanDevEvent.SetEventType(GET_WAN_DEV_EVENT);
    updateWanDevEvent.ret = event.ret;
    updateWanDevEvent.uid = event.uid;
    updateWanDevEvent.devInfos = event.devInfos;
    updateWanDevEvent.devCnt = event.devCnt;
    onUpdateWanDev(updateWanDevEvent);

    QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, m_nimOnline, COM_OK));
    QueueEvent(new ComGetUserProfileEvent(COM_GET_USER_PROFILE_EVENT, event.userProfile, COM_OK));
}

void MultiComMgr::onUpdateWanDev(const GetWanDevEvent &event)
{
    fnet::FreeInDestructorArg freeDevInfos(event.devInfos, m_networkIntfc->freeWanDevList, event.devCnt);
    if (m_uid != event.uid || !m_httpOnline || !m_nimOnline) {
        return;
    }
    if (event.ret != COM_OK) {
        maintianWanDev(event.ret, false, false);
        return;
    }
    std::map<std::string, fnet_wan_dev_info_t *> devInfoMap;
    for (int i = 0; i < event.devCnt; ++i) {
        const char *devId = event.devInfos[i].devId;
        if (strlen(devId) == 0 || devInfoMap.find(devId) != devInfoMap.end()) {
            BOOST_LOG_TRIVIAL(fatal) << devId << ", empty devId/duplicated devId";
        } else {
            devInfoMap.emplace(devId, &event.devInfos[i]);
        }
    }
    std::vector<std::string> removedNimAccountIds;
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN && devInfoMap.find(comPtr->deviceId()) == devInfoMap.end()) {
            comPtr.get()->disconnect(0);
            if (!comPtr->nimAccountId().empty()) {
                removedNimAccountIds.push_back(comPtr->nimAccountId());
            }
        }
    }
    if (!removedNimAccountIds.empty()) {
        ComWanNimConn::inst()->unsubscribeDevStatus(removedNimAccountIds);
    }
    m_pendingWanDevDatas.clear();
    std::vector<std::string> addedNimAccountIds;
    for (auto &item : devInfoMap) {
        const fnet_wan_dev_info_t &wanDevInfo = *item.second;
        auto it = m_devIdMap.find(wanDevInfo.devId);
        if (it == m_devIdMap.end()) {
            com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, m_uid,
                wanDevInfo.serialNumber, wanDevInfo.devId, wanDevInfo.nimAccountId, networkIntfc());
            initConnection(comPtr, makeWanDevData(&wanDevInfo));
            if (strlen(wanDevInfo.nimAccountId) != 0) {
                addedNimAccountIds.push_back(wanDevInfo.nimAccountId);
            }
        } else if (m_ptrMap.left.at(it->second)->isDisconnect()) {
            m_pendingWanDevDatas.push_back(makeWanDevData(&wanDevInfo));
        }
    }
    if (!addedNimAccountIds.empty()) {
        ComWanNimConn::inst()->updateDetail(addedNimAccountIds, m_nimData.nimTeamId);
        ComWanNimConn::inst()->subscribeDevStatus(addedNimAccountIds, SubscribeDevStatusSecond);
    }
}

void MultiComMgr::onUpdateUserProfile(const ComGetUserProfileEvent &event)
{
    if (!m_httpOnline || !m_nimOnline) {
        return;
    }
    if (event.ret == COM_UNAUTHORIZED) {
        maintianWanDev(event.ret, false, false);
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
    if (devData.devDetail == nullptr) {
        devData.devDetail = event.devDetail;
    } else {
        m_networkIntfc->freeDevDetail(event.devDetail);
    }
    m_readyIdSet.insert(event.id);
    if (devData.connectMode == COM_CONNECT_WAN) {
        m_devAliveTimeMap[event.id] = std_precise_clock::now();
    }
    QueueEvent(event.Clone());

    const std::string &serialNumber = m_ptrMap.left.at(event.id)->serialNumber();
    BOOST_LOG_TRIVIAL(info) << serialNumber << ", connection_ready";
    BOOST_LOG_TRIVIAL(info) << "devices count: " << m_readyIdSet.size();
}

void MultiComMgr::onConnectionExit(const ComConnectionExitEvent &event)
{
    if (m_readyIdSet.find(event.id) != m_readyIdSet.end()) {
        const std::string &serialNumber = m_ptrMap.left.at(event.id)->serialNumber();
        BOOST_LOG_TRIVIAL(info) << "devices count: " << m_readyIdSet.size();
        BOOST_LOG_TRIVIAL(info) << serialNumber << ", connection_exit";
    }
    ComConnection *comConnection = m_ptrMap.left.at(event.id);
    comConnection->joinThread();
    com_dev_data_t &devData = m_datMap.at(event.id);
    m_networkIntfc->freeDevProduct(devData.devProduct);
    m_networkIntfc->freeDevDetail(devData.devDetail);
    m_networkIntfc->freeGcodeList(devData.lanGcodeList.gcodeDatas, devData.lanGcodeList.gcodeCnt);
    m_networkIntfc->freeGcodeList(devData.wanGcodeList.gcodeDatas, devData.wanGcodeList.gcodeCnt);
    m_networkIntfc->freeTimeLapseVideoList(devData.wanTimeLapseVideoList.videoDatas,
        devData.wanTimeLapseVideoList.videoCnt);
    m_readyIdSet.erase(event.id);
    if (comConnection->connectMode() == COM_CONNECT_WAN) {
        m_devAliveTimeMap.erase(event.id);
        m_devIdMap.erase(devData.wanDevInfo.devId);
        if (!devData.wanDevInfo.nimAccountId.empty()) {
            m_nimAccountIdMap.erase(devData.wanDevInfo.nimAccountId);
        }
    }
    m_datMap.erase(event.id);
    m_ptrMap.left.erase(event.id);
    m_comPtrs.remove_if([comConnection](auto &ptr) { return ptr.get() == comConnection; });
    QueueEvent(event.Clone());
}

void MultiComMgr::onDevDetailUpdate(const ComDevDetailUpdateEvent &event)
{
    if (m_ptrMap.left.at(event.id)->isDisconnect()) {
        m_networkIntfc->freeDevDetail(event.devDetail);
        return;
    }
    com_dev_data_t &devData = m_datMap.at(event.id);
    m_networkIntfc->freeDevDetail(devData.devDetail);
    devData.devDetail = event.devDetail;
    devData.devDetailUpdated = true;
    if (m_readyIdSet.find(event.id) != m_readyIdSet.end()) {
        QueueEvent(event.Clone());
    }
    if (devData.connectMode != COM_CONNECT_WAN
     || devData.wanDevInfo.name == devData.devDetail->name
     && devData.wanDevInfo.status == devData.devDetail->status
     && devData.wanDevInfo.location == devData.devDetail->location) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << devData.devDetail->name << " status---" << devData.devDetail->status;
    devData.wanDevInfo.name = devData.devDetail->name;
    devData.wanDevInfo.status = devData.devDetail->status;
    devData.wanDevInfo.location = devData.devDetail->location;
    if (m_readyIdSet.find(event.id) != m_readyIdSet.end()) {
        QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, event.id));
    }
}

void MultiComMgr::onGetDevGcodeList(const ComGetDevGcodeListEvent &event)
{
    com_dev_data_t &devData = m_datMap.at(event.id);
    if (devData.connectMode == COM_CONNECT_LAN) {
        m_networkIntfc->freeGcodeList(devData.lanGcodeList.gcodeDatas, devData.lanGcodeList.gcodeCnt);
        devData.lanGcodeList = event.lanGcodeList;
    } else {
        m_networkIntfc->freeGcodeList(devData.wanGcodeList.gcodeDatas, devData.wanGcodeList.gcodeCnt);
        devData.wanGcodeList = event.wanGcodeList;
    }
    QueueEvent(event.Clone());
}

void MultiComMgr::onGetDevTimeLapseVideoList(const ComGetTimeLapseVideoListEvent &event)
{
    com_dev_data_t &devData = m_datMap.at(event.id);
    m_networkIntfc->freeTimeLapseVideoList(devData.wanTimeLapseVideoList.videoDatas,
        devData.wanTimeLapseVideoList.videoCnt);
    devData.wanTimeLapseVideoList = event.wanTimeLapseVideoList;
    QueueEvent(event.Clone());
}

void MultiComMgr::onCommandFailed(const CommandFailedEvent &event)
{
    if (!m_httpOnline || !m_nimOnline) {
        return;
    }
    if (event.fatalError || event.ret == COM_UNAUTHORIZED) {
        maintianWanDev(event.ret, false, false);
    } else if (!m_blockCommandFailedUpdate) {
        m_blockCommandFailedUpdate = true;
        m_threadPool->post([this]() {
            if (m_commandFailedUpdateTime != std_precise_clock::time_point::min()) {
                std::chrono::duration<double> duration = std_precise_clock::now() - m_commandFailedUpdateTime;
                int waitTime = 600000 - duration.count() * 1000;
                if (waitTime > 0) {
                    m_threadExitEvent.waitTrue(waitTime);
                }
            }
            if (!m_threadExitEvent.get()) {
                m_wanDevMaintainThd->setUpdateWanDev();
                m_commandFailedUpdateTime = std_precise_clock::now();
            }
            m_blockCommandFailedUpdate = false;
        });
    }
}

void MultiComMgr::onWanConnStatus(const WanConnStatusEvent &event)
{
    if (!m_login) {
        return;
    }
    switch (event.status) {
    case FNET_CONN_STATUS_LOGINED:
        m_nimOnline = true;
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, m_httpOnline, COM_OK));
        if (!m_nimFirstLogined) {
            m_wanDevMaintainThd->setUpdateUserProfile();
            m_wanDevMaintainThd->setUpdateWanDev();
            subscribeWanDevNimStatus();
            updateWanDevDetail();
            m_subscribeTime = std_precise_clock::now();
        }
        m_nimFirstLogined = false;
        break;
    case FNET_CONN_STATUS_LOGOUT:
        maintianWanDev(COM_OK, true, false);
        break;
    case FNET_CONN_STATUS_UNLOGIN:
        m_nimOnline = false;
        setWanDevOffline();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, false, COM_ERROR));
        break;
    }
}

void MultiComMgr::onWanConnRead(const WanConnReadEvent &event)
{
    auto isSpecialType = [](fnet_conn_read_data_type_t type) {
        return type == FNET_CONN_READ_SYNC_USER_PROFILE  || type == FNET_CONN_READ_SYNC_BIND_DEVICE
            || type == FNET_CONN_READ_SYNC_UNBIND_DEVICE || type == FNET_CONN_READ_UNREGISTER_USER;
    };
    if (!m_httpOnline && !isSpecialType(event.readData.type) || !m_nimOnline) {
        m_networkIntfc->freeString(event.readData.nimAccountId);
        return;
    }
    auto procDevDetailUpdate = [this](const fnet_conn_read_data_t &readData) {
        auto it = m_nimAccountIdMap.find(readData.nimAccountId);
        if (it != m_nimAccountIdMap.end()) {
            ComDevDetailUpdateEvent devDetailUpdateEvent(COM_DEV_DETAIL_UPDATE_EVENT,
                it->second, ComInvalidCommandId, (fnet_dev_detail_t *)readData.data);
            onDevDetailUpdate(devDetailUpdateEvent);
            m_devAliveTimeMap[it->second] = std_precise_clock::now(); // may receive a push before ready
        }
    };
    auto procDevKeepAlive = [this](const fnet_conn_read_data_t &readData) {
        auto it = m_nimAccountIdMap.find(readData.nimAccountId);
        if (it != m_nimAccountIdMap.end()) {
            com_dev_data_t &devData = m_datMap.at(it->second);
            if (devData.devDetail != nullptr && devData.devDetailUpdated) {
                devData.wanDevInfo.status = devData.devDetail->status;
                if (m_readyIdSet.find(it->second) != m_readyIdSet.end()) {
                    QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, it->second));
                }
            }
            m_devAliveTimeMap[it->second] = std_precise_clock::now();
        }
    };
    switch (event.readData.type) {
    case FNET_CONN_READ_SYNC_USER_PROFILE:
        m_wanDevMaintainThd->setUpdateUserProfile();
        break;
    case FNET_CONN_READ_SYNC_BIND_DEVICE:
    case FNET_CONN_READ_SYNC_UNBIND_DEVICE:
        m_wanDevMaintainThd->setUpdateWanDev();
        break;
    case FNET_CONN_READ_UNREGISTER_USER:
        maintianWanDev(COM_OK, false, true);
        break;
    case FNET_CONN_READ_DEVICE_DETAIL:
        procDevDetailUpdate(event.readData);
        break;
    case FNET_CONN_READ_DEVICE_KEEP_ALIVE:
        procDevKeepAlive(event.readData);
        break;
    }
    m_networkIntfc->freeString(event.readData.nimAccountId);
}

void MultiComMgr::onWanConnSubscribe(const WanConnSubscribeEvent &event)
{
    if (!m_httpOnline || !m_nimOnline) {
        return;
    }
    if (event.status == 2 || event.status == 3) {
        auto it = m_nimAccountIdMap.find(event.nimAccountId);
        if (it != m_nimAccountIdMap.end()) {
            m_datMap.at(it->second).wanDevInfo.status = "offline";
            if (m_readyIdSet.find(it->second) != m_readyIdSet.end()) {
                QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, it->second));
            }
        }
    }
}

void MultiComMgr::onRefreshToken(const ComRefreshTokenEvent &event)
{
    if (!m_login || event.ret != COM_OK) {
        return;
    }
    QueueEvent(event.Clone());
}

com_dev_data_t MultiComMgr::makeWanDevData(const fnet_wan_dev_info_t *wanDevInfo)
{
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_WAN;
    devData.wanDevInfo.devId = wanDevInfo->devId;
    devData.wanDevInfo.name = wanDevInfo->name;
    devData.wanDevInfo.model = wanDevInfo->model;
    devData.wanDevInfo.imageUrl = wanDevInfo->imageUrl;
    devData.wanDevInfo.status = "offline";
    devData.wanDevInfo.location = wanDevInfo->location;
    devData.wanDevInfo.serialNumber = wanDevInfo->serialNumber;
    devData.wanDevInfo.nimAccountId = wanDevInfo->nimAccountId;
    devData.devProduct = nullptr;
    devData.devDetail = nullptr;
    devData.lanGcodeList.gcodeCnt = 0;
    devData.lanGcodeList.gcodeDatas = nullptr;
    devData.wanGcodeList.gcodeCnt = 0;
    devData.wanGcodeList.gcodeDatas = nullptr;
    devData.wanTimeLapseVideoList.videoCnt = 0;
    devData.wanTimeLapseVideoList.videoDatas = nullptr;
    devData.devDetailUpdated = false;
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    return devData;
}

void MultiComMgr::maintianWanDev(ComErrno ret, bool repeatLogin, bool unregisterUser)
{
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::maintianWanDev " << (int)ret;
    if (repeatLogin || unregisterUser) {
        removeWanDev();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, false, false, ret));
        return;
    }
    if (ret != COM_OK) {
        m_httpOnline = false;
        m_wanDevMaintainThd->setReloginHttp();
        setWanDevOffline();
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, false, ret));
    }
}

void MultiComMgr::setWanDevOffline()
{
    for (auto &item : m_ptrMap.left) {
        if (item.second->connectMode() == COM_CONNECT_WAN) {
            com_dev_data_t &devData = m_datMap.at(item.first);
            devData.wanDevInfo.status = "offline";
            QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, item.first));
        }
    }
}

void MultiComMgr::subscribeWanDevNimStatus()
{
    if (m_nimOnline) {
        return;
    }
    std::vector<std::string> nimAccountIds;
    for (auto &item : m_ptrMap.left) {
        if (item.second->connectMode() == COM_CONNECT_WAN
        && !item.second->isDisconnect()
        && !item.second->nimAccountId().empty()) {
            nimAccountIds.push_back(item.second->nimAccountId());
        }
    }
    if (!nimAccountIds.empty()) {
        ComWanNimConn::inst()->subscribeDevStatus(nimAccountIds, SubscribeDevStatusSecond);
    }
}

void MultiComMgr::updateWanDevDetail()
{
    if (!m_httpOnline || !m_nimOnline) {
        return;
    }
    std::vector<std::string> nimAccountIds;
    for (auto &item : m_ptrMap.left) {
        if (item.second->connectMode() == COM_CONNECT_WAN
        && !item.second->isDisconnect()
        && !item.second->nimAccountId().empty()) {
            nimAccountIds.push_back(item.second->nimAccountId());
        }
    }
    if (!nimAccountIds.empty()) {
        ComWanNimConn::inst()->updateDetail(nimAccountIds, m_nimData.nimTeamId);
    }
}

std::string MultiComMgr::getNimAppDir(const std::string &dataDir)
{
    for (int i = 0; true; ++i) {
        wxString dirPath = wxString::FromUTF8(dataDir + "/nimData");
        if (i > 0) {
            dirPath += std::to_string(i);
        }
        if (!wxDir::Exists(dirPath)) {
            wxDir::Make(dirPath);
        }
        wxString fileLockPath = dirPath + "/" + m_nimDataFileLockName;
        if (!wxFile::Exists(fileLockPath)) {
            wxFile().Open(fileLockPath, wxFile::write);
        }
        try {
            auto fileLock = std::make_unique<boost::interprocess::file_lock>(fileLockPath.ToUTF8().data());
            if (fileLock->try_lock()) {
                m_nimDataDirFileLock.swap(fileLock);
                return dirPath.ToUTF8().data();
            }
        } catch (...) {
            BOOST_LOG_TRIVIAL(fatal) << "create file lock failed, " << fileLockPath;
            return dirPath.ToUTF8().data();
        }
    }
}

}} // namespace Slic3r::GUI
