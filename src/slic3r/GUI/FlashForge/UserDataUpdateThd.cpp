#include "UserDataUpdateThd.hpp"
#include <ctime>
#include <boost/bind/bind.hpp>
#include "FreeInDestructor.h"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(WAN_DEV_UPDATE_EVENT, WanDevUpdateEvent);

UserDataUpdateThd::UserDataUpdateThd(fnet::FlashNetworkIntfc *networkIntfc)
    : m_thread(boost::bind(&UserDataUpdateThd::run, this))
    , m_networkIntfc(networkIntfc)
{
}

void UserDataUpdateThd::exit()
{
    m_loopWaitEvent.set(true);
    m_exitThread = true;
    m_thread.join();
}

void UserDataUpdateThd::getToken(std::string &userName, std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    userName = m_userName;
    accessToken = m_accessToken;
}

void UserDataUpdateThd::setToken(const std::string &userName, const std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    m_userName = userName;
    m_accessToken = accessToken;
    m_loopWaitEvent.set(true);
}

void UserDataUpdateThd::clearToken()
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    m_userName.clear();
    m_accessToken.clear();
    m_oldUserName.clear();
}

void UserDataUpdateThd::setUpdateWanDev()
{
    m_loopWaitEvent.set(true);
}

void UserDataUpdateThd::run()
{
    std::string oldUserName, userName, accessToken;
    while (!m_exitThread) {
        clock_t startClock = clock();
        m_loopWaitEvent.set(false);
        getTokenPrivate(oldUserName, userName, accessToken);
        if (!accessToken.empty()) {
            if (oldUserName != userName) {
                updateUserProfile(accessToken);
                setOldUserName(userName);
            }
            ComErrno ret = updateWanDev(accessToken);
            if (ret != COM_OK) {
                clearToken();
            }
            QueueEvent(new ComWanDevMaintainEvent(COM_WAN_DEV_MAINTAIN_EVENT, ret));
        }
        int elapsedTime = (clock() - startClock) / CLOCKS_PER_SEC;
        m_loopWaitEvent.waitTrue(std::max(5000 - elapsedTime, 100));
    }
}

ComErrno UserDataUpdateThd::updateUserProfile(const std::string &accessToken)
{
    fnet_user_profile_t *fnetProfile;
    int fnetRet = m_networkIntfc->getUserProfile(accessToken.c_str(), &fnetProfile, ComTimeoutWan);
    fnet::FreeInDestructor freeProfile(fnetProfile, m_networkIntfc->freeUserProfile);
    com_user_profile_t userProfile;
    ComErrno ret = MultiComUtils::fnetRet2ComErrno(fnetRet);
    if (ret == COM_OK) {
        userProfile.nickname = fnetProfile->nickname;
        userProfile.headImgUrl = fnetProfile->headImgUrl;
    }
    QueueEvent(new ComGetUserProfileEvent(COM_GET_USER_PROFILE_EVENT, userProfile, ret));
    return ret;
}

ComErrno UserDataUpdateThd::updateWanDev(const std::string &accessToken)
{
    int fnetRet = FNET_OK;
    int tryCnt = 5;
    for (int i = 0; i < tryCnt && !m_exitThread; ++i) {
        int devCnt;
        fnet_wan_dev_info_t *devInfos;
        fnetRet = m_networkIntfc->getWanDevList(accessToken.c_str(), &devInfos, &devCnt, ComTimeoutWan);
        if (fnetRet == FNET_OK) {
            QueueEvent(new WanDevUpdateEvent(WAN_DEV_UPDATE_EVENT, accessToken, devInfos, devCnt));
            break;
        } else if (fnetRet == FNET_UNAUTHORIZED) {
            break;
        } else if (i + 1 < tryCnt) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
        }
    }
    return MultiComUtils::fnetRet2ComErrno(fnetRet);
}

void UserDataUpdateThd::getTokenPrivate(std::string &oldUserName, std::string &userName,
    std::string &accessToken)
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    oldUserName = m_oldUserName;
    userName = m_userName;
    accessToken = m_accessToken;
}

void UserDataUpdateThd::setOldUserName(const std::string &oldUserName)
{
    boost::mutex::scoped_lock lock(m_tokenMutex);
    m_oldUserName = oldUserName;
}

}} // namespace Slic3r::GUI
