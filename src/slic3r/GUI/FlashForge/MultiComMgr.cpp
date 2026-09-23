#include "MultiComMgr.hpp"
#include <boost/format.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <exception>
#include <iterator>
#include <utility>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/platinfo.h>
#include <wx/stdpaths.h>
#include "libslic3r/Utils.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "ExportLogs.hpp"
#include "FeedbackLogUploadClaim.hpp"
#include "FreeInDestructor.h"
#include "MultiComHelper.hpp"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

// [UNBIND] 解绑耗时诊断：与 SingleDeviceState / FFRTMPVideoCtrl 的 [UNBIND] 共用同一时间轴。
// 用于定位设备解绑时 UI 线程卡在哪一段（相邻两条时间戳差即该段耗时）。定位后置 false 关闭。
static const bool g_unbind_timing_enabled = false;
static void unbind_ts(const std::string &tag)
{
    if (!g_unbind_timing_enabled) return;
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string line = "[UNBIND] t=" + std::to_string(ms) + "ms | " + tag;
    BOOST_LOG_TRIVIAL(info) << line;
#ifdef _WIN32
    OutputDebugStringA((line + "\n").c_str());
#endif
}

namespace {

constexpr double WAN_DEVICE_STALE_TIMEOUT_SECONDS = 20.0;
constexpr int MQTT_CONNECT_TIMEOUT_SECONDS = 20;

std::string sanitized_client_id_part(std::string value)
{
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return !std::isalnum(c);
    }), value.end());
    return value;
}

std::string get_device_client_id_part()
{
    std::string device_id;
    if (wxGetApp().app_config) {
        device_id = wxGetApp().app_config->get("slicer_uuid");
    }

    device_id = sanitized_client_id_part(device_id);
    if (device_id.empty()) {
        device_id = sanitized_client_id_part(boost::uuids::to_string(boost::uuids::random_generator()()));
    }

    constexpr size_t max_device_id_len = 16;
    if (device_id.size() > max_device_id_len) {
        device_id.resize(max_device_id_len);
    }
    return device_id;
}

std::string client_id_device_prefix(const std::string &client_id)
{
    constexpr const char *pc_prefix = "pc_";
    if (client_id.rfind(pc_prefix, 0) != 0) {
        return {};
    }

    const size_t device_id_start = std::strlen(pc_prefix);
    const size_t device_id_end = client_id.find('_', device_id_start);
    if (device_id_end == std::string::npos || device_id_end == device_id_start) {
        return {};
    }

    return client_id.substr(0, device_id_end + 1);
}

bool is_same_device_client_id(const std::string &lhs, const std::string &rhs)
{
    const std::string lhs_prefix = client_id_device_prefix(lhs);
    return !lhs_prefix.empty() && lhs_prefix == client_id_device_prefix(rhs);
}

template<typename Request>
ComErrno execute_authenticated_wan_request(const char *operation, Request &&request)
{
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    ComErrno ret = request(token.accessToken());
    if (ret != COM_UNAUTHORIZED) {
        return ret;
    }

    BOOST_LOG_TRIVIAL(warning) << operation << " was unauthorized; refreshing access token"
                               << ", session=" << token.sessionGeneration()
                               << ", token_revision=" << token.tokenRevision();
    ret = WanDevTokenMgr::inst()->refreshToken(token);
    if (ret != COM_OK) {
        BOOST_LOG_TRIVIAL(warning) << operation << " token refresh failed: " << static_cast<int>(ret);
        return ret;
    }

    // The protected operation is retried exactly once. A concurrent refresh is
    // reused by refreshToken(), so parallel requests do not rotate the token twice.
    ret = request(token.accessToken());
    BOOST_LOG_TRIVIAL(info) << operation << " retry completed: " << static_cast<int>(ret)
                            << ", token_revision=" << token.tokenRevision();
    return ret;
}

const char *wan_device_status_source_name(WanDeviceStatusSource source)
{
    switch (source) {
    case WanDeviceStatusSource::Initial:          return "initial";
    case WanDeviceStatusSource::HttpDeviceList:   return "http_device_list";
    case WanDeviceStatusSource::SyncOnline:       return "sync_online";
    case WanDeviceStatusSource::SyncOffline:      return "sync_offline";
    case WanDeviceStatusSource::DeviceDetail:     return "device_detail";
    case WanDeviceStatusSource::KeepAlive:        return "keep_alive";
    case WanDeviceStatusSource::MessageActivity:  return "message_activity";
    case WanDeviceStatusSource::StaleTimeout:     return "stale_timeout";
    }
    return "unknown";
}

const char *wan_device_presence_name(WanDevicePresence presence)
{
    switch (presence) {
    case WanDevicePresence::Unknown: return "unknown";
    case WanDevicePresence::Online:  return "online";
    case WanDevicePresence::Offline: return "offline";
    }
    return "unknown";
}

} // namespace

MultiComMgr::MultiComMgr()
    : m_idNum(ComInvalidId + 1)
    , m_login(false)
    , m_httpOnline(false)
    , m_connOnline(false)
    , m_loopCheckTimer(this)
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
    m_dataDir = dataDir;
    std::string logFileDir = dataDir + "/FlashNetwork";
    bool debug = wxFileName::FileExists(appPathWithSep + "FLASHNETWORK_DEBUG");

    fnet_log_settings_t logSettings;
    logSettings.fileDir = logFileDir.c_str();
    logSettings.expireHours = 7 * 24;
    logSettings.level = debug ? FNET_LOG_LEVEL_DEBUG : FNET_LOG_LEVEL_INFO;

#ifdef __APPLE__
    std::string serverSettingsPath = (appPathWithSep + "../Resources/data/" + DAT_FILE_NAME).ToUTF8().data();
#elif defined(__linux__)
    std::string serverSettingsPath = Slic3r::resources_dir() + "/data/" + DAT_FILE_NAME;
#else
    std::string serverSettingsPath = (appPathWithSep + "resources/data/" + DAT_FILE_NAME).ToUTF8().data();
#endif
    m_networkIntfc.reset(new fnet::FlashNetworkIntfc(
        dllPath.c_str(), serverSettingsPath.c_str(), logSettings));
    if (!m_networkIntfc->isOk()) {
        BOOST_LOG_TRIVIAL(error) << "initalize FlashNetwork failed: " << dllPath;
        m_networkIntfc.reset();
        return false;
    }
    const char *userAgentFormat = "Orca-Flashforge/%s (PC; %s)";
    std::string version = Orca_Flashforge_VERSION;
    std::string osName = wxPlatformInfo::Get().GetOperatingSystemIdName().utf8_string();
    m_networkIntfc->setUserAgent((boost::format(userAgentFormat) % version % osName).str().c_str());
    m_wanDevMaintainThd.reset(new WanDevMaintainThd(m_networkIntfc.get()));
    m_wanDevMaintainThd->Bind(RELOGIN_HTTP_EVENT, &MultiComMgr::onReloginHttp, this);
    m_wanDevMaintainThd->Bind(GET_WAN_DEV_EVENT, &MultiComMgr::onUpdateWanDev, this);
    m_wanDevMaintainThd->Bind(COM_GET_USER_PROFILE_EVENT, &MultiComMgr::onUpdateUserProfile, this);

    auto queueEvent = [this](auto &event) { QueueEvent(event.Clone()); };
    m_sendGcodeThd.reset(new WanDevSendGcodeThd(m_networkIntfc.get()));
    m_sendGcodeThd->Bind(COM_SEND_GCODE_PROGRESS_EVENT, queueEvent);
    m_sendGcodeThd->Bind(COM_SEND_GCODE_FINISH_EVENT, queueEvent);

    m_threadPool.reset(new ComThreadPool(5, 30000));
    m_connectionStopThreadPool.reset(new ComThreadPool(1, 30000));
    m_logUploadThreadPool.reset(new ComThreadPool(1, 30000));
    m_threadExitEvent.set(false);
    m_loopCheckTimer.Start(1000);

    ComWanConn::inst()->Bind(WAN_CONN_STATUS_EVENT, &MultiComMgr::onWanConnStatus, this);
    ComWanConn::inst()->Bind(WAN_CONN_READ_EVENT, &MultiComMgr::onWanConnRead, this);
    ComWanConn::inst()->Bind(WAN_CONN_HTTP_UNAUTHORIZED, &MultiComMgr::onWanConnUnauthorized, this);
    Bind(COM_MQTT_CONFIG_REFRESH_EVENT, &MultiComMgr::onMqttConfigRefresh, this);
    Bind(COM_MQTT_CONNECTION_STOPPED_EVENT, &MultiComMgr::onMqttConnectionStopped, this);
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
    ComWanConn::inst()->Unbind(WAN_CONN_STATUS_EVENT, &MultiComMgr::onWanConnStatus, this);
    ComWanConn::inst()->Unbind(WAN_CONN_READ_EVENT, &MultiComMgr::onWanConnRead, this);
    ComWanConn::inst()->Unbind(WAN_CONN_HTTP_UNAUTHORIZED, &MultiComMgr::onWanConnUnauthorized, this);
    Unbind(COM_MQTT_CONFIG_REFRESH_EVENT, &MultiComMgr::onMqttConfigRefresh, this);
    Unbind(COM_MQTT_CONNECTION_STOPPED_EVENT, &MultiComMgr::onMqttConnectionStopped, this);
    m_logUploadThreadPool.reset();
    m_connectionStopThreadPool.reset();
    m_threadPool.reset();
    m_mqttStopInFlight = false;
    m_mqttStopContinuation = MqttStopContinuation::None;
    ++m_mqttStopGeneration;
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

