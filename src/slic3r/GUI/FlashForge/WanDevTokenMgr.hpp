#ifndef slic3r_GUI_WanDevTokenMgr_hpp_
#define slic3r_GUI_WanDevTokenMgr_hpp_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
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
    ScopedWanDevToken() = default;
    ScopedWanDevToken(std::string accessToken, uint64_t sessionGeneration, uint64_t tokenRevision)
        : m_accessToken(std::move(accessToken))
        , m_sessionGeneration(sessionGeneration)
        , m_tokenRevision(tokenRevision)
    {
    }
    const std::string &accessToken() const
    {
        return m_accessToken;
    }
    uint64_t sessionGeneration() const { return m_sessionGeneration; }
    uint64_t tokenRevision() const { return m_tokenRevision; }

private:
    std::string m_accessToken;
    uint64_t m_sessionGeneration { 0 };
    uint64_t m_tokenRevision { 0 };
};

class WanDevTokenMgr :  public wxEvtHandler, public Singleton<WanDevTokenMgr>
{
public:
    void initalize(const com_token_data_t &tokenData, fnet::FlashNetworkIntfc *networkIntfc,
        uint64_t sessionGeneration);

    void start();

    void exit();

    ScopedWanDevToken getScopedToken();

    // Refreshes the snapshot's revision at most once. Concurrent callers reuse
    // the newest token instead of issuing a second refresh request.
    ComErrno refreshToken(ScopedWanDevToken &scopedToken);

private:
    void run();

    ComErrno doRefreshToken(uint64_t sessionGeneration, uint64_t tokenRevision);

private:
    com_token_data_t                m_tokenData;
    boost::shared_mutex             m_tokenMutex;
    std::mutex                      m_refreshMutex;
    std::mutex                      m_threadMutex;
    uint64_t                        m_sessionGeneration { 0 };
    uint64_t                        m_tokenRevision { 0 };
    bool                            m_active { false };
    std::atomic_bool                m_exitThread { true };
    WaitEvent                       m_loopWaitEvent;
    std::unique_ptr<boost::thread>  m_thread;
    fnet::FlashNetworkIntfc        *m_networkIntfc { nullptr };
};

}} // namespace Slic3r::GUI

#endif
