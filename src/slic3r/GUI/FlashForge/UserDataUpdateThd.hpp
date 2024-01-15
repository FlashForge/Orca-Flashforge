#ifndef slic3r_GUI_WanDevUpdateThd_hpp_
#define slic3r_GUI_WanDevUpdateThd_hpp_

#include <atomic>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

struct WanDevUpdateEvent : public wxCommandEvent
{
    WanDevUpdateEvent(wxEventType _type, const std::string &_accessToken,
        fnet_wan_dev_info_t *_devInfos, int _devCnt)
    {
        SetEventType(_type);
        accessToken = _accessToken;
        devInfos = _devInfos;
        devCnt = _devCnt;
    }
    std::string accessToken;
    fnet_wan_dev_info_t *devInfos;
    int devCnt;
};
wxDECLARE_EVENT(WAN_DEV_UPDATE_EVENT, WanDevUpdateEvent);

class UserDataUpdateThd : public wxEvtHandler
{
public:
    UserDataUpdateThd(fnet::FlashNetworkIntfc *networkIntfc);

    void exit();

    void getToken(std::string &userName, std::string &accessToken);

    void setToken(const std::string &userName, const std::string &accessToken);

    void clearToken();

    void setUpdateWanDev();

private:
    void run();

    ComErrno updateUserProfile(const std::string &accessToken);

    ComErrno updateWanDev(const std::string &accessToken);

    void getTokenPrivate(std::string &oldUserName, std::string &userName,
        std::string &accessToken);

    void setOldUserName(const std::string &oldUserName);

private:
    std::string              m_oldUserName;
    std::string              m_userName;
    std::string              m_accessToken;
    boost::mutex             m_tokenMutex;
    WaitEvent                m_loopWaitEvent;
    std::atomic<bool>        m_exitThread;
    boost::thread            m_thread;
    fnet::FlashNetworkIntfc *m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
