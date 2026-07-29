#ifndef slic3r_GUI_SingleDeviceState_hpp_
#define slic3r_GUI_SingleDeviceState_hpp_

#include <ctime>
#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/timer.h>
#include "wx/webview.h"
#include <wx/simplebook.h>

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
#include "slic3r/GUI/Widgets/ScrolledWindow.hpp"
#include "slic3r/GUI/Widgets/NewTempInput.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/Widgets/FFScrollButton.hpp"
#include "slic3r/GUI/SelectMachine.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include "FFDownloadTool.hpp"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "MaterialStation.hpp"
#include "FFRTMPVideoCtrl.h"
#include "TimeLapseVideoPanel.hpp"
#include <mutex>
#include <functional>

namespace Slic3r { 
namespace GUI {

wxDECLARE_EVENT(EVT_SWITCH_TO_FILETER, wxCommandEvent);

class LampToolBar : public wxPanel
{
public:
    LampToolBar(wxWindow* parent);
    void lamp_btn_clicked(wxMouseEvent& event);
    void SetCurId(com_id_t curId);
    void BindCamera(FFRTMPVideoCtrl* camera, std::function<void()> onActivate = {});
    void SetLampState(bool isOffline, bool isOpen);
    void SetCameraVisible(bool visible);
    void SetCameraState(bool isOffline);

private:
    Button* m_lamp_btn{nullptr};
    Button* m_camera_btn{nullptr};
    com_id_t m_cur_id;
    FFRTMPVideoCtrl* m_camera{nullptr};
    // 点摄像头按钮时的回调：由 SingleDeviceState 提供，负责“重绑当前设备并切流 + 弹窗”。
    // 为空时退化为直接弹窗（仅显示已绑定设备）。
    std::function<void()> m_camera_activate_cb;
};

class MaterialImagePanel : public wxPanel
{
public:
    MaterialImagePanel(wxWindow *parent, const wxSize &size = wxDefaultSize);

    void SetImage(const wxImage &image);

private:
    void OnPaint(wxPaintEvent &event);
    void OnSize(wxSizeEvent &event);
    void CreateRegion(wxDC &dc);

private:
    wxImage m_image{wxNullImage};
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
    ~DeviceDetail() {};
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
    void setCoolingFanShow(bool hide);
    void setChamberFanSpeed(double fanSpeed);
    void updateGridSizer(int pid);
    void switchPage();

private:
    IconText       *m_device_material{nullptr};
    IconText       *m_device_initial_speed{nullptr};
    IconText       *m_device_layer{nullptr};
    IconText       *m_device_fill_rate{nullptr};
    IconBottonText *m_device_speed{nullptr};
    IconBottonText *m_device_z_axis{nullptr};
    IconBottonText *m_device_nozzle_fan{nullptr};
    IconBottonText *m_device_cooling_fan{nullptr};
    wxGridSizer*    m_grid_sizer{nullptr};
    wxPanel*        m_panel_rows{nullptr};
    MachineIconCombo* m_device_speed_u1{nullptr};

    int m_cur_id = -1;

};
class G3UDetail : public wxPanel
{
public:
    G3UDetail(wxWindow* parent);
    ~G3UDetail(){};
    void setCurId(int curId);
    void create_panel(wxWindow* parent);
    void setMaterialName(wxString materialName);
    void setRightMaterialName(wxString materialName);
    void setInitialSpeed(double initialSpeed);
    void setSpeed(double speed);
    void setZAxis(double value);
    void setLayer(int printLayer, int targetLayer);
    void setFillRate(double fillRate);
    void setCoolingFanSpeed(double fanSpeed);
    void setLeftCoolingFanSpeed(double leftFanSpeed);
    void setChamberFanSpeed(double fanSpeed);
    void switchPage();

private:
    IconText*       m_device_material{nullptr};
    wxStaticText*   m_rightMaterial{nullptr};
    IconText*       m_device_initial_speed{nullptr};
    IconText*       m_device_layer{nullptr};
    IconText*       m_device_fill_rate{nullptr};
    IconBottonText* m_device_speed{nullptr};
    IconBottonText* m_device_z_axis{nullptr};
    IconBottonText* m_device_cooling_fan{nullptr};
    IconBottonText* m_device_right_nozzle_fan{nullptr};
    IconBottonText* m_device_left_nozzle_fan{nullptr};

    int m_cur_id = -1;
};

wxDECLARE_EVENT(EVT_FILE_ITEM_CLICKED, wxCommandEvent);
class FileItem : public wxPanel
{
public:
    struct FileData
    {
        com_gcode_data_t gcodeData;
        wxString wxName;
        int      commandId; // use for download lan network pic
        wxImage  srcImage;
        wxImage  scaledImage;
        
