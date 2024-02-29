#ifndef slic3r_DeviceListPanel_hpp_
#define slic3r_DeviceListPanel_hpp_
#include <wx/simplebook.h>
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/PopupWindow.hpp"
#include "MultiComMgr.hpp"
#include "MultiComEvent.hpp"

namespace Slic3r {
namespace GUI {

#define LISTBOX_HEIGHT 40

class CustomComboBox : public wxPanel
{
private:
    wxStaticText    *m_staticText_name;
    wxStaticBitmap  *m_staticBitmap;
    wxString         m_name;

public:
    CustomComboBox(wxWindow* parent = nullptr, const wxString& name = wxEmptyString);

private:
    void initControl();
    void setCustomBoxSizer();
};

class ListBoxPopup : public PopupWindow
{
public:
    ListBoxPopup(wxWindow *parent, const wxArrayString& names);

    // PopupWindow virtual methods are all overridden to log them
    virtual void Popup(wxWindow *focus = NULL) wxOVERRIDE;
    virtual void OnDismiss() wxOVERRIDE;
    virtual bool ProcessLeftDown(wxMouseEvent &event) wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;

    bool was_dismiss() { return m_dismiss; }

    void resetSize(const wxSize &size);

private:
    bool         m_dismiss { false };
    wxListBox   *m_listBox;
};

class DeviceItemPanel : public wxPanel
{
public:
    struct DeviceInfo {
        bool wlanFlag {true};  // wlan or not
        int conn_id {-1};
        unsigned short pid;
        wxString name;
        wxString placement;
        wxString status;
    };
    DeviceItemPanel(wxWindow *parent, const DeviceInfo& info);

    void updateInfo(const DeviceInfo& info);
    const DeviceInfo& deviceInfo() const;

protected:
    void mouseDown(wxMouseEvent &event);
    void mouseReleased(wxMouseEvent &event);
    void onEnter(wxMouseEvent &event);
    void onLeave(wxMouseEvent &event);
    void onPaint(wxPaintEvent& event);

private:
    void build();
    void connectEvent();
    bool isPointIn(const wxPoint& pt);
    void sendEvent();
    void updateStatus();

    static wxBitmap machineBitmap(unsigned short pid);

private:
    bool            m_hovered {false};
    bool            m_pressed {false};
    DeviceInfo      m_info;
    wxStaticText*   m_name_text {nullptr};
    wxStaticBitmap* m_icon {nullptr};
    wxStaticText*   m_placement_text {nullptr};
    wxStaticText*   m_status_text {nullptr};
    wxBoxSizer*     m_main_sizer {nullptr};
    wxColour        m_bg_color = wxColour("#ffffff");
    wxColour        m_border_color = wxColour("#ffffff");
    wxColour        m_border_hover_color = wxColour("#999999");
    wxColour        m_border_press_color = wxColour("#328DFB");

    static std::map<unsigned short, wxBitmap> m_machineBitmapMap;
};


class DeviceListPanel : public wxPanel
{
public:
    DeviceListPanel(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, 
            const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString);
    ~DeviceListPanel();

    void msw_rescale();

private:
    void build();
    void initAllDeviceStatus(wxArrayString& names);
    void connectEvent();
    void initDeviceList();

    void onFocus(wxFocusEvent & event);
    void on_comboBox_position_clicked(wxMouseEvent &event);
    void on_comboBox_status_clicked(wxMouseEvent &event);
    void onModelBtnClicked(wxCommandEvent &event);
    void onComConnectionReady(ComConnectionReadyEvent& event);
    void onComConnectionExit(ComConnectionExitEvent& event);
    void onComDevDetailUpdate(ComDevDetailUpdateEvent& event);

private: 
    CustomComboBox* m_comboBox_position;
    ListBoxPopup*   m_listBox_position;
    CustomComboBox *m_comboBox_status;
    ListBoxPopup   *m_listBox_status;
    CustomComboBox *m_comboBox_type;
    wxButton*       m_btn_outer_net;
    wxButton*       m_btn_inner_net;
    wxButton*       m_btn_mode;
    
    wxSimplebook*   m_simple_book {nullptr};
    wxPanel*        m_no_device_panel {nullptr};
    wxStaticBitmap* m_no_device_bitmap {nullptr};
    wxStaticText*   m_no_device_staticText {nullptr};
    wxBoxSizer     *m_no_device_sizer {nullptr};
    wxPanel*        m_device_panel {nullptr};
    wxGridSizer*    m_device_sizer {nullptr};

    std::map<std::string, DeviceItemPanel*> m_device_map;
};

} // GUI
} // Slic3r

#endif /* slic3r_DeviceListPanel_hpp_ */
