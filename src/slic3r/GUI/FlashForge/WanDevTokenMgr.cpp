#include "WanDevTokenMgr.hpp"
#include "MultiComEvent.hpp"
#include "MultiComUtils.hpp"

namespace Slic3r { namespace GUI {

void WanDevTokenMgr::start(const com_token_data_t &tokenData, fnet::FlashNetworkIntfc *networkIntfc)
{
    m_tokenData = tokenData;
    m_exitThread = false;
    m_networkIntfc = networkIntfc;
    m_loopWaitEvent.set(true);
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

ComErrno WanDevTokenMgr::refreshToken(ScopedWanDevToken &scopedToken)
{
    scopedToken.unlockToken();
    ComErrno ret = doRefreshToken();
    if (ret == COM_OK) {
        scopedToken = getScopedToken();
    }
    return ret;
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
        doRefreshToken();
    }
}

ComErrno WanDevTokenMgr::doRefreshToken()
{
    boost::unique_lock<boost::shared_mutex> lock(m_tokenMutex);
    com_token_data_t tmpTokenData;
    ComErrno ret = MultiComUtils::refreshToken(m_tokenData.refreshToken, tmpTokenData, ComTimeoutWanB);
    if (ret == COM_OK) {
        m_tokenData = tmpTokenData;
    }
    QueueEvent(new ComRefreshTokenEvent(COM_REFRESH_TOKEN_EVENT, tmpTokenData, ret));
    return ret;
}

}} // namespace Slic3r::GUI
