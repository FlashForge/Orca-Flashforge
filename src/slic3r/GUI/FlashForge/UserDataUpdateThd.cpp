#include "UserDataUpdateThd.hpp"
#include <boost/bind/bind.hpp>
#include "FreeInDestructor.h"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(GET_WAN_DEV_EVENT, GetWanDevEvent);

UserDataUpdateThd::UserDataUpdateThd(fnet::FlashNetworkIntfc *networkIntfc)
    : m_thread(boost::bind(&UserDataUpdateThd::run, this))
    , m_networkIntfc(networkIntfc)
    , m_updateUserProfile(false)
    , m_updateWanDev(false)
    , m_exitThread(false)
{
}

void UserDataUpdateThd::exit()
{
    m_loopWaitEvent.set(true);
    m_exitThread = true;
    m_thread.join();
}

std::string UserDataUpdateThd::getToken()
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    return m_accessToken;
}

void UserDataUpdateThd::setToken(const std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    m_accessToken = accessToken;
}

void UserDataUpdateThd::setUpdateUserProfile()
{
    m_updateUserProfile = true;
    m_loopWaitEvent.set(true);
}

void UserDataUpdateThd::setUpdateWanDev()
{
    m_updateWanDev = true;
    m_loopWaitEvent.set(true);
}

void UserDataUpdateThd::run()
{
    while (!m_exitThread) {
        m_loopWaitEvent.waitTrue();
        m_loopWaitEvent.set(false);
        std::string accessToken = getToken();
        if (m_updateUserProfile) {
            updateUserProfile(accessToken);
            m_updateUserProfile = false;
        }
        if (m_updateWanDev) {
            updateWanDev(accessToken);
            m_updateWanDev = false;
        }
    }
}

void UserDataUpdateThd::updateUserProfile(const std::string &accessToken)
{
    int tryCnt = 3;
    int fnetRet = FNET_OK;
    com_user_profile_t userProfile;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        fnet_user_profile_t *fnetProfile;
        fnetRet = m_networkIntfc->getUserProfile(accessToken.c_str(), &fnetProfile, ComTimeoutWan);
        fnet::FreeInDestructor freeProfile(fnetProfile, m_networkIntfc->freeUserProfile);
        if (fnetRet == FNET_OK) {
            userProfile.uid = fnetProfile->uid;
            userProfile.nickname = fnetProfile->nickname;
            userProfile.headImgUrl = fnetProfile->headImgUrl;
            break;
        } else if (fnetRet == FNET_UNAUTHORIZED) {
            break;
        } else if (i + 1 < tryCnt) {
            int sleepTimes[] = {1, 3, 5};
            boost::this_thread::sleep_for(boost::chrono::seconds(sleepTimes[i < 3 ? i : 2]));
        }
    }
    if (!m_exitThread) {
        ComErrno ret = MultiComUtils::fnetRet2ComErrno(fnetRet);
        QueueEvent(new ComGetUserProfileEvent(COM_GET_USER_PROFILE_EVENT, userProfile, ret));
    }
}

void UserDataUpdateThd::updateWanDev(const std::string &accessToken)
{
    int tryCnt = 3;
    int fnetRet = FNET_OK;
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        fnetRet = m_networkIntfc->getWanDevList(accessToken.c_str(), &devInfos, &devCnt, ComTimeoutWan);
        if (fnetRet == FNET_OK || fnetRet == FNET_UNAUTHORIZED) {
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
        event->accessToken = accessToken;
        event->devInfos = devInfos;
        event->devCnt = devCnt;
        QueueEvent(event);
    }
}

}} // namespace Slic3r::GUI
