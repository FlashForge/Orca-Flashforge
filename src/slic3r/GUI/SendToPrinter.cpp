#include "SendToPrinter.hpp"
#include <algorithm>
#include <boost/filesystem.hpp>
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
#include "BitmapCache.hpp"
#include "Jobs/ExportSliceJob.hpp"

namespace Slic3r {
namespace GUI {

#define INITIAL_NUMBER_OF_MACHINES 0
#define LIST_REFRESH_INTERVAL 200
#define MACHINE_LIST_REFRESH_INTERVAL 2000


wxDEFINE_EVENT(EVT_MULTI_SEND_COMPLETED, wxCommandEvent);
wxDEFINE_EVENT(EVT_MULTI_SEND_PROGRESS, wxCommandEvent);
MultiSend::MultiSend(wxWindow* event_handler, int sync_num/*=5*/)
    : wxEvtHandler()
    , m_sync_num(sync_num)
    , m_event_handler(event_handler)
{
    //prepare();
    Bind(EVT_EXPORT_SLICE_COMPLETED, &MultiSend::on_export_slice_completed, this);
}

MultiSend::~MultiSend()
{
    cancel_export_job();
    remove_temp_path();
}

bool MultiSend::send_to_printer(int plate_idx, const com_id_list_t& com_ids, const std::string& job_name, bool send_and_print, bool leveling)
{
    BOOST_LOG_TRIVIAL(error) << "begin send_to_printer";
    if (m_is_sending) {
        BOOST_LOG_TRIVIAL(error) << "is sending";
        send_event(-1, _L("MultiSend:send_to_printer, is busy"));
        return false;   
    }
    m_slice_job_name = job_name;
    m_is_sending = true;
    m_send_and_print = send_and_print;
    m_leveling = leveling;
    m_printers.clear();
    m_send_jobs.clear();
    m_plate_idx = plate_idx;
    m_com_ids = com_ids;
    bind_com_event(true);

    for (auto& id : com_ids) {
        bool valid = false;
        MultiComMgr::inst()->devData(id, &valid);
        if (valid) {
            m_printers.emplace_back(id);
            m_send_jobs.emplace(id, ResultInfo{-1, false, Result_Ok, 0.0});
        }
    }
    if (m_printers.empty()) {
        BOOST_LOG_TRIVIAL(error) << "MultiSend: send_to_printer, no valid printer";
        send_event(-1, _L("no valid printer"));
        return false;
    }
    if (!prepare()) {
        for (auto& iter : m_printers) {
            m_send_jobs[iter].finish = true;
            m_send_jobs[iter].result = Result_Fail;
        }
        m_printers.clear();
        send_event(-1, _L("send prepare fail"));
        return false;
    }
#if 0
    for (auto& id : com_ids) {
        m_printers.emplace_back(id);
        m_send_jobs.emplace(id, ResultInfo{-1, false, Result_Ok, 0.0});
    }
    //wxGetApp().plater()->export_gcode(m_slice_path, m_plate_idx);

    m_export_job = std::make_shared<ExportSliceJob>(wxGetApp().plater(), m_slice_path, m_thumb_path, m_plate_idx);
    m_export_job->set_event_handle(this);
    m_export_job->start();
#else
    if (export_temp_file()) {
        if (m_printers.empty()) {
            send_next_job();
        } else {
            int sync_num = (m_sync_num > m_printers.size() ? m_printers.size() : m_sync_num);
            for (int i = 0; i < sync_num; ++i) {
                send_next_job();
            }
            update_progress();
        }
    } else {
        BOOST_LOG_TRIVIAL(error) << "MultiSend: export temp slice/thumb file error";
        for (auto& iter : m_printers) {
            m_send_jobs[iter].finish = true;
            m_send_jobs[iter].result = Result_Fail;
        }
        m_printers.clear();
        send_event(-1, "prepare error");
        return false;
    }
#endif
    BOOST_LOG_TRIVIAL(error) << "end send_to_printer";
    return true;
}

void MultiSend::cancel()
{
    BOOST_LOG_TRIVIAL(info) << "MultiSend: cancel";
    cancel_export_job();
    if (!m_printers.empty()) {
        for (auto& iter : m_printers) {
            m_send_jobs[iter].finish = true;
            m_send_jobs[iter].result = Result_Fail_Canceled;
        }
        m_printers.clear();
    }
    bool cancel_flag = false;
    for (auto& iter : m_send_jobs) {
        if (iter.second.cmd_id > 0 && !iter.second.finish) {
            cancel_flag = true;
            MultiComMgr::inst()->abortSendGcode(iter.first, iter.second.cmd_id);
        }
    }
    m_is_sending = false;
    if (!cancel_flag) {
        send_event(0, "");
    }
}

bool MultiSend::get_multi_send_result(std::map<com_id_t, MultiSend::Result>& result)
{
    result.clear();
    for (auto& iter : m_send_jobs) {
        result.emplace(iter.first, iter.second.result);
    }
    return true;
}

void MultiSend::reset()
{
    //m_com_ids.clear();
    m_printers.clear();
    m_send_jobs.clear();
}

void MultiSend::bind_com_event(bool bind)
{
    if (bind) {
        MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &MultiSend::on_cnnection_exit, this);
        MultiComMgr::inst()->Bind(COM_SEND_GCODE_FINISH_EVENT, &MultiSend::on_send_gcode_finished, this);
        MultiComMgr::inst()->Bind(COM_SEND_GCODE_PROGRESS_EVENT, &MultiSend::on_send_gcode_progress, this);
    } else {
        MultiComMgr::inst()->Unbind(COM_CONNECTION_EXIT_EVENT, &MultiSend::on_cnnection_exit, this);
        MultiComMgr::inst()->Unbind(COM_SEND_GCODE_FINISH_EVENT, &MultiSend::on_send_gcode_finished, this);
        MultiComMgr::inst()->Unbind(COM_SEND_GCODE_PROGRESS_EVENT, &MultiSend::on_send_gcode_progress, this);
    }
}

bool MultiSend::prepare()
{
    auto pid = get_current_pid();
    boost::filesystem::path temp_path(temporary_dir());
    temp_path = temp_path / "orca-flashforge" / "slice";
    try {
        if (!boost::filesystem::exists(temp_path)) {
            if (boost::filesystem::create_directories(temp_path.string())) {
                BOOST_LOG_TRIVIAL(info) << "create orca-flashforge slice path (" << temp_path << ") " << "success";
            } else {
                BOOST_LOG_TRIVIAL(info) << "create orca-flashforge slice path (" << temp_path << ") " << "fail";
                return false;
            }
        }
    } catch (...) {
        BOOST_LOG_TRIVIAL(info) << "create orca-flashforge slice path (" << temp_path << ") " << "exception";
        return false;
    }
    std::stringstream buf;
    buf << pid;
    std::string pidstr = buf.str();
    m_slice_path = (temp_path / (pidstr + ".3mf")).string();
    m_thumb_path = (temp_path / (pidstr + ".png")).string();

    return true;
}

void MultiSend::cancel_export_job()
{
    if (m_export_job) {
        m_export_job->cancel();
        m_export_job->join();
        //m_export_job.reset();
    }
}

void MultiSend::remove_temp_path()
{
    boost::filesystem::path temp_path(temporary_dir());
    try {
        temp_path = temp_path / "orca-flashforge" / "slice";
        boost::filesystem::directory_iterator dir(temp_path);
        for (auto& p : dir) {
            if (boost::filesystem::is_regular_file(p)) {
                bool ret = boost::filesystem::remove_all(p.path().filename());
                BOOST_LOG_TRIVIAL(info) << "remove path (" << p.path().filename() << ") " << (ret ? "success" : "fail");
            }
        }
    } catch (...) {
        BOOST_LOG_TRIVIAL(info) << "remove path (" << temp_path.string() << ") exception";
    }
    m_slice_path = "";
    m_thumb_path = "";
}

bool MultiSend::export_temp_file()
{
    Plater* plater = wxGetApp().plater();
    BOOST_LOG_TRIVIAL(info) << "export_temp_file: start process";
    if (!plater || m_slice_path.empty() || m_thumb_path.empty()) {
        BOOST_LOG_TRIVIAL(error) << "export_temp_file: export slice job fail, parameter invalid";
        return false;
    }
    BOOST_LOG_TRIVIAL(info) << "export_temp_file: slice_path: " << m_slice_path << ", thumb_path: " << m_thumb_path;
    ThumbnailData &data   = plater->get_partplate_list().get_curr_plate()->thumbnail_data;
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
        //image.SaveFile("D:/bbb.png");
        //image.Rescale(256, 256);
        if (image.SaveFile(wxString::FromUTF8(m_thumb_path))) {
            BOOST_LOG_TRIVIAL(info) << "export_temp_file: save thumb (" << m_thumb_path << ") success";
        } else {
            BOOST_LOG_TRIVIAL(error) << "export_temp_file: save thumb (" << m_thumb_path << ") fail";
            return false;
        }
    }
    int result = plater->export_3mf(m_slice_path, SaveStrategy::Silence | SaveStrategy::SplitModel | SaveStrategy::WithGcode | SaveStrategy::SkipModel, m_plate_idx);
    if (result < 0) {
        BOOST_LOG_TRIVIAL(error) << "export_temp_file: export 3mf error: " << result;
        return false;
    }
    BOOST_LOG_TRIVIAL(info) << "export_temp_file: export 3mf success: " << result;
    return true;
}

