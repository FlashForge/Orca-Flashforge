#include "ExportSliceJob.hpp"
#include <boost/log/trivial.hpp>
#include "slic3r/GUI/Plater.hpp"


namespace fs = boost::filesystem;

namespace Slic3r::GUI
{
    
wxDEFINE_EVENT(EVT_EXPORT_SLICE_COMPLETED, wxCommandEvent);
    
 ExportSliceJob::ExportSliceJob(Plater* plater, const std::string& slice_path, const std::string& thumb_path, int plate_idx)
     : PlaterJob(nullptr, plater)
     , m_slice_path(slice_path)
     , m_thumb_path(thumb_path) 
     , m_plate_idx(plate_idx)
{
}

void ExportSliceJob::process()
{
    BOOST_LOG_TRIVIAL(info) << "ExportSliceJob: start process";
    if (!m_plater || m_slice_path.empty() || m_thumb_path.empty()) {
        BOOST_LOG_TRIVIAL(error) << "ExportSliceJob: export slice job fail, parameter invalid";
        return send_event(-1, "parameter invalid");
    }
    BOOST_LOG_TRIVIAL(info) << "ExportSliceJob: : slice_path: " << m_slice_path << ", thumb_path: " << m_thumb_path;
    ThumbnailData &data   = m_plater->get_partplate_list().get_curr_plate()->thumbnail_data;
    if (data.is_valid()) {
        wxImage image(data.width, data.height);
        image.InitAlpha();
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr = (data.height - 1 - r) * data.width;
            for (unsigned int c = 0; c < data.width; ++c) {
                unsigned char *px = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                image.SetRGB((int) c, (int) r, px[0], px[1], px[2]);
                image.SetAlpha((int) c, (int) r, px[3]);
            }
        }
        image = image.Rescale(256, 256);
        if (image.SaveFile(wxString::FromUTF8(m_thumb_path))) {
            BOOST_LOG_TRIVIAL(info) << "ExportSliceJob: save thumb (" << m_thumb_path << ") success";
        } else {
            BOOST_LOG_TRIVIAL(error) << "ExportSliceJob: save thumb (" << m_thumb_path << ") fail";
            return send_event(-1, "save thumb fail");
        }
    }
    if (was_canceled()) {
        BOOST_LOG_TRIVIAL(info) << "ExportSliceJob: : canceled";
        return send_event(1, "canceled");
    }

    int result = m_plater->export_3mf(m_slice_path, SaveStrategy::Silence | SaveStrategy::SplitModel | SaveStrategy::WithGcode | SaveStrategy::SkipModel, m_plate_idx);
    if (result < 0) {
        BOOST_LOG_TRIVIAL(error) << "ExportSliceJob: : export 3mf error: " << result;
        return send_event(-1, "export 3mf fail");
    }
    BOOST_LOG_TRIVIAL(info) << "ExportSliceJob: : export 3mf success: " << result;
    return send_event(0, "success");
}

void ExportSliceJob::on_exception(const std::exception_ptr &eptr)
{
    try {
        if (eptr)
            std::rethrow_exception(eptr);
    } catch (std::exception &e) {
        PlaterJob::on_exception(eptr);
    }
}

void ExportSliceJob::finalize()
{
    if (was_canceled()) return;

    Job::finalize();
}

void ExportSliceJob::set_event_handle(wxEvtHandler *handle)
{
    m_event_handle = handle;
}

void ExportSliceJob::send_event(int code, const wxString& msg)
{
    if (m_event_handle) {
        wxCommandEvent event(EVT_EXPORT_SLICE_COMPLETED);
        event.SetInt(-1);
        event.SetString(msg);
        event.SetEventObject(m_event_handle);
        wxPostEvent(m_event_handle, event);
    }
}

} // namespace Slic3r::GUI
