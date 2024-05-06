#include "WanDevTokenMgr.hpp"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

void WanDevTokenMgr::start(const com_token_data_t &tokenData, fnet::FlashNetworkIntfc *networkIntfc)
{
    m_tokenData = tokenData;
    m_exitThread = false;
    m_networkIntfc = networkIntfc;
    m_thread.reset(new boost::thread(boost::bind(&WanDevTokenMgr::run, this)));
}

void WanDevTokenMgr::exit()
{
    m_exitThread = true;
    m_loopWaitEvent.set(true);
    m_thread->join();
    m_thread.reset(nullptr);
}

ScopedWanDevToken WanDevTokenMgr::getScopedToken()
{
    return ScopedWanDevToken(m_tokenData.accessToken, m_tokenMutex);
}

bool WanDevTokenMgr::tokenExpired(const std::string &accessToken)
{
    boost::shared_lock<boost::shared_mutex> sharedLock(m_tokenMutex);
    if (accessToken != m_tokenData.accessToken) {
        return false;
    }
    return m_tokenData.expiresIn < time(nullptr) - m_tokenData.startTime;
}

void WanDevTokenMgr::run()
{
    while (!m_exitThread) {
        m_loopWaitEvent.waitTrue(3000);
        m_loopWaitEvent.set(false);
        time_t elapsedSecond = time(nullptr) - m_tokenData.startTime;
        if (m_tokenData.expiresIn - elapsedSecond > 300) {
            continue;
        }
        boost::unique_lock<boost::shared_mutex> lock(m_tokenMutex);
        com_token_data_t tmpTokenData;
        ComErrno ret = MultiComUtils::refreshToken(m_tokenData.refreshToken, tmpTokenData);
        if (ret == COM_OK) {
            m_tokenData = tmpTokenData;
        }
        QueueEvent(new ComRefreshTokenEvent(COM_REFRESH_TOKEN_EVENT, tmpTokenData, ret));
    }
}

}} // namespace Slic3r::GUI
