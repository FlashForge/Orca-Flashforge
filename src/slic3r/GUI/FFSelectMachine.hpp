#ifndef slic3r_GUI_FFSelectMachine_hpp_
#define slic3r_GUI_FFSelectMachine_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/collpane.h>
#include <wx/dataview.h>
#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/dataview.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/string.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/hyperlink.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/popupwin.h>
#include <wx/spinctrl.h>
#include <wx/artprov.h>
#include <wx/wrapsizer.h>
#include <wx/srchctrl.h>

#include "AmsMappingPopup.hpp"
#include "ReleaseNote.hpp"
#include "GUI_Utils.hpp"
#include "wxExtensions.hpp"
#include "DeviceManager.hpp"
#include "Plater.hpp"
#include "BBLStatusBar.hpp"
#include "BBLStatusBarSend.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/ScrolledWindow.hpp"
#include "Widgets/PopupWindow.hpp"
#include "FlashForge/MultiComEvent.hpp"
#include "PrePrintChecker.hpp"
#include <wx/simplebook.h>
#include <wx/hashmap.h>
#include "slic3r/GUI/Widgets/FFPopupWindow.hpp"

namespace Slic3r { namespace GUI {

enum PrinterState {
    OFFLINE_LAN,
    OFFLINE_WAN,
    IDLE,
    BUSY,
    ONLINE_LAN, 
    ONLINE_WAN,
    IN_LAN, LOCK, OFFLINE
};

enum PrinterBindState {
    NONE,
    ALLOW_BIND,
    ALLOW_UNBIND
};

// move to seperate file
class MachineListModel : public wxDataViewVirtualListModel
{
public:
    enum {
        Col_MachineName           = 0,
        Col_MachineSN             = 1,
        Col_MachineBind           = 2,
        Col_MachinePrintingStatus = 3,
        Col_MachineIPAddress      = 4,
        Col_MachineConnection     = 5,
        Col_MachineTaskName       = 6,
        Col_Max                   = 7
    };
    MachineListModel();

    virtual unsigned int GetColumnCount() const wxOVERRIDE { return Col_Max; }

    virtual wxString GetColumnType(unsigned int col) const wxOVERRIDE { return "string"; }

    virtual void GetValueByRow(wxVariant &variant, unsigned int row, unsigned int col) const wxOVERRIDE;
    virtual bool GetAttrByRow(unsigned int row, unsigned int col, wxDataViewItemAttr &attr) const wxOVERRIDE;
    virtual bool SetValueByRow(const wxVariant &variant, unsigned int row, unsigned int col) wxOVERRIDE;

    void display_machines(std::map<std::string, MachineObject *> list);
    void add_machine(MachineObject *obj, bool reset = true);
    int  find_row_by_sn(wxString sn);

private:
    wxArrayString m_values[Col_Max];

    wxArrayString m_nameColValues;
    wxArrayString m_snColValues;
    wxArrayString m_bindColValues;
    wxArrayString m_connectionColValues;
    wxArrayString m_printingStatusValues;
    wxArrayString m_ipAddressValues;
};

class DeviceObject;
class MachineObjectPanel : public wxPanel
{
private:
    bool        m_is_my_devices {false};
    bool        m_show_edit{false};
    bool        m_show_bind{false};
    bool        m_hover {false};
    bool        m_press_flag{false};
    bool        m_is_macos_special_version{false};


    PrinterBindState   m_bind_state;
    PrinterState       m_state;

    ScalableBitmap m_unbind_img;
    ScalableBitmap m_edit_name_img;
    ScalableBitmap m_select_unbind_img;

    ScalableBitmap m_printer_status_offline_lan;
    ScalableBitmap m_printer_status_offline_wan;
    ScalableBitmap m_printer_status_busy;
    ScalableBitmap m_printer_status_idle;
    ScalableBitmap m_printer_online_lan;
    ScalableBitmap m_printer_online_wan;

    MachineObject *m_info;
    DeviceObject  *m_devInfo;

protected:
    wxStaticBitmap *m_bitmap_info;
    wxStaticBitmap *m_bitmap_bind;

public:
    MachineObjectPanel(wxWindow *      parent,
                       wxWindowID      id    = wxID_ANY,
                       const wxPoint & pos   = wxDefaultPosition,
                       const wxSize &  size  = wxDefaultSize,
                       long            style = wxTAB_TRAVERSAL,
                       const wxString &name  = wxEmptyString);
    
