#ifndef slic3r_GUI_MultiComMgr_hpp_
#define slic3r_GUI_MultiComMgr_hpp_

#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <boost/bimap.hpp>
#include <wx/event.h>
#include <wx/timer.h>
#include "ComConnection.hpp"
#include "ComThreadPool.hpp"
#include "ComWanNimConn.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "Singleton.hpp"
#include "WaitEvent.hpp"
#include "WanDevMaintainThd.hpp"
#include "WanDevSendGcodeThd.hpp"

namespace boost { namespace interprocess {
    class file_lock;
}}

namespace Slic3r { namespace GUI {

class MultiComMgr : public wxEvtHandler, public Singleton<MultiComMgr>
{
public:
    MultiComMgr();

    bool initalize(const std::string &dllPath, const std::string &dataDir);

    void uninitalize();

    fnet::FlashNetworkIntfc *networkIntfc();
    
    com_id_t addLanDev(const fnet_lan_dev_info_t &devInfo, const std::string &checkCode);

    void removeLanDev(com_id_t id);

    ComErrno addWanDev(const com_token_data_t &tokenData, com_add_wan_dev_data_t &addDevData,
        int tryCnt, int tryMsInterval);

    void removeWanDev();

    ComErrno bindWanDev(const std::string &ip, unsigned short port,
        const std::string &serialNumber, unsigned short pid, const std::string &name);

    ComErrno unbindWanDev(const std::string &serialNumber, const std::string &devId,
        const std::string &nimAccountId);

    com_id_list_t getReadyDevList();

    const com_dev_data_t &devData(com_id_t id, bool *valid = nullptr);

    bool putCommand(com_id_t id, ComCommand *command); // this method takes ownership of the command, i.e. it will delete it itself

    bool abortSendGcode(com_id_t id, int commandId);

    bool wanSendGcode(const std::vector<std::string> &devIds, const std::vector<std::string> &devSerialNumbers,
        const std::vector<std::string> &nimAccountIds, const com_send_gcode_data_t &sendGocdeData);

    bool abortWanSendGcode();

private:
    using std_precise_clock = std::chrono::high_resolution_clock;

    typedef std::map<com_id_t, std_precise_clock::time_point> dev_alive_time_map_t;

    typedef std::shared_ptr<ComConnection> com_ptr_t;

    typedef boost::bimap<com_id_t, ComConnection*> com_ptr_map_t;

    typedef boost::bimap<com_id_t, ComConnection*>::value_type com_ptr_map_val_t;
    
    typedef std::unique_ptr<boost::interprocess::file_lock> interprocess_file_lock_ptr_t;

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

    void onWanConnSubscribe(const WanConnSubscribeEvent &event);

    void onRefreshToken(const ComRefreshTokenEvent &event);

    com_dev_data_t makeWanDevData(const fnet_wan_dev_info_t *wanDevInfo);

    void maintianWanDev(ComErrno ret, bool repeatLogin, bool unregisterUser);

    void setWanDevOffline();

    void subscribeWanDevNimStatus();

    void updateWanDevDetail();

    std::string getNimAppDir(const std::string &dataDir);

    const int SubscribeDevStatusSecond = 10000;

private:
    int                                      m_idNum;
    bool                                     m_login;
    bool                                     m_httpOnline;
    bool                                     m_nimOnline;
    bool                                     m_nimFirstLogined;
    std::string                              m_uid;
    com_nim_data_t                           m_nimData;
    std::list<com_ptr_t>                     m_comPtrs;
    com_ptr_map_t                            m_ptrMap;
    std::map<com_id_t, com_dev_data_t>       m_datMap;
    std::set<com_id_t>                       m_readyIdSet;
    std::map<std::string, com_id_t>          m_devIdMap;
    std::map<std::string, com_id_t>          m_nimAccountIdMap;
    dev_alive_time_map_t                     m_devAliveTimeMap;
    std::list<com_dev_data_t>                m_pendingWanDevDatas;
    wxTimer                                  m_loopCheckTimer;
    std_precise_clock::time_point            m_subscribeTime;
    std::atomic_bool                         m_blockCommandFailedUpdate;
    std_precise_clock::time_point            m_commandFailedUpdateTime;
    std::unique_ptr<WanDevMaintainThd>       m_wanDevMaintainThd;
    std::unique_ptr<WanDevSendGcodeThd>      m_sendGcodeThd;
    std::unique_ptr<fnet::FlashNetworkIntfc> m_networkIntfc;
    std::unique_ptr<ComThreadPool>           m_threadPool;
    WaitEvent                                m_threadExitEvent;
    interprocess_file_lock_ptr_t             m_nimDataDirFileLock;
    std_precise_clock::time_point            m_showNimDataBaseErrorTime;
    const char                              *m_nimDataFileLockName;
};

}} // namespace Slic3r::GUI

#endif
