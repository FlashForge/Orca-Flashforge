#include "BindJob.hpp"

#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_BIND_MACHINE_SUCCESS, wxCommandEvent);
wxDEFINE_EVENT(EVT_BIND_MACHINE_FAIL, wxCommandEvent);


BindJob::BindJob(std::shared_ptr<ProgressIndicator> pri, Plater *plater, const std::string &serialNumber, unsigned short pid, const std::string &dev_name)
    : PlaterJob{std::move(pri), plater}
    , m_serial_number(serialNumber)
    , m_dev_pid(pid)
    , m_dev_name(dev_name)
{
}

void BindJob::on_exception(const std::exception_ptr &eptr)
{
    try {
        if (eptr)
            std::rethrow_exception(eptr);
    } catch (std::exception &e) {
        PlaterJob::on_exception(eptr);
    }
}

void BindJob::on_success(std::function<void()> success)
{
    m_success_fun = success;
}

void BindJob::process()
{
    if (m_serial_number.empty() || 0 == m_dev_pid) {
        BOOST_LOG_TRIVIAL(error) << "BindJob: Invalid parameter: serial_number(" << m_serial_number
            << "), dev_pid(" << m_dev_pid << ")";
        wxCommandEvent event(EVT_BIND_MACHINE_FAIL);
        event.SetInt(-1);
        event.SetEventObject(m_event_handle);
        wxPostEvent(m_event_handle, event);
        return;
    }

    ComErrno result = MultiComMgr::inst()->bindWanDev(m_serial_number, m_dev_pid, m_dev_name);
    if (result != COM_OK) {
        wxCommandEvent event(EVT_BIND_MACHINE_FAIL);
        event.SetInt(result);
        event.SetEventObject(m_event_handle);
        wxPostEvent(m_event_handle, event);
    } else {
        wxCommandEvent event(EVT_BIND_MACHINE_SUCCESS);
        event.SetEventObject(m_event_handle);
        wxPostEvent(m_event_handle, event);
    }
}

void BindJob::finalize()
{
    if (was_canceled()) return;

    Job::finalize();
}

void BindJob::set_event_handle(wxWindow *hanle)
{
    m_event_handle = hanle;
}

}} // namespace Slic3r::GUI
