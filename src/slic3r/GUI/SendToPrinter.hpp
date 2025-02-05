#ifndef slic3r_GUI_SendToSDcard_hpp_
#define slic3r_GUI_SendToSDcard_hpp_
#include <map>
#include <vector>
#include <string>
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

#include "SelectMachine.hpp"
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
#include "Widgets/FFCheckBox.hpp"
#include "Widgets/FFButton.hpp"
#include "Widgets/FFToggleButton.hpp"
#include "Widgets/ProgressBar.hpp"
#include "FlashForge/MultiComMgr.hpp"
#include <wx/simplebook.h>
#include <wx/hashmap.h>
#include <wx/event.h>
#include "TitleDialog.hpp"
#include "FlashForge/MultiComDef.hpp"
#include "MsgDialog.hpp"

namespace Slic3r {
namespace GUI {

//class ExportSliceJob; //by ymd
class MultiSend : public wxEvtHandler
{
public:
    enum Result {
        Result_Ok,
        Result_Fail,
        Result_Fail_Busy,
        Result_Fail_Canceled,
        Result_Fail_Network,
    };

public:
    MultiSend(wxWindow* event_handler, int sync_num = 5);
    ~MultiSend();

    bool send_to_printer(int plate_idx, const com_id_list_t& com_ids, const std::string& job_name, bool send_and_print, bool leveling);
    void cancel();
    const com_id_list_t& com_ids() const { return m_com_ids; }
    
    bool get_multi_send_result(std::map<com_id_t, Result>& result);
    void reset();

private:
    bool prepare();
    void bind_com_event(bool bind);
    void remove_temp_path();
    bool export_temp_file();
    void cancel_export_job();
    void send_next_job();
    void send_wan_job(const std::map<std::string, com_id_t>& com_ids);
    void do_send_next_job();
    void update_progress();
    Result convert_return_value(ComErrno error);
    Result convert_wan_error_value(int error);
    void send_event(int code, const wxString& msg);
    void on_cnnection_exit(ComConnectionExitEvent& event);
    void on_send_gcode_finished(ComSendGcodeFinishEvent& event);
    void on_send_gcode_progress(ComSendGcodeProgressEvent& event);
    void on_export_slice_completed(wxCommandEvent& event);

private:
    struct ResultInfo {
        int         cmd_id {ComInvalidId};
        bool        wan_flag {false};
        bool        finish {false};
        Result      result {Result_Ok};
        double      progress {0};
    };
    bool            m_is_sending {false};
    bool            m_send_and_print {false};
    bool            m_leveling {false};
    int             m_plate_idx {-1};
    int             m_sync_num {5};
    wxWindow*       m_event_handler {nullptr};
    std::string     m_slice_path;
    std::string     m_thumb_path;
    std::string     m_slice_job_name;
    com_id_list_t   m_com_ids;
    std::map<std::string, com_id_t> m_wan_ids_to_send;  // devId, com_id pair
    double                          m_wan_progress {0};
    double                          m_pre_batch_progress {0.0};
    std::deque<com_id_t>            m_lan_ids_to_send;
    std::map<com_id_t, ResultInfo>  m_send_jobs;
    //std::shared_ptr<ExportSliceJob> m_export_job; //by ymd
};
wxDECLARE_EVENT(EVT_MULTI_SEND_COMPLETED, wxCommandEvent);
wxDECLARE_EVENT(EVT_MULTI_SEND_PROGRESS, wxCommandEvent);


class SendToPrinterTipDialog : public TitleDialog
{
public:
    SendToPrinterTipDialog(wxWindow* parent, const wxStringList& success, const wxStringList& fail, const wxSize &size = wxDefaultSize);

    void on_dpi_changed(const wxRect& suggested_rect) override {};

private:
    wxPanel* createItem(wxWindow* parent, bool success, const wxString& name, int width);
    wxPanel* createListPanel(wxWindow* parent, const wxStringList& str_list, bool success_flag);
};


class MachineItem : public wxPanel
{
public:
    struct MachineData
    {
        int         flag;   // 0 wlan, 1 lan
        //std::string model;
        int         pid;
        wxString    name;
        com_id_t    comId;

