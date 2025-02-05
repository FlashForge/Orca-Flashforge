#ifndef slic3r_GUI_SingleDeviceState_hpp_
#define slic3r_GUI_SingleDeviceState_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/panel.h>
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
#include "slic3r/GUI/Widgets/TempInput.hpp"
//#include "slic3r/GUI/Widgets/StaticLine.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/SelectMachine.hpp"
#include "MultiComDef.hpp"
#include "MultiComEvent.hpp"
#include "MaterialStation.hpp"
#include <mutex>

namespace Slic3r { 
namespace GUI {

wxDECLARE_EVENT(EVT_SWITCH_TO_FILETER, wxCommandEvent);

class ComAsyncThread;
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
    wxImage getImageByType(const std::string& type);
    std::string getImageNameByType(const std::string& type);

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
    void modifyVideoPlayerAddress(const std::string &urlAddress);
    void notifyWebDevOffline();
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

    wxBoxSizer *create_monitoring_page();
    wxBoxSizer* create_machine_control_title();
    wxBoxSizer *create_machine_control_page();
    void setupLayout();
    void setupLayoutBusyPage(wxBoxSizer* busySizer,wxPanel* parent);
    void setupLayoutIdlePage(wxBoxSizer* idleSizer,wxPanel* parent);

    void msw_rescale();
    void connectEvent(); 

    void onScriptMessage(wxWebViewEvent &evt);
    void on_navigated(wxWebViewEvent &event);
    void onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event);
    void onComDevDetailUpdate(ComDevDetailUpdateEvent &event);
    void onComConnectReady(ComConnectionReadyEvent& event);
    void onConnectExit(ComConnectionExitEvent &event);
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

    void setTipMessage(const wxString &title = "", const std::string &titleColor = "", const wxString &info = "", bool showInfo = false, bool showBtn = false);

protected:
    void onMouseLeftUp(wxMouseEvent& evt);

private:
    wxString convertSecondsToHMS(int totalSeconds);
    void  fillValue(const com_dev_data_t &data,bool wanDev = false);

    void  setPageOffline();
    std::string getCurLanguage();
    void  setMaterialPic(const com_dev_data_t& data);
    void  setTempurature(const com_dev_data_t& data);
    void  splitIdleTextLabel();
    void  clearFileList();

    void initFileList(const std::vector<FileItem::FileData>& fileDataList);
    void downloadFileListImage(FileItem& fileItem);
    void downloadModelImage(const std::string& url);
    void changeMachineType(unsigned short pid);

protected:
//data

    com_id_t m_cur_id = ComInvalidId;

//UI
    wxPanel* m_panel_monitoring_title{nullptr};
    Label*   m_staticText_monitoring{nullptr};

    wxString    m_camera_play_url;
    wxWebView*  m_browser = {nullptr};
    wxPanel*    m_machine_ctrl_panel{nullptr};
    wxPanel*    m_machine_idle_panel{nullptr};
    MaterialStation* m_material_station{nullptr};

    Label*      m_staticText_device_name{nullptr};
    Label*      m_staticText_device_position{nullptr};
    Label*      m_staticText_device_tip{nullptr};
    FFButton*   m_staticText_device_info{nullptr};
    Button*     m_clear_button{nullptr};

    wxStaticBitmap*     m_material_weight_staticbitmap{nullptr};
    MaterialImagePanel* m_material_picture{nullptr};
    wxImage*            m_material_image{nullptr};

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
    Label*              m_staticText_count_time{nullptr};
    Label*              m_staticText_time_label{nullptr};

    wxStaticText*       m_staticText_file_name{nullptr};

    ProgressBar*        m_progress_bar{nullptr};

    Label*              m_material_weight_label{nullptr};

    Button*             m_print_button{nullptr};
    Button*             m_cancel_button{nullptr};
    CancelPrint*        m_cancel_confirm_page{nullptr};

    bool                m_print_button_pressed_down = false;
    bool                m_clear_fan_pressed_down    = false;

//temperature 
    TempInput*          m_tempCtrl_top{nullptr}; // 喷头温度
    TempInput*          m_tempCtrl_bottom{nullptr}; // 平台温度
    TempInput*          m_tempCtrl_mid{nullptr};    // 腔体温度

    Button*             m_device_info_button{nullptr};
    Button*             m_lamp_control_button{nullptr};
    Button*             m_filter_button{nullptr};

//
    DeviceDetail*       m_busy_device_detial{nullptr}; // 忙碌状态，文件信息按钮
    StartFilter*        m_busy_circula_filter{nullptr}; // 忙碌状态，过滤按钮
    ModifyTemp*         m_busy_temp_brn{nullptr};     // 忙碌状态，温度修改确认按钮
    G3UDetail*          m_busy_G3U_detail{nullptr};

    TempMixDevice*      m_idle_tempMixDevice{nullptr}; // 空闲状态，温度设备控件
    //
    double              m_last_speed = 0.00001;
    double              m_last_z_axis_compensation = 0.00001;
    double              m_last_cooling_fan_speed   = 0.00001;
    double              m_last_left_cooling_fan_speed = 0.00001;
    double              m_last_chamber_fan_speed   = 0.00001;
    std::string         m_camera_stream_url;
    int                 m_pid = 0x0023;

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
    std::vector<std::shared_ptr<ComAsyncThread>> m_download_pic_thread;

    //wxSimplebook*       m_fileBook{nullptr};
    //wxPanel*            m_filePanel{nullptr};
    //wxScrolledWindow*   m_fileListWindow{nullptr};
    //wxPanel*            m_fileListPanel{nullptr};
    //wxGridSizer*        m_fileListSizer{nullptr};
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
    wxPanel*               m_FileList_split_line{nullptr};
    std::string            m_cur_pic;
    std::string            m_last_pic;

    std::mutex             m_mutex;
};



} // namespace GUI
} // namespace Slic3r

#endif