        FileData() = default;
        FileData(const FileData& data) = default;
    };

public:
    FileItem(wxWindow* parent, const FileData& data);
    ~FileItem(){};

    bool IsPressed() const;
    void SetPressed(bool pressed);

protected:
    void OnPaint(wxPaintEvent& event);
    void render(wxDC& dc);
    void doRender(wxDC& dc);
    void on_mouse_enter(wxMouseEvent& evt);
    void on_mouse_leave(wxMouseEvent& evt);
    void on_mouse_left_up(wxMouseEvent& evt);

private:
    void create_panel(wxWindow* parent);

public:
    static wxImage getImageByType(const std::string& type);
    static std::string getImageNameByType(const std::string& type);

protected:
    bool        m_hovered{false}; 
    bool        m_pressed{false};
    bool        m_blockFlag{false};
    wxColour    m_bg_color           = wxColour("#D9EAFF");
    wxColour    m_border_presse_color= wxColour("#D9EAFF"); 
    wxColour    m_border_hover_color = wxColour("#328DFB");

public:
    FileData m_data;
    bool     m_remove{false};
    bool     m_finish_download{false};

private:
    wxPanel*        m_iconPanel;
    wxBoxSizer*     m_iconSizer;
    ThumbnailPanel* m_thumbnailPanel;
    wxStaticText*   m_fileLbl;
    wxStaticText*   m_nameLbl;
    wxBoxSizer*     m_mainSizer;
};

class SingleDeviceState : public wxScrolledWindow
{
public:
    SingleDeviceState(wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, 
            const wxSize& size = wxDefaultSize, long style = wxTAB_TRAVERSAL, const wxString& name = wxEmptyString);
    ~SingleDeviceState();

    void setCurId(int curId);
    void reInit();
    void reInitData();
    void reInitUI();
    void reInitMaterialPic();
    void reInitPage();
    void setDevProductAuthority(const fnet_dev_product_t &data);
    void setG3UProductAuthority(const fnet_dev_product_t& data);
    void reInitProductState();
    std::string getCurDevSerialNumber();
    void lostFocusmodifyTemp();
    void        openPrintCheck();
    void        setDisabledExtruderCtrl(bool b);

    wxBoxSizer* create_machine_status_page();
    wxBoxSizer* create_machine_info_page();
    wxBoxSizer *create_monitoring_page(wxPanel* parent);
    wxBoxSizer* create_machine_control_title();
    wxBoxSizer *create_machine_control_page();
    void showMaterialStation(bool show);
    void trackBtnClick(std::string str);
    void setupLayout();
    void setupLayoutBusyInfoPage(wxBoxSizer* busySizer,wxPanel* parent);
    void setupLayoutIdleInfoPage(wxBoxSizer* idleSizer,wxPanel* parent);
    void        setupLayoutBusyCtrlPage(wxBoxSizer* busySizer, wxPanel* parent);
    void        setupLayoutIdleCtrlPage(wxBoxSizer* idleSizer, wxPanel* parent);

    void msw_rescale();
    void connectEvent();
    void UpdateScrollVirtualSize();
 

private:
    void OnScrollWinSize(wxSizeEvent& evt);
    void onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent& event);
    void onComDevDetailUpdate(ComDevDetailUpdateEvent &event);
    void onComConnectReady(ComConnectionReadyEvent& event);
    void onConnectExit(ComConnectionExitEvent &event);
    // 账号登录/登出维护事件：登出(login=false)时关闭视频弹窗、停流并解绑摄像头。
    void onComWanDevMaintain(ComWanDevMaintainEvent &event);
    void onComCloudSliceUpdate(ComCloudSliceUpdateEvent& event);
    void onComJobInfoUpdate(ComJobInfoUpdateEvent& event);
    void onTargetTempModify(wxCommandEvent &event);
    void onModifyTempClicked(wxCommandEvent &event);
    void onDevStateChanged(std::string devState, const com_dev_data_t &data);
    void onCancelPrint(wxCommandEvent &event);
    void onContinuePrint(wxCommandEvent &event);
    void onFileListClicked(wxMouseEvent& event);
    void onFileListRefreshBtnClicked(wxMouseEvent& event);
    void onFileListUpdate(ComGetDevGcodeListEvent& event);
    void onFileListPrintBtnClicked(wxMouseEvent& event);
    void onFileSendFinished(ComStartJobEvent& event);
    void onLanThumbDownloadFinished(ComGetGcodeThumbEvent& event);
    void onTimeLapseVideoBtnClicked(wxMouseEvent& event);
    void onDownloadImageFinished(FFDownloadFinishedEvent& event);

public:
    void setTipMessage(const wxString &title = "", const std::string &titleColor = "", const wxString &info = "", bool showInfo = false, bool showBtn = false);
    void checkPrinterStatus();

private:
    wxString convertSecondsToHMS(int totalSeconds);
    void     updateDeviceInfoPanel(const com_dev_data_t& data);
    void     fillValue(const com_dev_data_t& data, bool wanDev = false);
    void     fillCloudValue(const fnet_slice_state_t& data);
    void fillJobValue(const fnet_job_info_t& info);

