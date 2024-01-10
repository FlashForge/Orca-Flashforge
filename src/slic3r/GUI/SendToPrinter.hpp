#ifndef slic3r_GUI_SendToSDcard_hpp_
#define slic3r_GUI_SendToSDcard_hpp_

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
#include "TitleDialog.hpp"

namespace Slic3r {
namespace GUI {

class MachineItem : public wxPanel
{
public:
    struct MachineData
    {
        int         flag;   // 0 network, 1 lan
        std::string model;
        wxString    name;

        MachineData() = default;
        MachineData(const MachineData& data) = default;//: flag(data.flag), model(data.model), name(data.name) {};
    };

public:
    MachineItem(wxWindow* parent, const MachineData& data);
    ~MachineItem() {};

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
    static std::map<std::string, wxImage> m_machineBitmapMap;
};

class SendToPrinterDialog : public TitleDialog//public DPIDialog
{
private:
	void init_bind();
	void init_timer();

	int									m_print_plate_idx;
    int									m_current_filament_id;
    int                                 m_print_error_code;
    int									timeout_count = 0;
    bool								m_is_in_sending_mode{ false };
    bool								m_is_rename_mode{ false };
    bool								enable_prepare_mode{ true };
    bool								m_need_adaptation_screen{ false };
    bool								m_export_3mf_cancel{ false };
    bool								m_is_canceled{ false };
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
	wxBoxSizer*							sizer_thumbnail;
    TextInput*							m_rename_input{ nullptr };
    wxSimplebook*						m_rename_switch_panel{ nullptr };
    wxPanel*                            m_renamePanel;
    wxStaticText*                       m_renameText { nullptr };
    Button*                             m_renameBtn {nullptr};

	wxBoxSizer*							m_sizer_main;
	wxStaticText*						m_file_name;
    PrintDialogStatus					m_print_status{ PrintStatusInit };
    FFCheckBox*                         m_levelCkb;
    wxStaticText*                       m_levelLbl;
    wxStaticText*                       m_selectPrinterLbl;
    FFToggleButton*                     m_netBtn;
    FFToggleButton*                     m_lanBtn;
    FFCheckBox*                         m_selectAll;
    wxStaticText*                       m_selectAllLbl;
    wxPanel*                            m_machinePanel {nullptr};
    wxBoxSizer*                         m_machineSizer {nullptr};
    wxScrolledWindow*                   m_machineListWindow;
    wxGridSizer*                        m_machineListSizer;
    wxPanel*                            m_machineLine {nullptr};
    wxPanel*                            m_errorMsgPanel {nullptr};
    wxStaticText*                       m_errorMsgLbl {nullptr};
    wxPanel*                            m_progressPanel {nullptr};
    ProgressBar*                        m_progressBar {nullptr};
    wxStaticText*                       m_progressInfoLbl {nullptr};
    wxStaticText*                       m_progressLbl {nullptr};
    FFButton*                           m_progressCancelBtn {nullptr};
    FFButton*                           m_sendBtn {nullptr};

    std::shared_ptr<SendJob>			m_send_job{nullptr};
    std::vector<wxString>               m_bedtype_list;
    std::map<std::string, ::CheckBox*>	m_checkbox_list;
    std::vector<MachineObject*>			m_list;
    std::vector<MachineItem::MachineData> m_machineList;
    wxColour							m_colour_def_color{ wxColour(255, 255, 255) };
    wxColour							m_colour_bold_color{ wxColour(38, 46, 48) };
	wxTimer*							m_refresh_timer{ nullptr };
    //std::shared_ptr<BBLStatusBarSend>   m_status_bar;
	wxScrolledWindow*                   m_sw_print_failed_info{nullptr};
   
public:
	SendToPrinterDialog(Plater* plater = nullptr);
    ~SendToPrinterDialog();

	bool Show(bool show);
	bool is_timeout();
    void on_rename_click(wxCommandEvent& event);
    void on_rename_enter();
    void stripWhiteSpace(std::string& str);
    void prepare_mode();
    void sending_mode();
    void reset_timeout();
    void update_user_printer();
    void update_show_status();
    void prepare(int print_plate_idx);
    void check_focus(wxWindow* window);
    void check_fcous_state(wxWindow* window);
    void update_priner_status_msg(wxString msg, bool is_warning = false);
    void update_print_status_msg(wxString msg, bool is_warning = false, bool is_printer = true);
	void update_printer_list(wxCommandEvent& event);
	void on_cancel(wxCloseEvent& event);
	void on_ok(wxCommandEvent& event);
	void clear_ip_address_config(wxCommandEvent& e);
	void on_refresh(wxCommandEvent& event);
	void on_print_job_cancel(wxCommandEvent& evt);
	void set_default();
	void on_timer(wxTimerEvent& event);
	void on_selection_changed(wxCommandEvent& event);
	void Enable_Refresh_Button(bool en);
	void show_status(PrintDialogStatus status, std::vector<wxString> params = std::vector<wxString>());
	void Enable_Send_Button(bool en);
	void on_dpi_changed(const wxRect& suggested_rect) override;
    void update_user_machine_list();
    void show_print_failed_info(bool show, int code = 0, wxString description = wxEmptyString, wxString extra = wxEmptyString);
    void update_print_error_info(int code, std::string msg, std::string extra);
    void on_change_color_mode() { wxGetApp().UpdateDlgDarkUI(this); }
    wxString format_text(wxString& m_msg);
	std::vector<std::string> sort_string(std::vector<std::string> strArray);

private:
    void onNetworkToggled(wxCommandEvent& event);
    void updateVisible();
};

wxDECLARE_EVENT(EVT_CLEAR_IPADDRESS, wxCommandEvent);
}
}

#endif
