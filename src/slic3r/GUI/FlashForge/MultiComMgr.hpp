#ifndef slic3r_GUI_MultiComMgr_hpp_
#define slic3r_GUI_MultiComMgr_hpp_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <boost/bimap.hpp>
#include <wx/event.h>
#include "ComConnection.hpp"
#include "ComWanAsyncConn.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "Singleton.hpp"
#include "UserDataUpdateThd.hpp"

namespace Slic3r { namespace GUI {

class MultiComMgr : public wxEvtHandler, public Singleton<MultiComMgr>
{
public:
    MultiComMgr();

    bool initalize(const std::string &newtworkDllPath, const std::string &logFileDir);

    void uninitalize();

    fnet::FlashNetworkIntfc *networkIntfc();
    
    com_id_t addLanDev(const fnet_lan_dev_info &devInfo, const std::string &checkCode);

    void removeLanDev(com_id_t id);

    ComErrno addWanDev(const std::string &accessToken);

    void removeWanDev();

    void setWanDevToken(const std::string &accessToken);

    ComErrno bindWanDev(const std::string &serialNumber, unsigned short pid,
        const std::string &name);

    ComErrno unbindWanDev(const std::string &serialNumber, const std::string &devId);

    com_id_list_t getReadyDevList();

    const com_dev_data_t &devData(com_id_t id, bool *valid = nullptr);

    void putCommand(com_id_t id, ComCommand *command); // this method takes ownership of the command, i.e. it will delete it itself

    void abortSendGcode(com_id_t id, int commandId);

private:
    typedef std::shared_ptr<ComConnection> com_ptr_t;

    typedef boost::bimap<com_id_t, ComConnection*> com_ptr_map_t;

    typedef boost::bimap<com_id_t, ComConnection*>::value_type com_ptr_map_val_t;

    void initConnection(const com_ptr_t &comPtr, const com_dev_data_t &devData);

    void onWanDevMaintian(const ComWanDevMaintainEvent &event);

    void onGetWanDev(const GetWanDevEvent &event);
    
    void onUpdateUserProfile(const ComGetUserProfileEvent &event);

    void onConnectionReady(const ComConnectionReadyEvent &event);

    void onConnectionExit(const ComConnectionExitEvent &event);

    void onDevDetailUpdate(const ComDevDetailUpdateEvent &event);

    void onCommandFailed(const CommandFailedEvent &event);

    void onWanConnReadData(const WanConnReadDataEvent &event);

    void onWanConnReconnect(const wxCommandEvent &);

    com_dev_data_t makeDevData(const fnet_wan_dev_info_t *wanDevInfo);

    void updateWanDevInfo(com_id_t id, const std::string &name, const std::string &status,
        const std::string &location);

private:
    int                                      m_idNum;
    std::string                              m_uid;
    std::string                              m_accessToken;
    std::list<com_ptr_t>                     m_comPtrs;
    com_ptr_map_t                            m_ptrMap;
    std::map<com_id_t, com_dev_data_t>       m_datMap;
    std::set<com_id_t>                       m_readyIdSet;
    std::map<std::string, com_id_t>          m_devIdMap;
    std::unique_ptr<UserDataUpdateThd>       m_userDataUpdateThd;
    std::unique_ptr<ComWanAsyncConn>         m_wanAsyncConn;
    std::unique_ptr<fnet::FlashNetworkIntfc> m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