    void  setPageOffline();
    // 按“摄像头绑定设备”(m_camera_cur_id) 刷新视频流：拉流/断线/占位，均与页面选中设备解耦。
    void  refreshCameraStream();
    // 把摄像头视频流切换到指定设备：关闭旧设备(WAN)推流、重绑并立即按新设备拉流。
    void  switchCameraStreamTo(int comId);
    // 点摄像头按钮：把摄像头绑定到当前选中设备(m_cur_id)并切流，然后弹窗显示。
    void  activateCameraForCurrentDevice();
    std::string getCurLanguage();
    void  setMaterialName(const std::string& printFileName);
    void  setMaterialPic(const com_dev_data_t& data);
    void  setTempurature(const com_dev_data_t& data);
    void  setIdlePrinterText(bool isOffline = false);
    void  splitIdleTextLabel();
    void  clearFileList();
    void initFileList(const std::vector<FileItem::FileData>& fileDataList);
    void changeMachineType(unsigned short pid);

protected:
//data

    com_id_t m_cur_id = ComInvalidId;

//UI
    LampToolBar* m_idle_lamp_bar{nullptr};
    LampToolBar* m_busy_lamp_bar{nullptr};
    wxPanel* m_panel_monitoring_title{nullptr};
    Label*   m_staticText_monitoring{nullptr};
    NewTempInputPanel* m_tempCtrl_panel{ nullptr };

    FFRTMPVideoCtrl* m_camera_panel{nullptr};

    wxPanel*         m_machine_ctrl_info_panel{nullptr};
    wxPanel*         m_machine_idle_info_panel{nullptr};
    wxPanel*         m_machine_ctrl_panel{nullptr};
    wxPanel*         m_machine_idle_panel{nullptr};
    wxPanel*         m_monitor_panel{nullptr};
    MaterialStation* m_material_station{nullptr};
    FFNozzles*       m_nozzles{nullptr};

    Label*          m_staticText_device_name{nullptr};
    Label*          m_staticText_device_position{nullptr};
    Label*          m_staticText_device_tip{nullptr};
    FFScrollButton* m_staticText_device_info{nullptr};
    Button*         m_clear_button{nullptr};

    wxStaticBitmap* m_print_check_bmp{ nullptr };
    Label* m_print_check_label{ nullptr };
    wxStaticBitmap*     m_material_weight_staticbitmap{nullptr};
    MaterialImagePanel* m_material_picture{nullptr};

    wxPanel*            m_panel_idle{nullptr};
    wxStaticBitmap*     m_idle_device_staticbitmap{nullptr};
    Label*              m_staticText_idle{nullptr};

    wxPanel*            m_panel_idle_text{nullptr};
    wxBitmap            m_idle_file_list_pic;
    wxStaticBitmap*     m_idle_file_list_staticbitmap{nullptr};
    Label*              m_staticText_file_list{nullptr};

    wxPanel*            m_panel_control_info{nullptr};

    Label*              m_staticText_file_head{nullptr};
    //Label*              m_staticText_file_name{nullptr};
    Label*              m_staticText_cloud_text{nullptr};
    Label*              m_staticText_cloud_queue_text{nullptr};
    Label*              m_staticText_cloud_queue_count{nullptr};
    Label*              m_staticText_cloud_queue_tip{nullptr};
    Label*              m_staticText_count_time{nullptr};
    Label*              m_staticText_time_label{nullptr};

    wxStaticText*       m_staticText_file_name{nullptr};

    ProgressBar*        m_progress_bar{nullptr};

    Label*              m_material_weight_label{nullptr};

    Button*             m_print_button{nullptr};
    Button*             m_cancel_button{nullptr};
    CancelPrint*        m_cancel_confirm_page{nullptr};
    Button*             m_cancel_queue_button{nullptr};
    Button*             m_retry_print_button{nullptr};
    Button*             m_cancel_slice_button{nullptr};
    wxPanel*            m_panel_control_cloud{nullptr};
    wxPanel*            m_panel_control_print{nullptr};
    std::string         m_slice_task_id;

