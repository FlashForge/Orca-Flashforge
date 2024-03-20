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
#include "MultiComEvent.hpp"

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
    void setAirFilterState(bool internalOpen,bool externalOpen);
    void setCurId(int curId);

private:
    void onAirFilterToggled(wxCommandEvent &event);

private:
    SwitchButton* m_internal_circulate_switch;//内循环过滤
    SwitchButton* m_external_circulate_switch;//外循环过滤
    int  m_cur_id = -1;
};
wxDECLARE_EVENT(EVT_MODIFY_TEMP_CLICKED, wxCommandEvent);
wxDECLARE_EVENT(EVT_MODIFY_TEMP_CANCEL_CLICKED, wxCommandEvent);
class ModifyTemp : public wxPanel
{
public:
    ModifyTemp(wxWindow *parent);
    void create_panel(wxWindow *parent);
    void setLabel(wxString labelText);

private:
    wxStaticText *m_staticText_title{nullptr};
    FFButton     *m_cancel_btn{nullptr};
    FFButton     *m_confirm_btn{nullptr};
};

class DeviceDetail : public wxPanel
{
public:
    DeviceDetail(wxWindow* parent);
    //~DeviceDetail();
    void setCurId(int curId);
    void create_panel(wxWindow* parent);
    //void initData();
    void setMaterialName(wxString materialName);
    void setInitialSpeed(double initialSpeed);
    void setSpeed(double speed);
    void setZAxis(double value);
    void setLayer(int printLayer, int targetLayer);
    void setFillRate(double fillRate);
    void setCoolingFanSpeed(double fanSpeed);
    void setChamberFanSpeed(double fanSpeed);

private:
    IconText       *m_device_material{nullptr};
    IconText       *m_device_initial_speed{nullptr};
    IconText       *m_device_layer{nullptr};
    IconText       *m_device_fill_rate{nullptr};
    IconBottonText *m_device_speed{nullptr};
    IconBottonText *m_device_z_axis{nullptr};
    IconBottonText *m_device_nozzle_fan{nullptr};
    IconBottonText *m_device_cooling_fan{nullptr};

    int m_cur_id = -1;

};

class SingleDeviceState : public wxScrolledWindow
{
public:
    SingleDeviceState(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, 
            const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString);
    ~SingleDeviceState(){};

    void setCurId(int curId);
    void modifyVideoPlayerAddress(const std::string &urlAddress);
    void notifyWebDevOffline();
    void reInit();
    void reInitData();
    void reInitUI();

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

    void onScriptMessage(wxWebViewEvent &evt);
    void on_navigated(wxWebViewEvent &event);
    void onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event);
    void onComDevDetailUpdate(ComDevDetailUpdateEvent &event);
    void onConnectExit(ComConnectionExitEvent &event);
    void onTargetTempModify(wxCommandEvent &event);
    void onModifyTempClicked(wxCommandEvent &event);
    void onDevStateChanged(std::string devState, const com_dev_data_t &data);

    void setTipMessage(const std::string &title = "", const std::string &titleColor = "", const std::string &info = "", bool showInfo = false);

private:
    std::string convertSecondsToHMS(int totalSeconds);
    void  fillValue(const com_dev_data_t &data);

    std::string truncateString(const std::string &s, size_t length);
    void  setPageOffline();

protected:
//data

    int m_cur_id = -1;

//UI
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

    Label*   m_staticText_file_head;
    Label*   m_staticText_file_name;
//    Label*   m_staticText_device_state;
    Label*   m_staticText_count_time;
    Label*   m_staticText_time_label;
    ProgressBar* m_progress_bar;

    Label* m_material_weight_label;

    Button* m_print_button;
    Button* m_cancel_button;

    bool m_print_button_pressed_down = false;

//temperature 
    TempInput* m_tempCtrl_top;  //喷头温度
    TempInput* m_tempCtrl_bottom;   //平台温度
    TempInput* m_tempCtrl_mid;  //腔体温度

    Button* m_device_info_button;
    Button* m_lamp_control_button;
    Button* m_filter_button;

//
    wxPanel* m_panel_idle_device_info;//空闲状态，文件信息按钮
    StartFilter* m_panel_circula_filter; //空闲状态，过滤按钮

    DeviceDetail* m_busy_device_detial; //忙碌状态，文件信息按钮
    StartFilter*  m_busy_circula_filter;//忙碌状态，过滤按钮
    ModifyTemp   *m_busy_temp_brn;      //忙碌状态，温度修改确认按钮

    TempMixDevice* m_idle_tempMixDevice;//空闲状态，温度设备控件
//
    double m_last_speed = 0.00;
    double m_last_z_axis_compensation = 0.00;
    double m_last_cooling_fan_speed   = 0.00;
    double m_last_chamber_fan_speed   = 0.00;
    std::string m_camera_stream_url;
    int         m_pid = 0x0023;

    std::string      m_file_pic_url;
    std::vector<char> m_pic_data;
    std::vector<char> m_last_pic_data;
    wxPanel         *m_panel_control_material{nullptr};

    std::string     m_cur_dev_state;
    std::string     m_cur_print_file_name;
    std::string     m_cur_dev_name;
    std::string     m_cur_dev_location;
    int             m_cur_printing_ctrl;  // 0:normal, 1: pause 2: continue  3: abort
    wxBitmap       *m_bitmap{nullptr};

    double m_right_target_temp;
    double m_plat_target_temp;
};



} // namespace GUI
} // namespace Slic3r

#endif