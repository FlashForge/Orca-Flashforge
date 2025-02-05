#ifndef __UnbindJob_HPP__
#define __UnbindJob_HPP__

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include "../GUI_App.hpp"
#include "Job.hpp"

namespace fs = boost::filesystem;

namespace Slic3r {
namespace GUI {

class DeviceObject;
class UnbindJob : public Job
{
    wxWindow *           m_event_handle{nullptr};
    DeviceObject*        m_dev_obj {nullptr};
    std::string          m_dev_id;
    std::string          m_bind_id;
    std::function<void()> m_success_fun{nullptr};
    bool                m_job_finished{ false };
    int                 m_print_job_completed_id = 0;
    bool                m_improved{false};

//protected:
//    void on_exception(const std::exception_ptr &) override;
public:
    UnbindJob(DeviceObject* dev_obj);
    UnbindJob(const std::string &dev_id, const std::string &bind_id);

    bool is_finished() { return m_job_finished;  }

    void on_success(std::function<void()> success);
    void process();
    void process(Ctl& ctl){};
    
    void update_status(int st, const std::string& msg = ""){};
    bool was_canceled() const { return true; };
    //void finalize() override;
    void set_event_handle(wxWindow* hanle);
    void set_improved(bool improved){m_improved = improved;};
};

wxDECLARE_EVENT(EVT_UNBIND_MACHINE_COMPLETED, wxCommandEvent);
}} // namespace Slic3r::GUI

#endif // ARRANGEJOB_HPP