    ~MachineObjectPanel();

    void set_printer_state(PrinterState state);
    void show_printer_bind(bool show, PrinterBindState state);
    void show_edit_printer_name(bool show);
    void update_machine_info(MachineObject *info, bool is_my_devices = false);
    void update_device_info(DeviceObject *info, bool is_my_devices = false);
    DeviceObject *device_info();

    void SetHover(bool hover);
    void SetPressed(bool pressed, bool hit);
    bool IsPressed() const { return m_press_flag; }
    
protected:
    void OnPaint(wxPaintEvent &event);
    void render(wxDC &dc);
    void doRender(wxDC &dc);
    void on_mouse_enter(wxMouseEvent &evt);
    void on_mouse_leave(wxMouseEvent &evt);
    void on_mouse_left_up(wxMouseEvent &evt);

private:
    bool is_wan_offline_lan_unbind(DeviceObject* &obj);
};

#define SELECT_MACHINE_POPUP_SIZE wxSize(FromDIP(216), FromDIP(674))
#define SELECT_MACHINE_LIST_SIZE wxSize(FromDIP(212), FromDIP(1920))
#define SELECT_MACHINE_ITEM_SIZE wxSize(FromDIP(190), FromDIP(35))
#define SELECT_MACHINE_GREY900 wxColour(38, 46, 48)
#define SELECT_MACHINE_GREY600 wxColour(144,144,144)
#define SELECT_MACHINE_GREY400 wxColour(206, 206, 206)
#define SELECT_MACHINE_BRAND wxColour(0, 150, 136)
#define SELECT_MACHINE_REMIND wxColour(255,111,0)
#define SELECT_MACHINE_LIGHT_GREEN wxColour(219, 253, 231)

class MachinePanel
{
public:
    wxString mIndex;
    MachineObjectPanel *mPanel;
};

class ThumbnailPanel;
class DeviceObject;
class DeviceListUpdateEvent;
class MachineListUpdateEvent;
#ifdef __WINDOWS__
class SelectMachinePopup : public PopupWindow
{
public:
    SelectMachinePopup(wxWindow *parent);
    ~SelectMachinePopup();

    // PopupWindow virtual methods are all overridden to log them
    virtual void Popup(wxWindow *focus = NULL) wxOVERRIDE;
    virtual void OnDismiss() wxOVERRIDE;
    virtual bool ProcessLeftDown(wxMouseEvent &event) wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;

    void update_machine_list(wxCommandEvent &event);
    bool was_dismiss() { return m_dismiss; }
public:
#ifdef __APPLE__
    static bool                       m_wan_bind_enable;
#endif
private:
    int                               m_my_devices_count{0};
    int                               m_other_devices_count{0};
    wxBoxSizer *                      m_sizer_body{nullptr};
    wxBoxSizer *                      m_sizer_my_devices{nullptr};
    wxBoxSizer *                      m_sizer_other_devices{nullptr};
    wxBoxSizer *                      m_sizer_search_bar{nullptr};
    wxSearchCtrl*                     m_search_bar{nullptr};
    wxScrolledWindow *                m_scrolledWindow{nullptr};
    wxWindow *                        m_panel_body{nullptr};
    wxTimer *                         m_refresh_timer{nullptr};
    std::vector<MachinePanel*>        m_user_list_machine_panel;
    std::vector<MachinePanel*>        m_other_list_machine_panel;
    boost::thread*                    get_print_info_thread{ nullptr };
    std::string                       m_print_info;
    bool                              m_dismiss { false };
    bool                              m_updateConnect { false };
    
    std::map<std::string, DeviceObject *> m_bind_machine_list;
    std::map<std::string, DeviceObject*>  m_free_device_list;

private:
    void OnLeftUp(wxMouseEvent &event);
    void on_dclick_up(wxMouseEvent &event);
    void on_timer(wxTimerEvent &event);

