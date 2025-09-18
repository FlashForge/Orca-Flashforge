#ifndef slic3r_BindDialog_hpp_
#define slic3r_BindDialog_hpp_

#include "I18N.hpp"

#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/gauge.h>
#include <wx/button.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/dialog.h>
#include <curl/curl.h>
#include "wxExtensions.hpp"
#include "Plater.hpp"
#include "Widgets/StepCtrl.hpp"
#include "Widgets/ProgressDialog.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/ProgressBar.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Jobs/BindJob.hpp"
#include "Jobs/UnbindJob.hpp"
#include "BBLStatusBar.hpp"
#include "BBLStatusBarBind.hpp"
#include "TitleDialog.hpp"
#include "Widgets/FFCheckBox.hpp"


#define BIND_DIALOG_GREY200 wxColour(248, 248, 248)
#define BIND_DIALOG_GREY800 wxColour(50, 58, 61)
#define BIND_DIALOG_GREY900 wxColour(38, 46, 48)
#define BIND_DIALOG_BUTTON_SIZE wxSize(FromDIP(68), FromDIP(24))
#define BIND_DIALOG_BUTTON_PANEL_SIZE wxSize(FromDIP(450), FromDIP(30))


class FFButton;
namespace Slic3r { namespace GUI {

struct MemoryStruct
{
    char * memory;
    size_t read_pos;
    size_t size;
};


class RoundImagePanel : public wxPanel
{
public:
    RoundImagePanel(wxWindow *parent, const wxSize& size = wxDefaultSize);

    void SetImage(const wxImage& image);
        
private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void CreateRegion(wxDC &dc);

private:
    wxImage     m_image { wxNullImage };
};

struct BindInfo;
//class DeviceObject;
class BindMachineDialog : public TitleDialog
{
private:
    class LinkLabel : public Label
    {
    public:
        LinkLabel(wxWindow *parent, const wxString &text, const wxString& link);

    private:
        wxString m_link;
    };

private:
    wxWindow*      m_panel_agreement;

    wxSimplebook *m_simplebook;
    wxPanel*       m_normal_panel;
    wxPanel*       m_top_panel;
    wxSizer*       m_machine_sizer;
    wxStaticBitmap *m_printer_img;
    wxStaticText * m_printer_name;
    wxSizer*       m_user_sizer;
    RoundImagePanel* m_user_panel;
    wxStaticText * m_user_name;
    wxStaticText * m_bind_text;
    FFCheckBox*    m_checkbox_privacy;
    LinkLabel*     m_privacy_title;
    LinkLabel*     m_terms_title;
    FFButton*      m_bind_btn;
    FFButton*      m_cancel_btn;
    wxPanel*       m_result_panel;
    wxBoxSizer*    m_result_sizer;
    wxStaticText*  m_result_text;
    FFButton*      m_result_btn;

    int            m_result_code;
    bool           m_unbind_flag;

    MachineObject *                   m_machine_info{nullptr};
    //DeviceObject                     *m_device_info {nullptr};
    BindInfo                *m_bind_info{nullptr};
    std::shared_ptr<BindJob>          m_bind_job;

public:
    BindMachineDialog();
    ~BindMachineDialog();

    void     on_cancel(wxCommandEvent& event);
    void     on_bind_fail(wxCommandEvent &event);
    void     on_bind_success(wxCommandEvent &event);
    void     on_bind_printer(wxCommandEvent &event);
    void     on_dpi_changed(const wxRect &suggested_rect) override;
    void     update_machine_info(MachineObject *info);
    //void     update_device_info(DeviceObject *info);
    void     update_device_info2(BindInfo* bind_info);
    void     on_show(wxShowEvent &event);
    void     on_close(wxCloseEvent& event);
    void     on_destroy();
    void     on_result_ok(wxCommandEvent& event);
    void     on_user_image_updated(wxCommandEvent& event);
};

// unbind
class UnBindMachineDialog : public TitleDialog
{
private:
    wxWindow*      m_panel_agreement;

    wxSizer*       m_machine_sizer;
    wxStaticBitmap *m_printer_img;
    wxStaticText * m_printer_name;
    wxSizer*       m_user_sizer;
    RoundImagePanel* m_user_panel;
    wxStaticText * m_user_name;
    wxStaticText * m_unbind_text;
    FFButton*      m_unbind_btn;
    FFButton*      m_cancel_btn;

    int            m_result_code;

    MachineObject *                   m_machine_info{nullptr};
    //DeviceObject                     *m_device_info {nullptr};
    BindInfo                  *m_unbind_info{nullptr};
    std::shared_ptr<UnbindJob>        m_unbind_job;

public:
    UnBindMachineDialog();
    ~UnBindMachineDialog();

    void     on_cancel(wxCommandEvent& event);
    void     on_unbind_printer(wxCommandEvent &event);
    void     on_unbind_completed(wxCommandEvent &event);
    void     on_dpi_changed(const wxRect &suggested_rect) override;
    void     update_machine_info(MachineObject *info);
    //void     update_device_info(DeviceObject *info);
    void     update_device_info2(BindInfo* info);
    void     on_show(wxShowEvent &event);
    void     on_close(wxCloseEvent& event);
    void     on_destroy();
    void     on_result_ok(wxCommandEvent& event);
    void     on_user_image_updated(wxCommandEvent& event);
};

}} // namespace Slic3r::GUI

#endif /* slic3r_BindDialog_hpp_ */
