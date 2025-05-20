#ifndef slic3r_GUI_WanDevTokenMgr_hpp_
#define slic3r_GUI_WanDevTokenMgr_hpp_

#include <atomic>
#include <memory>
#include <boost/thread/thread.hpp>
#include <wx/event.h>
#include "FlashNetworkIntfc.h"
#include "MultiComDef.hpp"
#include "Singleton.hpp"
#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

class ScopedWanDevToken
{
public:
    explicit ScopedWanDevToken(const std::string &accessToken, boost::shared_mutex &mutex)
        : m_accessToken(accessToken)
        , m_sharedLock(mutex)
    {
    }
    ScopedWanDevToken(ScopedWanDevToken &&that) noexcept
        : m_accessToken(std::move(that.m_accessToken))
        , m_sharedLock(std::move(that.m_sharedLock))
    {
    }
    void unlockToken()
    {
        m_sharedLock.unlock();
    }
    const std::string &accessToken() const
    {
        return m_accessToken;
    }
    ScopedWanDevToken &operator=(ScopedWanDevToken &&that) noexcept
    {
        m_accessToken = std::move(that.m_accessToken);
        m_sharedLock = std::move(that.m_sharedLock);
        return *this;
    }

private:
    std::string m_accessToken;
    boost::shared_lock<boost::shared_mutex> m_sharedLock;
};

class WanDevTokenMgr :  public wxEvtHandler, public Singleton<WanDevTokenMgr>
{
public:
    void start(const com_token_data_t &tokenData, fnet::FlashNetworkIntfc *networkIntfc);

    void exit();

    ScopedWanDevToken getScopedToken();

    // The original token will be unlocked, and if the token refresh fails, it will become unavailable.
    ComErrno refreshToken(ScopedWanDevToken &scopedToken);

private:
    void run();

    ComErrno doRefreshToken();

private:
    com_token_data_t                m_tokenData;
    boost::shared_mutex             m_tokenMutex;
    std::atomic_bool                m_exitThread;
    WaitEvent                       m_loopWaitEvent;
    std::unique_ptr<boost::thread>  m_thread;
    fnet::FlashNetworkIntfc        *m_networkIntfc;
};

}} // namespace Slic3r::GUI

#endif
