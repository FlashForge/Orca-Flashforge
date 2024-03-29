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
    , m_updateWanDev(false)
    , m_updateUserProfile(false)
    , m_exitThread(false)
{
}

void UserDataUpdateThd::exit()
{
    m_loopWaitEvent.set(true);
    m_updateWanDev = false;
    m_updateUserProfile = false;
    m_exitThread = true;
    m_thread.join();
}

void UserDataUpdateThd::getUidToken(std::string &uid, std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_uidTokenMutex);
    uid = m_uid;
    accessToken = m_accessToken;
}

void UserDataUpdateThd::setUidToken(const std::string &uid, const std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_uidTokenMutex);
    m_uid = uid;
    m_accessToken = accessToken;
}

void UserDataUpdateThd::setToken(const std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_uidTokenMutex);
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

        std::string uid, accessToken;
        getUidToken(uid, accessToken);
        if (m_updateWanDev) {
            updateWanDev(uid, accessToken);
            m_updateWanDev = false;
        }
        if (m_updateUserProfile) {
            updateUserProfile(accessToken);
            m_updateUserProfile = false;
        }
    }
}

void UserDataUpdateThd::updateWanDev(const std::string &uid, const std::string &accessToken)
{
    int tryCnt = 5;
    int fnetRet = FNET_OK;
    fnet_wan_dev_info_t *devInfos = nullptr;
    int devCnt = 0;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        auto getWanDevList =  m_networkIntfc->getWanDevList;
        fnetRet = getWanDevList(uid.c_str(), accessToken.c_str(), &devInfos, &devCnt, ComTimeoutWan);
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

void UserDataUpdateThd::updateUserProfile(const std::string &accessToken)
{
    int tryCnt = 5;
    ComErrno ret = COM_OK;
    com_user_profile_t userProfile;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        ret = MultiComUtils::getUserProfile(accessToken, userProfile);
        if (ret == COM_OK || ret == COM_UNAUTHORIZED) {
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
