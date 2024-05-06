#ifndef slic3r_GUI_WanDevMaintainThd_hpp_
#define slic3r_GUI_WanDevMaintainThd_hpp_

#include <atomic>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "ComWanAsyncConn.hpp"
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

struct ReloginEvent : public wxCommandEvent {
    ComErrno ret;
    std::string uid;
    std::string accessToken;
    fnet_wan_dev_info_t *devInfos;
    int devCnt;
    std::unique_ptr<ComWanAsyncConn> wanAsyncConn;
};

struct GetWanDevEvent : public wxCommandEvent {
    ComErrno ret;
    std::string uid;
    fnet_wan_dev_info_t *devInfos;
    int devCnt;
};

wxDECLARE_EVENT(RELOGIN_EVENT, ReloginEvent);
wxDECLARE_EVENT(GET_WAN_DEV_EVENT, GetWanDevEvent);

class WanDevMaintainThd : public wxEvtHandler
{
public:
    WanDevMaintainThd(fnet::FlashNetworkIntfc *networkIntfc);

    void exit();

    void setUid(const std::string &uid);

    void setRelogin();

    void setUpdateWanDev();

    void setUpdateUserProfile();

    void stop();

private:
    void run();

    std::string getUid();

    bool relogin(const std::string &uid, const std::string &accessToken);

    void updateWanDev(const std::string &uid, const std::string &accessToken);

    void updateUserProfile(const std::string &accessToken);

private:
    std::string             m_uid;
    boost::mutex            m_uidMutex;
    WaitEvent               m_loopWaitEvent;
    std::atomic_bool        m_relogin;
    std::atomic_bool        m_updateWanDev;
    std::atomic_bool        m_updateUserProfile;
    std::atomic_bool        m_exitThread;
    boost::thread           m_thread;
    fnet::FlashNetworkIntfc*m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
