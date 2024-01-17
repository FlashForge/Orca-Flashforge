#ifndef __BindJob_HPP__
#define __BindJob_HPP__

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include "PlaterJob.hpp"

namespace fs = boost::filesystem;

namespace Slic3r {
namespace GUI {

class BindJob : public PlaterJob
{
    wxWindow *           m_event_handle{nullptr};
    std::function<void()> m_success_fun{nullptr};
    std::string         m_serial_number;
    unsigned short      m_dev_pid;
    std::string         m_dev_name;
    bool                m_job_finished{ false };
    int                 m_print_job_completed_id = 0;
    bool                m_improved{false};

protected:
    void on_exception(const std::exception_ptr &) override;
public:
    BindJob(std::shared_ptr<ProgressIndicator> pri, Plater *plater, const std::string &serialNumber, unsigned short pid, const std::string &dev_name);

    int  status_range() const override
    {
        return 100;
    }

    bool is_finished() { return m_job_finished;  }

    void on_success(std::function<void()> success);
    void process() override;
    void finalize() override;
    void set_event_handle(wxWindow* hanle);
    void set_improved(bool improved){m_improved = improved;};
};

wxDECLARE_EVENT(EVT_BIND_MACHINE_SUCCESS, wxCommandEvent);
wxDECLARE_EVENT(EVT_BIND_MACHINE_FAIL, wxCommandEvent);
}} // namespace Slic3r::GUI

#endif // ARRANGEJOB_HPP
