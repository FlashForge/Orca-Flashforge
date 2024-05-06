#ifndef Slic3r_GUI_ExportSliceJob_hpp
#define Slic3r_GUI_ExportSliceJob_hpp
#include "PlaterJob.hpp"

namespace Slic3r::GUI {

class ExportSliceJob : public PlaterJob
{
public:
    ExportSliceJob(Plater* plater, const std::string& slice_path, const std::string& thumb_path, int plate_idx);

    void set_event_handle(wxEvtHandler* handle);

protected:
    void on_exception(const std::exception_ptr &) override;

private:
    std::string         m_slice_path;
    std::string         m_thumb_path;
    int                 m_plate_idx {-1};
    wxEvtHandler*       m_event_handle{nullptr};

    void process() override;
    void finalize() override;
    void send_event(int code, const wxString& msg);
};
wxDECLARE_EVENT(EVT_EXPORT_SLICE_COMPLETED, wxCommandEvent);

}

#endif /*Slic3r_GUI_ExportSliceJob_hpp*/