#include "WaitEvent.hpp"

namespace Slic3r { namespace GUI {

WaitEvent::WaitEvent(bool state/* =false */)
    : m_state(state)
{
}

bool WaitEvent::get()
{
    boost::mutex::scoped_lock lock(m_stateMutex);
    return m_state;
}

void WaitEvent::set(bool state)
{
    boost::mutex::scoped_lock lock(m_stateMutex);
    if (state == m_state) {
        return;
    }
    if (state) {
        m_condition.notify_all();
    }
    m_state = state;
}

bool WaitEvent::waitTrue(unsigned int milliseconds /* = -1 */)
{
    boost::mutex::scoped_lock lock(m_stateMutex);
    if (m_state) {
        return true;
    }
    auto chronoMs = boost::chrono::milliseconds(milliseconds);
    return m_condition.wait_for(lock, chronoMs) == boost::cv_status::no_timeout;
}

}} // namespace Slic3r::GUI
