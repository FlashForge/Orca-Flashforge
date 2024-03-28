#ifndef slic3r_GUI_ConnectPrinter_hpp_
#define slic3r_GUI_ConnectPrinter_hpp_

#include "GUI.hpp"
#include "GUI_Utils.hpp"
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/statbmp.h>
#include "Widgets/Button.hpp"
#include "Widgets/TextInput.hpp"
#include "Widgets/Label.hpp"
#include "DeviceManager.hpp"
#include "TitleDialog.hpp"

class FFPushButton;
namespace Slic3r { namespace GUI {
class DeviceObject;
class ConnectPrinterDialog : public TitleDialog
{
private:
protected:
    bool            m_need_connect{true};
    wxStaticText *  m_staticText_connection_code;
    TextInput *     m_textCtrl_code;
    //Button *        m_button_confirm;
    FFPushButton   *m_button_confirm{nullptr};
    wxStaticText*   m_staticText_hints;
    Label*          m_label_error_info;

    MachineObject*  m_obj{ nullptr };
    DeviceObject*   m_devObj {nullptr};
    wxString        m_input_access_code;
    wxPanel        *m_error_panel;

public:
    //ConnectPrinterDialog(wxWindow       *parent,
    //                     wxWindowID      id       = wxID_ANY,
    //                     const wxString &title    = wxEmptyString,
    //                     bool            err_hint = false,
    //                     const wxPoint  &pos      = wxDefaultPosition,
    //                     const wxSize   &size     = wxDefaultSize,
    //                     long            style    = wxCLOSE_BOX | wxCAPTION);
    ConnectPrinterDialog(bool err_hint = false);
    ~ConnectPrinterDialog();

    void go_connect_printer(bool need) {m_need_connect = need;};
    void end_modal(wxStandardID id);
    void set_machine_object(MachineObject* obj);
    void set_device_object(DeviceObject* devObj);
    void on_input_enter(wxCommandEvent& evt);
    void on_button_confirm(wxMouseEvent &event); 
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_show(wxShowEvent &event);
};
}} // namespace Slic3r::GUI

#endif