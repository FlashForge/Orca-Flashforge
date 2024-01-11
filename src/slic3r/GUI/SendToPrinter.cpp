#include "SendToPrinter.hpp"
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/Thread.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_Preview.hpp"
#include "MainFrame.hpp"
#include "format.hpp"
#include "Widgets/ProgressDialog.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/StaticBox.hpp"
#include "ConnectPrinter.hpp"

#include <wx/progdlg.h>
#include <wx/clipbrd.h>
#include <wx/dcgraph.h>
#include <miniz.h>
#include <algorithm>
#include "BitmapCache.hpp"

namespace Slic3r {
namespace GUI {

#define INITIAL_NUMBER_OF_MACHINES 0
#define LIST_REFRESH_INTERVAL 200
#define MACHINE_LIST_REFRESH_INTERVAL 2000

wxDEFINE_EVENT(EVT_UPDATE_USER_MACHINE_LIST, wxCommandEvent);
wxDEFINE_EVENT(EVT_PRINT_JOB_CANCEL, wxCommandEvent);
wxDEFINE_EVENT(EVT_SEND_JOB_SUCCESS, wxCommandEvent);
wxDEFINE_EVENT(EVT_CLEAR_IPADDRESS, wxCommandEvent);


std::map<std::string, wxImage> MachineItem::m_machineBitmapMap;
MachineItem::MachineItem(wxWindow* parent, const MachineData& data)
    : wxPanel(parent, wxID_ANY)
    , m_data(data)
{
    initBitmap();
    build();
}

bool MachineItem::IsChecked() const
{
    return m_checkBox->GetValue();
}

void MachineItem::SetChecked(bool checked)
{
    m_checkBox->SetValue(checked);
}

void MachineItem::SetDefaultColor(const wxColor& color)
{
    m_defaultColor = color;
}
    
void MachineItem::build()
{
    m_checkBox = new FFCheckBox(this, wxID_ANY);
    m_checkBox->SetValue(false);

    m_iconPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    //m_iconPanel->SetBackgroundColour("#333333");

    m_iconSizer = new wxBoxSizer(wxVERTICAL);
    m_thumbnailPanel = new ThumbnailPanel(m_iconPanel);
    m_thumbnailPanel->SetSize(wxSize(FromDIP(46), FromDIP(46)));
    m_thumbnailPanel->SetMinSize(wxSize(FromDIP(46), FromDIP(46)));
    m_thumbnailPanel->SetMaxSize(wxSize(FromDIP(46), FromDIP(46)));
    m_iconSizer->Add(m_thumbnailPanel, 0, wxEXPAND, 0);
    m_iconPanel->SetSizer(m_iconSizer);
    m_iconPanel->Layout();

    //m_iconPanel = new ThumbnailPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    //m_iconPanel->SetWindowStyle(m_iconPanel->GetWindowStyle() | wxTAB_TRAVERSAL);
    //m_iconPanel->SetBackgroundColour(m_defaultColor);
    //m_iconPanel->SetSize(wxSize(FromDIP(46), FromDIP(46)));
    //m_iconPanel->SetMinSize(wxSize(FromDIP(46), FromDIP(46)));
    //m_iconPanel->SetMaxSize(wxSize(FromDIP(46), FromDIP(46)));
    
    auto iter = m_machineBitmapMap.find(m_data.model);
    if (iter != m_machineBitmapMap.end()) {
        m_thumbnailPanel->set_thumbnail(iter->second);
    }

    m_nameLbl = new wxStaticText(this, wxID_ANY, _(m_data.name));
    
    m_mainSizer = new wxBoxSizer(wxHORIZONTAL);
    m_mainSizer->Add(m_checkBox, 0, wxALIGN_CENTER_VERTICAL);
    //m_mainSizer->AddSpacer(FromDIP(5));
    m_mainSizer->Add(m_iconPanel, 0, wxALIGN_CENTER_VERTICAL);
    //m_mainSizer->AddSpacer(FromDIP(5));
    m_mainSizer->Add(m_nameLbl, 1, wxALIGN_CENTER_VERTICAL);
    
    SetSizer(m_mainSizer);
    Layout();
}

void MachineItem::initBitmap()
{
    if (!m_machineBitmapMap.empty()) {
        return;
    }
    m_machineBitmapMap["Adventurer 5M"] = create_scaled_bitmap("adventurer_5m", 0, 46).ConvertToImage();
    m_machineBitmapMap["Adventurer 5M Pro"] = create_scaled_bitmap("adventurer_5m_pro", 0, 46).ConvertToImage();
}


void SendToPrinterDialog::stripWhiteSpace(std::string& str)
{
    if (str == "") { return; }

    string::iterator cur_it;
    cur_it = str.begin();

    while (cur_it != str.end()) {
        if ((*cur_it) == '\n' || (*cur_it) == ' ') {
            cur_it = str.erase(cur_it);
        }
        else {
            cur_it++;
        }
    }
}

wxString SendToPrinterDialog::format_text(wxString &m_msg)
{
	if (wxGetApp().app_config->get("language") != "zh_CN") { return m_msg; }

	wxString out_txt = m_msg;
	wxString count_txt = "";
	int      new_line_pos = 0;

#if 0
	for (int i = 0; i < m_msg.length(); i++) {
		auto text_size = m_statictext_printer_msg->GetTextExtent(count_txt);
		if (text_size.x < (FromDIP(400))) {
			count_txt += m_msg[i];
		}
		else {
			out_txt.insert(i - 1, '\n');
			count_txt = "";
		}
	}
#endif
	return out_txt;
}

void SendToPrinterDialog::check_focus(wxWindow* window)
{
    if (window == m_rename_input || window == m_rename_input->GetTextCtrl()) {
        on_rename_enter();
    }
}

void SendToPrinterDialog::check_fcous_state(wxWindow* window)
{
    check_focus(window);
    auto children = window->GetChildren();
    for (auto child : children) {
        check_fcous_state(child);
    }
}

void SendToPrinterDialog::on_rename_click(wxCommandEvent& event)
{
    m_is_rename_mode = true;
    m_rename_input->GetTextCtrl()->SetValue(m_current_project_name);
    m_rename_switch_panel->SetSelection(1);
    m_rename_input->GetTextCtrl()->SetFocus();
    m_rename_input->GetTextCtrl()->SetInsertionPointEnd();
}

void SendToPrinterDialog::on_rename_enter()
{
    if (m_is_rename_mode == false) {
        return;
    }
    else {
        m_is_rename_mode = false;
    }

    auto     new_file_name = m_rename_input->GetTextCtrl()->GetValue();
    auto     m_valid_type = Valid;
    wxString info_line;

    const char* unusable_symbols = "<>[]:/\\|?*\"";

    const std::string unusable_suffix = PresetCollection::get_suffix_modified(); //"(modified)";
    for (size_t i = 0; i < std::strlen(unusable_symbols); i++) {
        if (new_file_name.find_first_of(unusable_symbols[i]) != std::string::npos) {
            info_line = _L("Name is invalid;") + "\n" + _L("illegal characters:") + " " + unusable_symbols;
            m_valid_type = NoValid;
            break;
        }
    }

    if (m_valid_type == Valid && new_file_name.find(unusable_suffix) != std::string::npos) {
        info_line = _L("Name is invalid;") + "\n" + _L("illegal suffix:") + "\n\t" + from_u8(PresetCollection::get_suffix_modified());
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.empty()) {
        info_line = _L("The name is not allowed to be empty.");
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.find_first_of(' ') == 0) {
        info_line = _L("The name is not allowed to start with space character.");
        m_valid_type = NoValid;
    }

    if (m_valid_type == Valid && new_file_name.find_last_of(' ') == new_file_name.length() - 1) {
        info_line = _L("The name is not allowed to end with space character.");
        m_valid_type = NoValid;
    }

    if (m_valid_type != Valid) {
        MessageDialog msg_wingow(nullptr, info_line, "", wxICON_WARNING | wxOK);
        if (msg_wingow.ShowModal() == wxID_OK) {
            m_rename_switch_panel->SetSelection(0);
            m_renameText->SetLabel(m_current_project_name);
            m_renamePanel->Layout();
            return;
        }
    }

    m_current_project_name = new_file_name;
    m_rename_switch_panel->SetSelection(0);
    m_renameText->SetLabel(m_current_project_name);
    m_renamePanel->Layout();
}

SendToPrinterDialog::SendToPrinterDialog(Plater *plater)
    //: TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, _L("Send to Printer SD card"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    : TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Send to Printer SD card"), 6)
    , m_plater(plater), m_export_3mf_cancel(false)
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