    void      update_other_devices();
    void      update_user_devices();
    bool      search_for_printer(MachineObject* obj);
    void      on_dissmiss_win(wxCommandEvent &event);
    wxWindow *create_title_panel(wxString text);
    void      on_connect_exit(ComConnectionExitEvent &event);
    void      on_connect_ready(ComConnectionReadyEvent &event);
    void      on_devList_Updated(DeviceListUpdateEvent &event);
};
#else // non-Windows (macOS / Linux): "#else if __APPLE__" was a no-op typo for "#else"
class SelectMachinePopup : public FFPopupWindow
{
public:
    SelectMachinePopup(wxWindow *parent);
    ~SelectMachinePopup();

    // PopupWindow virtual methods are all overridden to log them
    virtual void Popup(wxWindow *focus = NULL) wxOVERRIDE;
    void OnDismiss() override;
    //virtual bool ProcessLeftDown(wxMouseEvent &event) wxOVERRIDE;
    //virtual bool Show(bool show = true) wxOVERRIDE;

    void update_machine_list(wxCommandEvent &event);
    bool was_dismiss() { return m_dismiss; }
    
private:
    void ProcessLeftDown(const wxPoint& pnt) override;
    void ProcessLeftUp(const wxPoint& pnt) override;
    void ProcessMotion(const wxPoint& pnt) override;
    bool ShowDevList(bool show = true);
public:
#ifdef __APPLE__
    static bool                       m_wan_bind_enable;
#endif
private:
    int                               m_my_devices_count{0};
    int                               m_other_devices_count{0};
    wxBoxSizer *                      m_sizer_body{nullptr};
    wxBoxSizer *                      m_sizer_my_devices{nullptr};
    wxBoxSizer *                      m_sizer_other_devices{nullptr};
    wxBoxSizer *                      m_sizer_search_bar{nullptr};
    wxSearchCtrl*                     m_search_bar{nullptr};
    wxScrolledWindow *                m_scrolledWindow{nullptr};
    wxWindow *                        m_panel_body{nullptr};
    wxTimer *                         m_refresh_timer{nullptr};
    std::vector<MachinePanel*>        m_user_list_machine_panel;
    std::vector<MachinePanel*>        m_other_list_machine_panel;
    boost::thread*                    get_print_info_thread{ nullptr };
    std::string                       m_print_info;
    bool                              m_dismiss { false };
    bool                              m_updateConnect { false };
#if 0
    bool m_left_down{false};
    wxPoint m_mouse_pos;
    wxPoint m_scroll_pos_start;
#endif
    std::map<std::string, DeviceObject *> m_bind_machine_list;
    std::map<std::string, DeviceObject*>  m_free_device_list;

private:
    void OnLeftUp(wxMouseEvent &event);
    void on_dclick_up(wxMouseEvent &event);
    void on_timer(wxTimerEvent &event);

	void      update_other_devices();
    void      update_user_devices();    
    bool      search_for_printer(MachineObject* obj);
    void      on_dissmiss_win(wxCommandEvent &event);
    wxWindow *create_title_panel(wxString text);
    void      on_connect_exit(ComConnectionExitEvent &event);
    void      on_connect_ready(ComConnectionReadyEvent &event);
    void      on_devList_Updated(DeviceListUpdateEvent &event);
};
#endif

#define SELECT_MACHINE_DIALOG_BUTTON_SIZE wxSize(FromDIP(68), FromDIP(23))
#define SELECT_MACHINE_DIALOG_SIMBOOK_SIZE wxSize(FromDIP(370), FromDIP(64))

wxDECLARE_EVENT(EVT_FINISHED_UPDATE_MACHINE_LIST, wxCommandEvent);
wxDECLARE_EVENT(EVT_REQUEST_BIND_LIST, wxCommandEvent);
wxDECLARE_EVENT(EVT_WILL_DISMISS_MACHINE_LIST, wxCommandEvent);
wxDECLARE_EVENT(EVT_UPDATE_WINDOWS_POSITION, wxCommandEvent);
wxDECLARE_EVENT(EVT_DISSMISS_MACHINE_LIST, wxCommandEvent);
wxDECLARE_EVENT(EVT_CONNECT_LAN_PRINT, wxCommandEvent);
wxDECLARE_EVENT(EVT_EDIT_PRINT_NAME, wxCommandEvent);
wxDECLARE_EVENT(EVT_UNBIND_MACHINE, wxCommandEvent);
wxDECLARE_EVENT(EVT_BIND_MACHINE, wxCommandEvent);

}} // namespace Slic3r::GUI

#endif
