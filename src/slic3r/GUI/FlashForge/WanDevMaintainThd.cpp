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

void WanDevMaintainThd::setReqHeaders(const std::string &clientId, int64_t appId, int64_t platId,
    uint64_t sessionGeneration)
{
    boost::mutex::scoped_lock lock(m_reqHeadersMutex);
    m_clientId = clientId;
    m_appId = appId;
    m_platId = platId;
    m_sessionGeneration = sessionGeneration;
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
        std::string clientId;
        int64_t appId, platId;
        uint64_t sessionGeneration = 0;
        getReqHeaders(clientId, appId, platId, sessionGeneration);
        if (m_reloginHttp) {
            ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
            if (reloginHttp(clientId, appId, platId, sessionGeneration, token)) {
                m_reloginHttp = false;
            }
        } else {
            ScopedWanDevToken token = WanDevTokenMgr::inst()->getScopedToken();
            if (m_updateWanDev) {
                m_updateWanDev = false;
                updateWanDev(clientId, appId, platId, sessionGeneration, token.accessToken());
            }
            if (m_updateUserProfile) {
                m_updateUserProfile = false;
                updateUserProfile(sessionGeneration, token.accessToken());
            }
        }
    }
}

void WanDevMaintainThd::getReqHeaders(std::string &clientId, int64_t &appId, int64_t &platId,
    uint64_t &sessionGeneration)
{
    boost::mutex::scoped_lock lock(m_reqHeadersMutex);
    clientId = m_clientId;
    appId = m_appId;
    platId = m_platId;
    sessionGeneration = m_sessionGeneration;
}

bool WanDevMaintainThd::reloginHttp(const std::string &clientId, int64_t &appId, int64_t &platId,
    uint64_t sessionGeneration, ScopedWanDevToken &scopedToken)
{
    com_user_profile_t userProfile;
    ComErrno ret = MultiComUtils::getUserProfile(scopedToken.accessToken(), userProfile, ComTimeoutWanB);
    if (m_reloginHttp && ret == COM_UNAUTHORIZED) {
        ret = WanDevTokenMgr::inst()->refreshToken(scopedToken);
        if (m_reloginHttp && ret == COM_OK) {
            ret = MultiComUtils::getUserProfile(scopedToken.accessToken(), userProfile, ComTimeoutWanB);
        }
    }
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    if (m_reloginHttp && ret == COM_OK) {
        ret = MultiComUtils::fnetRet2ComErrno(m_networkIntfc->getWanDevList(clientId.c_str(),
            scopedToken.accessToken().c_str(), appId, platId, &devInfos, &devCnt, ComTimeoutWanB));
    }
    if (m_reloginHttp) {
        ReloginHttpEvent *event = new ReloginHttpEvent;
        event->SetEventType(RELOGIN_HTTP_EVENT);
        event->ret = ret;
        event->sessionGeneration = sessionGeneration;
        event->clientId = clientId;
        event->accessToken = scopedToken.accessToken();
        event->userProfile = userProfile;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        QueueEvent(event);
        // A new account may have started while this request was in flight.
        // Do not let the old request clear the new session's relogin flag.
        std::string currentClientId;
        int64_t currentAppId = 0;
        int64_t currentPlatId = 0;
        uint64_t currentSessionGeneration = 0;
        getReqHeaders(currentClientId, currentAppId, currentPlatId, currentSessionGeneration);
        (void) currentClientId;
        (void) currentAppId;
        (void) currentPlatId;
        return ret == COM_OK && scopedToken.sessionGeneration() == sessionGeneration
            && currentSessionGeneration == sessionGeneration;
    } else {
        m_networkIntfc->freeWanDevList(devInfos, devCnt);
        return false;
    }
}

void WanDevMaintainThd::updateWanDev(const std::string &clientId, int64_t &appId, int64_t &platId,
    uint64_t sessionGeneration, const std::string &accessToken)
{
    int tryCnt = 3;
    int fnetRet = FNET_OK;
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        auto getWanDevList =  m_networkIntfc->getWanDevList;
        fnetRet = getWanDevList(clientId.c_str(), accessToken.c_str(), appId, platId, &devInfos, &devCnt,
            ComTimeoutWanB);
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
        event->sessionGeneration = sessionGeneration;
        event->clientId = clientId;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        QueueEvent(event);
    } else {
        m_networkIntfc->freeWanDevList(devInfos, devCnt);
    }
}

void WanDevMaintainThd::updateUserProfile(uint64_t sessionGeneration, const std::string &accessToken)
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
        QueueEvent(new ComGetUserProfileEvent(
            COM_GET_USER_PROFILE_EVENT, userProfile, ret, sessionGeneration));
    }
}

}} // namespace Slic3r::GUI