    bool                m_print_button_pressed_down = false;
    bool                m_clear_fan_pressed_down    = false;

//temperature 
    //TempInput*          m_tempCtrl_top{nullptr}; // 喷头温度
    //TempInput*          m_tempCtrl_bottom{nullptr}; // 平台温度
    //TempInput*          m_tempCtrl_mid{nullptr};    // 腔体温度

    Button*             m_device_info_button{nullptr};
    Button*             m_lamp_control_button{nullptr};
    Button*             m_filter_button{nullptr};
    Button*             m_idle_device_info_button{nullptr};

    //
    DeviceDetail*       m_busy_device_detial{nullptr}; // 忙碌状态，文件信息按钮
    StartFilter*        m_busy_circula_filter{nullptr}; // 忙碌状态，过滤按钮
    ModifyTemp*         m_busy_temp_brn{nullptr};     // 忙碌状态，温度修改确认按钮
    G3UDetail*          m_busy_G3U_detail{nullptr};
    DeviceInfoPanel*            m_busy_device_info{nullptr};

    TempMixDevice*      m_idle_tempMixDevice{nullptr}; // 空闲状态，温度设备控件
    //
    double              m_last_speed = 0.00001;
    double              m_last_z_axis_compensation = 0.00001;
    double              m_last_cooling_fan_speed   = 0.00001;
    double              m_last_left_cooling_fan_speed = 0.00001;
    double              m_last_chamber_fan_speed   = 0.00001;
    // 摄像头“绑定设备”正在播放的拉流地址（跟随 m_camera_cur_id，与页面选中设备解耦）。
    std::string         m_camera_stream_url;
    // 摄像头“绑定设备”的 id（与整页 m_cur_id 解耦）：视频画面始终跟随此设备。
    // 首次选中设备时自动绑定并后台拉流；之后切换设备不改变绑定（画面保持旧设备），
    // 只有用户点摄像头按钮才重绑为当前选中设备并切流（activateCameraForCurrentDevice）。
    int                 m_camera_cur_id = -1;
    int                 m_pid = OTHER;

    std::string         m_file_pic_url;
    std::string         m_file_pic_name;
    wxPanel*            m_panel_control_material{nullptr};

    std::string         m_cur_dev_state;
    std::string         m_cur_print_file_name;
    std::string         m_cur_dev_name;
    std::string         m_cur_dev_location;
    int                 m_cur_printing_ctrl;  // 0:normal, 1: pause 2: continue  3: abort
    wxBitmap*           m_bitmap{nullptr};

    double              m_right_target_temp;
    double              m_plat_target_temp;
    double              m_chamber_target_temp;
    std::string         m_cur_serial_number;

    std::vector<FileItem*> m_fileItemList;
    FFButton*           m_printBtn{nullptr};
    Button*             m_refreshBtn{nullptr};
    Button*             m_fileListbutton{nullptr};
    wxPanel*            m_panel_separotor8{nullptr};
    wxPanel*            m_panel_print_btn{nullptr};

    wxBoxSizer*         m_sizer_my_devices{nullptr};
    wxScrolledWindow*   m_scrolledWindow{nullptr};
    FileItem*           m_curSelectedFileItem{nullptr};
    std::vector<wxWindow*> m_idleWnd;
    bool                   m_curId_first_Click_fileList = true;
    wxPanel*               m_busyState_top_gap{nullptr};
    wxPanel*               m_busyState_bottom_gap{nullptr};
    wxPanel*               m_offline_info_page_gap{nullptr};
    wxPanel*               m_FileList_split_line{nullptr};
    wxPanel*               m_extruderSperator{nullptr};
    wxPanel*               m_extruderSperator1{nullptr};
    wxPanel*               m_extruderLine{nullptr};
    wxBoxSizer*            m_vSizer3{nullptr};
    wxPanel*               m_plateSperator{nullptr};

    Button*                m_timeLapseVideoBtn;
    TimeLapseVideoPanel*   m_timeLapseVideoPnl;

    wxTimer                m_check_printer_status_timer;
    bool                   m_block_status_check{false};
    time_t                 m_status_check_message_show_time{0};
    std::string            m_status_check_error_code;

    FFDownloadTool           m_download_tool;
    int                      m_download_title_image_task_id;
    std::map<int, FileItem*> m_download_file_list_image_map;

    bool m_isNozzlesPrinter{false};
    bool m_isCloudState{false};
};

} // namespace GUI
} // namespace Slic3r

#endif
