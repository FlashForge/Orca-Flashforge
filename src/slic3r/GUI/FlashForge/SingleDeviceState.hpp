#ifndef slic3r_GUI_SingleDeviceState_hpp_
#define slic3r_GUI_SingleDeviceState_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include "wx/webview.h"

#if wxUSE_WEBVIEW_EDGE
#include "wx/msw/webview_edge.h"
#endif

#include "slic3r/GUI/GUI_Utils.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/SwitchButton.hpp"
#include "slic3r/GUI/wxMediaCtrl2.h"
#include "slic3r/GUI/MediaPlayCtrl.h"
#include "slic3r/GUI/Widgets/ProgressBar.hpp"
#include "slic3r/GUI/Widgets/TempInput.hpp"
//#include "slic3r/GUI/Widgets/StaticLine.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"

namespace Slic3r { 
namespace GUI {

class MaterialPanel : public wxPanel
{
public:
    MaterialPanel(wxWindow* parent);
    ~MaterialPanel();
    void create_panel(wxWindow* parent);

private:
    wxPanel* m_panel_printing_title;
    wxStaticText*  m_staticText_printing;
    wxStaticText*   m_staticText_subtask_value;
};

class StartFilter : public wxPanel
{
public:
    StartFilter(wxWindow* parent);
    ~StartFilter();
    void create_panel(wxWindow* parent);
private:
    SwitchButton* m_internal_circulate_switch;//内循环过滤
    SwitchButton* m_external_circulate_switch;//外循环过滤
};

class DeviceDetail : public wxPanel
{
public:
    DeviceDetail(wxWindow* parent);
    //~DeviceDetail();
    void create_panel(wxWindow* parent);
    //void initData();
private:


};

class SingleDeviceState : public wxScrolledWindow
{
public:
    SingleDeviceState(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, 
            const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString);
    //~SingleDeviceState();

    wxBoxSizer* create_top_device_info_page();
    wxBoxSizer *create_monitoring_page();
    wxBoxSizer* create_machine_control_title();
    wxBoxSizer *create_machine_control_page();
    void setupLayout();
    void setupLayoutBusyPage(wxBoxSizer* busySizer,wxPanel* parent);
    void setupLayoutIdlePage(wxBoxSizer* idleSizer,wxPanel* parent);

    void setupLayoutIdleDeviceState(wxBoxSizer *deviceStateSizer, wxPanel *parent);
    void setupLayoutDeviceInfo(wxBoxSizer *deviceStateSizer, wxPanel *parent);

    void msw_rescale();
    void connectEvent();

protected:
    wxPanel*  m_panel_monitoring_title;
    Label*   m_staticText_monitoring;

    wxString   m_camera_play_url;
    wxWebView* m_browser = {nullptr};
    wxPanel*       m_machine_ctrl_panel;
    wxPanel*       m_machine_idle_panel;
    wxPanel*       m_panel_separator_right;
    wxPanel*       m_panel_separotor_bottom;
    MaterialPanel* m_material_panel;

    wxPanel*       m_panel_control_title;
    Label *        m_staticText_control;

    wxPanel*       m_panel_control_title2;
    Label*         m_staticText_control2;

    wxPanel* m_panel_top_title;
    Label*   m_staticText_device_name;
    Label*   m_staticText_device_position;
    Label*   m_staticText_device_tip;
    Label*   m_staticText_device_info;
    Button*  m_clear_button;

    wxBitmap    m_material_weight_pic;
    wxBitmap    m_material_pic;
    wxStaticBitmap*  m_material_weight_staticbitmap;
    wxStaticBitmap*  m_material_staticbitmap;

    wxPanel*    m_panel_idle;
    wxBitmap    m_idle_device_pic;
    wxStaticBitmap*  m_idle_device_staticbitmap;
    Label*      m_staticText_idle;

    wxPanel*    m_panel_idle_text;
    wxBitmap    m_idle_file_list_pic;
    wxStaticBitmap*  m_idle_file_list_staticbitmap;
    Label*      m_staticText_file_list;

    wxPanel *m_panel_idle_device_state;
    wxPanel *m_panel_idle_file_list;
    Button  *m_idle_device_info_button;
    Button  *m_idle_lamp_control_button;
    Button  *m_idle_filter_button;


    wxPanel*       m_panel_control_info;
    wxPanel*       m_panel_separator_top_title_right;
    wxPanel*       m_panel_top_right_info;

    Label*   m_staticText_file_name;
    Label*   m_staticText_device_state;
    Label*   m_staticText_count_time;
    Label*   m_staticText_time_label;
    ProgressBar* m_progress_bar;

    Label* m_material_weight_label;

    Button* m_print_button;
    Button* m_cancel_button;

//temperature 
    TempInput* m_tempCtrl_top;
    TempInput* m_tempCtrl_bottom;
    TempInput* m_tempCtrl_mid;

    Button* m_device_info_button;
    Button* m_lamp_control_button;
    Button* m_filter_button;

//
    wxPanel* m_panel_idle_device_info;//空闲状态，文件信息按钮
    StartFilter* m_panel_circula_filter; //空闲状态，过滤按钮

    DeviceDetail* m_busy_device_detial; //忙碌状态，文件信息按钮
    StartFilter*  m_busy_circula_filter;//忙碌状态，过滤按钮

    TempMixDevice* m_idle_tempMixDevice;//空闲状态，温度设备控件
};



} // namespace GUI
} // namespace Slic3r

#endif