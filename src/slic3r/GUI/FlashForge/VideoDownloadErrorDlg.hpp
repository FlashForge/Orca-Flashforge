#ifndef _Slic3r_GUI_VideoDownloadErrorDlg_hpp_
#define _Slic3r_GUI_VideoDownloadErrorDlg_hpp_

#include <wx/wx.h>
#include <wx/listctrl.h>
#include "slic3r/GUI/TitleDialog.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"

namespace Slic3r { namespace GUI {

class VideoDownloadErrorDlg : public wxDialog
{
public:
    VideoDownloadErrorDlg(wxWindow* parent, const std::vector<wxString>& file_infos);
    ~VideoDownloadErrorDlg();

private:
    wxBoxSizer* m_sizer_main{nullptr};
    wxBoxSizer* m_sizer_video_info{nullptr};
    wxBoxSizer* m_sizer_scroll{nullptr};

    wxStaticText*       m_msg_Lbl{nullptr};
    wxStaticText*       m_list_Lbl{nullptr};
    FFButton*           m_btn_confirm{nullptr};
    wxScrolledWindow*   m_scroll_wgt{nullptr};

    std::vector<wxString> m_file_infos;
};

}} // namespace Slic3r::GUI

#endif