std::string MultiComMgr::homePageUrl() 
{ 
    if (networkIntfc() == nullptr) {
        BOOST_LOG_TRIVIAL(error) << "homePageUrl: FlashNetwork Failed, can't connect server!";
        return "";
    }
    return m_networkIntfc->getHomePageUrl(); 
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

ComErrno MultiComMgr::addWanDev(const com_token_data_t &tokenData, bool isCheckVersionOnTestServer,
    com_add_wan_dev_data_t &addDevData, int tryCnt, int tryMsInterval, ComSessionInitStage *failedStage)
{
    if (failedStage)
        *failedStage = ComSessionInitStage::None;

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
    if (networkIntfc() == nullptr) {
        if (failedStage)
            *failedStage = ComSessionInitStage::NetworkInterface;
        return COM_ERROR;
    }
    if (m_login) {
        if (failedStage)
            *failedStage = ComSessionInitStage::ExistingSession;
        return COM_ERROR;
    }
    m_clientId = generateClientId();
    if (failedStage)
        *failedStage = ComSessionInitStage::UserProfile;
    ComErrno ret = tryDo([&]() {
        return MultiComUtils::getUserProfile(tokenData.accessToken, addDevData.userProfile, ComTimeoutWanA);
    });
    if (ret != COM_OK) {
        return ret;
    }
    if (failedStage)
        *failedStage = ComSessionInitStage::AccountBinding;
    ret = tryDo([&]() {
        return MultiComUtils::bindAccountRelp(m_clientId, tokenData.accessToken, addDevData.userProfile.email,
            addDevData.showUserPoints, ComTimeoutWanA);
    });
    if (ret != COM_OK) {
        return ret;
    }
    m_userId = addDevData.userProfile.uid;
    const uint64_t sessionGeneration = ++m_wanSessionGeneration;
    m_login = true;
    m_httpOnline = true;
    m_blockCommandFailedUpdate = false;
    m_commandFailedUpdateTime = std_precise_clock::time_point::min();
    m_pendingSetUpdateWanDevTime = std_precise_clock::time_point::max();
    setMaintainThdReqHeader(isCheckVersionOnTestServer);
    MultiComHelper::inst()->loginInit(m_clientId, addDevData.userProfile.uid);
    WanDevTokenMgr::inst()->initalize(tokenData, networkIntfc(), sessionGeneration); // initialize global token
    // Token maintenance belongs to the authenticated account session and must
    // continue even when MQTT is unavailable.
    WanDevTokenMgr::inst()->start();
    startFeedbackLogSession();
    // Publish the initial profile as account data. GUI handlers may update
    // profile/preset state, but login completion remains owned by
    // EVT_ASYNC_LOGIN_FINISHED.
    QueueEvent(new ComGetUserProfileEvent(
        COM_GET_USER_PROFILE_EVENT, addDevData.userProfile, COM_OK, sessionGeneration));
    if (failedStage)
        *failedStage = ComSessionInitStage::None;
    return ret;
}

void MultiComMgr::removeWanDev()
{
    invalidateWanSession(WanLogoutReason::UserRequested, COM_OK);
}

void MultiComMgr::invalidateWanSession(WanLogoutReason reason, ComErrno ret)
{
    // Several asynchronous sources may discover an invalid session at nearly
    // the same time. Only the first one owns teardown and publishes logout.
    if (!m_login.exchange(false)) {
        return;
    }

    const uint64_t sessionGeneration = ++m_wanSessionGeneration;
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::invalidateWanSession reason=" << static_cast<int>(reason)
                            << ", ret=" << static_cast<int>(ret)
                            << ", generation=" << sessionGeneration;

    teardownWanSession();
    QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, false, false, ret,
        WanServiceState::LoggedOut, reason, sessionGeneration));
}

void MultiComMgr::teardownWanSession()
{
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::teardownWanSession";
    disableMqtt();
    stopFeedbackLogSession();
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN) {
            comPtr->disconnect(0);
        }
    }
    m_userId.clear();
    m_httpOnline = false;
    m_connOnline = false;
    m_wanDevMaintainThd->stop();
    WanDevTokenMgr::inst()->exit();
}

void MultiComMgr::startWanMqtt()
{
    if (!m_login || m_mqttState != MqttRecoveryState::Disabled) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "Starting MQTT subsystem, session=" << m_wanSessionGeneration.load();
    m_mqttBlockedRefreshTokenRevision = 0;
    startMqttRecovery();
}

bool MultiComMgr::requestAccessTokenRefresh(uint64_t requestGeneration)
{
    if (!m_login || !m_threadPool) {
        return false;
    }

    const uint64_t sessionGeneration = m_wanSessionGeneration.load();
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    BOOST_LOG_TRIVIAL(info) << "Request access token refresh, session=" << sessionGeneration
                            << ", request=" << requestGeneration
                            << ", token_revision=" << token.tokenRevision();
    m_threadPool->post([this, sessionGeneration, requestGeneration, token = std::move(token)]() mutable {
        const ComErrno ret = WanDevTokenMgr::inst()->refreshToken(token);
        QueueEvent(new ComAccessTokenRefreshResultEvent(COM_ACCESS_TOKEN_REFRESH_RESULT_EVENT, ret,
            token.accessToken(), sessionGeneration, requestGeneration, token.tokenRevision()));
    });
    return true;
}

void MultiComMgr::requestWanDeviceListRefresh()
{
    if (!m_login || !m_wanDevMaintainThd) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "Request WAN device list refresh, session="
                            << m_wanSessionGeneration.load();
    m_wanDevMaintainThd->setUpdateWanDev();
}

void MultiComMgr::startFeedbackLogSession()
{
    stopFeedbackLogSession();
    std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
    ++m_feedbackLogSessionGeneration;
    m_feedbackLogCancelEvent = std::make_shared<WaitEvent>(false);
}

void MultiComMgr::stopFeedbackLogSession()
{
    std::shared_ptr<WaitEvent> cancelEvent;
    {
        std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
        ++m_feedbackLogSessionGeneration;
        cancelEvent = std::move(m_feedbackLogCancelEvent);
    }
    if (cancelEvent != nullptr) {
        cancelEvent->set(true);
    }
    if (m_logUploadThreadPool != nullptr) {
        m_logUploadThreadPool->clear();
        m_logUploadThreadPool->wait();
    }
    {
        std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
        m_activeFeedbackLogIds.clear();
        m_completedFeedbackLogIds.clear();
        m_feedbackLogHistory.clear();
    }
}

ComErrno MultiComMgr::bindWanDev(const std::string &ip, unsigned short port, const std::string &serialNumber,
    unsigned short pid, const std::string &name, unsigned short bindType)
{
    if (!m_login || !m_httpOnline) {
        return COM_ERROR;
    }

    const uint64_t sessionGeneration = m_wanSessionGeneration.load();
    const ComErrno ret = execute_authenticated_wan_request("Bind WAN device", [&](const std::string &accessToken) {
        fnet_wan_dev_bind_data_t *bindData = nullptr;
        const int networkRet = m_networkIntfc->bindWanDev(m_clientId.c_str(), accessToken.c_str(),
            serialNumber.c_str(), pid, name.c_str(), bindType, &bindData, ComTimeoutWanA);
        if (bindData != nullptr) {
            m_networkIntfc->freeBindData(bindData);
        }
        return MultiComUtils::fnetRet2ComErrno(networkRet);
    });
    if (ret != COM_OK) {
        return ret;
    }
    if (!m_login || sessionGeneration != m_wanSessionGeneration.load()) {
        return COM_ABORTED_BY_USER;
    }

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
    m_pendingSetUpdateWanDevTime = std_precise_clock::now();
    return COM_OK;
}

