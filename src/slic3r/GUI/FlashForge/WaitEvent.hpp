#ifndef slic3r_GUI_WaitEvent_hpp_
#define slic3r_GUI_WaitEvent_hpp_

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

namespace Slic3r { namespace GUI {

class WaitEvent
{
public:
    WaitEvent(bool state=false);

    bool get();
    void set(bool state);
    bool waitTrue(unsigned int milliseconds = -1);

private:
    bool m_state;
    boost::mutex m_stateMutex;
    boost::condition_variable m_condition;
};

}} // namespace Slic3r::GUI

#endif