    // bind
    Bind(wxEVT_CLOSE_WINDOW, &SendToPrinterDialog::on_cancel, this);

    // font
    SetFont(wxGetApp().normal_font());

    // icon
    //std::string icon_path = (boost::format("%1%/images/Orca-FlashforgeTitle.ico") % resources_dir()).str();
    //SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    Freeze();
    SetBackgroundColour(m_colour_def_color);

    //m_sizer_main = new wxBoxSizer(wxVERTICAL);
    m_sizer_main = MainSizer();

    m_sizer_main->SetMinSize(wxSize(0, -1));

    m_topPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_topSizer = new wxBoxSizer(wxVERTICAL); 

    m_imagePanel = new wxPanel(m_topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_imagePanel->SetBackgroundColour(m_colour_def_color);

    sizer_thumbnail = new wxBoxSizer(wxVERTICAL);
    m_thumbnailPanel = new ThumbnailPanel(m_imagePanel);
    m_thumbnailPanel->SetSize(wxSize(FromDIP(108), FromDIP(117)));
    m_thumbnailPanel->SetMinSize(wxSize(FromDIP(108), FromDIP(117)));
    m_thumbnailPanel->SetMaxSize(wxSize(FromDIP(108), FromDIP(117)));
    sizer_thumbnail->Add(m_thumbnailPanel, 0, wxEXPAND, 0);
    m_imagePanel->SetSizer(sizer_thumbnail);
    m_imagePanel->Layout();

    wxBoxSizer *m_sizer_basic        = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *m_sizer_basic_weight = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer *m_sizer_basic_time   = new wxBoxSizer(wxHORIZONTAL);

    auto timeimg = new wxStaticBitmap(m_topPanel, wxID_ANY, create_scaled_bitmap("ff_print_time", this, 14), wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    m_sizer_basic_weight->Add(timeimg, 1, wxEXPAND | wxALL, FromDIP(5));
    m_stext_time = new wxStaticText(m_topPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    m_sizer_basic_weight->Add(m_stext_time, 0, wxALL, FromDIP(5));
    m_sizer_basic->Add(m_sizer_basic_weight, 0, wxALIGN_CENTER, 0);
    m_sizer_basic->Add(0, 0, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));

    auto weightimg = new wxStaticBitmap(m_topPanel, wxID_ANY, create_scaled_bitmap("ff_print_weight", this, 14), wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    m_sizer_basic_time->Add(weightimg, 1, wxEXPAND | wxALL, FromDIP(5));
    m_stext_weight = new wxStaticText(m_topPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_sizer_basic_time->Add(m_stext_weight, 0, wxALL, FromDIP(5));
    m_sizer_basic->Add(m_sizer_basic_time, 0, wxALIGN_CENTER, 0);

    // bind
    Bind(EVT_SHOW_ERROR_INFO, [this](auto& e) {
        show_print_failed_info(true);
    });

    // bind
    Bind(EVT_UPDATE_USER_MACHINE_LIST, &SendToPrinterDialog::update_printer_list, this);
    Bind(EVT_PRINT_JOB_CANCEL, &SendToPrinterDialog::on_print_job_cancel, this);

    //file name
    //rename normal
    m_rename_switch_panel = new wxSimplebook(m_topPanel);
    m_rename_switch_panel->SetSize(wxSize(FromDIP(320), FromDIP(25)));
    m_rename_switch_panel->SetMinSize(wxSize(FromDIP(320), FromDIP(25)));
    m_rename_switch_panel->SetMaxSize(wxSize(FromDIP(320), FromDIP(25)));

    m_renamePanel = new wxPanel(m_rename_switch_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_renamePanel->SetBackgroundColour(*wxWHITE);
    rename_sizer_v = new wxBoxSizer(wxVERTICAL);
    rename_sizer_h = new wxBoxSizer(wxHORIZONTAL);

    m_renameText = new wxStaticText(m_renamePanel, wxID_ANY, wxT("MyLabel"), wxDefaultPosition, wxDefaultSize, 0);
    m_renameText->SetForegroundColour(*wxBLACK);
    m_renameText->SetFont(::Label::Body_13);
    m_renameText->SetMaxSize(wxSize(FromDIP(320), -1));
    m_renameBtn = new Button(m_renamePanel, "", "ff_editable", wxBORDER_NONE, FromDIP(12));
    m_renameBtn->SetBackgroundColor(*wxWHITE);
    m_renameBtn->SetBackgroundColour(*wxWHITE);

    rename_sizer_h->Add(m_renameText, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);
    rename_sizer_h->Add(m_renameBtn, 0, wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL, 0);
    rename_sizer_v->Add(rename_sizer_h, 1, wxALIGN_LEFT, 0);
    m_renamePanel->SetSizer(rename_sizer_v);
    m_renamePanel->Layout();
    rename_sizer_v->Fit(m_renamePanel);

    //rename edit
    auto m_rename_edit_panel = new wxPanel(m_rename_switch_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_rename_edit_panel->SetBackgroundColour(*wxWHITE);
    auto rename_edit_sizer_v = new wxBoxSizer(wxVERTICAL);

    m_rename_input = new ::TextInput(m_rename_edit_panel, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_rename_input->GetTextCtrl()->SetFont(::Label::Body_13);
    m_rename_input->SetSize(wxSize(FromDIP(320), FromDIP(24)));
    m_rename_input->SetMinSize(wxSize(FromDIP(320), FromDIP(24)));
    m_rename_input->SetMaxSize(wxSize(FromDIP(320), FromDIP(24)));
    m_rename_input->Bind(wxEVT_TEXT_ENTER, [this](auto& e) {on_rename_enter();});
    m_rename_input->Bind(wxEVT_KILL_FOCUS, [this](auto& e) {
        if (!m_rename_input->HasFocus() && !m_renameText->HasFocus())
            on_rename_enter();
        else
            e.Skip(); });
    rename_edit_sizer_v->Add(m_rename_input, 1, wxEXPAND | wxALIGN_LEFT, 0);

    m_rename_edit_panel->SetSizer(rename_edit_sizer_v);
    m_rename_edit_panel->Layout();
    rename_edit_sizer_v->Fit(m_rename_edit_panel);

    m_renameBtn->Bind(wxEVT_BUTTON, &SendToPrinterDialog::on_rename_click, this);
    m_rename_switch_panel->AddPage(m_renamePanel, wxEmptyString, true);
    m_rename_switch_panel->AddPage(m_rename_edit_panel, wxEmptyString, false);

    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_ESCAPE) {
            if (m_rename_switch_panel->GetSelection() == 0) {
                e.Skip();
            }
            else {
                m_rename_switch_panel->SetSelection(0);
                m_renameText->SetLabel(m_current_project_name);
                m_renamePanel->Layout();
            }
        }
        else {
            e.Skip();
        }
        });

    //m_panel_prepare->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
    //    check_fcous_state(this);
    //    e.Skip();
    //    });

    m_topPanel->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        check_fcous_state(this);
        e.Skip();
        });

    Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        check_fcous_state(this);
        e.Skip();
        });

