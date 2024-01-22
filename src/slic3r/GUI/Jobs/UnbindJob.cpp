#include "UnbindJob.hpp"
#include <boost/log/trivial.hpp>
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/GUI_App.hpp"

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_UNBIND_MACHINE_COMPLETED, wxCommandEvent);
UnbindJob::UnbindJob(DeviceObject* dev_obj, const std::string &serialNumber, const std::string &dev_id)
    : PlaterJob{nullptr, wxGetApp().plater()}
    , m_dev_obj(dev_obj)
    , m_serial_number(serialNumber)
    , m_dev_id(dev_id)
{
}

void UnbindJob::on_exception(const std::exception_ptr &eptr)
{
    try {
        if (eptr)
            std::rethrow_exception(eptr);
    } catch (std::exception &e) {
        Job::on_exception(eptr);
    }
}

void UnbindJob::on_success(std::function<void()> success)
{
    m_success_fun = success;
}

void UnbindJob::process()
{
    if (!m_dev_obj || m_serial_number.empty() || m_dev_id.empty()) {
        BOOST_LOG_TRIVIAL(error) << "UnbindJob: Invalid parameter: serial_number(" << m_serial_number
            << "), dev_id(" << m_dev_id << ")";
        wxCommandEvent event(EVT_UNBIND_MACHINE_COMPLETED);
        event.SetInt(-1);
        event.SetEventObject(m_event_handle);
        wxPostEvent(m_event_handle, event);
        return;
    }

    ComErrno result = m_dev_obj->unbind_wan_machine(m_serial_number, m_dev_id);
    wxCommandEvent event(EVT_UNBIND_MACHINE_COMPLETED);
    event.SetInt(result);
    event.SetEventObject(m_event_handle);
    wxPostEvent(m_event_handle, event);
}

void UnbindJob::finalize()
{
    if (was_canceled()) return;

    Job::finalize();
}

void UnbindJob::set_event_handle(wxWindow *hanle)
{
    m_event_handle = hanle;
}

}} // namespace Slic3r::GUI
