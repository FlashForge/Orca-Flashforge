#ifndef slic3r_GUI_MultiComMgr_hpp_
#define slic3r_GUI_MultiComMgr_hpp_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <boost/bimap.hpp>
#include <wx/event.h>
#include <wx/timer.h>
#include "ComConnection.hpp"
#include "ComThreadPool.hpp"
#include "ComWanConn.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "Singleton.hpp"
#include "WaitEvent.hpp"
#include "WanDevMaintainThd.hpp"
#include "WanDevSendGcodeThd.hpp"

namespace Slic3r { namespace GUI {

class MultiComMgr : public wxEvtHandler, public Singleton<MultiComMgr>
{
public:
    MultiComMgr();

    bool initalize(const std::string &dllPath, const std::string &dataDir);

    void uninitalize();

    fnet::FlashNetworkIntfc *networkIntfc();

    const std::string &clientId() const { return m_clientId; }
    bool isLoggedIn() const { return m_login; }
    uint64_t wanSessionGeneration() const { return m_wanSessionGeneration.load(); }
    const std::string &userId() const { return m_userId; }

    std::string homePageUrl();
    
    com_id_t addLanDev(const fnet_lan_dev_info_t &devInfo, const std::string &checkCode);

    void removeLanDev(com_id_t id);

    ComErrno addWanDev(const com_token_data_t &tokenData, bool isCheckVersionOnTestServer,
        com_add_wan_dev_data_t &addDevData, int tryCnt, int tryMsInterval,
        ComSessionInitStage *failedStage = nullptr);

    void removeWanDev();

    // Starts the optional MQTT subsystem after the account session has been
    // published as logged in. Must be called on the wx event thread.
    void startWanMqtt();

    // Refreshes the active account token without rebuilding the WAN session.
    // Completion is reported asynchronously through
    // COM_ACCESS_TOKEN_REFRESH_RESULT_EVENT.
    bool requestAccessTokenRefresh(uint64_t requestGeneration);

    // Requests the latest WAN device list immediately. The protected request
    // also drives the existing Unauthorized -> refresh -> retry recovery path.
    void requestWanDeviceListRefresh();

    ComErrno bindWanDev(const std::string &ip, unsigned short port, const std::string &serialNumber,
        unsigned short pid, const std::string &name, unsigned short bindType);

    ComErrno unbindWanDev(const std::string &serialNumber, const std::string &devId);

    com_id_list_t getReadyDevList();

    const com_dev_data_t &devData(com_id_t id, bool *valid = nullptr);

    bool putCommand(com_id_t id, ComCommand *command); // this method takes ownership of the command, i.e. it will delete it itself

    bool abortSendGcode(com_id_t id, int commandId);

    bool wanSendGcode(const std::vector<std::string> &devIds, const std::vector<std::string> &devSerialNumbers,
        const com_send_gcode_data_t &sendGocdeData);

    bool abortWanSendGcode();

    const std::unordered_map<std::string, std::vector<std::string>>& getDevUnupdateList();

private:
    enum class MqttRecoveryState {
        Disabled,
        Stopping,
        FetchingConfig,
        Connecting,
        Online,
        RetryScheduled
    };

    enum class MqttStopContinuation {
        None,
        RefreshConfig,
        ScheduleRetry
    };

    using std_precise_clock = std::chrono::high_resolution_clock;

    typedef std::shared_ptr<ComConnection> com_ptr_t;

    typedef boost::bimap<com_id_t, ComConnection*> com_ptr_map_t;

    typedef boost::bimap<com_id_t, ComConnection*>::value_type com_ptr_map_val_t;
    
    void initConnection(const com_ptr_t &comPtr, const com_dev_data_t &devData);

    void onTimer(const wxTimerEvent &event);

    void onReloginHttp(ReloginHttpEvent &event);

    void onUpdateWanDev(const GetWanDevEvent &event);
    
    void onUpdateUserProfile(const ComGetUserProfileEvent &event);

    void onConnectionReady(const ComConnectionReadyEvent &event);

    void onConnectionExit(const ComConnectionExitEvent &event);

    void onDevDetailUpdate(const ComDevDetailUpdateEvent &event);

    void onGetDevGcodeList(const ComGetDevGcodeListEvent &event);

    void onGetDevTimeLapseVideoList(const ComGetTimeLapseVideoListEvent &event);

    void onCommandFailed(const CommandFailedEvent &event);

    void onWanConnStatus(const WanConnStatusEvent &event);

    void onWanConnRead(const WanConnReadEvent &event);

