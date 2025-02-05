#ifndef slic3r_GUI_MultiComMgr_hpp_
#define slic3r_GUI_MultiComMgr_hpp_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <boost/bimap.hpp>
#include <wx/event.h>
#include <wx/timer.h>
#include "ComConnection.hpp"
#include "ComWanAsyncConn.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "Singleton.hpp"
#include "WanDevMaintainThd.hpp"
#include "WanDevSendGcodeThd.hpp"

namespace Slic3r { namespace GUI {

class MultiComMgr : public wxEvtHandler, public Singleton<MultiComMgr>
{
public:
    MultiComMgr();

    bool initalize(const std::string &dllPath, const std::string &logFileDir);

    void uninitalize();

    fnet::FlashNetworkIntfc *networkIntfc();
    
    com_id_t addLanDev(const fnet_lan_dev_info &devInfo, const std::string &checkCode);

    void removeLanDev(com_id_t id);

    ComErrno addWanDev(const com_token_data_t &tokenData, int tryCnt, int tryMsInterval);

    void removeWanDev();

    ComErrno bindWanDev(const std::string &serialNumber, unsigned short pid,
        const std::string &name);

    ComErrno unbindWanDev(const std::string &serialNumber, const std::string &devId);

    com_id_list_t getReadyDevList();

    const com_dev_data_t &devData(com_id_t id, bool *valid = nullptr);

    bool putCommand(com_id_t id, ComCommand *command); // this method takes ownership of the command, i.e. it will delete it itself

    bool abortSendGcode(com_id_t id, int commandId);

    bool wanSendGcode(const std::vector<std::string> &devIds, const std::string &gcodeFilePath,
        const std::string &thumbFilePath, const std::string &gcodeDstName, bool printNow,
        bool levelingBeforePrint);

    bool abortWanSendGcode();

private:
    typedef std::shared_ptr<ComConnection> com_ptr_t;

    typedef boost::bimap<com_id_t, ComConnection*> com_ptr_map_t;

    typedef boost::bimap<com_id_t, ComConnection*>::value_type com_ptr_map_val_t;

    void initConnection(const com_ptr_t &comPtr, const com_dev_data_t &devData);

    void onTimer(const wxTimerEvent &event);

    void onRelogin(ReloginEvent &event);

    void onUpdateWanDev(const GetWanDevEvent &event);
    
    void onUpdateUserProfile(const ComGetUserProfileEvent &event);

    void onConnectionReady(const ComConnectionReadyEvent &event);

    void onConnectionExit(const ComConnectionExitEvent &event);

    void onDevDetailUpdate(const ComDevDetailUpdateEvent &event);

    void onGetDevGcodeList(const ComGetDevGcodeListEvent &event);

    void onCommandFailed(const CommandFailedEvent &event);

    void onWanConnReadData(const WanConnReadDataEvent &event);

    void onWanConnReconnect(const wxCommandEvent &);

    void onWanConnExit(const WanConnExitEvent &event);

    void onRefreshToken(const ComRefreshTokenEvent &event);

    com_dev_data_t makeDevData(const fnet_wan_dev_info_t *wanDevInfo);

    void maintianWanDev(ComErrno ret);

    void updateWanDevInfo(com_id_t id, const std::string &name, const std::string &status,
        const std::string &location);

private:
    int                                      m_idNum;
    bool                                     m_login;
    std::string                              m_uid;
    std::list<com_ptr_t>                     m_comPtrs;
    com_ptr_map_t                            m_ptrMap;
    std::map<com_id_t, com_dev_data_t>       m_datMap;
    std::set<com_id_t>                       m_readyIdSet;
    std::map<std::string, com_id_t>          m_devIdMap;
    std::list<com_dev_data_t>                m_pendingWanDevDatas;
    wxTimer                                  m_procPendingWanDevTimer;
    std::unique_ptr<ComWanAsyncConn>         m_wanAsyncConn;
    std::unique_ptr<WanDevMaintainThd>       m_wanDevMaintainThd;
    std::unique_ptr<WanDevSendGcodeThd>      m_sendGcodeThd;
    std::unique_ptr<fnet::FlashNetworkIntfc> m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
