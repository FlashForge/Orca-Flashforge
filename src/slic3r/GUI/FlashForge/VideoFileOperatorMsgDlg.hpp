#ifndef slic3r_GUI_VideoFileOperatorMsgDlg_hpp_
#define slic3r_GUI_VideoFileOperatorMsgDlg_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include "slic3r/GUI/TitleDialog.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"

namespace Slic3r { 
namespace GUI {

class VideoFileOperatorMsgDlg : public wxDialog
{
public:
    enum class VIDEO_FILE_OPERATOR_TYPE { VIDEO_FILE_DOWNLOAD_SUCCEED, VIDEO_FILE_DOWNLOAD_FAILED, VIDEO_FILE_DELETE };

public:
    VideoFileOperatorMsgDlg(wxWindow* parent, VIDEO_FILE_OPERATOR_TYPE type);
    ~VideoFileOperatorMsgDlg();

private:
    void initWidget();

    void initDownloadSucceedWidget();
    void initDownloadFailedWidget();
    void initDeleteWidget();

private:
    VIDEO_FILE_OPERATOR_TYPE m_video_file_operator_type;

    wxBoxSizer* m_sizer_main{nullptr};

    FFButton* m_btn_yes{nullptr};
    FFButton* m_btn_no{nullptr};
    FFButton* m_btn_confirm{nullptr};
};

} // namespace GUI
} // namespace Slic3r

#endif