void MultiSend::send_next_job()
{
    if (m_printers.empty()) {
        bool all_completed = true;
        for (auto& iter : m_send_jobs) {
            if (!iter.second.finish) {
                all_completed = false;
                break;
            }
        }
        if (all_completed) {
            return send_event(0, "finish");
        }
    } else {
        auto com_id = m_printers.front();
        m_printers.pop_front();

        auto cmd = new ComSendGcode(m_slice_path, m_thumb_path, m_slice_job_name, m_send_and_print, m_leveling);
        m_send_jobs[com_id].cmd_id = cmd->commandId();
        m_send_jobs[com_id].progress = 0;
        MultiComMgr::inst()->putCommand(com_id, cmd);
    }
}

void MultiSend::update_progress()
{
    double pre_job_progress = 100.0 / m_send_jobs.size();
    double total_progress = 0;
    BOOST_LOG_TRIVIAL(info) << "update progress: count: " << m_send_jobs.size();
    for (auto& iter : m_send_jobs) {
        if (iter.second.cmd_id > 0) {
            if (iter.second.finish) {
                total_progress += pre_job_progress;
            } else {
                total_progress += pre_job_progress * iter.second.progress;
            }
            BOOST_LOG_TRIVIAL(info) << "progress: " << iter.second.progress;
        }
    }
    BOOST_LOG_TRIVIAL(info) << "update progress: " << total_progress;
    if (m_event_handler) {
        wxCommandEvent event(EVT_MULTI_SEND_PROGRESS);
        event.SetInt((int)total_progress);
        event.SetEventObject(m_event_handler);
        wxPostEvent(m_event_handler, event);
    }
}

void MultiSend::send_event(int code, const wxString& msg)
{
    m_is_sending = false;
    remove_temp_path();
    bind_com_event(false);
    if (m_event_handler) {
        wxCommandEvent event(EVT_MULTI_SEND_COMPLETED);
        event.SetInt(code);
        event.SetString(msg);
        event.SetEventObject(m_event_handler);
        wxPostEvent(m_event_handler, event);
    }
}

void MultiSend::on_cnnection_exit(ComConnectionExitEvent& event)
{
    event.Skip();    
    BOOST_LOG_TRIVIAL(info) << "MultiSend: com connection exit, com_id: " << event.id;
    auto jobIter = m_send_jobs.find(event.id);
    if (jobIter == m_send_jobs.end()) {
        return;
    } else if (!jobIter->second.finish) {
        jobIter->second.finish = true;
        jobIter->second.result = Result_Fail_Network;
        BOOST_LOG_TRIVIAL(info) << "MultiSend: com connection exit, com_id: " << event.id << ", fail";
    }
    auto iter = std::find(m_printers.begin(), m_printers.end(), event.id);
    if (iter != m_printers.end()) {
        m_printers.erase(iter);
        m_send_jobs[event.id].finish = true;
        m_send_jobs[event.id].result = Result_Fail_Network;
        BOOST_LOG_TRIVIAL(info) << "MultiSend: com connection exit, com_id: " << event.id << ", fail";
    }
    flush_logs();
    send_next_job();
    update_progress();
}