        MachineData() = default;
        MachineData(const MachineData& data) = default;//: flag(data.flag), model(data.model), name(data.name) {};
    };

public:
    MachineItem(wxWindow* parent, const MachineData& data);
    ~MachineItem() {};

    const MachineData& data() const;
    bool IsChecked() const;
    void SetChecked(bool checked);
    void SetDefaultColor(const wxColor& color);
    
private:
    static void initBitmap();
    void build();

private:
    wxColour		m_defaultColor { wxColour(255, 255, 255) };
    FFCheckBox*     m_checkBox;
    wxPanel*        m_iconPanel;
    wxBoxSizer*     m_iconSizer;
    ThumbnailPanel*	m_thumbnailPanel;
    wxStaticText*   m_nameLbl;
    wxBoxSizer*     m_mainSizer;
    MachineData     m_data;
    static std::map<int, wxImage> m_machineBitmapMap;
};


class SendToPrinterDialog : public TitleDialog//public DPIDialog
{
private:
    enum SendResultType
    {
        Result_None = -1,
        Result_Ok,
        Result_Fail,
        Result_Fail_Busy,
        Result_Fail_Canceled,
    };

    struct SendJobInfo {
        int             cmdId;
        SendResultType  result;
        double          progress;
    };

private:
	int									m_print_plate_idx{0};
    int									m_current_filament_id{0};
    int                                 m_print_error_code{0};
    bool								m_is_in_sending_mode{ false };
    bool								m_is_rename_mode{ false };
    bool								enable_prepare_mode{ true };
    bool								m_need_adaptation_screen{ false };
    bool								m_export_3mf_cancel{ false };
    bool								m_is_canceled{ false };
    bool                                m_send_and_print { false };
    bool                                m_need_redirect {false};
    std::string                         m_print_error_msg;
    std::string                         m_print_error_extra;
    std::string							m_print_info;
	std::string							m_printer_last_select;
    wxString							m_current_project_name;

	Plater*								m_plater{ nullptr };
    wxPanel*                            m_topPanel {nullptr};
    wxBoxSizer*							m_topSizer {nullptr};
    wxPanel*                            m_imagePanel {nullptr};
	wxStaticBitmap*						m_staticbitmap{ nullptr };
	ThumbnailPanel*						m_thumbnailPanel{ nullptr };
    wxStaticText*                       m_stext_weight {nullptr};
    wxStaticText*                       m_stext_time = {nullptr};
    wxBoxSizer*							rename_sizer_v{ nullptr };
    wxBoxSizer*							rename_sizer_h{ nullptr };
	wxBoxSizer*							sizer_thumbnail {nullptr};
    TextInput*							m_rename_input{ nullptr };
    wxSimplebook*						m_rename_switch_panel{ nullptr };
    wxPanel*                            m_renamePanel {nullptr};
    wxStaticText*                       m_renameText { nullptr };
    Button*                             m_renameBtn {nullptr};
    
