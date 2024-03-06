#ifndef slic3r_GUI_UserDataUpdateThd_hpp_
#define slic3r_GUI_UserDataUpdateThd_hpp_

#include <atomic>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

struct GetWanDevEvent : public wxCommandEvent {
    ComErrno ret;
    std::string accessToken;
    fnet_wan_dev_info_t *devInfos;
    int devCnt;
};
wxDECLARE_EVENT(GET_WAN_DEV_EVENT, GetWanDevEvent);

class UserDataUpdateThd : public wxEvtHandler
{
public:
    UserDataUpdateThd(fnet::FlashNetworkIntfc *networkIntfc);

    void exit();

    void getUidToken(std::string &uid, std::string &accessToken);

    void setUidToken(const std::string &uid, const std::string &accessToken);

    void setToken(const std::string &accessToken);

    void setUpdateWanDev();

private:
    void run();

    void updateWanDev(const std::string &uid, const std::string &accessToken);

private:
    std::string              m_uid;
    std::string              m_accessToken;
    boost::mutex             m_uidTokenMutex;
    WaitEvent                m_loopWaitEvent;
    std::atomic_bool         m_exitThread;
    boost::thread            m_thread;
    fnet::FlashNetworkIntfc *m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
