#include "WanDevTokenMgr.hpp"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

void WanDevTokenMgr::initalize(const com_token_data_t &tokenData, fnet::FlashNetworkIntfc *networkIntfc,
    uint64_t sessionGeneration)
{
    boost::unique_lock<boost::shared_mutex> lock(m_tokenMutex);
    m_tokenData = tokenData;
    m_sessionGeneration = sessionGeneration;
    m_tokenRevision = 1;
    m_active = true;
    m_exitThread = false;
    m_networkIntfc = networkIntfc;
    m_loopWaitEvent.set(true);
}

void WanDevTokenMgr::start()
{
    std::lock_guard<std::mutex> threadLock(m_threadMutex);
    if (m_thread) {
        return;
    }
    {
        boost::shared_lock<boost::shared_mutex> tokenLock(m_tokenMutex);
        if (!m_active) {
            return;
        }
    }
    m_exitThread = false;
    m_thread.reset(new boost::thread(boost::bind(&WanDevTokenMgr::run, this)));
}

void WanDevTokenMgr::exit()
{
    {
        boost::unique_lock<boost::shared_mutex> lock(m_tokenMutex);
        m_active = false;
    }
    m_exitThread = true;
    m_loopWaitEvent.set(true);
    std::lock_guard<std::mutex> threadLock(m_threadMutex);
    if (m_thread) {
        m_thread->join();
        m_thread.reset(nullptr);
    }
}

ScopedWanDevToken WanDevTokenMgr::getScopedToken()
{
    boost::shared_lock<boost::shared_mutex> lock(m_tokenMutex);
    return ScopedWanDevToken(m_tokenData.accessToken, m_sessionGeneration, m_tokenRevision);
}

ComErrno WanDevTokenMgr::refreshToken(ScopedWanDevToken &scopedToken)
{
    ComErrno ret = doRefreshToken(scopedToken.sessionGeneration(), scopedToken.tokenRevision());
    if (ret == COM_OK) {
        ScopedWanDevToken latestToken = getScopedToken();
        if (latestToken.sessionGeneration() != scopedToken.sessionGeneration()) {
            return COM_ABORTED_BY_USER;
        }
        scopedToken = std::move(latestToken);
    }
    return ret;
}

void WanDevTokenMgr::run()
{
    while (!m_exitThread) {
        m_loopWaitEvent.waitTrue(3000);
        m_loopWaitEvent.set(false);
        com_token_data_t tokenData;
        uint64_t sessionGeneration = 0;
        uint64_t tokenRevision = 0;
        {
            boost::shared_lock<boost::shared_mutex> lock(m_tokenMutex);
            if (!m_active) {
                continue;
            }
            tokenData = m_tokenData;
            sessionGeneration = m_sessionGeneration;
            tokenRevision = m_tokenRevision;
        }
        const time_t now = time(nullptr);
        const bool invalid_time = tokenData.startTime <= 0 || tokenData.expiresIn <= 0
            || tokenData.startTime > now + 300;
        if (!invalid_time) {
            const time_t elapsed_seconds = now - tokenData.startTime;
            if (tokenData.expiresIn - elapsed_seconds > 300)
                continue;
        }
        doRefreshToken(sessionGeneration, tokenRevision);
    }
}

ComErrno WanDevTokenMgr::doRefreshToken(uint64_t sessionGeneration, uint64_t tokenRevision)
{
    std::lock_guard<std::mutex> refreshLock(m_refreshMutex);
    std::string refreshToken;
    {
        boost::shared_lock<boost::shared_mutex> lock(m_tokenMutex);
        if (!m_active || sessionGeneration != m_sessionGeneration) {
            return COM_ABORTED_BY_USER;
        }
        if (tokenRevision != m_tokenRevision) {
            return COM_OK;
        }
        refreshToken = m_tokenData.refreshToken;
    }

    com_token_data_t tmpTokenData;
    const ComErrno ret = MultiComUtils::refreshToken(refreshToken, tmpTokenData, ComTimeoutWanB);
    uint64_t newRevision = tokenRevision;
    {
        boost::unique_lock<boost::shared_mutex> lock(m_tokenMutex);
        if (!m_active || sessionGeneration != m_sessionGeneration || tokenRevision != m_tokenRevision) {
            return COM_ABORTED_BY_USER;
        }
        if (ret == COM_OK) {
            m_tokenData = tmpTokenData;
            newRevision = ++m_tokenRevision;
        }
    }
    QueueEvent(new ComRefreshTokenEvent(COM_REFRESH_TOKEN_EVENT, tmpTokenData, ret,
        sessionGeneration, tokenRevision, newRevision));
    return ret;
}

}} // namespace Slic3r::GUI