    wxBoxSizer* rightTopSizer = new wxBoxSizer(wxVERTICAL);
    rightTopSizer->Add(m_rename_switch_panel, 0, wxALIGN_LEFT | wxALIGN_BOTTOM, FromDIP(5));
    rightTopSizer->Add(0, 0, 0, wxTOP, FromDIP(5));
    rightTopSizer->Add(m_sizer_basic, 0, wxALIGN_LEFT | wxALIGN_TOP, 0);

    wxBoxSizer* scrollableTopSizer = new wxBoxSizer(wxHORIZONTAL);
    scrollableTopSizer->Add(m_imagePanel, 0, wxALIGN_CENTER_VERTICAL, 0);    
    scrollableTopSizer->Add(0, 0, 0, wxLEFT, FromDIP(5));
    scrollableTopSizer->Add(rightTopSizer, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT, 0);

    m_topSizer->Add(scrollableTopSizer, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    //m_sizer_scrollable_region->Add(0, 0, 0, wxTOP, FromDIP(10));
    //m_sizer_scrollable_region->Add(m_sizer_basic, 0, wxALIGN_CENTER_HORIZONTAL, 0);
	m_topPanel->SetSizer(m_topSizer);
	m_topPanel->Layout();

    auto line_materia = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    line_materia->SetForegroundColour(wxColour("#DDDDDD"));
    line_materia->SetBackgroundColour(wxColour("#DDDDDD"));
    auto line_level = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    line_level->SetForegroundColour(wxColour("#DDDDDD"));
    line_level->SetBackgroundColour(wxColour("#DDDDDD"));

    m_levelCkb = new FFCheckBox(this);
    m_levelCkb->SetValue(true);
    m_levelLbl = new wxStaticText(this, wxID_ANY, _("Levelling"));
    m_levelLbl->SetForegroundColour(wxColour("#333333"));

    auto levelSizer = new wxBoxSizer(wxHORIZONTAL);
    levelSizer->Add(m_levelCkb, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    levelSizer->Add(m_levelLbl, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));

    m_selectPrinterLbl = new wxStaticText(this, wxID_ANY, _("Select Printer"));
    m_netBtn = new FFToggleButton(this, _("Network"));
    m_netBtn->SetWindowStyle(m_netBtn->GetWindowStyle() | wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
    Bind(wxEVT_TOGGLEBUTTON, &SendToPrinterDialog::onNetworkToggled, this);
    m_lanBtn = new FFToggleButton(this, _("Lan"));
    m_lanBtn->SetWindowStyle(m_netBtn->GetWindowStyle() | wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
    Bind(wxEVT_TOGGLEBUTTON, &SendToPrinterDialog::onNetworkToggled, this);
    auto networkLine = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(1, -1), wxTAB_TRAVERSAL);
    networkLine->SetForegroundColour(wxColour("#DDDDDD"));
    networkLine->SetBackgroundColour(wxColour("#DDDDDD"));

    wxBoxSizer* networkSizer = new wxBoxSizer(wxHORIZONTAL);
    networkSizer->Add(m_selectPrinterLbl, 1, wxLEFT | wxEXPAND | wxALIGN_LEFT, FromDIP(10));
    networkSizer->Add(m_netBtn, 0, wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL | wxRIGHT, FromDIP(5));
    networkSizer->Add(networkLine, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(3));
    networkSizer->Add(m_lanBtn, 0, wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL | wxLEFT, FromDIP(5));

    // machine book
    m_machineBook = new wxSimplebook(this, wxID_ANY);
    
    // machine
    m_machinePanel = new wxPanel(m_machineBook);
    m_machinePanel->SetBackgroundColour(wxColour("#FAFAFA"));

    m_selectAll = new FFCheckBox(m_machinePanel, wxID_ANY);
    m_selectAll->SetValue(false);
    m_selectAllLbl = new wxStaticText(m_machinePanel, wxID_ANY, _("Select All"));
    m_selectAllLbl->SetForegroundColour("#333333");
    auto selectSizer = new wxBoxSizer(wxHORIZONTAL);
    selectSizer->Add(m_selectAll, 0, wxALIGN_LEFT);
    selectSizer->AddSpacer(FromDIP(10));
    selectSizer->Add(m_selectAllLbl, 0, wxALIGN_LEFT);

    m_machineListWindow = new wxScrolledWindow(m_machinePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    //m_machinePanel->SetBackgroundColour(wxColour("#FAFAFA"));
    m_machineListWindow->EnableScrolling(false, true);
    m_machineListWindow->SetScrollRate(0, 10);
    m_machineListWindow->SetSize(-1, 236);
    m_machineListWindow->SetMinSize(wxSize(-1, 236));
    m_machineListWindow->SetMaxSize(wxSize(-1, 236));
    m_machineListWindow->SetVirtualSize(-1, 236);
    m_machineListSizer = new wxGridSizer(2);
    m_machineListSizer->SetHGap(FromDIP(20));
    m_machineListWindow->SetSizer(m_machineListSizer);
    
    m_machineSizer = new wxBoxSizer(wxVERTICAL);
    m_machineSizer->AddSpacer(FromDIP(20));
    m_machineSizer->Add(selectSizer, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(10));
    m_machineSizer->AddSpacer(FromDIP(10));
    m_machineSizer->Add(m_machineListWindow, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    m_machineSizer->AddSpacer(FromDIP(20));
    m_machinePanel->SetSizer(m_machineSizer);
    m_machinePanel->Show(false);
    //m_machineSizer->Fit(m_machinePanel);
    m_machineBook->AddPage(m_machinePanel, wxEmptyString, true);

    // no machine
    m_noMachinePanel = new wxPanel(m_machineBook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    m_machineLine = new wxPanel(m_noMachinePanel, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    m_machineLine->SetForegroundColour(wxColour("#DDDDDD"));
    m_machineLine->SetBackgroundColour(wxColour("#DDDDDD"));

    m_noMachineBitmap = new wxStaticBitmap(m_noMachinePanel, wxID_ANY, create_scaled_bitmap("ff_warning", this, 16), wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    m_noMachineText = new wxStaticText(m_noMachinePanel, wxID_ANY, _("No printer connected, please connect printer first!"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_noMachineText->SetForegroundColour(wxColour("#FB4747"));
    m_noMachineText->SetMaxSize(wxSize(FromDIP(380), -1));
    wxBoxSizer* textSizer = new wxBoxSizer(wxHORIZONTAL);
    textSizer->Add(m_noMachineBitmap, 0, wxALIGN_CENTER);
    textSizer->AddSpacer(FromDIP(10));
    textSizer->Add(m_noMachineText, 1, wxALIGN_CENTER_VERTICAL);

    auto noMachineSizer = new wxBoxSizer(wxVERTICAL);
    noMachineSizer->AddSpacer(FromDIP(10));
    noMachineSizer->Add(m_machineLine, 0);
    noMachineSizer->AddSpacer(FromDIP(20));
    noMachineSizer->Add(textSizer, 0, wxALIGN_LEFT);
    m_noMachinePanel->SetSizer(noMachineSizer);
    m_noMachinePanel->Layout();
    m_machineBook->AddPage(m_noMachinePanel, wxEmptyString, false);

    // send book: 0: send panel, 1: progress panel
    m_sendBook = new wxSimplebook(this, wxID_ANY);

    // send panel
    m_sendPanel = new wxPanel(m_sendBook, wxID_ANY);

    m_errorPanel = new wxPanel(m_sendPanel);
    m_errorBitmap = new wxStaticBitmap(m_errorPanel, wxID_ANY, create_scaled_bitmap("ff_warning", this, 16), wxDefaultPosition, wxSize(FromDIP(16), FromDIP(16)), 0);
    m_errorText = new wxStaticText(m_errorPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_errorText->SetForegroundColour(wxColour("#FB4747"));
    m_errorText->SetMaxSize(wxSize(FromDIP(380), -1));
    wxBoxSizer* errorSizer = new wxBoxSizer(wxHORIZONTAL);
    errorSizer->Add(m_errorBitmap, 0, wxALIGN_CENTER);
    errorSizer->AddSpacer(FromDIP(10));
    errorSizer->Add(m_errorText, 1, wxALIGN_CENTER_VERTICAL);
    m_errorPanel->SetSizer(errorSizer);
    m_errorPanel->Layout();

    wxBoxSizer* sendSizer = new wxBoxSizer(wxVERTICAL);
    m_sendBtn = new FFButton(m_sendPanel, wxID_ANY, _("Send"), FromDIP(4), false);
    m_sendBtn->SetFontColor(wxColour("#ffffff"));
    m_sendBtn->SetFontHoverColor(wxColor("#ffffff"));
    m_sendBtn->SetFontPressColor(wxColor("#ffffff"));
    m_sendBtn->SetFontDisableColor(wxColor("#ffffff"));
    m_sendBtn->SetBGColor(wxColour("#419488"));
    m_sendBtn->SetBGHoverColor(wxColour("#65A79E"));
    m_sendBtn->SetBGPressColor(wxColour("#1A8676"));
    m_sendBtn->SetBGDisableColor(wxColour("#dddddd"));
    m_sendBtn->SetSize(wxSize(FromDIP(101), FromDIP(44)));
    m_sendBtn->SetMinSize(wxSize(FromDIP(101), FromDIP(44)));
    m_sendBtn->SetMaxSize(wxSize(FromDIP(101), FromDIP(44)));
    sendSizer->Add(m_errorPanel, 1, wxEXPAND | wxALIGN_LEFT);
    sendSizer->Add(m_sendBtn, 0, wxALIGN_CENTER);
    m_sendPanel->SetSizer(sendSizer);
    m_sendPanel->Layout();
    m_sendBook->AddPage(m_sendPanel, wxEmptyString, true);

    // progress
    m_progressPanel = new wxPanel(m_sendBook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    m_progressBar = new ProgressBar(m_progressPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, FromDIP(8)));
    m_progressBar->ShowNumber(false);
    m_progressBar->SetValue(50);
    m_progressInfoLbl = new wxStaticText(m_progressPanel, wxID_ANY, "Just For test");
    m_progressLbl = new wxStaticText(m_progressPanel, wxID_ANY, "100%");
    m_progressCancelBtn = new FFButton(m_progressPanel, wxID_ANY, _("Cancel"), FromDIP(4), true);
    wxBoxSizer* progressDownSizer = new wxBoxSizer(wxHORIZONTAL);
    progressDownSizer->Add(m_progressBar, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, FromDIP(6));
    progressDownSizer->AddSpacer(FromDIP(11));
    progressDownSizer->Add(m_progressLbl, 0, wxALIGN_CENTER_VERTICAL);
    progressDownSizer->AddSpacer(FromDIP(20));
    progressDownSizer->Add(m_progressCancelBtn, 0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* progressSizer = new wxBoxSizer(wxVERTICAL);
    progressSizer->Add(m_progressInfoLbl, 1, wxEXPAND | wxALIGN_LEFT | wxALIGN_BOTTOM);
    //progressSizer->AddSpacer(FromDIP(5));
    progressSizer->Add(progressDownSizer, 1, wxEXPAND | wxALIGN_LEFT);
    m_progressPanel->SetSizer(progressSizer);
    m_progressPanel->Layout();
    m_sendBook->AddPage(m_progressPanel, wxEmptyString, false);

    // main layout
    m_sizer_main->Add(0, 0, 0, wxTOP, FromDIP(10));
    m_sizer_main->Add(m_topPanel, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(6));
    //m_sizer_main->Add(m_rename_switch_panel, 0, wxALIGN_CENTER_HORIZONTAL, 0);
    //m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(6));
    m_sizer_main->Add(line_materia, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(12));
    m_sizer_main->Add(levelSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(12));
    m_sizer_main->Add(line_level, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(12));
    m_sizer_main->Add(networkSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(12));
    m_sizer_main->Add(m_machineBook, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    //m_sizer_main->Add(m_sizer_printer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->Add(0, 0, 0, wxEXPAND | wxTOP, FromDIP(10));
    m_sizer_main->Add(m_sendBook, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(40));
    m_sizer_main->AddSpacer(FromDIP(10));

    show_print_failed_info(false);
    //SetSizer(m_sizer_main);
    Layout();
    Fit();
    Thaw();

    init_bind();
    init_timer();
    // CenterOnParent();
    Centre(wxBOTH);
    wxGetApp().UpdateDlgDarkUI(this);
}

void SendToPrinterDialog::update_print_error_info(int code, std::string msg, std::string extra)
{
    m_print_error_code = code;
    m_print_error_msg = msg;
    m_print_error_extra = extra;
}

void SendToPrinterDialog::show_print_failed_info(bool show, int code, wxString description, wxString extra)
{
#if 0
    if (show) {
        if (!m_sw_print_failed_info->IsShown()) {
            m_sw_print_failed_info->Show(true);

            m_st_txt_error_code->SetLabelText(wxString::Format("%d", m_print_error_code));
            m_st_txt_error_desc->SetLabelText( wxGetApp().filter_string(m_print_error_msg));
            m_st_txt_extra_info->SetLabelText( wxGetApp().filter_string(m_print_error_extra));

            m_st_txt_error_code->Wrap(FromDIP(260));
            m_st_txt_error_desc->Wrap(FromDIP(260));
            m_st_txt_extra_info->Wrap(FromDIP(260));
        }
        else {
            m_sw_print_failed_info->Show(false);
        }
        Layout();
        Fit();
    }
    else {
        if (!m_sw_print_failed_info->IsShown()) { return; }
        m_sw_print_failed_info->Show(false);
        m_st_txt_error_code->SetLabelText(wxEmptyString);
        m_st_txt_error_desc->SetLabelText(wxEmptyString);
        m_st_txt_extra_info->SetLabelText(wxEmptyString);
        Layout();
        Fit();
    }
#endif
}

void SendToPrinterDialog::prepare_mode()
{
#if 0
	m_is_in_sending_mode = false;
	if (m_send_job) {
		m_send_job->join();
	}

	if (wxIsBusy())
		wxEndBusyCursor();
	Enable_Send_Button(true);
    show_print_failed_info(false);

    m_status_bar->reset();
	if (m_simplebook->GetSelection() != 0) {
		m_simplebook->SetSelection(0);
	}
#endif
}

void SendToPrinterDialog::sending_mode()
{
#if 0
    m_is_in_sending_mode = true;
    if (m_simplebook->GetSelection() != 1){
        m_simplebook->SetSelection(1);
        Layout();
        Fit();
    }
#endif
}

void SendToPrinterDialog::prepare(int print_plate_idx)
{
    m_print_plate_idx = print_plate_idx;
}

void SendToPrinterDialog::update_priner_status_msg(wxString msg, bool is_warning) 
{
#if 0
    auto colour = is_warning ? wxColour(0xFF, 0x6F, 0x00) : wxColour(0x6B, 0x6B, 0x6B);
    m_statictext_printer_msg->SetForegroundColour(colour);

    if (msg.empty()) {
        if (!m_statictext_printer_msg->GetLabel().empty()) {
            m_statictext_printer_msg->SetLabel(wxEmptyString);
            m_statictext_printer_msg->Hide();
            Layout();
            Fit();
        }
    } else {
        msg          = format_text(msg);

        auto str_new = msg.ToStdString();
        stripWhiteSpace(str_new);

        auto str_old = m_statictext_printer_msg->GetLabel().ToStdString();
        stripWhiteSpace(str_old);

        if (str_new != str_old) {
            if (m_statictext_printer_msg->GetLabel() != msg) {
                m_statictext_printer_msg->SetLabel(msg);
                m_statictext_printer_msg->SetMinSize(wxSize(FromDIP(400), -1));
                m_statictext_printer_msg->SetMaxSize(wxSize(FromDIP(400), -1));
                m_statictext_printer_msg->Wrap(FromDIP(400));
                m_statictext_printer_msg->Show();
                Layout();
                Fit();
            }
        }
    }
#endif 
}

void SendToPrinterDialog::update_print_status_msg(wxString msg, bool is_warning, bool is_printer_msg)
{
    if (is_printer_msg) {
        update_priner_status_msg(msg, is_warning);
    } else {
        update_priner_status_msg(wxEmptyString, false);
    }
}


void SendToPrinterDialog::init_bind()
{
    Bind(wxEVT_TIMER, &SendToPrinterDialog::on_timer, this);
    Bind(EVT_CLEAR_IPADDRESS, &SendToPrinterDialog::clear_ip_address_config, this);
}

void SendToPrinterDialog::init_timer()
{
    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
}

void SendToPrinterDialog::on_cancel(wxCloseEvent &event)
{
    if (m_send_job) {
        if (m_send_job->is_running()) {
            m_send_job->cancel();
            m_send_job->join();
        }
    }
    this->EndModal(wxID_CANCEL);
}
 
void SendToPrinterDialog::on_ok(wxCommandEvent &event)
{
#if 0
    BOOST_LOG_TRIVIAL(info) << "print_job: on_ok to send";
    m_is_canceled = false;
    Enable_Send_Button(false);
    if (m_is_in_sending_mode)
        return;

    int result = 0;
    if (m_printer_last_select.empty()) {
        return;
    }

    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    MachineObject *obj_ = dev->get_selected_machine();
    
    if (obj_ == nullptr) {
        m_printer_last_select = "";
        m_comboBox_printer->SetTextLabel("");
        return;
    }
    assert(obj_->dev_id == m_printer_last_select);


    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << ", print_job: for send task, current printer id =  " << m_printer_last_select << std::endl;
    show_status(PrintDialogStatus::PrintStatusSending);

    m_status_bar->reset();
    m_status_bar->set_prog_block();
    m_status_bar->set_cancel_callback_fina([this]() {
        BOOST_LOG_TRIVIAL(info) << "print_job: enter canceled";
        if (m_send_job) {
            if (m_send_job->is_running()) {
                BOOST_LOG_TRIVIAL(info) << "send_job: canceled";
                m_send_job->cancel();
            }
            m_send_job->join();
        }
        m_is_canceled = true;
        wxCommandEvent* event = new wxCommandEvent(EVT_PRINT_JOB_CANCEL);
        wxQueueEvent(this, event);
    });

    if (m_is_canceled) {
        BOOST_LOG_TRIVIAL(info) << "send_job: m_is_canceled";
        //m_status_bar->set_status_text(task_canceled_text);
        return;
    }

    // enter sending mode
    sending_mode();

    result = m_plater->send_gcode(m_print_plate_idx, [this](int export_stage, int current, int total, bool &cancel) {
        if (this->m_is_canceled) return;
        bool     cancelled = false;
        wxString msg       = _L("Preparing print job");
        m_status_bar->update_status(msg, cancelled, 10, true);
        m_export_3mf_cancel = cancel = cancelled;
    });

    if (m_is_canceled || m_export_3mf_cancel) {
        BOOST_LOG_TRIVIAL(info) << "send_job: m_export_3mf_cancel or m_is_canceled";
        //m_status_bar->set_status_text(task_canceled_text);
        return;
    }

    if (result < 0) {
        wxString msg = _L("Abnormal print file data. Please slice again");
        m_status_bar->set_status_text(msg);
        return;
    }

    // export config 3mf if needed
    if (!obj_->is_lan_mode_printer()) {
        result = m_plater->export_config_3mf(m_print_plate_idx);
        if (result < 0) {
            BOOST_LOG_TRIVIAL(trace) << "export_config_3mf failed, result = " << result;
            return;
        }
    }
    if (m_is_canceled || m_export_3mf_cancel) {
        BOOST_LOG_TRIVIAL(info) << "send_job: m_export_3mf_cancel or m_is_canceled";
        //m_status_bar->set_status_text(task_canceled_text);
        return;
    }

   /* std::string  file_name       = "";
	auto default_output_file    = wxGetApp().plater()->get_export_gcode_filename(".3mf");
    if (!default_output_file.empty()) {
		fs::path default_output_file_path = boost::filesystem::path(default_output_file.c_str());
		file_name = default_output_file_path.filename().string();
    }*/
    


    m_send_job                      = std::make_shared<SendJob>(m_status_bar, m_plater, m_printer_last_select);
    m_send_job->m_dev_ip            = obj_->dev_ip;
    m_send_job->m_access_code       = obj_->get_access_code();


#if !BBL_RELEASE_TO_PUBLIC
    m_send_job->m_local_use_ssl_for_ftp = wxGetApp().app_config->get("enable_ssl_for_ftp") == "true" ? true : false;
    m_send_job->m_local_use_ssl_for_mqtt = wxGetApp().app_config->get("enable_ssl_for_mqtt") == "true" ? true : false;
#else
    m_send_job->m_local_use_ssl_for_ftp = obj_->local_use_ssl_for_ftp;
    m_send_job->m_local_use_ssl_for_mqtt = obj_->local_use_ssl_for_mqtt;
#endif

    m_send_job->connection_type     = obj_->connection_type();
    m_send_job->cloud_print_only    = true;
    m_send_job->has_sdcard          = obj_->has_sdcard();
    m_send_job->set_project_name(m_current_project_name.utf8_string());
 
    enable_prepare_mode = false;

    m_send_job->on_check_ip_address_fail([this]() {
        wxCommandEvent* evt = new wxCommandEvent(EVT_CLEAR_IPADDRESS);
        wxQueueEvent(this, evt);
        wxGetApp().show_ip_address_enter_dialog();
    });

    if (obj_->is_lan_mode_printer()) {
        m_send_job->set_check_mode();
        m_send_job->check_and_continue();
        m_send_job->start();
    }
    else {
        m_send_job->start();
    }

    BOOST_LOG_TRIVIAL(info) << "send_job: send print job";
#endif
}

void SendToPrinterDialog::clear_ip_address_config(wxCommandEvent& e)
{
    enable_prepare_mode = true;
    prepare_mode();
}

void SendToPrinterDialog::update_user_machine_list()
{
    m_machineList.clear();
    com_id_list_t idList = MultiComMgr::inst()->getReadyDevList();
    if (!idList.empty()) {
        bool valid = false;
        for (auto id : idList) {
            auto data = MultiComMgr::inst()->devData(id, &valid);
            if (valid) {
                MachineItem::MachineData mdata;
                mdata.flag = data.connectMode;
                mdata.model = data.devDetail->model;
                mdata.name = data.devDetail->name;
                m_machineList.emplace_back(mdata);
            } else {
                BOOST_LOG_TRIVIAL(warning) << "com_id (" << id << "): get com data error";
            }
        }
    }
    if (m_machineList.empty()) {
        for (int i = 0; i < 5; ++i) {
            MachineItem::MachineData mdata;
            mdata.flag = 0;
            if (i % 2 == 0) {
                mdata.model = "Adventurer 5M Pro";
            } else {
                mdata.model = "Adventurer 5M";
            }
            mdata.name = "Just For Test";
            m_machineList.emplace_back(mdata);
        }
    }
    wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
    event.SetEventObject(this);
    wxPostEvent(this, event);
#if 0
    NetworkAgent* m_agent = wxGetApp().getAgent();
    if (m_agent && m_agent->is_user_login()) {
        boost::thread get_print_info_thread = Slic3r::create_thread([&] {
            NetworkAgent* agent = wxGetApp().getAgent();
            unsigned int http_code;
            std::string body;
            int result = agent->get_user_print_info(&http_code, &body);
            if (result == 0) {
                m_print_info = body;
            }
            else {
                m_print_info = "";
            }
            wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
            event.SetEventObject(this);
            wxPostEvent(this, event);
        });
    } else {
        wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
        event.SetEventObject(this);
        wxPostEvent(this, event);
    }
#endif
}

void SendToPrinterDialog::on_refresh(wxCommandEvent &event)
{
    BOOST_LOG_TRIVIAL(info) << "m_printer_last_select: on_refresh";
    show_status(PrintDialogStatus::PrintStatusRefreshingMachineList);

    update_user_machine_list();
}

void SendToPrinterDialog::on_print_job_cancel(wxCommandEvent &evt)
{
    BOOST_LOG_TRIVIAL(info) << "print_job: canceled";
    show_status(PrintDialogStatus::PrintStatusSendingCanceled);
    // enter prepare mode
    prepare_mode();
}

std::vector<std::string> SendToPrinterDialog::sort_string(std::vector<std::string> strArray)
{
    std::vector<std::string> outputArray;
    std::sort(strArray.begin(), strArray.end());
    std::vector<std::string>::iterator st;
    for (st = strArray.begin(); st != strArray.end(); st++) { outputArray.push_back(*st); }

    return outputArray;
}

bool  SendToPrinterDialog::is_timeout()
{
    if (timeout_count > 15 * 1000 / LIST_REFRESH_INTERVAL) {
        return true;
    }
    return false;
}

void  SendToPrinterDialog::reset_timeout()
{
    timeout_count = 0;
}

void SendToPrinterDialog::update_user_printer()
{
    Freeze();
    m_machineListSizer->Clear();
    int index = 1;
    if (!m_machineList.empty()) {
        size_t cnt = m_machineList.size();
        size_t rows = (cnt + 1) / 2;
        m_machineListSizer->SetRows(rows);
        for (auto& m : m_machineList) {
            m_machineListSizer->Add(new MachineItem(m_machineListWindow, m), 1, wxEXPAND | wxALIGN_LEFT);
        }
        rows = (rows > 4) ? 4 : rows;
        int height = rows * 46 + (rows - 1) * 10;
        m_machineListWindow->SetVirtualSize(-1, height);
        m_machineListWindow->SetSize(-1, height);
        m_machineListSizer->Layout();
        m_machineListSizer->Fit(m_machineListWindow);
        index = 0;
    }
    if (m_machineBook->GetSelection()!= index) {
        m_machineBook->SetSelection(index);
        m_machineBook->Layout();
        m_machineBook->Fit();
        Layout();
        Fit();
    }
    //updateVisible();
    Thaw();
#if 0
    Slic3r::DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    // update user print info
    if (!m_print_info.empty()) {
        dev->parse_user_print_info(m_print_info);
        m_print_info = "";
    }

    // clear machine list
    m_list.clear();
    m_comboBox_printer->Clear();
    std::vector<std::string>              machine_list;
    wxArrayString                         machine_list_name;
    std::map<std::string, MachineObject*> option_list;

    option_list = dev->get_my_machine_list();

    // same machine only appear once
    for (auto it = option_list.begin(); it != option_list.end(); it++) {
        if (it->second && (it->second->is_online() || it->second->is_connected())) {
            machine_list.push_back(it->second->dev_name);
        }
    }
    machine_list = sort_string(machine_list);
    for (auto tt = machine_list.begin(); tt != machine_list.end(); tt++) {
        for (auto it = option_list.begin(); it != option_list.end(); it++) {
            if (it->second->dev_name == *tt) {
                m_list.push_back(it->second);
                wxString dev_name_text = from_u8(it->second->dev_name);
                if (it->second->is_lan_mode_printer()) {
                    dev_name_text += "(LAN)";
                }
                machine_list_name.Add(dev_name_text);
                break;
            }
        }
    }

    m_comboBox_printer->Set(machine_list_name);

    MachineObject* obj = dev->get_selected_machine();
    if (obj) {
        m_printer_last_select = obj->dev_id;
    } else {
        m_printer_last_select = "";
    }

    if (m_list.size() > 0) {
        // select a default machine
        if (m_printer_last_select.empty()) {
            m_printer_last_select = m_list[0]->dev_id;
            m_comboBox_printer->SetSelection(0);
            wxCommandEvent event(wxEVT_COMBOBOX);
            event.SetEventObject(m_comboBox_printer);
            wxPostEvent(m_comboBox_printer, event);
        }
        for (auto i = 0; i < m_list.size(); i++) {
            if (m_list[i]->dev_id == m_printer_last_select) {
                m_comboBox_printer->SetSelection(i);
                wxCommandEvent event(wxEVT_COMBOBOX);
                event.SetEventObject(m_comboBox_printer);
                wxPostEvent(m_comboBox_printer, event);
            }
        }
    }
    else {
        m_printer_last_select = "";
        m_comboBox_printer->SetTextLabel("");
    }
#endif
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "for send task, current printer id =  " << m_printer_last_select << std::endl;
}

void SendToPrinterDialog::update_printer_list(wxCommandEvent &event)
{
    show_status(PrintDialogStatus::PrintStatusInit);
    update_user_printer();
}

void SendToPrinterDialog::on_timer(wxTimerEvent &event)
{
    wxGetApp().reset_to_active();
    update_show_status();
}

void SendToPrinterDialog::on_selection_changed(wxCommandEvent &event)
{
#if 0
    /* reset timeout and reading printer info */
    //m_status_bar->reset();
    timeout_count      = 0;

    auto selection = m_comboBox_printer->GetSelection();
    DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;

    MachineObject* obj = nullptr;
    for (int i = 0; i < m_list.size(); i++) {
        if (i == selection) {
            m_printer_last_select = m_list[i]->dev_id;
            obj = m_list[i];
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "for send task, current printer id =  " << m_printer_last_select << std::endl;
            break;
        }
    }

    if (obj && !obj->get_lan_mode_connection_state()) {
        obj->command_get_version();
        obj->command_request_push_all();
        if (!dev->get_selected_machine()) {
            dev->set_selected_machine(m_printer_last_select, true);
        }else if (dev->get_selected_machine()->dev_id != m_printer_last_select) {
            dev->set_selected_machine(m_printer_last_select, true);
        }
    }
    else {
        BOOST_LOG_TRIVIAL(error) << "on_selection_changed dev_id not found";
        return;
    }

    update_show_status();
#endif
}

void SendToPrinterDialog::update_show_status()
{
#if 0
    NetworkAgent* agent = Slic3r::GUI::wxGetApp().getAgent();
    DeviceManager* dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!agent) return;
    if (!dev) return;
    MachineObject* obj_ = dev->get_my_machine(m_printer_last_select);
    if (!obj_) {
        if (agent) {
            if (agent->is_user_login()) {
                show_status(PrintDialogStatus::PrintStatusInvalidPrinter);
            }
            else {
                show_status(PrintDialogStatus::PrintStatusNoUserLogin);
            }
        }
        return;
    }

    /* check cloud machine connections */
    if (!obj_->is_lan_mode_printer()) {
        if (!agent->is_server_connected()) {
            agent->refresh_connection();
            show_status(PrintDialogStatus::PrintStatusConnectingServer);
            reset_timeout();
            return;
        }
    }

    if (!obj_->is_info_ready()) {
        if (is_timeout()) {
            (PrintDialogStatus::PrintStatusReadingTimeout);
            return;
        }
        else {
            timeout_count++;
            show_status(PrintDialogStatus::PrintStatusReading);
            return;
        }
        return;
    }

    reset_timeout();

    bool is_suppt = obj_->is_function_supported(PrinterFunction::FUNC_SEND_TO_SDCARD);
    if (!is_suppt) {
        show_status(PrintDialogStatus::PrintStatusNotSupportedSendToSDCard);
        return;
    }

    // reading done
    if (obj_->is_in_upgrading()) {
        show_status(PrintDialogStatus::PrintStatusInUpgrading);
        return;
    }
    else if (obj_->is_system_printing()) {
        show_status(PrintDialogStatus::PrintStatusInSystemPrinting);
        return;
    }

    // check sdcard when if lan mode printer
   /* if (obj_->is_lan_mode_printer()) {
    }*/
	if (obj_->get_sdcard_state() == MachineObject::SdcardState::NO_SDCARD) {
		show_status(PrintDialogStatus::PrintStatusNoSdcard);
		return;
	}

    if (obj_->dev_ip.empty()) {
        show_status(PrintDialogStatus::PrintStatusNotOnTheSameLAN);
        return;
    }
    
    show_status(PrintDialogStatus::PrintStatusReadingFinished);
#endif
}

void SendToPrinterDialog::Enable_Refresh_Button(bool en)
{
#if 0
    if (!en) {
        if (m_button_refresh->IsEnabled()) {
            m_button_refresh->Disable();
            m_button_refresh->SetBackgroundColor(wxColour(0x90, 0x90, 0x90));
            m_button_refresh->SetBorderColor(wxColour(0x90, 0x90, 0x90));
        }
    } else {
        if (!m_button_refresh->IsEnabled()) {
            m_button_refresh->Enable();
            m_button_refresh->SetBackgroundColor(btn_bg_enable);
            m_button_refresh->SetBorderColor(btn_bg_enable);
        }
    }
#endif
}

void SendToPrinterDialog::show_status(PrintDialogStatus status, std::vector<wxString> params)
{
#if 0
	if (m_print_status != status)
		BOOST_LOG_TRIVIAL(info) << "select_machine_dialog: show_status = " << status;
	m_print_status = status;

	// m_comboBox_printer
	if (status == PrintDialogStatus::PrintStatusRefreshingMachineList)
		m_comboBox_printer->Disable();
	else
		m_comboBox_printer->Enable();

	// m_panel_warn m_simplebook
	if (status == PrintDialogStatus::PrintStatusSending) {
		sending_mode();
	}

	// other
	if (status == PrintDialogStatus::PrintStatusInit) {
		update_print_status_msg(wxEmptyString, false, false);
		Enable_Send_Button(false);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusNoUserLogin) {
		wxString msg_text = _L("No login account, only printers in LAN mode are displayed");
		update_print_status_msg(msg_text, false, true);
		Enable_Send_Button(false);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusInvalidPrinter) {
		update_print_status_msg(wxEmptyString, true, true);
		Enable_Send_Button(false);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusConnectingServer) {
		wxString msg_text = _L("Connecting to server");
		update_print_status_msg(msg_text, true, true);
		Enable_Send_Button(true);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusReading) {
		wxString msg_text = _L("Synchronizing device information");
		update_print_status_msg(msg_text, false, true);
		Enable_Send_Button(false);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusReadingFinished) {
		update_print_status_msg(wxEmptyString, false, true);
		Enable_Send_Button(true);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusReadingTimeout) {
		wxString msg_text = _L("Synchronizing device information time out");
		update_print_status_msg(msg_text, true, true);
		Enable_Send_Button(true);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusInUpgrading) {
		wxString msg_text = _L("Cannot send the print task when the upgrade is in progress");
		update_print_status_msg(msg_text, true, true);
		Enable_Send_Button(false);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusRefreshingMachineList) {
		update_print_status_msg(wxEmptyString, false, true);
		Enable_Send_Button(false);
		Enable_Refresh_Button(false);
	}
	else if (status == PrintDialogStatus::PrintStatusSending) {
		Enable_Send_Button(false);
		Enable_Refresh_Button(false);
	}
	else if (status == PrintDialogStatus::PrintStatusSendingCanceled) {
		Enable_Send_Button(true);
		Enable_Refresh_Button(true);
	}
	else if (status == PrintDialogStatus::PrintStatusNoSdcard) {
		wxString msg_text = _L("An SD card needs to be inserted before send to printer SD card.");
		update_print_status_msg(msg_text, true, true);
		Enable_Send_Button(false);
		Enable_Refresh_Button(true);
    }
    else if (status == PrintDialogStatus::PrintStatusNotOnTheSameLAN) {
        wxString msg_text = _L("The printer is required to be in the same LAN as Orca-Flashforge.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    }
    else if (status == PrintDialogStatus::PrintStatusNotSupportedSendToSDCard) {
        wxString msg_text = _L("The printer does not support sending to printer SD card.");
        update_print_status_msg(msg_text, true, true);
        Enable_Send_Button(false);
        Enable_Refresh_Button(true);
    }
    else {
		Enable_Send_Button(true);
		Enable_Refresh_Button(true);
    }
#endif
}


void SendToPrinterDialog::Enable_Send_Button(bool en)
{
#if 0
    if (!en) {
        if (m_button_ensure->IsEnabled()) {
            m_button_ensure->Disable();
            m_button_ensure->SetBackgroundColor(wxColour(0x90, 0x90, 0x90));
            m_button_ensure->SetBorderColor(wxColour(0x90, 0x90, 0x90));
        }
    } else {
        if (!m_button_ensure->IsEnabled()) {
            m_button_ensure->Enable();
            m_button_ensure->SetBackgroundColor(btn_bg_enable);
            m_button_ensure->SetBorderColor(btn_bg_enable);
        }
    }
#endif
}

void SendToPrinterDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    //m_status_bar->msw_rescale();
    Fit();
    Refresh();
}

void SendToPrinterDialog::set_default()
{
    //project name
    m_rename_switch_panel->SetSelection(0);

    wxString filename = m_plater->get_export_gcode_filename("", true, m_print_plate_idx == PLATE_ALL_IDX ? true : false);

    if (m_print_plate_idx == PLATE_ALL_IDX && filename.empty()) {
        filename = _L("Untitled");
    }

    if (filename.empty()) {
        filename = m_plater->get_export_gcode_filename("", true);
        if (filename.empty()) filename = _L("Untitled");
    }

    fs::path filename_path(filename.c_str());
    m_current_project_name = wxString::FromUTF8(filename_path.filename().string());

    //unsupported character filter
    m_current_project_name = from_u8(filter_characters(m_current_project_name.ToUTF8().data(), "<>[]:/\\|?*\""));

    m_renameText->SetLabelText(m_current_project_name);
    m_renamePanel->Layout();

    enable_prepare_mode = true;
    prepare_mode();

    // rset status bar
    //m_status_bar->reset();
    
    NetworkAgent* agent = wxGetApp().getAgent();
    if (agent) {
        if (agent->is_user_login()) {
            show_status(PrintDialogStatus::PrintStatusInit);
        } else {
            show_status(PrintDialogStatus::PrintStatusNoUserLogin);
        }
    }

    // thumbmail
    //wxBitmap bitmap;
    ThumbnailData &data   = m_plater->get_partplate_list().get_curr_plate()->thumbnail_data;
    if (data.is_valid()) {
        wxImage image(data.width, data.height);
        image.InitAlpha();
        for (unsigned int r = 0; r < data.height; ++r) {
            unsigned int rr = (data.height - 1 - r) * data.width;
            for (unsigned int c = 0; c < data.width; ++c) {
                unsigned char *px = (unsigned char *) data.pixels.data() + 4 * (rr + c);
                image.SetRGB((int) c, (int) r, px[0], px[1], px[2]);
                image.SetAlpha((int) c, (int) r, px[3]);
            }
        }
        image  = image.Rescale(FromDIP(108), FromDIP(117));
        m_thumbnailPanel->set_thumbnail(image);
    }
    
    std::vector<std::string> materials;
    std::vector<std::string> display_materials;
    {
        auto preset_bundle = wxGetApp().preset_bundle;
        for (auto filament_name : preset_bundle->filament_presets) {
            for (auto iter = preset_bundle->filaments.lbegin(); iter != preset_bundle->filaments.end(); iter++) {
                if (filament_name.compare(iter->name) == 0) {
                    std::string display_filament_type;
                    std::string filament_type = iter->config.get_filament_type(display_filament_type);
                    display_materials.push_back(display_filament_type);
                    materials.push_back(filament_type);
                }
            }
        }
    }

    m_topPanel->Layout();
    m_topPanel->Fit();
    Layout();
    Fit();

  
    wxSize screenSize = wxGetDisplaySize();
    auto dialogSize = this->GetSize();


    // basic info
    auto       aprint_stats = m_plater->get_partplate_list().get_current_fff_print().print_statistics();
    wxString   time;
    PartPlate *plate = m_plater->get_partplate_list().get_curr_plate();
    if (plate) {
        if (plate->get_slice_result()) { time = wxString::Format("%s", short_time(get_time_dhms(plate->get_slice_result()->print_statistics.modes[0].time))); }
    }

    char weight[64];
    ::sprintf(weight, "  %.2f g", aprint_stats.total_weight);

    m_stext_time->SetLabel(time);
    m_stext_weight->SetLabel(weight);
}

bool SendToPrinterDialog::Show(bool show)
{
    show_status(PrintDialogStatus::PrintStatusInit);

    // set default value when show this dialog
    if (show) {
        wxGetApp().reset_to_active();
        set_default();
        update_user_machine_list();
    }

    if (show) {
        m_refresh_timer->Start(LIST_REFRESH_INTERVAL);
    } else {
        m_refresh_timer->Stop();
    }

    Layout();
    Fit();
    if (show) { CenterOnParent(); }
    return DPIDialog::Show(show);
}

void SendToPrinterDialog::onNetworkToggled(wxCommandEvent& event)
{

}

void SendToPrinterDialog::updateVisible()
{
    bool hasMachine = !m_machineList.empty();
    //m_errorMsgPanel->Show(!hasMachine);
    m_machineLine->Show(!hasMachine);
    m_machinePanel->Show(hasMachine);
    m_progressPanel->Show(m_is_in_sending_mode);
    m_sendBtn->Enable(hasMachine && !m_is_in_sending_mode);
    
    if (m_machinePanel->IsShown()) {
        m_machinePanel->Layout();
        m_machineSizer->Fit(m_machinePanel);
    }
}

SendToPrinterDialog::~SendToPrinterDialog()
{
    delete m_refresh_timer;
}


}
}