ComErrno MultiComMgr::unbindWanDev(const std::string &serialNumber, const std::string &devId)
{
    if (!m_login || !m_httpOnline) {
        return COM_ERROR;
    }

    const uint64_t sessionGeneration = m_wanSessionGeneration.load();
    const ComErrno ret = execute_authenticated_wan_request("Unbind WAN device", [&](const std::string &accessToken) {
        return MultiComUtils::fnetRet2ComErrno(m_networkIntfc->unbindWanDev(
            m_clientId.c_str(), accessToken.c_str(), devId.c_str(), ComTimeoutWanA));
    });
    if (ret != COM_OK) {
        return ret;
    }
    if (!m_login || sessionGeneration != m_wanSessionGeneration.load()) {
        return COM_ABORTED_BY_USER;
    }

    std::string devTopic = getDevTopic(devId);
    if (!devTopic.empty()) {
        ComWanConn::inst()->unsubscribe(std::vector<std::string>(1, devTopic));
    }
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->devId() == devId) {
            if (m_readyIdSet.find(comPtr->id()) != m_readyIdSet.end()) {
                BOOST_LOG_TRIVIAL(info) << "unbind disconnect";
            }
            comPtr->disconnect(0);
            break;
        }
    }
    return COM_OK;
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
        BOOST_LOG_TRIVIAL(error) << "putCommand, invalid com_id_t, " << id;
        return false;
    }
    if (it->second->connectMode() == COM_CONNECT_WAN && (!m_httpOnline || !m_connOnline)) {
        BOOST_LOG_TRIVIAL(error) << "putCommand, invalid com state, " << id;
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
    const std::vector<std::string> &devSerialNumbers, const com_send_gcode_data_t &sendGocdeData)
{
    if (!m_httpOnline || !m_connOnline) {
        return false;
    }
    std::vector<std::string> devTopics(devIds.size());
    for (size_t i = 0; i < devIds.size(); ++i) {
        devTopics[i] = getDevTopic(devIds[i]);
    }
    bool isSend = false;
    int  loop   = 3;
    while (loop--) {
        isSend = m_sendGcodeThd->startSendGcode(m_clientId, devIds, devSerialNumbers, devTopics, sendGocdeData);
        if (isSend)
            break;
    }
    return isSend;
}

bool MultiComMgr::abortWanSendGcode()
{
    return m_sendGcodeThd->abortSendGcode();
}

const std::unordered_map<std::string, std::vector<std::string>>& MultiComMgr::getDevUnupdateList() { 
    return m_unUpdateDevList;
}

void MultiComMgr::initConnection(const com_ptr_t &comPtr, const com_dev_data_t &devData)
{
    m_comPtrs.push_back(comPtr);
    m_ptrMap.insert(com_ptr_map_val_t(comPtr->id(), comPtr.get()));
    m_datMap.emplace(comPtr->id(), devData);
    if (devData.connectMode == COM_CONNECT_WAN) {
        m_devIdMap.emplace(devData.wanDevInfo.devId, comPtr->id());
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
    const auto now = std::chrono::steady_clock::now();
    if (m_mqttState == MqttRecoveryState::RetryScheduled
        && m_nextMqttConfigRefreshTime != std::chrono::steady_clock::time_point {}
        && now >= m_nextMqttConfigRefreshTime) {
        m_nextMqttConfigRefreshTime = {};
        requestMqttConfigRefresh();
    }
    if (m_mqttState == MqttRecoveryState::Connecting
        && m_mqttConnectDeadline != std::chrono::steady_clock::time_point {}
        && now >= m_mqttConnectDeadline) {
        BOOST_LOG_TRIVIAL(warning) << "MQTT connection timed out";
        stopMqttConnectionAsync(MqttStopContinuation::ScheduleRetry);
        QueueEvent(new ComWanDevMaintainEvent(
            COM_WAN_DEV_MAINTAIN_EVENT, true, false, COM_ERROR, WanServiceState::Reconnecting,
            WanLogoutReason::Unknown, m_wanSessionGeneration.load()));
    }
    if (!m_pendingWanDevDatas.empty()) {
        if (!m_connOnline) {
            // Keep pending device data during a temporary MQTT disconnect and
            // recreate the device connections after MQTT recovers.
            return;
        }
        std::vector<std::string> devTopics;
        for (auto it = m_pendingWanDevDatas.begin(); it != m_pendingWanDevDatas.end();) {
            const com_wan_dev_info_t &wanDevInfo = it->wanDevInfo;
            if (m_devIdMap.find(wanDevInfo.devId) == m_devIdMap.end()) {
                com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, m_clientId,
                    wanDevInfo.serialNumber, wanDevInfo.devId, wanDevInfo.devTopic, networkIntfc());
                initConnection(comPtr, *it);
                if (!wanDevInfo.devTopic.empty()) {
                    devTopics.push_back(wanDevInfo.devTopic);
                }
                it = m_pendingWanDevDatas.erase(it);
            } else {
                ++it;
            }
        }
        if (!devTopics.empty()) {
            ComWanConn::inst()->updateDetail(devTopics);
            ComWanConn::inst()->subscribe(devTopics);
        }
    }
    if (m_connOnline) {
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::string> staleDevTopics;
        for (auto comId : m_readyIdSet) {
            com_dev_data_t &devData = m_datMap.at(comId);
            if (devData.connectMode == COM_CONNECT_WAN) {
                if (!is_wan_device_online(devData) || devData.wanLastMessageTime == std::chrono::steady_clock::time_point {}
                    || devData.wanDevInfo.devTopic.empty()) {
                    continue;
                }
                const std::chrono::duration<double> aliveDuration = now - devData.wanLastMessageTime;
                if (aliveDuration.count() > WAN_DEVICE_STALE_TIMEOUT_SECONDS
                    && devData.wanDevInfo.status != "printing") {
                    BOOST_LOG_TRIVIAL(warning) << "WAN device has not sent a message for 20 seconds; marking it offline";
#ifdef _WIN32
                    OutputDebugStringA("[CAMDBG] MultiComMgr: WAN device message timeout -> offline\n");
#endif
                    updateWanDeviceState(comId, WanDevicePresence::Offline,
                        WanDeviceStatusSource::StaleTimeout, nullptr, true);
                }
            }
        }
        if (m_pendingSetUpdateWanDevTime != std_precise_clock::time_point::max()) {
            std::chrono::duration<double> duration = std_precise_clock::now() - m_pendingSetUpdateWanDevTime;
            if (duration.count() > 3) {
                m_wanDevMaintainThd->setUpdateWanDev();
                m_pendingSetUpdateWanDevTime = std_precise_clock::time_point::max();
            }
        }
    }
}

void MultiComMgr::onReloginHttp(ReloginHttpEvent &event)
{
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || event.ret != COM_OK && event.ret != COM_UNAUTHORIZED) {
        m_networkIntfc->freeWanDevList(event.devInfos, event.devCnt);
        return;
    }
    if (event.ret == COM_UNAUTHORIZED) {
        m_networkIntfc->freeWanDevList(event.devInfos, event.devCnt);
        // This event may represent either a rejected refresh token or a newly
        // issued access token rejected by the profile API. The token manager's
        // own event carries the precise RefreshTokenRejected reason.
        invalidateWanSession(WanLogoutReason::ServerForcedLogout, event.ret);
        return;
    }
    m_httpOnline = true;
    updateWanDevDetail();

    GetWanDevEvent updateWanDevEvent;
    updateWanDevEvent.SetEventType(GET_WAN_DEV_EVENT);
    updateWanDevEvent.ret = event.ret;
    updateWanDevEvent.sessionGeneration = event.sessionGeneration;
    updateWanDevEvent.clientId = event.clientId;
    updateWanDevEvent.devInfos = event.devInfos;
    updateWanDevEvent.devCnt = event.devCnt;
    onUpdateWanDev(updateWanDevEvent);

    QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, m_connOnline, COM_OK,
        m_connOnline ? WanServiceState::Connected : WanServiceState::Reconnecting,
        WanLogoutReason::Unknown, event.sessionGeneration));
    QueueEvent(new ComGetUserProfileEvent(
        COM_GET_USER_PROFILE_EVENT, event.userProfile, COM_OK, event.sessionGeneration));
}

