#include "UnbindJob.hpp"
#include <boost/log/trivial.hpp>
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/DeviceData.hpp"
#include "slic3r/GUI/GUI_App.hpp"

namespace Slic3r {
namespace GUI {

wxDEFINE_EVENT(EVT_UNBIND_MACHINE_COMPLETED, wxCommandEvent);
UnbindJob::UnbindJob(DeviceObject* dev_obj)
    : PlaterJob{nullptr, wxGetApp().plater()}
    , m_dev_obj(dev_obj)
{
}

UnbindJob::UnbindJob(const std::string &dev_id, const std::string &bind_id) 
    : PlaterJob{nullptr, wxGetApp().plater()}, 
    m_dev_id(dev_id) , m_bind_id(bind_id)
{}

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
    DeviceObjectOpr *devOpr = wxGetApp().getDeviceObjectOpr();
    if (!devOpr || /*!m_dev_obj*/m_dev_id.empty() || m_bind_id.empty()) {
        if (!devOpr) {
            BOOST_LOG_TRIVIAL(error) << "UnbindJob: Invalid parameter: device object opr is null";
        } else {
            BOOST_LOG_TRIVIAL(error) << "UnbindJob: Invalid parameter: dev id or bind id is empty";
        }
        //if (!m_dev_obj) {
        //    BOOST_LOG_TRIVIAL(error) << "UnbindJob: Invalid parameter: device object is null";
        //}
        wxCommandEvent event(EVT_UNBIND_MACHINE_COMPLETED);
        event.SetInt(-1);
        event.SetEventObject(m_event_handle);
        wxPostEvent(m_event_handle, event);
        return;
    }

    //ComErrno result = devOpr->unbind_wan_machine(m_dev_obj);
    ComErrno       result = devOpr->unbind_wan_machine2(m_dev_id, m_bind_id);
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