    void onWanConnUnauthorized(const WanConnUnauthorizedEvent &event);

    void onRefreshToken(const ComRefreshTokenEvent &event);

    void onMqttConfigRefresh(const ComMqttConfigRefreshEvent &event);

    void onMqttConnectionStopped(const ComMqttConnectionStoppedEvent &event);

    void requestMqttConfigRefresh();

    void scheduleMqttConfigRefreshRetry();

    void startMqttRecovery();

    void disableMqtt();

    void stopMqttConnectionAsync(MqttStopContinuation continuation);

    std::string generateClientId();

    std::string getDevTopic(const std::string &devId);

    com_dev_data_t makeWanDevData(const fnet_wan_dev_info_t *wanDevInfo);

    bool updateWanDeviceState(com_id_t id, WanDevicePresence presence, WanDeviceStatusSource source,
        const std::string *workStatus = nullptr, bool notify = true, bool forceNotify = false);

    bool mergeWanDeviceInfo(com_id_t id, const fnet_wan_dev_info_t &wanDevInfo);

    void maintianWanDev(ComErrno ret);

    void invalidateWanSession(WanLogoutReason reason, ComErrno ret);

    void teardownWanSession();

    void setMaintainThdReqHeader(bool isCheckVersionTestServer);

    void subscribeWanDevTopic();

    void updateWanDevDetail();

    void freeConnReadData(const WanConnReadEvent& event);

    void startFeedbackLogSession();

    void stopFeedbackLogSession();

private:
    int                                                       m_idNum;
    std::atomic_bool                                          m_login;
    bool                                                      m_httpOnline;
    bool                                                      m_connOnline;
    std::atomic<uint64_t>                                     m_wanSessionGeneration { 0 };
    MqttRecoveryState                                         m_mqttState { MqttRecoveryState::Disabled };
    uint64_t                                                  m_mqttRequestGeneration { 0 };
    uint64_t                                                  m_mqttConnectionGeneration { 0 };
    uint64_t                                                  m_mqttStopGeneration { 0 };
    bool                                                      m_mqttStopInFlight { false };
    MqttStopContinuation                                      m_mqttStopContinuation { MqttStopContinuation::None };
    uint64_t                                                  m_mqttBlockedRefreshTokenRevision { 0 };
    unsigned int                                              m_mqttConfigRefreshRetryCount { 0 };
    std::chrono::steady_clock::time_point                     m_nextMqttConfigRefreshTime {};
    std::chrono::steady_clock::time_point                     m_mqttConnectDeadline {};
    std::string                                               m_clientId;
    std::string                                               m_userId;
    std::string                                               m_dataDir;
    com_mqtt_config_t                                         m_mqttConfig;
    std::list<com_ptr_t>                                      m_comPtrs;
    com_ptr_map_t                                             m_ptrMap;
    std::map<com_id_t, com_dev_data_t>                        m_datMap;
    std::set<com_id_t>                                        m_readyIdSet;
    std::unordered_map<std::string, std::vector<std::string>> m_unUpdateDevList;
    std::map<std::string, com_id_t>                           m_devIdMap;
    std::set<std::string>                                     m_activeFeedbackLogIds;
    std::set<std::string>                                     m_completedFeedbackLogIds;
    std::deque<std::string>                                   m_feedbackLogHistory;
    std::mutex                                                m_feedbackLogMutex;
    std::shared_ptr<WaitEvent>                                m_feedbackLogCancelEvent;
    std::uint64_t                                             m_feedbackLogSessionGeneration { 0 };
    std::list<com_dev_data_t>                                 m_pendingWanDevDatas;
    wxTimer                                                   m_loopCheckTimer;
    std::atomic_bool                                          m_blockCommandFailedUpdate;
    std_precise_clock::time_point                             m_commandFailedUpdateTime;
    std_precise_clock::time_point                             m_pendingSetUpdateWanDevTime;
    std::unique_ptr<WanDevMaintainThd>                        m_wanDevMaintainThd;
    std::unique_ptr<WanDevSendGcodeThd>                       m_sendGcodeThd;
    std::unique_ptr<fnet::FlashNetworkIntfc>                  m_networkIntfc;
    std::unique_ptr<ComThreadPool>                            m_threadPool;
    std::unique_ptr<ComThreadPool>                            m_connectionStopThreadPool;
    std::unique_ptr<ComThreadPool>                            m_logUploadThreadPool;
    WaitEvent                                                 m_threadExitEvent;
};

}} // namespace Slic3r::GUI

#endif