void MultiComMgr::onUpdateWanDev(const GetWanDevEvent &event)
{
    fnet::FreeInDestructorArg freeDevInfos(event.devInfos, m_networkIntfc->freeWanDevList, event.devCnt);
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || m_clientId != event.clientId) {
        return;
    }
    if (event.ret != COM_OK) {
        maintianWanDev(event.ret);
        return;
    }
    if (!m_httpOnline || !m_connOnline) {
        return;
    }
    m_unUpdateDevList.clear();
    std::map<std::string, fnet_wan_dev_info_t *> devInfoMap;
    for (int i = 0; i < event.devCnt; ++i) {
        const char *devId = event.devInfos[i].devId;
        if (strlen(devId) == 0 || devInfoMap.find(devId) != devInfoMap.end()) {
            BOOST_LOG_TRIVIAL(fatal) << "Empty or duplicated device ID";
        } else {
            devInfoMap.emplace(devId, &event.devInfos[i]);
            if (std::string(event.devInfos[i].updateInfo.status) == "device") {
                if (std::string(event.devInfos[i].model) == "AD5X") {
                    m_unUpdateDevList["AD5X"].emplace_back(event.devInfos[i].name);
                } else if (std::string(event.devInfos[i].model) == "Adventurer 5M" ||
                           std::string(event.devInfos[i].model) == "Adventurer 5M Pro") {
                    m_unUpdateDevList["5M/5MP"].emplace_back(event.devInfos[i].name);     
                }
            }
        }
    }

    std::vector<std::string> removedDevTopics;
    for (auto &comPtr : m_comPtrs) {
        if (comPtr->connectMode() == COM_CONNECT_WAN && devInfoMap.find(comPtr->devId()) == devInfoMap.end()) {
            comPtr.get()->disconnect(0);
            if (!comPtr->devTopic().empty()) {                                     
                removedDevTopics.push_back(comPtr->devTopic());
            }
        }
    }
    if (!removedDevTopics.empty()) {
        ComWanConn::inst()->unsubscribe(removedDevTopics);
    }
    m_pendingWanDevDatas.clear();
    std::vector<std::string> addedDevTopics;
    for (auto &item : devInfoMap) {
        const fnet_wan_dev_info_t &wanDevInfo = *item.second;
        auto it = m_devIdMap.find(wanDevInfo.devId);
        if (it == m_devIdMap.end()) {
            com_ptr_t comPtr = std::make_shared<ComConnection>(m_idNum++, m_clientId,
                wanDevInfo.serialNumber, wanDevInfo.devId, wanDevInfo.devTopic, networkIntfc());
            initConnection(comPtr, makeWanDevData(&wanDevInfo));
            if (strlen(wanDevInfo.devTopic) != 0) {
                addedDevTopics.push_back(wanDevInfo.devTopic);
            }
        } else {
            ComConnection *connection = m_ptrMap.left.at(it->second);
            if (connection->isDisconnect()) {
                m_pendingWanDevDatas.push_back(makeWanDevData(&wanDevInfo));
            } else if (connection->devTopic() != wanDevInfo.devTopic) {
                if (!connection->devTopic().empty()) {
                    ComWanConn::inst()->unsubscribe(std::vector<std::string>(1, connection->devTopic()));
                }
                m_pendingWanDevDatas.push_back(makeWanDevData(&wanDevInfo));
                connection->disconnect(0);
            } else {
                mergeWanDeviceInfo(it->second, wanDevInfo);
            }
        }
    }
    if (!addedDevTopics.empty()) {
        ComWanConn::inst()->updateDetail(addedDevTopics);
        ComWanConn::inst()->subscribe(addedDevTopics);
    }
}

void MultiComMgr::onUpdateUserProfile(const ComGetUserProfileEvent &event)
{
    // User profile is HTTP account data. MQTT availability must not block it.
    if (event.sessionGeneration != m_wanSessionGeneration.load() || !m_httpOnline) {
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
    if (devData.devDetail == nullptr) {
        devData.devDetail = event.devDetail;
    } else {
        m_networkIntfc->freeDevDetail(event.devDetail);
    }
    m_readyIdSet.insert(event.id);
    if (devData.connectMode == COM_CONNECT_WAN) {
        devData.devDetailUpdated = devData.devDetail != nullptr;
        if (devData.devDetail != nullptr && devData.devDetail->status != nullptr) {
            const std::string workStatus = devData.devDetail->status;
            const WanDevicePresence presence = workStatus == "offline" ? WanDevicePresence::Offline
                                                                        : WanDevicePresence::Online;
            updateWanDeviceState(
                event.id, presence, WanDeviceStatusSource::DeviceDetail, &workStatus, false);
        }
    }
    QueueEvent(event.Clone());

    BOOST_LOG_TRIVIAL(info) << "connection_ready";
    BOOST_LOG_TRIVIAL(info) << "devices count: " << m_readyIdSet.size();
}

void MultiComMgr::onConnectionExit(const ComConnectionExitEvent &event)
{
    // [UNBIND] 本函数在 UI 线程执行，且先于 SingleDeviceState::onConnectExit（A.）运行。
    // 若解绑卡顿发生在这里（joinThread 或 free* 阻塞），A. 之前就会有明显时间空档。
    unbind_ts("M0. MultiComMgr::onConnectionExit ENTER id=" + std::to_string(event.id));
    if (m_readyIdSet.find(event.id) != m_readyIdSet.end()) {
        BOOST_LOG_TRIVIAL(info) << "devices count: " << m_readyIdSet.size();
        BOOST_LOG_TRIVIAL(info) << "connection_exit";
    }
    ComConnection *comConnection = m_ptrMap.left.at(event.id);
    unbind_ts("M1. joinThread BEGIN (UI线程等连接线程退出)");
    comConnection->joinThread();
    unbind_ts("M2. joinThread END");
    com_dev_data_t &devData = m_datMap.at(event.id);
    unbind_ts("M3. free dev data BEGIN (freeDevProduct/Detail/GcodeList/TimeLapse)");
    m_networkIntfc->freeDevProduct(devData.devProduct);
    m_networkIntfc->freeDevDetail(devData.devDetail);
    m_networkIntfc->freeGcodeList(devData.lanGcodeList.gcodeDatas, devData.lanGcodeList.gcodeCnt);
    m_networkIntfc->freeGcodeList(devData.wanGcodeList.gcodeDatas, devData.wanGcodeList.gcodeCnt);
    m_networkIntfc->freeTimeLapseVideoList(devData.wanTimeLapseVideoList.videoDatas,
        devData.wanTimeLapseVideoList.videoCnt);
    unbind_ts("M4. free dev data END");
    m_readyIdSet.erase(event.id);
    if (comConnection->connectMode() == COM_CONNECT_WAN) {
        m_devIdMap.erase(devData.wanDevInfo.devId);
    }
    m_datMap.erase(event.id);
    m_ptrMap.left.erase(event.id);
    m_comPtrs.remove_if([comConnection](auto &ptr) { return ptr.get() == comConnection; });
    unbind_ts("M5. QueueEvent(clone) -> 广播给 SingleDeviceState::onConnectExit (A.)");
    QueueEvent(event.Clone());
    unbind_ts("M6. MultiComMgr::onConnectionExit EXIT");
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
    if (devData.connectMode != COM_CONNECT_WAN) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "Device status updated";

    const std::string workStatus = devData.devDetail->status;
    const bool metadataChanged = devData.wanDevInfo.name != devData.devDetail->name
        || devData.wanDevInfo.location != devData.devDetail->location;
    devData.wanDevInfo.name = devData.devDetail->name;
    devData.wanDevInfo.location = devData.devDetail->location;
    const WanDevicePresence presence = workStatus == "offline" ? WanDevicePresence::Offline : WanDevicePresence::Online;
    updateWanDeviceState(
        event.id, presence, WanDeviceStatusSource::DeviceDetail, &workStatus, true, metadataChanged);
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
    if (!m_httpOnline || !m_connOnline) {
        return;
    }
    if (event.fatalError || event.ret == COM_UNAUTHORIZED) {
        maintianWanDev(event.ret);
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
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || event.connectionGeneration != m_mqttConnectionGeneration) {
        return;
    }
    switch (event.status) {
    case FNET_CONN_STATUS_CONNECTED:
        if (m_mqttState != MqttRecoveryState::Connecting && m_mqttState != MqttRecoveryState::Online) {
            return;
        }
        m_connOnline = true;
        m_mqttState = MqttRecoveryState::Online;
        BOOST_LOG_TRIVIAL(info) << "MQTT state=Online, session=" << event.sessionGeneration
                                << ", connection=" << event.connectionGeneration;
        m_mqttConfigRefreshRetryCount = 0;
        m_nextMqttConfigRefreshTime = {};
        m_mqttConnectDeadline = {};
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, m_httpOnline, COM_OK,
            m_httpOnline ? WanServiceState::Connected : WanServiceState::ApiUnavailable,
            WanLogoutReason::Unknown, event.sessionGeneration));
        // cleanSession subscriptions are not retained. Restore all topics and
        // resynchronize MQTT session and device data after the first connection
        // and reconnects. User profile initialization belongs to the HTTP login
        // flow and is refreshed only by an explicit profile-change notification.
        ComWanConn::inst()->subscribe(std::vector<std::string>(1, m_mqttConfig.userTopic));
        ComWanConn::inst()->subscribe(m_mqttConfig.commonTopics);
        subscribeWanDevTopic();
        ComWanConn::inst()->syncLogin(m_mqttConfig.userTopic);
        m_wanDevMaintainThd->setUpdateWanDev();
        updateWanDevDetail();
        break;
    case FNET_CONN_STATUS_DISCONNECTED:
        m_connOnline = false;
        if (m_mqttState == MqttRecoveryState::Disabled) {
            break;
        }
        BOOST_LOG_TRIVIAL(warning) << "MQTT disconnected, session=" << event.sessionGeneration
                                   << ", connection=" << event.connectionGeneration;
        stopMqttConnectionAsync(MqttStopContinuation::ScheduleRetry);
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, false, COM_ERROR,
            WanServiceState::Reconnecting, WanLogoutReason::Unknown, event.sessionGeneration));
        break;
    }
}