void MultiSend::on_send_gcode_finished(ComSendGcodeFinishEvent& event)
{
    event.Skip();
    BOOST_LOG_TRIVIAL(info) << "MultiSend: on_send_gcode_finished";
    auto iter = m_send_jobs.find(event.id);
    if (iter == m_send_jobs.end()) {
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "MultiSend: com send gcode finish, com_id: " << event.id
        << ", " << event.ret;
    flush_logs();
    iter->second.finish = true;
    iter->second.progress = 100;
    switch (event.ret) {
    case COM_OK:
        iter->second.result = Result_Ok;
        break;
    case COM_DEVICE_IS_BUSY:
        iter->second.result = Result_Fail_Busy;
        break;
    case COM_ABORTED_BY_USER:
        iter->second.result = Result_Fail_Canceled;
        break;
    default:
        iter->second.result = Result_Fail;
    }
    send_next_job();
    update_progress();
}

void MultiSend::on_send_gcode_progress(ComSendGcodeProgressEvent& event)
{
    auto iter = m_send_jobs.find(event.id);
    if (iter == m_send_jobs.end()) {
        event.Skip();
        return;
    }
    iter->second.finish = false;
    iter->second.progress = event.now / event.total;
    update_progress();
    event.Skip();
}

void MultiSend::on_export_slice_completed(wxCommandEvent& event)
{
    if (event.GetId() != 0) {
        BOOST_LOG_TRIVIAL(error) << "MultiSend: export temp slice/thumb file error, " << event.GetString();
        send_event(-1, "prepare error");
    } else {
        if (m_printers.empty()) {
            send_next_job();
        } else {
            int sync_num = (m_sync_num > m_printers.size() ? m_printers.size() : m_sync_num);
            for (int i = 0; i < sync_num; ++i) {
                send_next_job();
            }
            update_progress();
        }
    }
    event.Skip();
}


SendToPrinterTipDialog::SendToPrinterTipDialog(wxWindow* parent, const wxStringList& success, const wxStringList& fail, const wxSize &size/*=xDefaultSize*/)
    : TitleDialog(parent, _L("Tip"), 6, size)
{
    Freeze();
    wxBoxSizer* sizer = MainSizer();//new wxBoxSizer(wxVERTICAL);
    sizer->AddSpacer(40);
    if (!success.empty()) {
        wxPanel* panel1 = createListPanel(this, success, true);
        //auto sz = panel1->GetSize();
        //panel1->SetBackgroundColour(wxColour("#ff0000"));
        sizer->Add(panel1, 0, wxEXPAND | wxLEFT | wxRIGHT, 40);
    }

    if (!fail.empty()) {
        if (!success.empty()) {
            wxPanel* line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
            line->SetBackgroundColour(wxColour("#DDDDDD"));
            sizer->AddSpacer(20);
            sizer->Add(line, 0, wxEXPAND | wxLEFT | wxRIGHT, 40);
            sizer->AddSpacer(20);
        }
        wxPanel* panel2 = createListPanel(this, fail, false);
        //auto sz = panel2->GetSize();
        //panel2->SetBackgroundColour(wxColour("#ffff00"));
        sizer->Add(panel2, 0, wxEXPAND | wxLEFT | wxRIGHT, 40);
    }
    sizer->AddSpacer(40);
    sizer->Layout();
    //sizer->Fit(this);
    Layout();
    Fit();
    Thaw();
    //Centre();
    GetSizer()->Fit(this);

    Bind(wxEVT_SHOW, [=](auto& e) { if (e.IsShown()) Layout(); e.Skip(); });
}

wxPanel* SendToPrinterTipDialog::createItem(wxWindow* parent, bool success, const wxString& name)
{
    int sz = FromDIP(16);
    wxPanel* panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, sz));
    panel->SetMinSize(wxSize(-1, sz + 6));
    panel->SetMaxSize(wxSize(-1, sz + 6));
    panel->SetSize(wxSize(-1, sz + 6));
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* nameText = new wxStaticText(panel, wxID_ANY, name);
    nameText->SetForegroundColour(wxColor("#333333"));
    nameText->SetBackgroundColour("#ffffff");
    wxStaticBitmap* bitmap = new wxStaticBitmap(panel, wxID_ANY, create_scaled_bitmap(success ? "ff_complete" : "ff_error", this, 16), wxDefaultPosition, wxSize(sz, sz), 0);
    sizer->Add(bitmap, 0, wxALIGN_CENTER_HORIZONTAL);
    sizer->AddSpacer(10);
    sizer->Add(nameText, 1, wxALIGN_LEFT | wxALIGN_CENTER_HORIZONTAL);
    panel->SetSizer(sizer);
    panel->Layout();
    panel->Fit();
    return panel;
}

wxPanel* SendToPrinterTipDialog::createListPanel(wxWindow* parent, const wxStringList& str_list, bool success_flag)
{
    wxPanel* panel = new wxPanel(parent, wxID_ANY);
    wxString msg = success_flag ? _L("File sent successfully") :
        _L("File sending failure: inconsistent model and slicing configuration or network abnormality");
    wxStaticText* text = new wxStaticText(panel, wxID_ANY, msg);
    text->SetMaxSize(wxSize(FromDIP(680), -1));
    text->Wrap(FromDIP(680));
    text->SetForegroundColour("#333333");
    text->SetBackgroundColour("#ffffff");
    text->Fit();
    auto text_sz = text->GetSize();

    wxScrolledWindow* scroll_window = new wxScrolledWindow(panel, wxID_ANY);
    //scroll_window->SetBackgroundColour(wxColour("#ff0000"));
    scroll_window->EnableScrolling(false, true);
    scroll_window->SetScrollRate(0, 20);

    wxPanel* item_panel = new wxPanel(scroll_window, wxID_ANY);
    int row = (str_list.size() + 1) / 2;
    wxGridSizer* gridSizer = new wxGridSizer(row, 2, 10, 10);
    for (auto& str : str_list) {
        gridSizer->Add(createItem(item_panel, success_flag, str), 1, wxEXPAND | wxALIGN_LEFT);
    }
    item_panel->SetSizer(gridSizer);

    int item_height = FromDIP(16) + 6;
    int vh = row * item_height + (row - 1) * 10;
    if (row > 6) row = 6;
    int height = row * item_height + (row - 1) * 10;
    item_panel->SetMinSize(wxSize(-1, vh));
    item_panel->SetMaxSize(wxSize(-1, vh));
    item_panel->SetSize(wxSize(-1, vh));
    item_panel->Layout();

    scroll_window->SetMinSize(wxSize(-1, height));
    scroll_window->SetMaxSize(wxSize(-1, height));
    scroll_window->SetSize(wxSize(-1, height));
    scroll_window->SetVirtualSize(wxSize(-1, vh));
    wxBoxSizer* scroll_sizer = new wxBoxSizer(wxVERTICAL);
    scroll_sizer->Add(item_panel, 1, wxEXPAND);
    scroll_window->SetSizer(scroll_sizer);
    scroll_window->Layout();

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(text, 1, wxALIGN_LEFT);
    sizer->AddSpacer(20);
    sizer->Add(scroll_window, 1, wxEXPAND);
    sizer->AddSpacer(20);
    panel->SetSizer(sizer);
    int panel_height = text_sz.y + height + 20 * 2;
    panel->SetMinSize(wxSize(-1, panel_height));
    panel->SetMaxSize(wxSize(-1, panel_height));
    panel->SetSize(wxSize(-1, panel_height));
    panel->Layout();
    return panel;
}


std::map<int, wxImage> MachineItem::m_machineBitmapMap;
MachineItem::MachineItem(wxWindow* parent, const MachineData& data)
    : wxPanel(parent, wxID_ANY)
    , m_data(data)
{
    initBitmap();
    build();
}

const MachineItem::MachineData& MachineItem::data() const
{
    return m_data;
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
    SetMinSize(wxSize(FromDIP(46), -1));
    SetMaxSize(wxSize(FromDIP(46), -1));
    SetSize(wxSize(FromDIP(46), -1));
    m_checkBox = new FFCheckBox(this, wxID_ANY);
    m_checkBox->SetValue(false);

    m_iconPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    //SetBackgroundColour("#ffff00");

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
    
    auto iter = m_machineBitmapMap.find(m_data.pid);
    if (iter != m_machineBitmapMap.end()) {
        m_thumbnailPanel->set_thumbnail(iter->second);
    }

    m_nameLbl = new wxStaticText(this, wxID_ANY, m_data.name);
    m_nameLbl->SetMaxSize(wxSize(FromDIP(200), -1));
    m_nameLbl->Wrap(FromDIP(200));
    
    m_mainSizer = new wxBoxSizer(wxHORIZONTAL);
    m_mainSizer->Add(m_checkBox, 0, wxALIGN_CENTER_VERTICAL);
    //m_mainSizer->AddSpacer(FromDIP(5));
    m_mainSizer->Add(m_iconPanel, 0, wxALIGN_CENTER_VERTICAL);
    //m_mainSizer->AddSpacer(FromDIP(5));
    m_mainSizer->Add(m_nameLbl, 1, wxALIGN_CENTER_VERTICAL);
    
    SetSizer(m_mainSizer);
    Layout();
    Fit();
}

