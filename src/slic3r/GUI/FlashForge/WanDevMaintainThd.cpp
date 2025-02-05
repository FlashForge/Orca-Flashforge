#include "WanDevMaintainThd.hpp"
#include <boost/bind/bind.hpp>
#include "FreeInDestructor.h"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"
#include "WanDevTokenMgr.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(RELOGIN_EVENT, ReloginEvent);
wxDEFINE_EVENT(GET_WAN_DEV_EVENT, GetWanDevEvent);

WanDevMaintainThd::WanDevMaintainThd(fnet::FlashNetworkIntfc *networkIntfc)
    : m_relogin(false)
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

void WanDevMaintainThd::setRelogin()
{
    m_relogin = true;
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
    m_relogin = false;
    m_updateWanDev = false;
    m_updateUserProfile = false;
}

void WanDevMaintainThd::run()
{
    while (!m_exitThread) {
        m_loopWaitEvent.waitTrue(5000);
        m_loopWaitEvent.set(false);
        if (!m_relogin && !m_updateWanDev && !m_updateUserProfile) {
            continue;
        }
        std::string uid = getUid();
        ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
        if (m_relogin) {
            if (relogin(uid, token.accessToken())) {
                m_relogin = false;
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

bool WanDevMaintainThd::relogin(const std::string &uid, const std::string &accessToken)
{
    ComErrno ret = MultiComUtils::checkToken(accessToken);
    std::unique_ptr<ComWanAsyncConn> wanAsyncConn(new ComWanAsyncConn(m_networkIntfc));
    if (ret != COM_UNAUTHORIZED) {
        ret = wanAsyncConn->createConn(uid, accessToken);
    }
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    if (m_relogin && ret == COM_OK) {
        ret = MultiComUtils::fnetRet2ComErrno(m_networkIntfc->getWanDevList(
            uid.c_str(), accessToken.c_str(), &devInfos, &devCnt, 10000));
    }
    if (m_relogin) {
        ReloginEvent *event = new ReloginEvent;
        event->SetEventType(RELOGIN_EVENT);
        event->ret = ret;
        event->uid = m_uid;
        event->accessToken = accessToken;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        event->wanAsyncConn = std::move(wanAsyncConn);
        QueueEvent(event);
        return ret == COM_OK;
    } else {
        m_networkIntfc->freeWanDevList(devInfos, devCnt);
    }
    return false;
}

void WanDevMaintainThd::updateWanDev(const std::string &uid, const std::string &accessToken)
{
    int tryCnt = 5;
    int fnetRet = FNET_OK;
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        auto getWanDevList =  m_networkIntfc->getWanDevList;
        fnetRet = getWanDevList(uid.c_str(), accessToken.c_str(), &devInfos, &devCnt, 10000);
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
        event->uid = m_uid;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        QueueEvent(event);
    } else {
        m_networkIntfc->freeWanDevList(devInfos, devCnt);
    }
}

void WanDevMaintainThd::updateUserProfile(const std::string &accessToken)
{
    int tryCnt = 5;
    ComErrno ret = COM_OK;
    com_user_profile_t userProfile;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        ret = MultiComUtils::getUserProfile(accessToken, userProfile);
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