void MultiComMgr::onWanConnRead(const WanConnReadEvent& event)
{
    // The MQTT callback already received this command. Its queued UI delivery may race a later connection-state change.
    const bool isUploadLogCommand = event.readData.type == FNET_CONN_READ_UPLOAD_LOG;
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || event.connectionGeneration != m_mqttConnectionGeneration
        || (!m_connOnline && !isUploadLogCommand)) {
        freeConnReadData(event);
        return;
    }
    auto procRepeatLogin = [this](const fnet_conn_read_data_t &readData) {
        fnet_sync_login_info_t *loginInfo = (fnet_sync_login_info_t *)readData.data;
        if (loginInfo == nullptr || loginInfo->clientType == nullptr || loginInfo->clientId == nullptr) {
            return;
        }

        const std::string other_client_type = loginInfo->clientType;
        const std::string other_client_id = loginInfo->clientId;
        if (other_client_type == "pc" && other_client_id != m_clientId) {
            if (is_same_device_client_id(other_client_id, m_clientId)) {
                BOOST_LOG_TRIVIAL(info) << "Ignore repeated pc login from same device, client_id=" << other_client_id;
                return;
            }
            invalidateWanSession(WanLogoutReason::RepeatedLogin, COM_OK);
        }
    };
    auto procDevOffline = [this](const fnet_conn_read_data_t &readData) {
        auto *onlineInfo = (fnet_sync_online_info_t *) readData.data;
        if (onlineInfo == nullptr || onlineInfo->devId == nullptr) {
            return;
        }
        BOOST_LOG_TRIVIAL(warning) << "Receive device offline notification";
        auto it = m_devIdMap.find(onlineInfo->devId);
        if (it != m_devIdMap.end()) {
            updateWanDeviceState(it->second, WanDevicePresence::Offline, WanDeviceStatusSource::SyncOffline);
        }
    };
    auto procDevOnline = [this](const fnet_conn_read_data_t &readData) {
        auto *onlineInfo = (fnet_sync_online_info_t *) readData.data;
        if (onlineInfo == nullptr || onlineInfo->devId == nullptr) {
            return;
        }
        BOOST_LOG_TRIVIAL(info) << "receive device online, " << onlineInfo->devId;
        auto it = m_devIdMap.find(onlineInfo->devId);
        if (it == m_devIdMap.end()) {
            // The device may have been bound by another client. Refresh the list
            // before resolving the device topic.
            m_wanDevMaintainThd->setUpdateWanDev();
            return;
        }
        const com_id_t comId = it->second;
        updateWanDeviceState(comId, WanDevicePresence::Online, WanDeviceStatusSource::SyncOnline);
        com_dev_data_t &devData = m_datMap.at(comId);
        if (!devData.wanDevInfo.devTopic.empty()) {
            // SYNC_ONLINE confirms presence but does not contain the operational
            // state. Restore the last detail and immediately request fresh detail.
            ComWanConn::inst()->updateDetail(std::vector<std::string>(1, devData.wanDevInfo.devTopic));
            devData.wanLastDetailRefreshTime = std::chrono::steady_clock::now();
        }
    };
    auto procDevDetailUpdate = [this](const fnet_conn_read_data_t &readData) {
        auto *detail = (fnet_dev_detail_t *) readData.data;
        if (detail == nullptr || detail->devId == nullptr) {
            return;
        }
        auto it = m_devIdMap.find(detail->devId);
        if (it != m_devIdMap.end()) {
            ComDevDetailUpdateEvent devDetailUpdateEvent(COM_DEV_DETAIL_UPDATE_EVENT,
                it->second, ComInvalidCommandId, detail);
            onDevDetailUpdate(devDetailUpdateEvent);
        } else {
            m_networkIntfc->freeDevDetail(detail);
        }
    };
    auto procCloudSliceUpdate = [this](const fnet_conn_read_data_t& readData) {
        auto it = m_devIdMap.find(((fnet_slice_state_t*) readData.data)->deviceId);
        if (it != m_devIdMap.end()) {
            auto evt = new ComCloudSliceUpdateEvent(COM_CONN_CLOUD_SLICE_EVENT, it->second, ComInvalidCommandId,
                                                         (fnet_slice_state_t*) readData.data);
            QueueEvent(evt);
        } else {
            m_networkIntfc->freeSliceState((fnet_slice_state_t*) readData.data);
        }
    };
    auto procJobInfoUpdate = [this](const fnet_conn_read_data_t& readData) {
        auto it = m_devIdMap.find(((fnet_job_info_t*) readData.data)->deviceId);
        if (it != m_devIdMap.end()) {
            updateWanDeviceState(it->second, WanDevicePresence::Online, WanDeviceStatusSource::MessageActivity);
            auto evt = new ComJobInfoUpdateEvent(COM_CONN_JOB_INFO_EVENT, it->second, ComInvalidCommandId, (fnet_job_info_t*) readData.data);
            QueueEvent(evt);
        } else {
            m_networkIntfc->freeJobInfo((fnet_job_info_t*) readData.data);
        }
    };
    auto procDevKeepAlive = [this](const fnet_conn_read_data_t& readData) {
        auto it = m_devIdMap.find((char *)readData.data);
        if (it != m_devIdMap.end()) {
            com_dev_data_t &devData = m_datMap.at(it->second);
            if (devData.devDetail != nullptr && devData.devDetailUpdated && devData.devDetail->status != nullptr) {
                const std::string workStatus = devData.devDetail->status;
                updateWanDeviceState(it->second, WanDevicePresence::Online, WanDeviceStatusSource::KeepAlive, &workStatus);
            } else {
                updateWanDeviceState(it->second, WanDevicePresence::Online, WanDeviceStatusSource::KeepAlive);
            }
        }
    };
    auto procUploadLog = [this](const fnet_conn_read_data_t& readData) {
        const auto* uploadLogInfo = static_cast<const fnet_upload_log_info_t*>(readData.data);
        if (uploadLogInfo == nullptr || uploadLogInfo->feedbackId == nullptr || uploadLogInfo->clientType == nullptr) {
            BOOST_LOG_TRIVIAL(warning) << "Ignore invalid feedback log upload command";
            return;
        }
        if (readData.topic == nullptr || m_mqttConfig.userTopic != readData.topic) {
            BOOST_LOG_TRIVIAL(warning) << "Ignore feedback log upload command from an unexpected topic";
            return;
        }

        const std::string clientType = uploadLogInfo->clientType;
        const std::string feedbackId = uploadLogInfo->feedbackId;
        if (clientType != "slice" || feedbackId.empty()) {
            BOOST_LOG_TRIVIAL(warning) << "Ignore feedback log upload command for clientType=" << clientType;
            return;
        }
        if (m_logUploadThreadPool == nullptr) {
            BOOST_LOG_TRIVIAL(error) << "Cannot schedule feedback log upload";
            return;
        }

        ScopedWanDevToken scopedToken = WanDevTokenMgr::inst()->getScopedToken();
        const std::string accessToken = scopedToken.accessToken();
        if (accessToken.empty()) {
            BOOST_LOG_TRIVIAL(error) << "Cannot upload feedback logs without an access token";
            return;
        }
        const std::string          clientId = m_clientId;
        std::shared_ptr<WaitEvent> cancelEvent;
        std::uint64_t              sessionGeneration = 0;
        {
            std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
            cancelEvent       = m_feedbackLogCancelEvent;
            sessionGeneration = m_feedbackLogSessionGeneration;
            if (cancelEvent == nullptr || cancelEvent->get()) {
                BOOST_LOG_TRIVIAL(warning) << "Ignore feedback log upload command outside an active login session";
                return;
            }
            if (m_activeFeedbackLogIds.find(feedbackId) != m_activeFeedbackLogIds.end() ||
                m_completedFeedbackLogIds.find(feedbackId) != m_completedFeedbackLogIds.end()) {
                BOOST_LOG_TRIVIAL(info) << "Ignore duplicate feedback log upload command";
                return;
            }
            m_activeFeedbackLogIds.insert(feedbackId);
        }
        try {
            const std::string dataDir      = m_dataDir;
            const std::string accountScope = m_mqttConfig.userTopic;
            m_logUploadThreadPool->post([this, clientId, accessToken, feedbackId, dataDir, accountScope, cancelEvent,
                                         sessionGeneration]() {
                FeedbackLogUploadClaim claim(dataDir, accountScope, feedbackId);
                const auto             claimStatus = claim.tryAcquire();
                if (claimStatus == FeedbackLogUploadClaim::Status::Busy) {
                    BOOST_LOG_TRIVIAL(info) << "Feedback log upload is already active in another local client";
                    std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
                    if (sessionGeneration == m_feedbackLogSessionGeneration) {
                        m_activeFeedbackLogIds.erase(feedbackId);
                    }
                    return;
                }
                if (claimStatus == FeedbackLogUploadClaim::Status::Error) {
                    BOOST_LOG_TRIVIAL(error) << "Cannot coordinate feedback log upload between local clients";
                    std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
                    if (sessionGeneration == m_feedbackLogSessionGeneration) {
                        m_activeFeedbackLogIds.erase(feedbackId);
                    }
                    return;
                }

                bool succeeded = claimStatus == FeedbackLogUploadClaim::Status::Completed;
                if (succeeded) {
                    BOOST_LOG_TRIVIAL(info) << "Feedback log upload was already completed by another local client";
                } else {
                    succeeded = ExportLogs::uploadFeedback(clientId, accessToken, feedbackId, cancelEvent.get());
                    if (succeeded && !claim.markCompleted()) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to persist feedback log upload completion state";
                    }
                }
                std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
                if (sessionGeneration != m_feedbackLogSessionGeneration || cancelEvent != m_feedbackLogCancelEvent) {
                    return;
                }
                m_activeFeedbackLogIds.erase(feedbackId);
                if (!succeeded) {
                    return;
                }
                constexpr size_t maxFeedbackLogHistory = 256;
                if (m_feedbackLogHistory.size() >= maxFeedbackLogHistory) {
                    m_completedFeedbackLogIds.erase(m_feedbackLogHistory.front());
                    m_feedbackLogHistory.pop_front();
                }
                m_completedFeedbackLogIds.insert(feedbackId);
                m_feedbackLogHistory.push_back(feedbackId);
            });
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
            if (sessionGeneration == m_feedbackLogSessionGeneration) {
                m_activeFeedbackLogIds.erase(feedbackId);
            }
            BOOST_LOG_TRIVIAL(error) << "Cannot schedule feedback log upload: " << e.what();
        } catch (...) {
            std::lock_guard<std::mutex> lock(m_feedbackLogMutex);
            if (sessionGeneration == m_feedbackLogSessionGeneration) {
                m_activeFeedbackLogIds.erase(feedbackId);
            }
            BOOST_LOG_TRIVIAL(error) << "Cannot schedule feedback log upload";
        }
    };
    switch (event.readData.type) {
    case FNET_CONN_READ_SYS_NOTIFY: QueueEvent(new ComConnSysNotifyEvent(COM_CONN_SYS_NOTIFY_EVENT, (char*) event.readData.data)); break;
    case FNET_CONN_READ_UPDATE_NOTIFY:
        QueueEvent(new ComConnSysNotifyEvent(COM_CONN_UPDATE_NOTIFY_EVENT, (char*) event.readData.data)); 
        break;
    case FNET_CONN_READ_SYNC_USER_PROFILE:
        m_wanDevMaintainThd->setUpdateUserProfile();
        break;
    case FNET_CONN_READ_SYNC_UNREGISTER_USER:
        invalidateWanSession(WanLogoutReason::UserUnregistered, COM_OK);
        break;
    case FNET_CONN_READ_SYNC_LOGIN:
        procRepeatLogin(event.readData);
        break;
    case FNET_CONN_READ_SYNC_BIND_DEVICE:
    case FNET_CONN_READ_SYNC_UNBIND_DEVICE:
        m_wanDevMaintainThd->setUpdateWanDev();
        m_pendingSetUpdateWanDevTime = std_precise_clock::time_point::max();
        break;
    case FNET_CONN_READ_SYNC_ONLINE:
        procDevOnline(event.readData);
        break;
    case FNET_CONN_READ_SYNC_OFFLINE:
        procDevOffline(event.readData);
        break;
    case FNET_CONN_READ_DEVICE_DETAIL:
        procDevDetailUpdate(event.readData);
        break;
    case FNET_CONN_READ_SLICE_STATE: 
        procCloudSliceUpdate(event.readData);
        break;
    case FNET_CONN_READ_JOB_DOWNLOAD:
    case FNET_CONN_READ_JOB_UNZIP: 
        procJobInfoUpdate(event.readData); 
        break;
    case FNET_CONN_READ_DEVICE_KEEP_ALIVE:
        procDevKeepAlive(event.readData);
        break;
    case FNET_CONN_READ_UPLOAD_LOG:
        procUploadLog(event.readData);
        break;
    }
    if (event.readData.type == FNET_CONN_READ_DEVICE_DETAIL) {
        m_networkIntfc->freeString(event.readData.topic);
    } else {
        freeConnReadData(event);
    }
}