void MachineItem::initBitmap()
{
    if (!m_machineBitmapMap.empty()) {
        return;
    }
    m_machineBitmapMap[0x0023] = create_scaled_bitmap("adventurer_5m", 0, 46).ConvertToImage();
    m_machineBitmapMap[0x0024] = create_scaled_bitmap("adventurer_5m_pro", 0, 46).ConvertToImage();
}



wxDEFINE_EVENT(EVT_UPDATE_USER_MACHINE_LIST, wxCommandEvent);
SendToPrinterDialog::SendToPrinterDialog(Plater *plater/*=nullptr*/)
    //: TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY, _L("Send to Printer SD card"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
    : TitleDialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Send printing tasks to"), 6)
    , m_plater(plater), m_export_3mf_cancel(false)
    , m_multiSend(std::make_shared<MultiSend>(this))
{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif //__WINDOWS__

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

    //file name
    //rename normal
    m_rename_switch_panel = new wxSimplebook(m_topPanel);
    m_rename_switch_panel->SetSize(wxSize(FromDIP(300), FromDIP(25)));
    m_rename_switch_panel->SetMinSize(wxSize(FromDIP(300), FromDIP(25)));
    m_rename_switch_panel->SetMaxSize(wxSize(FromDIP(300), FromDIP(25)));

    m_renamePanel = new wxPanel(m_rename_switch_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_renamePanel->SetBackgroundColour(*wxWHITE);
    rename_sizer_v = new wxBoxSizer(wxVERTICAL);
    rename_sizer_h = new wxBoxSizer(wxHORIZONTAL);

    m_renameText = new wxStaticText(m_renamePanel, wxID_ANY, wxT("MyLabel"), wxDefaultPosition, wxDefaultSize, 0);
    m_renameText->SetForegroundColour(*wxBLACK);
    m_renameText->SetFont(::Label::Body_13);
    m_renameText->SetMaxSize(wxSize(FromDIP(280), -1));
    m_renameText->Wrap(FromDIP(280));
    m_renameBtn = new Button(m_renamePanel, "", "ff_editable", wxBORDER_NONE, FromDIP(12));
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
    m_rename_input->SetSize(wxSize(FromDIP(280), FromDIP(24)));
    m_rename_input->SetMinSize(wxSize(FromDIP(280), FromDIP(24)));
    m_rename_input->SetMaxSize(wxSize(FromDIP(280), FromDIP(24)));
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

    m_sizer_material = new wxGridSizer(0, 4, 0, FromDIP(5));
    m_material_panel = new wxPanel(this, wxID_ANY);
    m_material_panel->SetSizer(m_sizer_material);

    auto line_materia = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    line_materia->SetForegroundColour(wxColour("#DDDDDD"));
    line_materia->SetBackgroundColour(wxColour("#DDDDDD"));
    auto line_level = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
    line_level->SetForegroundColour(wxColour("#DDDDDD"));
    line_level->SetBackgroundColour(wxColour("#DDDDDD"));

    m_levelCkb = new FFCheckBox(this);
    m_levelCkb->SetValue(false);
    m_levelLbl = new wxStaticText(this, wxID_ANY, _L("Levelling"));
    m_levelLbl->SetForegroundColour(wxColour("#333333"));

    auto levelSizer = new wxBoxSizer(wxHORIZONTAL);
    levelSizer->Add(m_levelCkb, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));
    levelSizer->Add(m_levelLbl, 0, wxLEFT | wxALIGN_LEFT, FromDIP(10));

    wxPanel* network_panel = new wxPanel(this);
    m_selectPrinterLbl = new wxStaticText(network_panel, wxID_ANY, _L("Select Printer"));
    m_wlanBtn = new FFToggleButton(network_panel, _L("Network"));
    m_wlanBtn->SetBackgroundColour(*wxWHITE);
    m_wlanBtn->SetWindowStyle(m_wlanBtn->GetWindowStyle() | wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL);
    Bind(wxEVT_TOGGLEBUTTON, &SendToPrinterDialog::onNetworkTypeToggled, this);
    m_lanBtn = new FFToggleButton(network_panel, _L("Lan"));
    m_lanBtn->SetWindowStyle(m_wlanBtn->GetWindowStyle() | wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL);
    m_lanBtn->SetBackgroundColour(*wxWHITE);
    Bind(wxEVT_TOGGLEBUTTON, &SendToPrinterDialog::onNetworkTypeToggled, this);
    auto networkLine = new wxPanel(network_panel, wxID_ANY, wxDefaultPosition, wxSize(1, -1), wxTAB_TRAVERSAL);
    networkLine->SetForegroundColour(wxColour("#DDDDDD"));
    networkLine->SetBackgroundColour(wxColour("#DDDDDD"));

    wxBoxSizer* networkSizer = new wxBoxSizer(wxHORIZONTAL);
    networkSizer->Add(m_selectPrinterLbl, 0, wxLEFT | wxALIGN_LEFT | wxALIGN_BOTTOM, FromDIP(10));
    networkSizer->AddStretchSpacer(1);
    networkSizer->Add(m_wlanBtn, 0, wxALIGN_RIGHT | wxALIGN_CENTRE_VERTICAL | wxRIGHT, FromDIP(5));
    networkSizer->Add(networkLine, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(3));
    networkSizer->Add(m_lanBtn, 0, wxALIGN_LEFT | wxALIGN_CENTRE_VERTICAL | wxLEFT, FromDIP(5));
    network_panel->SetSizer(networkSizer);
    network_panel->Layout();

    // machine book
    m_machineBook = new wxSimplebook(this, wxID_ANY);
    //m_machineBook->SetBackgroundColour("#FFFF00");
    
    // machine
    m_machinePanel = new wxPanel(m_machineBook);
    //m_machinePanel->SetBackgroundColour(wxColour("#FAFAFA"));
    //m_machinePanel->SetBackgroundColour(wxColour("#Ff0000"));
    m_selectAll = new FFCheckBox(m_machinePanel, wxID_ANY);
    m_selectAll->Bind(wxEVT_TOGGLEBUTTON, &SendToPrinterDialog::onMachineSelectionToggled, this);
    m_selectAll->SetValue(false);
    m_selectAllLbl = new wxStaticText(m_machinePanel, wxID_ANY, _L("Select All"));
    m_selectAllLbl->SetForegroundColour("#333333");
    auto selectSizer = new wxBoxSizer(wxHORIZONTAL);
    selectSizer->Add(m_selectAll, 0, wxALIGN_LEFT);
    selectSizer->AddSpacer(FromDIP(10));
    selectSizer->Add(m_selectAllLbl, 0, wxALIGN_LEFT);

    m_machineListWindow = new wxScrolledWindow(m_machinePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    //m_machineListWindow->SetBackgroundColour(wxColour("#FF0000"));
    m_machineListWindow->EnableScrolling(false, true);
    m_machineListWindow->SetScrollRate(0, 10);
    //m_machineListWindow->SetSize(-1, 236);
    //m_machineListWindow->SetMinSize(wxSize(-1, 236));
    //m_machineListWindow->SetMaxSize(wxSize(-1, 236));
    //m_machineListWindow->SetVirtualSize(-1, 236);

    m_machineListPanel = new wxPanel(m_machineListWindow, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    m_machineListSizer = new wxGridSizer(2);
    m_machineListSizer->SetHGap(FromDIP(30));
    m_machineListSizer->SetVGap(FromDIP(10));
    m_machineListPanel->SetSizer(m_machineListSizer);

    auto window_sizer = new wxBoxSizer(wxVERTICAL);
    window_sizer->Add(m_machineListPanel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
    m_machineListWindow->SetSizer(window_sizer);
   
    m_machineSizer = new wxBoxSizer(wxVERTICAL);
    m_machineSizer->AddSpacer(FromDIP(20));
    m_machineSizer->Add(selectSizer, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(10));
    m_machineSizer->AddSpacer(FromDIP(10));
    m_machineSizer->Add(m_machineListWindow, 0, wxEXPAND);
    m_machineSizer->AddSpacer(FromDIP(10));
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
    m_noMachineText = new wxStaticText(m_noMachinePanel, wxID_ANY, _L("No printer connected, please connect printer first!"), wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_noMachineText->SetForegroundColour(wxColour("#FB4747"));
    m_noMachineText->SetMaxSize(wxSize(FromDIP(380), -1));
    wxBoxSizer* textSizer = new wxBoxSizer(wxHORIZONTAL);
    textSizer->AddSpacer(FromDIP(10));
    textSizer->Add(m_noMachineBitmap, 0, wxALIGN_CENTER);
    textSizer->AddSpacer(FromDIP(10));
    textSizer->Add(m_noMachineText, 1, wxALIGN_CENTER_VERTICAL);

    auto noMachineSizer = new wxBoxSizer(wxVERTICAL);
    noMachineSizer->AddSpacer(FromDIP(10));
    noMachineSizer->Add(m_machineLine, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(3));
    noMachineSizer->AddSpacer(FromDIP(20));
    noMachineSizer->Add(textSizer, 0, wxALIGN_LEFT);
    noMachineSizer->AddSpacer(FromDIP(50));
    m_noMachinePanel->SetSizer(noMachineSizer);
    m_noMachinePanel->Layout();
    //m_noMachinePanel->Fit();
    m_machineBook->AddPage(m_noMachinePanel, wxEmptyString, false);

    // send book: 0: send panel, 1: progress panel
    m_sendBook = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, wxSize(-1, -1));

    // send panel
    m_sendPanel = new wxPanel(m_sendBook, wxID_ANY, wxDefaultPosition, wxSize(-1, 0));

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
    m_errorPanel->Show(false);

    m_sendBtn = new FFButton(m_sendPanel, wxID_ANY, _L("Send"), FromDIP(4), false);
    m_sendBtn->Bind(wxEVT_BUTTON, &SendToPrinterDialog::onSendClicked, this);
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
    wxBoxSizer* sendSizer = new wxBoxSizer(wxVERTICAL);
    sendSizer->Add(m_errorPanel, 0, wxEXPAND | wxALIGN_LEFT);
    sendSizer->AddSpacer(FromDIP(10));
    sendSizer->Add(m_sendBtn, 0, wxALIGN_CENTER_HORIZONTAL);
    m_sendPanel->SetSizer(sendSizer);
    m_sendPanel->Layout();
    m_sendBook->AddPage(m_sendPanel, wxEmptyString, true);

    // progress
    m_progressPanel = new wxPanel(m_sendBook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
    m_progressBar = new ProgressBar(m_progressPanel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, FromDIP(8)));
    m_progressBar->ShowNumber(false);
    m_progressBar->SetValue(50);
    m_progressInfoLbl = new wxStaticText(m_progressPanel, wxID_ANY, wxEmptyString);
    m_progressInfoLbl->SetMaxSize(wxSize(FromDIP(500), -1));
    m_progressInfoLbl->Wrap(FromDIP(500));
    m_progressLbl = new wxStaticText(m_progressPanel, wxID_ANY, wxEmptyString);
    m_progressCancelBtn = new FFButton(m_progressPanel, wxID_ANY, _L("Cancel"), FromDIP(4), true);
    //m_progressCancelBtn->SetMinSize(wxSize(FromDIP(50), FromDIP(24)));

    m_progressCancelBtn->Bind(wxEVT_BUTTON, &SendToPrinterDialog::on_cancel, this);
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
    m_sendBook->Layout();

    // main layout
    m_sizer_main->AddSpacer(FromDIP(10));
    m_sizer_main->Add(m_topPanel, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->AddSpacer(FromDIP(6));
    m_sizer_main->Add(m_material_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(40));
    m_sizer_main->AddSpacer(FromDIP(12));
    m_sizer_main->Add(line_materia, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->AddSpacer(FromDIP(12));
    m_sizer_main->Add(levelSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->AddSpacer(FromDIP(12));
    m_sizer_main->Add(line_level, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->AddSpacer(FromDIP(12));
    m_sizer_main->Add(network_panel, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->AddSpacer(FromDIP(12));
    m_sizer_main->Add(m_machineBook, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(30));
    m_sizer_main->AddSpacer(FromDIP(10));
    m_sizer_main->Add(m_sendBook, 0, wxEXPAND | wxALIGN_LEFT | wxLEFT | wxRIGHT, FromDIP(40));
    m_sizer_main->AddSpacer(FromDIP(45));

    m_redirect_timer = new wxTimer();
    //m_redirect_timer->SetOwner(this);

    //SetSizer(m_sizer_main);
    Layout();
    Fit();
    Thaw();

    init_bind();
    // CenterOnParent();
    Centre(wxBOTH);
    wxGetApp().UpdateDlgDarkUI(this);
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

void SendToPrinterDialog::update_print_error_info(int code, std::string msg, std::string extra)
{
    m_print_error_code = code;
    m_print_error_msg = msg;
    m_print_error_extra = extra;
}

void SendToPrinterDialog::prepare(int print_plate_idx, bool send_and_print)
{
    m_print_plate_idx = print_plate_idx;
    m_send_and_print = send_and_print;
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
    Bind(wxEVT_SIZE, &SendToPrinterDialog::on_size, this);
    Bind(wxEVT_CLOSE_WINDOW, &SendToPrinterDialog::on_close, this);
    //Bind(EVT_UPDATE_USER_MACHINE_LIST, &SendToPrinterDialog::update_printer_list, this);
    MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &SendToPrinterDialog::onConnectionReady, this);
    Bind(EVT_MULTI_SEND_COMPLETED, &SendToPrinterDialog::on_multi_send_completed, this);
    Bind(EVT_MULTI_SEND_PROGRESS, &SendToPrinterDialog::on_multi_send_progress, this);
    m_redirect_timer->Bind(wxEVT_TIMER, &SendToPrinterDialog::on_redirect_timer, this);
    //MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &SendToPrinterDialog::onConnectionExit, this);
    //MultiComMgr::inst()->Bind(COM_SEND_GCODE_FINISH_EVENT, &SendToPrinterDialog::onSendGcodeFinished, this);
    //MultiComMgr::inst()->Bind(COM_SEND_GCODE_PROGRESS_EVENT, &SendToPrinterDialog::onSendGcodeProgress, this);
}

void SendToPrinterDialog::update_user_machine_list()
{
    m_selectAll->SetValue(false);
    m_machineListMap.clear();
    com_id_list_t idList = MultiComMgr::inst()->getReadyDevList();
    if (!idList.empty()) {
        bool valid = false;
        for (auto id : idList) {
            auto data = MultiComMgr::inst()->devData(id, &valid);
            if (valid) {
                std::string dev_id, status;      
                MachineItem::MachineData mdata;
                mdata.comId = id;
                mdata.flag = data.connectMode;
                if (COM_CONNECT_LAN == data.connectMode) {
                    dev_id = data.lanDevInfo.serialNumber;
                    mdata.pid = data.lanDevInfo.pid;
                    mdata.name = wxString::FromUTF8(data.devDetail->name);
                    status = data.devDetail->status;
                } else if (COM_CONNECT_WAN == data.connectMode) {
                    dev_id = data.wanDevInfo.serialNumber;
                    mdata.pid = data.devDetail->pid;
                    mdata.name = wxString::FromUTF8(data.devDetail->name);
                    status = data.wanDevInfo.status;
                }
                if (!status.empty() && status != "offline") {
                    auto iter = m_machineListMap.find(dev_id);
                    if (iter == m_machineListMap.end()) {
                        m_machineListMap.emplace(dev_id, mdata);    
                    } else if (COM_CONNECT_LAN == data.connectMode) {
                        iter->second = mdata;
                    }
                }
            } else {
                BOOST_LOG_TRIVIAL(warning) << "com_id (" << id << "): get com data error";
            }
        }
    }
    //wxCommandEvent event(EVT_UPDATE_USER_MACHINE_LIST);
    //event.SetEventObject(this);
    //wxPostEvent(this, event);
    update_user_printer();
}

std::vector<std::string> SendToPrinterDialog::sort_string(std::vector<std::string> strArray)
{
    std::vector<std::string> outputArray;
    std::sort(strArray.begin(), strArray.end());
    std::vector<std::string>::iterator st;
    for (st = strArray.begin(); st != strArray.end(); st++) { outputArray.push_back(*st); }

    return outputArray;
}

void SendToPrinterDialog::update_user_printer()
{
    Freeze();
    clear_machine_list();
    m_selectAll->SetValue(false);
    int index = 1;
    bool wlanFlag = false, lanFlag = false;
    if (!m_machineListMap.empty()) {
        size_t cnt = m_machineListMap.size();
        size_t rows = (cnt + 1) / 2;
        m_machineListSizer->SetRows(rows);
        for (auto& m : m_machineListMap) {
            if (m.second.flag == COM_CONNECT_WAN) {
                wlanFlag = true;
                if (!m_wlanBtn->GetValue()) continue;
            } else if (m.second.flag == COM_CONNECT_LAN) {
                lanFlag = true;
                if (!m_lanBtn->GetValue()) continue;
            }
            auto mitem = new MachineItem(m_machineListPanel, m.second);
            mitem->Bind(wxEVT_TOGGLEBUTTON, &SendToPrinterDialog::onMachineSelectionToggled, this);
            m_machineListSizer->Add(mitem, 1, wxEXPAND | wxALIGN_LEFT);
            mitem->SetChecked(false);
            m_machineItemList.emplace_back(mitem);
        }
        int vh = rows * FromDIP(46) + (rows - 1) * FromDIP(10);
        rows = (rows > 4) ? 4 : rows;
        int height = rows * FromDIP(46) + (rows -1) * FromDIP(10);
        m_machineListPanel->SetSize(-1, height);
        //m_machineListPanel->SetMinSize(wxSize(-1, height));
        //m_machineListPanel->SetMaxSize(wxSize(-1, height));
        m_machineListPanel->SetSize(wxSize(-1, vh));
        m_machineListWindow->SetMinSize(wxSize(-1, height));
        m_machineListWindow->SetMaxSize(wxSize(-1, height));
        m_machineListWindow->SetSize(wxSize(-1, height));
        m_machineListWindow->SetVirtualSize(-1, vh);
        int sh = m_selectAll->GetSize().y > m_selectAllLbl->GetSize().y
            ? m_selectAll->GetSize().y : m_selectAllLbl->GetSize().y;
        int hh = sh + height +FromDIP(40);
        m_machinePanel->SetMinSize(wxSize(-1, hh));
        m_machinePanel->SetMaxSize(wxSize(-1, hh));
        m_machinePanel->SetSize(wxSize(-1, hh));
        m_machineBook->SetMinSize(wxSize(-1, hh));
        m_machineBook->SetMaxSize(wxSize(-1, hh));
        m_machineBook->SetSize(wxSize(-1, hh));
        m_machineListPanel->Layout();
        m_machineListWindow->Layout();
        m_machinePanel->Layout();
        m_machineBook->Layout();
        //m_machinePanel->Fit();
        //m_machineListWindow->Fit();
        //m_machineListPanel->Fit();
        index = 0;
    } else {
        m_machineListWindow->SetSize(-1, 1);
        m_machineListWindow->Layout();
        m_noMachinePanel->Fit();
        //m_machineBook->SetBackgroundColour(wxColour("#ff0000"));
    }
    //m_wlanBtn->Enable(wlanFlag);
    //m_lanBtn->Enable(lanFlag);
    if (m_machineBook->GetSelection()!= index) {
        m_machineBook->SetSelection(index);
        m_machineBook->InvalidateBestSize();
        auto panel = m_machineBook->GetCurrentPage();
        //auto sz = panel->GetBestSize().GetHeight();
        //m_machineBook->SetSize(-1, panel->GetBestSize().GetHeight());
        panel->Layout();
        panel->Fit();
        auto sz = panel->GetBestSize();
        m_machineBook->SetMinSize(wxSize(-1, sz.GetHeight()));
        m_machineBook->SetMaxSize(wxSize(-1, sz.GetHeight()));
        m_machineBook->SetSize(-1, sz.GetHeight());
        m_machineBook->Layout();
        m_machineBook->Fit();
    }
    //m_machineBook->Layout();
    //m_machineBook->Fit();
    //m_sendPanel->Layout();
    //m_sendBook->Layout();
    //updateVisible();
    Layout();
    Fit();
    //MainSizer()->Fit(this);
    Thaw();
    updateSendButtonState();
    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "for send task, current printer id =  " << m_printer_last_select << std::endl;
}

void SendToPrinterDialog::clear_machine_list()
{
    m_machineListSizer->Clear();
    for (auto &iter : m_machineItemList) {
        iter->Destroy();
    }
    m_machineItemList.clear();
}

void SendToPrinterDialog::update_printer_list(wxCommandEvent &event)
{
    update_user_printer();
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
    m_need_redirect = false;
    m_is_in_sending_mode = false;
    m_wlanBtn->SetValue(true);
    m_lanBtn->SetValue(true);
    m_rename_switch_panel->SetSelection(0);
    m_machineBook->SetSelection(0);
    m_sendBook->SetSelection(0);
    m_progressBar->SetProgress(0);
    m_progressInfoLbl->SetLabel(wxEmptyString);
    m_progressLbl->SetLabel("0%");
    m_progressInfoLbl->SetForegroundColour(wxColour("#333333"));

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
        //image.SaveFile("D:/aaa.png");
        m_thumbnailPanel->set_thumbnail(image);
    }
    
    //for black list
    std::vector<std::string> materials;
    std::vector<std::string> brands;
    std::vector<std::string> display_materials;

    auto preset_bundle = wxGetApp().preset_bundle;
    for (auto filament_name : preset_bundle->filament_presets) {
        for (auto iter = preset_bundle->filaments.lbegin(); iter != preset_bundle->filaments.end(); iter++) {
            if (filament_name.compare(iter->name) == 0) {
                std::string display_filament_type;
                std::string filament_type = iter->config.get_filament_type(display_filament_type);
                display_materials.push_back(display_filament_type);
                materials.push_back(filament_type);

                if (iter->vendor && !iter->vendor->name.empty())
                    brands.push_back(iter->vendor->name);
                else
                    brands.push_back("");
            }
        }
    }

    //init MaterialItem
    auto        extruders = wxGetApp().plater()->get_partplate_list().get_curr_plate()->get_used_extruders();
    BitmapCache bmcache;

    MaterialHash::iterator iter = m_materialList.begin();
    while (iter != m_materialList.end()) {
        int       id = iter->first;
        Material* item = iter->second;
        item->item->Destroy();
        delete item;
        iter++;
    }
    m_sizer_material->Clear();
    m_materialList.clear();
    m_filaments.clear();

    for (auto i = 0; i < extruders.size(); i++) {
        auto          extruder = extruders[i] - 1;
        auto          colour   = wxGetApp().preset_bundle->project_config.opt_string("filament_colour", (unsigned int) extruder);
        unsigned char rgb[4];
        bmcache.parse_color4(colour, rgb);

        auto          colour_rgb = wxColour((int) rgb[0], (int) rgb[1], (int) rgb[2], (int) rgb[3]);
        if (extruder >= materials.size() || extruder < 0 || extruder >= display_materials.size())
            continue;

        MaterialItem* item = new MaterialItem(m_material_panel, colour_rgb, _L(display_materials[extruder]));
        m_sizer_material->Add(item, 0, wxALL, FromDIP(4));

        Material* material_item = new Material();
        material_item->id = extruder;
        material_item->item = item;
        m_materialList[i] = material_item;

        // build for ams mapping
        if (extruder < materials.size() && extruder >= 0) {
            FilamentInfo info;
            info.id = extruder;
            info.type = materials[extruder];
            info.brand = brands[extruder];
            info.color = wxString::Format("#%02X%02X%02X%02X", colour_rgb.Red(), colour_rgb.Green(), colour_rgb.Blue(), colour_rgb.Alpha()).ToStdString();
            m_filaments.push_back(info);
        }
    }

    if (extruders.size() <= 4) {
        m_sizer_material->SetCols(extruders.size());
    }
    else {
        m_sizer_material->SetCols(4);
    }
    m_material_panel->Layout();
    m_material_panel->Fit();

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

void SendToPrinterDialog::redirect_window()
{
    if (m_msg_window) {
        m_need_redirect = true;
        return;
        //m_msg_window->Show(false);
        //m_msg_window->Destroy();
        //m_msg_window = nullptr;
    }
    if (!IsVisible()) return;
    if (m_send_error) {
        m_sendBook->SetSelection(0);
        Layout();
    } else {
        EndModal(wxID_OK);
        //wxGetApp().mainframe->select_tab(size_t(MainFrame::tpMonitor));
        const auto& com_ids = m_multiSend->com_ids();
        if (!com_ids.empty()) {
            int redirect_type = EVT_SWITCH_TO_DEVICE_LIST;
            com_id_t com_id = -1;
            if (com_ids.size() == 1) {
                redirect_type = EVT_SWITCH_TO_DEVICE_STATUS;
                com_id = com_ids[0];
            }
            wxGetApp().mainframe->jump_to_monitor(redirect_type,  com_id);
        }
    }
}

bool SendToPrinterDialog::Show(bool show)
{
    BOOST_LOG_TRIVIAL(error) << "SendToPrinterDialog::Show, " << show ? "show" : "no show";
    if (show) {
        // set default value when show this dialog
        Bind(EVT_UPDATE_USER_MACHINE_LIST, &SendToPrinterDialog::update_printer_list, this);
        wxGetApp().reset_to_active();
        set_default();
        update_user_machine_list();
        Layout();
        Fit();
        CenterOnParent();
        Refresh();
    } else {
        Unbind(EVT_UPDATE_USER_MACHINE_LIST, &SendToPrinterDialog::update_printer_list, this);
    }
    return DPIDialog::Show(show);
}

void SendToPrinterDialog::on_close(wxCloseEvent& event)
{
    if (m_is_in_sending_mode && !m_msg_window) {
        m_msg_window = new MessageDialog(nullptr, _L("Sending task, do you want to cancel it?"), _L("Warning"), wxYES_NO);
        if (wxID_YES == m_msg_window->ShowModal()) {
            if (m_multiSend) {
                m_multiSend->cancel();
            }
            m_is_in_sending_mode = false;
        }
    }
    event.Skip();
}

void SendToPrinterDialog::on_size(wxSizeEvent& event)
{
    event.Skip();
    return;
#if 0
    int width = event.GetSize().GetWidth() - 190;
    wxSize sz(width, FromDIP(24));
    m_rename_switch_panel->SetSize(sz);
    m_thumbnailPanel->SetMinSize(sz);
    m_thumbnailPanel->SetMaxSize(sz); 
    m_renameText->SetSize(sz);
    m_renameText->SetMinSize(sz);
    m_renameText->SetMaxSize(sz);
    m_rename_input->SetSize(sz);
    m_rename_input->SetMinSize(sz);
    m_rename_input->SetMaxSize(sz);
#endif
}

void SendToPrinterDialog::onNetworkTypeToggled(wxCommandEvent& event)
{
    if (event.GetId() == m_wlanBtn->GetId() || event.GetId() == m_lanBtn->GetId()) {
        update_user_printer();
    }
}

void SendToPrinterDialog::onMachineSelectionToggled(wxCommandEvent& event)
{
    if (event.GetId() == m_selectAll->GetId()) {
        for (auto &item : m_machineItemList) {
            item->SetChecked(event.IsChecked());
        }
        m_selectAll->SetValue(event.IsChecked());
    } else {
        bool all_select = true;
        for (auto& iter : m_machineItemList) {
            if (!iter->IsChecked()) {
                all_select = false;
                break;
            }
        }
        m_selectAll->SetValue(all_select);
    }
    updateSendButtonState();
}

void SendToPrinterDialog::onSendClicked(wxCommandEvent& event)
{
    BOOST_LOG_TRIVIAL(error) << "begin send button clicked";
    m_is_in_sending_mode = true;
    com_id_list_t com_ids;
    for (auto& iter : m_machineItemList) {
        if (iter->IsChecked()) {
            com_ids.emplace_back(iter->data().comId);
            //m_sendJobMap.emplace(iter->data().comId, SendJobInfo{0, Result_None, 0.0});
        }
    }
    wxString job_name = m_renameText->GetLabel();
    if (job_name.empty()) {
        job_name = _L("Untitled");
    }
    if (!job_name.EndsWith(".3mf")) {
        job_name += ".3mf";
    }
    int ret = m_multiSend->send_to_printer(m_print_plate_idx, com_ids, job_name.ToUTF8().data(), m_send_and_print, m_levelCkb->GetValue());
    if (!ret) {
        m_is_in_sending_mode = false;
        update_user_machine_list();
        BOOST_LOG_TRIVIAL(info) << "send_to_printer error";
        return;
    } else {
        BOOST_LOG_TRIVIAL(error) << "send_to_printer success";
        m_progressInfoLbl->SetLabel(_L("Preparing printing task"));
        m_progressInfoLbl->SetForegroundColour(wxColour("#333333"));
        m_progressBar->SetValue(0);
        m_progressCancelBtn->Enable(true);
        m_progressCancelBtn->Show(true);
        m_progressLbl->SetLabel("0%");
        m_sendBook->SetSelection(1);
        m_sendBook->Layout();
        Layout();
    }
}

void SendToPrinterDialog::on_cancel(wxCommandEvent& event)
{
    if (!m_is_in_sending_mode) {
        return;
    }
    if (!m_msg_window) {
        m_msg_window = new MessageDialog(nullptr, _L("Sending task, do you want to cancel it?"), _L("Warning"), wxYES_NO);
        if (m_is_in_sending_mode && wxID_YES == m_msg_window->ShowModal()) {
            m_multiSend->cancel();
            if (m_progressInfoLbl->IsShown()) {
                m_progressInfoLbl->SetLabel(_L("Canceling the print job, please wait"));
            }
            m_progressCancelBtn->Enable(false);
        }
        m_msg_window->Destroy();
        m_msg_window = nullptr;
        if (m_need_redirect) {
            redirect_window();
            m_need_redirect = false;
        }
    }
}

void SendToPrinterDialog::on_redirect_timer(wxTimerEvent& event)
{
    if (m_msg_window) {
        m_need_redirect = true;
    } else {
        redirect_window();
    }
    event.Skip();
}

void SendToPrinterDialog::onConnectionReady(ComConnectionReadyEvent& event)
{
    if (!m_is_in_sending_mode) {
        update_user_machine_list();
    }
    event.Skip();
}

void SendToPrinterDialog::on_multi_send_progress(wxCommandEvent& event)
{
    if (m_is_in_sending_mode) {
        int intv = event.GetInt();
        wxString str = event.GetString();
        m_progressBar->SetValue(event.GetInt());
        m_progressLbl->SetLabel(wxString::Format("%d", event.GetInt()) + "%");
    }
    event.Skip();
}

void SendToPrinterDialog::on_multi_send_completed(wxCommandEvent& event)
{
    BOOST_LOG_TRIVIAL(info) << "SendToPrinterDialog: receive multi send completed";
    m_is_in_sending_mode = false;
    std::map<com_id_t, MultiSend::Result> send_result;
    m_multiSend->get_multi_send_result(send_result);
    if (send_result.empty()) {
        m_multiSend->reset();
        BOOST_LOG_TRIVIAL(error) << "SendToPrinterDialog: result is empty";
        return;
    }
    if (m_msg_window && m_msg_window->IsShown()) {
        m_msg_window->EndModal(wxID_NO);
    }
    BOOST_LOG_TRIVIAL(info) << "SendToPrinterDialog: receive multi send completed 2";
    m_progressCancelBtn->Show(false);
    m_progressBar->SetValue(100);
    m_progressLbl->SetLabel("100%");
    if (send_result.size() == 1) {
        auto iter = send_result.begin();
        if (iter->second == Result_Ok) {
            //m_progressBar->SetValue(100);
            //m_progressLbl->SetLabel("100%");
            m_progressInfoLbl->SetLabel(_L("Send completed, automatically redirected to device status"));
            m_progressInfoLbl->SetForegroundColour(wxColour("#333333"));
            m_send_error = false;
            //m_sendBook->SetSelection(0);
            m_redirect_timer->StartOnce(3000);
        } else {
            if (iter->second == Result_Fail_Busy) {
                m_progressInfoLbl->SetLabel(_L("The printer is busy and cannot receive printing commands."));
                m_progressInfoLbl->SetForegroundColour(wxColour("#FB4747"));
            } else if (iter->second == Result_Fail_Canceled) {
                m_progressInfoLbl->SetLabel(_L("Canceling the print job successfully."));
                m_progressInfoLbl->SetForegroundColour(wxColour("#333333"));
            } else {
                m_progressInfoLbl->SetLabel(_L("Send failed, please check network or device status"));
                m_progressInfoLbl->SetForegroundColour(wxColour("#FB4747"));
            }
            m_send_error = true;
            m_redirect_timer->StartOnce(3000);
        }
        //m_progressPanel->Layout();
        //auto sz = m_progressInfoLbl->GetSize();
        //auto y = sz.y;
        auto psz = m_progressPanel->GetSize();
        m_progressInfoLbl->SetMaxSize(wxSize(psz.x, -1));
        m_progressInfoLbl->SetMaxSize(wxSize(psz.x, -1));
        m_progressInfoLbl->Wrap(m_progressInfoLbl->GetSize().x);
        m_progressInfoLbl->Fit();
        //sz = m_progressInfoLbl->GetSize();
        //sz = m_progressInfoLbl->GetSize();
        m_progressPanel->Fit();
        //psz = m_progressPanel->GetSize();

        //EndModal(wxID_OK);
        //wxGetApp().mainframe->select_tab(size_t(MainFrame::tpMonitor));
    } else {
        m_progressInfoLbl->SetLabel(_L("Send completed"));
        m_progressInfoLbl->SetForegroundColour(wxColour("#333333"));
        m_progressPanel->Layout();

        wxStringList successList, failList;
        for (auto& iter : send_result) {
            wxString name;
            bool valid = false;
            auto data = MultiComMgr::inst()->devData(iter.first, &valid);
            if (valid) {
                std::string dev_id = (COM_CONNECT_LAN == data.connectMode) ? data.lanDevInfo.serialNumber : data.wanDevInfo.serialNumber;
                auto res = m_machineListMap.find(dev_id);
                if (res != m_machineListMap.end()) {
                    name = res->second.name;
                } else {
                    name = wxString::FromUTF8((COM_CONNECT_LAN == data.connectMode) ? data.devDetail->name : data.wanDevInfo.name);
                }
            } else {
                for (const auto& it : m_machineListMap) {
                    if (it.second.comId == iter.first) {
                        name = it.second.name;
                        break;
                    }
                }
            }
            (iter.second == MultiSend::Result_Ok) ? successList.Add(name) : failList.Add(name);
        }
        //if (successList.size() == send_result.size()) {
        //m_progressBar->SetValue(100);
        //m_progressLbl->SetLabel("100%");
        //}
        BOOST_LOG_TRIVIAL(info) << "Send multi job completed";
        SendToPrinterTipDialog dlg(this, successList, failList);
        dlg.ShowModal();
        if (failList.empty()) {
            m_send_error = false;
            //if (m_msg_window) {
                //m_msg_window->EndModal(0);
            //    m_need_redirect = true;
            //} else {
            //    redirect_window();
                //wxGetApp().mainframe->select_tab(size_t(MainFrame::tpMonitor));
            //}
            redirect_window();
        } else {
            m_sendBook->SetSelection(0);
        }
    }
    m_multiSend->reset();
    Layout();
    Fit();
    GetSizer()->Fit(this);
    Refresh();
}

void SendToPrinterDialog::onConnectionExit(ComConnectionExitEvent& event)
{
    if (!m_is_in_sending_mode) {
        update_user_machine_list();
    }
    event.Skip();
}

void SendToPrinterDialog::updateVisible()
{
    bool hasMachine = !m_machineListMap.empty();
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

void SendToPrinterDialog::updateSendButtonState()
{
    bool enable = false;
    for (auto& item : m_machineItemList) {
        if (item->IsChecked()) {
            enable = true;
            break;
        }
    }
    m_sendBtn->Enable(enable);
}

SendToPrinterDialog::~SendToPrinterDialog()
{
    delete m_redirect_timer;
}

}
}