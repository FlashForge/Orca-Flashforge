#include "WanDevMaintainThd.hpp"
#include <boost/bind/bind.hpp>
#include "FreeInDestructor.h"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(RELOGIN_HTTP_EVENT, ReloginHttpEvent);
wxDEFINE_EVENT(GET_WAN_DEV_EVENT, GetWanDevEvent);

WanDevMaintainThd::WanDevMaintainThd(fnet::FlashNetworkIntfc *networkIntfc)
    : m_reloginHttp(false)
    , m_updateWanDev(false)
    , m_updateUserProfile(false)
    , m_exitThread(false)
    , m_thread(boost::bind(&WanDevMaintainThd::run, this))
    , m_networkIntfc(networkIntfc)
{
}

void WanDevMaintainThd::exit()
{
    stop();
    m_exitThread = true;
    m_loopWaitEvent.set(true);
    m_thread.join();
}

void WanDevMaintainThd::setUid(const std::string &uid)
{
    boost::mutex::scoped_lock lock(m_uidMutex);
    m_uid = uid;
}

void WanDevMaintainThd::setReloginHttp()
{
    m_reloginHttp = true;
    m_loopWaitEvent.set(true);
}

void WanDevMaintainThd::setUpdateUserProfile()
{
    m_updateUserProfile = true;
    m_loopWaitEvent.set(true);
}

void WanDevMaintainThd::setUpdateWanDev()
{
    m_updateWanDev = true;
    m_loopWaitEvent.set(true);
}

void WanDevMaintainThd::stop()
{
    m_reloginHttp = false;
    m_updateWanDev = false;
    m_updateUserProfile = false;
}

void WanDevMaintainThd::run()
{
    while (!m_exitThread) {
        m_loopWaitEvent.waitTrue(5000);
        m_loopWaitEvent.set(false);
        if (!m_reloginHttp && !m_updateWanDev && !m_updateUserProfile) {
            continue;
        }
        std::string uid = getUid();
        ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
        if (m_reloginHttp) {
            if (reloginHttp(uid, token.accessToken())) {
                m_reloginHttp = false;
            }
        } else {
            if (m_updateWanDev) {
                updateWanDev(uid, token.accessToken());
                m_updateWanDev = false;
            }
            if (m_updateUserProfile) {
                updateUserProfile(token.accessToken());
                m_updateUserProfile = false;
            }
        }
    }
}

std::string WanDevMaintainThd::getUid()
{
    boost::mutex::scoped_lock lock(m_uidMutex);
    return m_uid;
}

bool WanDevMaintainThd::reloginHttp(std::string &uid, const std::string &accessToken)
{
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(m_networkIntfc->checkToken(
        accessToken.c_str(), ComTimeoutWanB));
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    if (m_reloginHttp && ret == COM_OK) {
        ret = MultiComUtils::fnetRet2ComErrno(m_networkIntfc->getWanDevList(
            uid.c_str(), accessToken.c_str(), &devInfos, &devCnt, ComTimeoutWanB));
    }
    com_user_profile_t userProfile;
    if (m_reloginHttp && ret == COM_OK) {
        ret = MultiComUtils::getUserProfile(accessToken, userProfile, ComTimeoutWanB);
    }
    if (m_reloginHttp) {
        ReloginHttpEvent *event = new ReloginHttpEvent;
        event->SetEventType(RELOGIN_HTTP_EVENT);
        event->ret = ret;
        event->uid = uid;
        event->accessToken = accessToken;
        event->userProfile = userProfile;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        QueueEvent(event);
        return ret == COM_OK;
    } else {
        m_networkIntfc->freeWanDevList(devInfos, devCnt);
        return false;
    }
}

void WanDevMaintainThd::updateWanDev(const std::string &uid, const std::string &accessToken)
{
    int tryCnt = 3;
    int fnetRet = FNET_OK;
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        auto getWanDevList =  m_networkIntfc->getWanDevList;
        fnetRet = getWanDevList(uid.c_str(), accessToken.c_str(), &devInfos, &devCnt, ComTimeoutWanB);
        if (fnetRet == FNET_OK || fnetRet == FNET_UNAUTHORIZED || m_exitThread) {
            break;
        } else if (i + 1 < tryCnt) {
            int sleepTimes[] = {1, 3, 5};
            boost::this_thread::sleep_for(boost::chrono::seconds(sleepTimes[i < 3 ? i : 2]));
        }
    }
    if (!m_exitThread) {
        GetWanDevEvent *event = new GetWanDevEvent;
        event->SetEventType(GET_WAN_DEV_EVENT);
        event->ret = MultiComUtils::fnetRet2ComErrno(fnetRet);
        event->uid = uid;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        QueueEvent(event);
    } else {
        m_networkIntfc->freeWanDevList(devInfos, devCnt);
    }
}

void WanDevMaintainThd::updateUserProfile(const std::string &accessToken)
{
    int tryCnt = 3;
    ComErrno ret = COM_OK;
    com_user_profile_t userProfile;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        ret = MultiComUtils::getUserProfile(accessToken, userProfile, ComTimeoutWanB);
        if (ret == COM_OK || ret == COM_UNAUTHORIZED || m_exitThread) {
            break;
        } else if (i + 1 < tryCnt) {
            int sleepTimes[] = {1, 3, 5};
            boost::this_thread::sleep_for(boost::chrono::seconds(sleepTimes[i < 3 ? i : 2]));
        }
    }
    if (!m_exitThread) {
        QueueEvent(new ComGetUserProfileEvent(COM_GET_USER_PROFILE_EVENT, userProfile, ret));
    }
}

}} // namespace Slic3r::GUI