void MultiComMgr::onWanConnUnauthorized(const WanConnUnauthorizedEvent &event)
{
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || event.connectionGeneration != m_mqttConnectionGeneration) {
        return;
    }
    BOOST_LOG_TRIVIAL(warning) << "MQTT runtime config was unauthorized; restarting MQTT recovery";
    startMqttRecovery();
}

void MultiComMgr::requestMqttConfigRefresh()
{
    if (!m_login || m_mqttState != MqttRecoveryState::RetryScheduled
        || m_mqttStopInFlight || !m_threadPool) {
        return;
    }
    m_mqttState = MqttRecoveryState::FetchingConfig;
    const uint64_t sessionGeneration = m_wanSessionGeneration.load();
    const uint64_t requestGeneration = ++m_mqttRequestGeneration;
    const std::string clientId = m_clientId;
    ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
    const bool allowTokenRefresh = token.tokenRevision() != m_mqttBlockedRefreshTokenRevision;
    BOOST_LOG_TRIVIAL(info) << "MQTT state=FetchingConfig, session=" << sessionGeneration
                            << ", request=" << requestGeneration
                            << ", token_revision=" << token.tokenRevision()
                            << ", allow_token_refresh=" << allowTokenRefresh;
    m_threadPool->post([this, sessionGeneration, requestGeneration, clientId,
                        token = std::move(token), allowTokenRefresh]() mutable {
        com_mqtt_config_t config;
        ComErrno ret = MultiComUtils::getMqttConfig(
            clientId, token.accessToken(), config, ComTimeoutWanB);
        bool tokenRefreshAttempted = false;
        bool tokenRefreshSucceeded = false;
        if (ret == COM_UNAUTHORIZED && allowTokenRefresh) {
            tokenRefreshAttempted = true;
            const uint64_t previousRevision = token.tokenRevision();
            ret = WanDevTokenMgr::inst()->refreshToken(token);
            if (ret == COM_OK) {
                tokenRefreshSucceeded = token.tokenRevision() != previousRevision;
                ret = MultiComUtils::getMqttConfig(
                    clientId, token.accessToken(), config, ComTimeoutWanB);
            }
        }
        QueueEvent(new ComMqttConfigRefreshEvent(COM_MQTT_CONFIG_REFRESH_EVENT, config, ret,
            sessionGeneration, requestGeneration, token.tokenRevision(),
            tokenRefreshAttempted, tokenRefreshSucceeded));
    });
}

void MultiComMgr::scheduleMqttConfigRefreshRetry()
{
    static constexpr int retryDelaysSeconds[] = { 1, 3, 5, 10, 20, 30 };
    const size_t delayIndex = std::min<size_t>(
        m_mqttConfigRefreshRetryCount, std::size(retryDelaysSeconds) - 1);
    const int delaySeconds = retryDelaysSeconds[delayIndex];
    ++m_mqttConfigRefreshRetryCount;
    m_mqttState = MqttRecoveryState::RetryScheduled;
    m_nextMqttConfigRefreshTime =
        std::chrono::steady_clock::now() + std::chrono::seconds(delaySeconds);
    m_mqttConnectDeadline = {};
    BOOST_LOG_TRIVIAL(warning) << "MQTT recovery retry scheduled in " << delaySeconds
                               << " seconds, attempt=" << m_mqttConfigRefreshRetryCount
                               << ", session=" << m_wanSessionGeneration.load();
}

