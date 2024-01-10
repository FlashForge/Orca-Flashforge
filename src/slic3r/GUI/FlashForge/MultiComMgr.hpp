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

    bool initalize(const std::string &newtworkDllPath);

    void uninitalize();

    fnet::FlashNetworkIntfc *networkIntfc();
    
    com_id_t addLanDev(const fnet_lan_dev_info &devInfo, const std::string &checkCode);

    void removeLanDev(com_id_t id);

    void setWanDevToken(const std::string &userName, const std::string &accessToken);

    void removeWanDev();

    ComErrno bindWanDev(const std::string &serialNumber, unsigned short pid,
        const std::string &name);

    ComErrno unbindWanDev(const std::string &serialNumber, const std::string &devId);

    com_id_list_t getReadyDevList();

    const com_dev_data_t &devData(com_id_t id, bool *valid = nullptr);

    void putCommand(com_id_t id, const ComCommandPtr &command);

private:
    typedef std::shared_ptr<ComConnection> com_ptr_t;

    typedef boost::bimap<com_id_t, ComConnection*> com_ptr_map_t;

    typedef boost::bimap<com_id_t, ComConnection*>::value_type com_ptr_map_val_t;

    void initConnection(const com_ptr_t &comPtr);

    void onWanDevMaintian(const ComWanDevMaintainEvent &event);

    void onWanDevUpdated(const WanDevUpdateEvent &event);

    void onConnectionExit(const ComConnectionExitEvent &event);

    void onDevDetailUpdate(const ComDevDetailUpdateEvent &event);

private:
    int                                      m_idNum;
    std::list<com_ptr_t>                     m_comPtrs;
    com_ptr_map_t                            m_ptrMap;
    std::map<com_id_t, com_dev_data_t>       m_datMap;
    std::set<com_id_t>                       m_readyIdSet;
    std::set<std::string>                    m_serialNumberSet;
    std::unique_ptr<UserDataUpdateThd>       m_userDataUpdateThd;
    std::unique_ptr<fnet::FlashNetworkIntfc> m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