    std::vector<FilamentInfo>           m_filaments;
    MaterialHash                        m_materialList;
    wxGridSizer*                        m_sizer_material{ nullptr };
    wxPanel*                            m_material_panel{nullptr};
	wxBoxSizer*							m_sizer_main {nullptr};
	wxStaticText*						m_file_name {nullptr};
    PrintDialogStatus					m_print_status{ PrintStatusInit };
    FFCheckBox*                         m_levelCkb {nullptr};
    wxStaticText*                       m_levelLbl {nullptr};
    wxStaticText*                       m_selectPrinterLbl;
    FFToggleButton*                     m_wlanBtn {nullptr};
    FFToggleButton*                     m_lanBtn {nullptr};
    wxSimplebook*                       m_machineBook {nullptr};
    wxPanel*                            m_machinePanel {nullptr};
    wxBoxSizer*                         m_machineSizer {nullptr};
    FFCheckBox*                         m_selectAll {nullptr};
    wxStaticText*                       m_selectAllLbl {nullptr};
    wxPanel*                            m_machineListPanel {nullptr};
    wxGridSizer*                        m_machineListSizer {nullptr};
    wxScrolledWindow*                   m_machineListWindow {nullptr};
    wxPanel*                            m_machineLine {nullptr};
    wxPanel*                            m_noMachinePanel {nullptr};
    wxStaticBitmap*                     m_noMachineBitmap {nullptr};
    wxStaticText*                       m_noMachineText {nullptr};
    wxSimplebook*                       m_sendBook { nullptr };
    wxPanel*                            m_sendPanel { nullptr };
    wxPanel*                            m_errorPanel { nullptr };
    wxStaticBitmap*                     m_errorBitmap {nullptr};
    wxStaticText*                       m_errorText {nullptr};
    FFButton*                           m_sendBtn {nullptr};
    wxPanel*                            m_progressPanel {nullptr};
    ProgressBar*                        m_progressBar {nullptr};
    wxStaticText*                       m_progressInfoLbl {nullptr};
    wxStaticText*                       m_progressLbl {nullptr};
    FFButton*                           m_progressCancelBtn {nullptr};

    std::map<std::string, MachineItem::MachineData> m_machineListMap;
    std::vector<MachineItem*>           m_machineItemList;
    std::shared_ptr<MultiSend>          m_multiSend;

    wxColour							m_colour_def_color{ wxColour(255, 255, 255) };
    wxColour							m_colour_bold_color{ wxColour(38, 46, 48) };
    wxTimer*                            m_redirect_timer {nullptr};
    MessageDialog*                      m_msg_window {nullptr};
    bool                                m_send_error {false};
   
public:
	SendToPrinterDialog(Plater* plater = nullptr);
    ~SendToPrinterDialog();

    bool Show(bool show);
    void on_rename_click(wxCommandEvent& event);
    void on_rename_enter();
    void update_user_printer();
    void prepare(int print_plate_idx, bool send_and_print);
    void check_focus(wxWindow* window);
    void check_fcous_state(wxWindow* window);
    void update_priner_status_msg(wxString msg, bool is_warning = false);
    void update_print_status_msg(wxString msg, bool is_warning = false, bool is_printer = true);
	void update_printer_list(wxCommandEvent& event);
	void set_default();
	void on_dpi_changed(const wxRect& suggested_rect) override;
    void update_user_machine_list();
    void update_print_error_info(int code, std::string msg, std::string extra);
    void on_change_color_mode() { wxGetApp().UpdateDlgDarkUI(this); }
    wxString format_text(wxString& m_msg);
	std::vector<std::string> sort_string(std::vector<std::string> strArray);
    void set_progress_info(const wxString& msg);

private:
	void init_bind();
    void updateVisible();
    void updateSendButtonState();
    void clear_machine_list();
    void redirect_window();
    void on_close(wxCloseEvent& event);
    void on_size(wxSizeEvent& event);
    void onNetworkTypeToggled(wxCommandEvent& event);
    void onMachineSelectionToggled(wxCommandEvent& event);
    void onSendClicked(wxCommandEvent& event);
    void on_cancel(wxCommandEvent& event);
    void onConnectionReady(ComConnectionReadyEvent& event);
    void onConnectionExit(ComConnectionExitEvent& event);
    void on_multi_send_progress(wxCommandEvent& event);
    void on_multi_send_completed(wxCommandEvent& event);
    void on_redirect_timer(wxTimerEvent &event);
    void onLevellingCheckBoxChanged(wxCommandEvent& event);

    std::vector<std::pair<std::string, MachineItem::MachineData>> sortByName(const std::map<std::string, MachineItem::MachineData>& devList);
};

}
}

#endif