void MultiComMgr::startMqttRecovery()
{
    if (!m_login) {
        return;
    }
    // Config refreshes are accepted only after the old connection is fully
    // stopped, so a late timer or callback cannot race connection teardown.
    m_mqttState = MqttRecoveryState::RetryScheduled;
    m_mqttConfigRefreshRetryCount = 0;
    m_mqttConfig = {};
    stopMqttConnectionAsync(MqttStopContinuation::RefreshConfig);
    QueueEvent(new ComWanDevMaintainEvent(
        COM_WAN_DEV_MAINTAIN_EVENT, true, false, COM_OK, WanServiceState::Reconnecting,
        WanLogoutReason::Unknown, m_wanSessionGeneration.load()));
}

void MultiComMgr::disableMqtt()
{
    m_mqttState = MqttRecoveryState::Disabled;
    m_mqttConfigRefreshRetryCount = 0;
    m_mqttBlockedRefreshTokenRevision = 0;
    m_mqttConfig = {};
    stopMqttConnectionAsync(MqttStopContinuation::None);
}

void MultiComMgr::stopMqttConnectionAsync(MqttStopContinuation continuation)
{
    ++m_mqttRequestGeneration;
    ++m_mqttConnectionGeneration;
    m_connOnline = false;
    m_nextMqttConfigRefreshTime = {};
    m_mqttConnectDeadline = {};
    m_mqttStopContinuation = continuation;
    m_mqttState = continuation == MqttStopContinuation::None
        ? MqttRecoveryState::Disabled : MqttRecoveryState::Stopping;

    if (m_mqttStopInFlight) {
        return;
    }
    if (!m_connectionStopThreadPool) {
        BOOST_LOG_TRIVIAL(error) << "Unable to stop MQTT connection: worker pool is unavailable";
        return;
    }

    m_mqttStopInFlight = true;
    const uint64_t stopGeneration = ++m_mqttStopGeneration;
    BOOST_LOG_TRIVIAL(info) << "MQTT state=Stopping, stop=" << stopGeneration
                            << ", session=" << m_wanSessionGeneration.load();
    m_connectionStopThreadPool->post([this, stopGeneration]() {
        ComWanConn::inst()->freeConnBlocking();
        QueueEvent(new ComMqttConnectionStoppedEvent(
            COM_MQTT_CONNECTION_STOPPED_EVENT, stopGeneration));
    });
}

void MultiComMgr::onMqttConnectionStopped(const ComMqttConnectionStoppedEvent &event)
{
    if (!m_mqttStopInFlight || event.stopGeneration != m_mqttStopGeneration) {
        return;
    }

    m_mqttStopInFlight = false;
    const MqttStopContinuation continuation = m_mqttStopContinuation;
    m_mqttStopContinuation = MqttStopContinuation::None;
    BOOST_LOG_TRIVIAL(info) << "MQTT connection stopped, stop=" << event.stopGeneration
                            << ", session=" << m_wanSessionGeneration.load();

    if (!m_login || continuation == MqttStopContinuation::None) {
        m_mqttState = MqttRecoveryState::Disabled;
        return;
    }

    // Both continuations enter RetryScheduled first because config requests
    // are accepted only from that state. A disconnect preserves the existing
    // backoff, while an explicit recovery starts immediately.
    m_mqttState = MqttRecoveryState::RetryScheduled;
    if (continuation == MqttStopContinuation::ScheduleRetry) {
        scheduleMqttConfigRefreshRetry();
    } else {
        requestMqttConfigRefresh();
    }
}

void MultiComMgr::onMqttConfigRefresh(const ComMqttConfigRefreshEvent &event)
{
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || event.requestGeneration != m_mqttRequestGeneration
        || m_mqttState != MqttRecoveryState::FetchingConfig) {
        return;
    }
    if (event.ret != COM_OK) {
        if (event.ret == COM_UNAUTHORIZED && event.tokenRefreshAttempted
            && event.tokenRefreshSucceeded) {
            m_mqttBlockedRefreshTokenRevision = event.tokenRevision;
        }
        BOOST_LOG_TRIVIAL(warning) << "MQTT config request failed: " << (int) event.ret;
        scheduleMqttConfigRefreshRetry();
        QueueEvent(new ComWanDevMaintainEvent(
            COM_WAN_DEV_MAINTAIN_EVENT, true, false, event.ret, WanServiceState::Reconnecting,
            WanLogoutReason::Unknown, event.sessionGeneration));
        return;
    }
    m_mqttBlockedRefreshTokenRevision = 0;
    m_mqttConfig = event.config;
    m_connOnline = false;
    m_mqttState = MqttRecoveryState::Connecting;
    m_nextMqttConfigRefreshTime = {};
    const uint64_t connectionGeneration = ++m_mqttConnectionGeneration;
    m_mqttConnectDeadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(MQTT_CONNECT_TIMEOUT_SECONDS);
    BOOST_LOG_TRIVIAL(info) << "MQTT state=Connecting, session=" << event.sessionGeneration
                            << ", connection=" << connectionGeneration;
    const ComErrno createRet = ComWanConn::inst()->createConn(networkIntfc(), m_clientId.c_str(),
        event.sessionGeneration, connectionGeneration);
    if (createRet != COM_OK) {
        ++m_mqttConnectionGeneration;
        BOOST_LOG_TRIVIAL(warning) << "MQTT connection creation failed: " << (int) createRet;
        scheduleMqttConfigRefreshRetry();
        QueueEvent(new ComWanDevMaintainEvent(
            COM_WAN_DEV_MAINTAIN_EVENT, true, false, createRet, WanServiceState::Reconnecting,
            WanLogoutReason::Unknown, event.sessionGeneration));
    }
}
void MultiComMgr::onRefreshToken(const ComRefreshTokenEvent &event)
{
    if (!m_login || event.sessionGeneration != m_wanSessionGeneration.load()
        || event.ret != COM_OK && event.ret != COM_UNAUTHORIZED) {
        return;
    }
    if (event.ret == COM_UNAUTHORIZED) {
        invalidateWanSession(WanLogoutReason::RefreshTokenRejected, event.ret);
        return;
    }
    QueueEvent(event.Clone());
}

std::string MultiComMgr::generateClientId()
{
    std::string uuidStr = boost::uuids::to_string(boost::uuids::random_generator()());
    uuidStr = sanitized_client_id_part(uuidStr);
    return "pc_" + get_device_client_id_part() + "_" + uuidStr;
}

std::string MultiComMgr::getDevTopic(const std::string &devId)
{
    auto devIdIt = m_devIdMap.find(devId);
    if (devIdIt == m_devIdMap.end()) {
        return std::string();
    }
    auto comIdIt = m_datMap.find(devIdIt->second);
    if (comIdIt == m_datMap.end()) {
        return std::string();
    }
    if (comIdIt->second.connectMode != COM_CONNECT_WAN) {
        return std::string();
    }
    return comIdIt->second.wanDevInfo.devTopic;
}

bool MultiComMgr::updateWanDeviceState(com_id_t id, WanDevicePresence presence, WanDeviceStatusSource source,
    const std::string *workStatus, bool notify, bool forceNotify)
{
    auto dataIt = m_datMap.find(id);
    if (dataIt == m_datMap.end() || dataIt->second.connectMode != COM_CONNECT_WAN) {
        return false;
    }

    com_dev_data_t &devData = dataIt->second;
    const WanDevicePresence oldPresence = devData.wanPresence;
    const std::string oldWorkStatus = devData.wanDevInfo.status;
    const auto now = std::chrono::steady_clock::now();

    devData.wanPresence = presence;
    devData.wanStatusSource = source;
    if (workStatus != nullptr && !workStatus->empty() && *workStatus != "offline") {
        devData.wanDevInfo.status = *workStatus;
    }

    switch (source) {
    case WanDeviceStatusSource::SyncOnline:
    case WanDeviceStatusSource::SyncOffline:
    case WanDeviceStatusSource::DeviceDetail:
    case WanDeviceStatusSource::KeepAlive:
    case WanDeviceStatusSource::MessageActivity:
        devData.wanLastMessageTime = now;
        break;
    case WanDeviceStatusSource::Initial:
    case WanDeviceStatusSource::HttpDeviceList:
    case WanDeviceStatusSource::StaleTimeout:
        break;
    }
    if (source == WanDeviceStatusSource::DeviceDetail) {
        devData.wanLastDetailTime = now;
        devData.wanLastDetailRefreshTime = {};
    }

    const bool stateChanged = oldPresence != devData.wanPresence || oldWorkStatus != devData.wanDevInfo.status;
    if (stateChanged) {
        devData.wanLastStateChangeTime = now;
        BOOST_LOG_TRIVIAL(info) << "wan device state changed, devId=" << devData.wanDevInfo.devId
                                << ", presence=" << wan_device_presence_name(devData.wanPresence)
                                << ", workStatus=" << devData.wanDevInfo.status
                                << ", source=" << wan_device_status_source_name(source);
    }
    if (notify && (stateChanged || forceNotify) && m_readyIdSet.find(id) != m_readyIdSet.end()) {
        QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, id));
    }
    return stateChanged || forceNotify;
}

bool MultiComMgr::mergeWanDeviceInfo(com_id_t id, const fnet_wan_dev_info_t &wanDevInfo)
{
    auto dataIt = m_datMap.find(id);
    if (dataIt == m_datMap.end() || dataIt->second.connectMode != COM_CONNECT_WAN) {
        return false;
    }

    com_dev_data_t &devData = dataIt->second;
    com_wan_dev_info_t &current = devData.wanDevInfo;
    const bool metadataChanged = current.name != wanDevInfo.name || current.model != wanDevInfo.model
        || current.imageUrl != wanDevInfo.imageUrl || current.location != wanDevInfo.location
        || current.serialNumber != wanDevInfo.serialNumber || current.devTopic != wanDevInfo.devTopic
        || current.updateInfo.status != wanDevInfo.updateInfo.status || current.updateInfo.title != wanDevInfo.updateInfo.title
        || current.updateInfo.content != wanDevInfo.updateInfo.content || current.updateInfo.tips != wanDevInfo.updateInfo.tips;

    current.name = wanDevInfo.name;
    current.model = wanDevInfo.model;
    current.imageUrl = wanDevInfo.imageUrl;
    current.location = wanDevInfo.location;
    current.serialNumber = wanDevInfo.serialNumber;
    current.devTopic = wanDevInfo.devTopic;
    current.updateInfo.status = wanDevInfo.updateInfo.status;
    current.updateInfo.title = wanDevInfo.updateInfo.title;
    current.updateInfo.content = wanDevInfo.updateInfo.content;
    current.updateInfo.tips = wanDevInfo.updateInfo.tips;

    if (devData.wanPresence == WanDevicePresence::Unknown) {
        const std::string httpStatus = wanDevInfo.status;
        const WanDevicePresence presence = httpStatus.empty() ? WanDevicePresence::Unknown
            : httpStatus == "offline" ? WanDevicePresence::Offline : WanDevicePresence::Online;
        const std::string *workStatus = !httpStatus.empty() && httpStatus != "offline" ? &httpStatus : nullptr;
        return updateWanDeviceState(id, presence, WanDeviceStatusSource::HttpDeviceList, workStatus, true, metadataChanged);
    }

    if (metadataChanged && m_readyIdSet.find(id) != m_readyIdSet.end()) {
        QueueEvent(new ComWanDevInfoUpdateEvent(COM_WAN_DEV_INFO_UPDATE_EVENT, id));
    }
    return metadataChanged;
}

com_dev_data_t MultiComMgr::makeWanDevData(const fnet_wan_dev_info_t *wanDevInfo)
{
    com_dev_data_t devData;
    devData.connectMode = COM_CONNECT_WAN;
    devData.wanDevInfo.devId = wanDevInfo->devId;
    devData.wanDevInfo.name = wanDevInfo->name;
    devData.wanDevInfo.model = wanDevInfo->model;
    devData.wanDevInfo.imageUrl = wanDevInfo->imageUrl;
    devData.wanDevInfo.location = wanDevInfo->location;
    devData.wanDevInfo.serialNumber = wanDevInfo->serialNumber;
    devData.wanDevInfo.devTopic = wanDevInfo->devTopic;
    devData.wanDevInfo.updateInfo.status = wanDevInfo->updateInfo.status;
    devData.wanDevInfo.updateInfo.title = wanDevInfo->updateInfo.title;
    devData.wanDevInfo.updateInfo.content = wanDevInfo->updateInfo.content;
    devData.wanDevInfo.updateInfo.tips = wanDevInfo->updateInfo.tips;
    devData.devProduct = nullptr;
    devData.devDetail = nullptr;
    devData.lanGcodeList.gcodeCnt = 0;
    devData.lanGcodeList.gcodeDatas = nullptr;
    devData.wanGcodeList.gcodeCnt = 0;
    devData.wanGcodeList.gcodeDatas = nullptr;
    devData.wanTimeLapseVideoList.videoCnt = 0;
    devData.wanTimeLapseVideoList.videoDatas = nullptr;
    devData.devDetailUpdated = false;
    const std::string httpStatus = wanDevInfo->status;
    if (httpStatus == "offline") {
        devData.wanPresence = WanDevicePresence::Offline;
    } else if (!httpStatus.empty()) {
        devData.wanPresence = WanDevicePresence::Online;
        devData.wanDevInfo.status = httpStatus;
    } else {
        devData.wanPresence = WanDevicePresence::Unknown;
    }
    devData.wanStatusSource = WanDeviceStatusSource::HttpDeviceList;
    if (devData.wanPresence != WanDevicePresence::Unknown) {
        devData.wanLastStateChangeTime = std::chrono::steady_clock::now();
    }
    memset(&devData.lanDevInfo, 0, sizeof(devData.lanDevInfo));
    return devData;
}

void MultiComMgr::maintianWanDev(ComErrno ret)
{
    if (!m_login) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "MultiComMgr::maintianWanDev " << (int)ret;
    if (ret != COM_OK && m_httpOnline) {
        m_httpOnline = false;
        m_wanDevMaintainThd->setReloginHttp();
        // HTTP maintenance failure does not prove that any printer is offline.
        // Keep the last device states while the HTTP session recovers.
        QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, true, false, ret,
            WanServiceState::ApiUnavailable, WanLogoutReason::Unknown, m_wanSessionGeneration.load()));
    }
}

void MultiComMgr::setMaintainThdReqHeader(bool isCheckVersionOnTestServer)
{
#ifdef _WIN32
    int64_t appId = 31;
    int64_t platId = 14;
    if (isCheckVersionOnTestServer) {
        appId = 46;
        platId = 20;
    }
#else // Mac OS
    int64_t appId = 31;
    int64_t platId = 15;
    if (isCheckVersionOnTestServer) {
        appId = 46;
        platId = 19;
    }
#endif
    m_wanDevMaintainThd->setReqHeaders(
        m_clientId, appId, platId, m_wanSessionGeneration.load());
}

void MultiComMgr::subscribeWanDevTopic()
{
    if (!m_connOnline) {
        return;
    }
    std::vector<std::string> devTopics;
    for (auto &item : m_ptrMap.left) {
        if (item.second->connectMode() == COM_CONNECT_WAN
        && !item.second->isDisconnect()
        && !item.second->devTopic().empty()) {
            devTopics.push_back(item.second->devTopic());
        }
    }
    if (!devTopics.empty()) {
        ComWanConn::inst()->subscribe(devTopics);
    }
}

void MultiComMgr::updateWanDevDetail()
{
    if (!m_httpOnline || !m_connOnline) {
        return;
    }
    std::vector<std::string> devTopics;
    for (auto &item : m_ptrMap.left) {
        if (item.second->connectMode() == COM_CONNECT_WAN
        && !item.second->isDisconnect()
        && !item.second->devTopic().empty()) {
            devTopics.push_back(item.second->devTopic());
        }
    }
    if (!devTopics.empty()) {
        ComWanConn::inst()->updateDetail(devTopics);
    }
}

void MultiComMgr::freeConnReadData(const WanConnReadEvent &event)
{
    switch (event.readData.type) {
    case FNET_CONN_READ_SYS_NOTIFY:
    case FNET_CONN_READ_UPDATE_NOTIFY:
    case FNET_CONN_READ_DEVICE_KEEP_ALIVE:
        m_networkIntfc->freeString((char *)event.readData.data);
        break;
    case FNET_CONN_READ_SYNC_USER_PROFILE:
    case FNET_CONN_READ_SYNC_UNREGISTER_USER:
        break;
    case FNET_CONN_READ_SYNC_LOGIN:
        m_networkIntfc->freeSyncLoginInfo((fnet_sync_login_info_t *)event.readData.data);
        break;
    case FNET_CONN_READ_SYNC_BIND_DEVICE:
    case FNET_CONN_READ_SYNC_UNBIND_DEVICE:
        m_networkIntfc->freeSyncBindInfo((fnet_sync_bind_info *)event.readData.data);
        break;
    case FNET_CONN_READ_SYNC_ONLINE:
    case FNET_CONN_READ_SYNC_OFFLINE:
        m_networkIntfc->freeSyncOnlineInfo((fnet_sync_online_info_t *)event.readData.data);
        break;
    case FNET_CONN_READ_UPLOAD_LOG:
        if (m_networkIntfc->freeUploadLogInfo != nullptr) {
            m_networkIntfc->freeUploadLogInfo((fnet_upload_log_info_t *)event.readData.data);
        }
        break;
    }
    m_networkIntfc->freeString(event.readData.topic);
}

}} // namespace Slic3r::GUI
