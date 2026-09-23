#include "SingleDeviceState.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/BitmapCache.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include <slic3r/GUI/Widgets/WebView.hpp>
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/MultiComHelper.hpp"
#include "slic3r/GUI/FlashForge/PrintDevLocalFileDlg.hpp"
#include "slic3r/GUI/FlashForge/PrinterErrorMsgDlg.hpp"
#include <nlohmann/json.hpp>
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Plater.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <wx/dcgraph.h>
#include <chrono>
#include <functional>
#include <sstream>
using namespace std::literals;
using json   = nlohmann::json;
namespace pt = boost::property_tree;

// 临时诊断日志：与 [FFRTMP] 一起输出到 VS 调试窗口，定位“设备断线”根因，定位后可撤�?
// [CAMDBG] 调试日志总开关：�?false 关闭全部 [CAMDBG] 输出（需要排查时改回 true）�?
static const bool g_camdbg_log_enabled = false;

static void camdbg_log(const std::string &msg)
{
    if (!g_camdbg_log_enabled) return;
    BOOST_LOG_TRIVIAL(info) << "[CAMDBG] " << msg;
#ifdef _WIN32
    OutputDebugStringA(("[CAMDBG] " + msg + "\n").c_str());
#endif
}

// [UNBIND] 解绑耗时诊断：每条日志带 steady_clock 毫秒时间戳，用于定位设备解绑
// 卡在哪一段（相邻两条日志的时间戳差即该段耗时）。定位后把开关置 false 即可关闭�?
// Temporary diagnostic for printers whose camera is installed after the
// printer was manufactured. This is independent from the verbose UI log.
static const bool g_camera_capability_log_enabled = false;

static void camera_capability_log(const std::string &msg)
{
    if (!g_camera_capability_log_enabled) return;
    BOOST_LOG_TRIVIAL(info) << "[CAMERA_CAPABILITY] " << msg;
#ifdef _WIN32
    OutputDebugStringA(("[CAMERA_CAPABILITY] " + msg + "\n").c_str());
#endif
}

static const bool g_unbind_timing_enabled = false;
static void unbind_ts(const std::string &tag)
{
    if (!g_unbind_timing_enabled) return;
    int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch()).count();
    std::string line = "[UNBIND] t=" + std::to_string(ms) + "ms | " + tag;
    BOOST_LOG_TRIVIAL(info) << line;
#ifdef _WIN32
    OutputDebugStringA((line + "\n").c_str());
#endif
}

namespace Slic3r {
namespace GUI {

namespace {

constexpr auto FF_DEVICE_PAGE_PRIMARY_TEXT = "#333333";

enum class CameraStreamProtocol { Source, Rtmp };

// Change this single value to compare the device's fixed HLS and RTMP URLs.
static constexpr CameraStreamProtocol g_camera_stream_protocol = CameraStreamProtocol::Rtmp;

std::string switch_camera_stream_url(const std::string& source, CameraStreamProtocol protocol)
{
    if (source.empty()) return source;

    std::string result = source;
    if (protocol == CameraStreamProtocol::Rtmp) {
        if (result.rfind("https://", 0) == 0)
            result.replace(0, 8, "rtmp://");
        else if (result.rfind("http://", 0) == 0)
            result.replace(0, 7, "rtmp://");

        const size_t suffix = result.find(".m3u8");
        if (suffix != std::string::npos) result.erase(suffix, 5);
        const size_t suffix2 = result.find(".flv");
        if (suffix2 != std::string::npos) result.erase(suffix2, 4);
    } else if (protocol == CameraStreamProtocol::Source) {
    }
    return result;
}

void apply_device_page_light_text(wxWindow* window, const wxColour& color)
{
#ifdef __APPLE__
    if (window) {
        window->SetForegroundColour(color);
    }
#else
    (void) window;
    (void) color;
#endif
}

static const bool g_device_detail_layout_log_enabled = false;

void log_device_detail_layout(const std::string& message)
{
    if (!g_device_detail_layout_log_enabled) return;
    BOOST_LOG_TRIVIAL(info) << "[DeviceDetailLayout] " << message;
#ifdef _WIN32
    OutputDebugStringA(("[DeviceDetailLayout] " + message + "\n").c_str());
#endif
}

wxString format_cumulative_filament(double cumulative_filament, ComConnectMode connect_mode)
{
    const double filament_cm = connect_mode == COM_CONNECT_WAN ? cumulative_filament * 100.0 : cumulative_filament / 10.0;
    return wxString::Format("%.0fcm", filament_cm);
}

wxString format_cumulative_print_time(const fnet_dev_detail_t &detail, ComConnectMode connect_mode)
{
    const fnet::FlashNetworkIntfc *intfc = MultiComMgr::inst()->networkIntfc();
    if (intfc != nullptr && !intfc->isLegacyLibrary() && detail.hasCumulativePrintTimeSecond != 0) {
        const double cumulative_seconds = detail.cumulativePrintTimeSecond > 0.0 ? detail.cumulativePrintTimeSecond : 0.0;
        const long long total_minutes   = static_cast<long long>(cumulative_seconds / 60.0);
        if (total_minutes < 60) {
            return wxString::Format(_L("%lld minutes"), total_minutes);
        }

        const long long hours   = total_minutes / 60;
        const long long minutes = total_minutes % 60;
        return wxString::Format(_L("%lld hours %lld minutes"), hours, minutes);
    }

    const double old_hours = connect_mode == COM_CONNECT_LAN ? detail.cumulativePrintTime / 60.0 : detail.cumulativePrintTime;
    return wxString::Format("%.2f hours", old_hours);
}

} // namespace

wxDEFINE_EVENT(EVT_SWITCH_TO_FILETER, wxCommandEvent);

const std::string CLOSE = "close";
const std::string OPEN  = "open";
const std::string CANCEL ="cancel";
const std::string PAUSE  = "pause";
const std::string CONTINUE = "continue";
const std::string CLEAR_PLATFORM = "setClearPlatform";

const std::string OPENSTREAM = "streamCtrl_cmd";

const std::string P_READY = "ready"; 
const std::string P_COMPLETED = "completed";
const std::string P_ERROR     = "error";
const std::string P_PAUSING  = "pausing";
const std::string P_BUSY      = "busy";
const std::string P_HEATING   = "heating";
const std::string P_CALIBRATE = "calibrate_doing";
const std::string P_LOADING = "loading";
const std::string P_CLOUD_SLICING = "cloud_slicing";
const std::string P_SENDING       = "sending";
const std::string P_DOWNLOADING   = "downloading";
const std::string P_UNZIPPING = "unzipping";

const wxString    TEMPERATURE = _L("Temperature");
const wxString    TEMP_CANCEL  = _L("cancel");
const wxString    TEMP_CONFIRM = _L("confirm");

const wxString HAS_NO_PRINTING = "The current device has \nno printing projects";

const wxString HAS_NO_PRINTING_U1 = "Full Power! Create Now!";

const int MATERIAL_PIC_WIDTH  = 80;
const int MATERIAL_PIC_HEIGHT = 80;
const int IDLE_NAME_LENGTH    = 150;
const int PRINT_FILE_INFO_WIDTH = 384;
const int PRINT_FILE_NAME_WIDTH = 320;
const int PRINT_FILE_HEAD_WIDTH_PADDING = 8;

const int FILELIST_PIC_WIDTH = 41;
const int FILELIST_PIC_HEIGHT = 45;

MaterialImagePanel::MaterialImagePanel(wxWindow *parent, const wxSize &size /*=wxDefaultSize*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
{
    Bind(wxEVT_PAINT, &MaterialImagePanel::OnPaint, this);
    // Bind(wxEVT_SIZE, &RoundImagePanel::OnSize, this);
}

void MaterialImagePanel::SetImage(const wxImage &image)
{
    m_image = image;
#ifdef __WIN32__
    Refresh();
#endif
    
}

void MaterialImagePanel::OnSize(wxSizeEvent &event) {}

void MaterialImagePanel::OnPaint(wxPaintEvent &event)
{
    if (!m_image.IsOk()) {
        event.Skip();
        return;
    }

    wxSize  size = GetSize();

    wxImage img  = m_image;
    img.Rescale(size.x, size.y);
    wxPaintDC dc(this);

    //if (!img.HasAlpha()) {
    //    img.InitAlpha();
    //}

    wxBitmap  bmp(size.x, size.y);
    {
        wxMemoryDC memdc;
        memdc.SelectObject(bmp);
#ifdef _WIN32
        memdc.Blit({0, 0}, size, &dc, {0, 0});
#endif
        wxGCDC dc2(memdc);
        dc2.SetFont(GetFont());
        CreateRegion(dc2);
        memdc.SelectObject(wxNullBitmap);
    }
    wxImage ref_img = bmp.ConvertToImage();
    for (int y = 0; y < img.GetHeight(); ++y) {
        for (int x = 0; x < img.GetWidth(); ++x) {
            img.SetAlpha(x, y, ref_img.GetRed(x, y));
        }
    }
    dc.DrawBitmap(wxBitmap(img), 0, 0);
}

void MaterialImagePanel::CreateRegion(wxDC &dc)
{
    wxSize sz = GetSize();
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, sz.x, sz.y);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(*wxRED);
    dc.DrawRoundedRectangle(0, 0, sz.x, sz.y,10);
}

StartFilter::StartFilter(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

StartFilter::~StartFilter()
{
}

void StartFilter::create_panel(wxWindow* parent)
{
    parent->SetMinSize(wxSize(-1, FromDIP(277)));
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* bSizer_filtering_title = new wxBoxSizer(wxHORIZONTAL);

    auto m_panel_filtering_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    m_panel_filtering_title->SetBackgroundColour(wxColour(248, 248, 248));

    //过滤标题
    auto m_staticText_filtering = new wxStaticText(m_panel_filtering_title, wxID_ANY, _L("Start Filtering"));
    m_staticText_filtering->Wrap(-1);
    //m_staticText_filtering->SetFont(wxFont(wxFontInfo(16)));
    m_staticText_filtering->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_filtering_title->Add(m_staticText_filtering, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_filtering_title->Add(0, 1, wxEXPAND, 0);
    m_panel_filtering_title->SetSizer(bSizer_filtering_title);
    m_panel_filtering_title->Layout();
    bSizer_filtering_title->Fit(m_panel_filtering_title);

    //内循环过�?
    wxBoxSizer *bSizer_internal_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel*    internal_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
    auto m_staticText_internal_circulate = new wxStaticText(internal_circulate_panel, wxID_ANY, _L("Internal Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    apply_device_page_light_text(m_staticText_internal_circulate, wxColour(FF_DEVICE_PAGE_PRIMARY_TEXT));
    //m_staticText_internal_circulate->SetFont(wxFont(wxFontInfo(16)));
    m_internal_circulate_switch = new SwitchButton(internal_circulate_panel);
    m_internal_circulate_switch->SetBackgroundColour(*wxWHITE);
    m_internal_circulate_switch->Bind(wxEVT_TOGGLEBUTTON, &StartFilter::onAirFilterToggled, this);

    bSizer_internal_circulate_hor->AddSpacer(FromDIP(17));
    bSizer_internal_circulate_hor->Add(m_staticText_internal_circulate, 0, wxLEFT, 0);
    bSizer_internal_circulate_hor->AddSpacer(FromDIP(7));
    bSizer_internal_circulate_hor->Add(m_internal_circulate_switch, 0, wxLEFT, 0);

    internal_circulate_panel->SetSizer(bSizer_internal_circulate_hor);
    internal_circulate_panel->Layout();
    bSizer_internal_circulate_hor->Fit(internal_circulate_panel);

    //外循环过�?
    wxBoxSizer *bSizer_external_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel*    external_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
    auto m_staticText_external_circulate = new wxStaticText(external_circulate_panel, wxID_ANY, _L("External Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    apply_device_page_light_text(m_staticText_external_circulate, wxColour(FF_DEVICE_PAGE_PRIMARY_TEXT));
    //m_staticText_external_circulate->SetFont(wxFont(wxFontInfo(16)));
    m_external_circulate_switch = new SwitchButton(external_circulate_panel);
    m_external_circulate_switch->SetBackgroundColour(*wxWHITE);
    m_external_circulate_switch->Bind(wxEVT_TOGGLEBUTTON, &StartFilter::onAirFilterToggled, this);

    bSizer_external_circulate_hor->AddSpacer(FromDIP(17));
    bSizer_external_circulate_hor->Add(m_staticText_external_circulate, 0, wxLEFT, 0);
    bSizer_external_circulate_hor->AddSpacer(FromDIP(7));
    bSizer_external_circulate_hor->Add(m_external_circulate_switch, 0, wxLEFT, 0);

    external_circulate_panel->SetSizer(bSizer_external_circulate_hor);
    external_circulate_panel->Layout();
    bSizer_external_circulate_hor->Fit(external_circulate_panel);

    sizer->Add(m_panel_filtering_title, 0, wxALL | wxEXPAND, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(internal_circulate_panel, 0, wxLEFT, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(external_circulate_panel, 0, wxLEFT, 0);
    sizer->AddStretchSpacer();
    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit();  
}

void StartFilter::setAirFilterState(bool internalOpen, bool externalOpen)
{ 
    m_internal_circulate_switch->SetValue(internalOpen);
    m_external_circulate_switch->SetValue(externalOpen);
}

void StartFilter::setCurId(int curId) 
{
    if (curId < 0) {
        return;
    }
    m_cur_id = curId; 
}

void StartFilter::onAirFilterToggled(wxCommandEvent &event)
{
    event.Skip();
    SwitchButton *click_btn   = dynamic_cast<SwitchButton *>(event.GetEventObject());
    std::string inter_state = CLOSE;
    std::string exter_state = CLOSE;

    if (m_internal_circulate_switch) {
        inter_state = m_internal_circulate_switch->GetValue() ? OPEN : CLOSE;
    }

    if (m_external_circulate_switch) {
        exter_state = m_external_circulate_switch->GetValue() ? OPEN : CLOSE;
    }

    if (m_internal_circulate_switch->GetValue() && m_external_circulate_switch->GetValue()) {
        if (click_btn == m_internal_circulate_switch) {
            m_external_circulate_switch->SetValue(false);
            exter_state = CLOSE;
        } else if (click_btn == m_external_circulate_switch) {
            m_internal_circulate_switch->SetValue(false);
            inter_state = CLOSE;
        }
    }
    ComAirFilterCtrl *filterCtrl = new ComAirFilterCtrl(inter_state, exter_state);
    if (m_cur_id >= 0) {
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, filterCtrl);
    }
}

wxDEFINE_EVENT(EVT_MODIFY_TEMP_CLICKED, wxCommandEvent);
wxDEFINE_EVENT(EVT_MODIFY_TEMP_CANCEL_CLICKED, wxCommandEvent);
ModifyTemp::ModifyTemp(wxWindow *parent) 
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

void ModifyTemp::create_panel(wxWindow *parent) 
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *bSizer_temp_title = new wxBoxSizer(wxHORIZONTAL);

    auto m_panel_temp_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    m_panel_temp_title->SetBackgroundColour(wxColour(248, 248, 248));

    // 温度标题
    m_staticText_title = new wxStaticText(m_panel_temp_title, wxID_ANY, TEMPERATURE);
    m_staticText_title->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_temp_title->Add(m_staticText_title, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_temp_title->Add(0, 0, 1, wxEXPAND, 0);
    m_panel_temp_title->SetSizer(bSizer_temp_title);
    m_panel_temp_title->Layout();
    bSizer_temp_title->Fit(m_panel_temp_title);

    // 确认、取消按�?
    //垂直布局、水平布局
    wxBoxSizer *bSizer_operate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel *operate_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(109)), wxTAB_TRAVERSAL);
    m_cancel_btn = new FFButton(operate_panel, wxID_ANY, TEMP_CANCEL);
    m_cancel_btn->SetMinSize(wxSize(FromDIP(64), FromDIP(32)));
    m_cancel_btn->SetFontHoverColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBGHoverColor(wxColour(127, 127, 127));
    m_cancel_btn->SetBorderHoverColor(wxColour(127, 127, 127));

    m_cancel_btn->SetFontPressColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBGPressColor(wxColour(31, 31, 31));
    m_cancel_btn->SetBorderPressColor(wxColour(31, 31, 31));

    m_cancel_btn->SetFontColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBorderColor(wxColour(75, 75, 75));
    m_cancel_btn->SetBGColor(wxColour(75, 75, 75));
    m_cancel_btn->Bind(wxEVT_LEFT_DOWN, [this, operate_panel](wxMouseEvent &event) {
        event.Skip();
        wxCommandEvent ev(EVT_MODIFY_TEMP_CANCEL_CLICKED, GetId());
        ev.SetEventObject(this);
        wxPostEvent(this, ev);
    });

    bSizer_operate_hor->AddStretchSpacer();
    bSizer_operate_hor->Add(m_cancel_btn, 0, wxALIGN_CENTER, 0);
    bSizer_operate_hor->AddSpacer(FromDIP(19));

    m_confirm_btn = new FFButton(operate_panel, wxID_ANY, TEMP_CONFIRM);
    m_confirm_btn->SetMinSize(wxSize(FromDIP(64), FromDIP(32)));
    m_confirm_btn->SetFontHoverColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBGHoverColor(wxColour(149, 197, 255));
    m_confirm_btn->SetBorderHoverColor(wxColour(149, 197, 255));

    m_confirm_btn->SetFontPressColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBGPressColor(wxColour(17, 111, 223));
    m_confirm_btn->SetBorderPressColor(wxColour(17, 111, 223));

    m_confirm_btn->SetFontColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBorderColor(wxColour(50, 141, 251));
    m_confirm_btn->SetBGColor(wxColour(50, 141, 251));
    m_confirm_btn->Bind(wxEVT_LEFT_DOWN, [this, operate_panel](wxMouseEvent &event) {
        event.Skip();
        wxCommandEvent ev(EVT_MODIFY_TEMP_CLICKED, GetId());
        ev.SetEventObject(this);
        wxPostEvent(this, ev);
    });

    bSizer_operate_hor->Add(m_confirm_btn, 0, wxALIGN_CENTER, 0);
    bSizer_operate_hor->AddStretchSpacer();

    operate_panel->SetSizer(bSizer_operate_hor);
    operate_panel->Layout();
    bSizer_operate_hor->Fit(operate_panel);

    sizer->Add(m_panel_temp_title, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(operate_panel, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(87));

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit();
}

void ModifyTemp::setLabel(wxString labelText) 
{
    m_staticText_title->SetLabel(labelText); 
}

DeviceDetail::DeviceDetail(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        event.Skip();
        const wxSize size = event.GetSize();
        if (size != m_last_logged_size) {
            m_last_logged_size = size;
            CallAfter([this] { logLayoutState("size"); });
        }
    });
}

void DeviceDetail::setCurId(int curId) 
{ 
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
}

void DeviceDetail::create_panel(wxWindow* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *bSizer_confirm_row = new wxBoxSizer(wxHORIZONTAL);
    auto confirm_push_btn = new FFPushButton(this, wxID_ANY, "push_button_confirm_normal", 
        "push_button_confirm_hover", "push_button_confirm_press", "push_button_confirm_normal");
    confirm_push_btn->SetBackgroundColour(wxColour(255, 255, 255));
    confirm_push_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) {
        event.Skip();
        double speed;
        double z_axis;
        double nozzle_fan;
        double cooling_fan;
        switchPage();
        if (!FFUtils::isNozzlesPrinter(FFUtils::getPid(m_cur_id))) {
            if (m_device_speed) {
                wxString str_speed = m_device_speed->getTextValue();
                str_speed.ToDouble(&speed);
            }
        } else {
            if (m_device_speed_u1) {
                auto s = static_cast<int>((uintptr_t)m_device_speed_u1->combobox()->GetClientData(m_device_speed_u1->combobox()->GetSelection()));
                speed = s;
            }
        }
        if (m_device_z_axis) {
            m_device_z_axis->getTextValue().ToDouble(&z_axis);
        }
        if (m_device_nozzle_fan) {
            m_device_nozzle_fan->getTextValue().ToDouble(&nozzle_fan);
        }
        if (m_device_cooling_fan) {
            m_device_cooling_fan->getTextValue().ToDouble(&cooling_fan);
        }
        ComPrintCtrl *printCtrl = new ComPrintCtrl(z_axis, speed, nozzle_fan, 0, cooling_fan);
        if (m_cur_id >= 0) {
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, printCtrl);
        }
    });
    sizer->AddSpacer(FromDIP(15));
    bSizer_confirm_row->AddStretchSpacer();
    bSizer_confirm_row->Add(confirm_push_btn, 0, wxRIGHT, FromDIP(20));
    confirm_push_btn->Hide();
    sizer->Add(bSizer_confirm_row, 0, wxEXPAND, 0);
//
    auto m_panel_separotor10 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor10->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor10->SetMinSize(wxSize(-1,FromDIP(20)));

    sizer->Add(m_panel_separotor10);
//
    m_grid_sizer = new wxFlexGridSizer(2, FromDIP(35), 0);
    m_panel_rows = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);

    m_device_material            = new IconText(m_panel_rows, wxString("device_material"), 16, wxString("PLA-12345678901234567890"), 12);
    m_device_layer = new IconText(m_panel_rows, wxString("device_layer"), 16, wxString("1000000/15000000"), 12);
    m_device_initial_speed = new IconText(m_panel_rows, wxString("device_initial_speed"), 16, wxString("1000000mm/s"), 12);
    m_device_fill_rate = new IconText(m_panel_rows, wxString("device_fill_rate"), 16, wxString("200%"), 12);
    m_device_speed = new IconBottonText(m_panel_rows, wxString("device_speed"), 16, wxString("90"), 12);
    m_device_speed->setLimit(50, 150);
    m_device_speed->setAdjustValue(10);
    m_device_speed_u1 = new MachineIconCombo(m_panel_rows, wxID_ANY, "device_speed", 16, wxDefaultPosition, wxDefaultSize);
    m_device_speed_u1->combobox()->Append(_L("Silent Mode (50%)")   , (void*) 50);
    m_device_speed_u1->combobox()->Append(_L("Standard Mode (100%)"), (void*) 100);
    m_device_speed_u1->combobox()->Append(_L("Motion Mode (140%)")  , (void*) 140);
    m_device_speed_u1->combobox()->SetSelection(0);
    m_device_speed_u1->Hide();
    m_device_nozzle_fan = new IconBottonText(m_panel_rows, wxString("device_nozzle_fan"), 16, wxString("50"), 12);
    m_device_nozzle_fan->setLimit(0, 100);
    m_device_nozzle_fan->setAdjustValue(10);
    m_device_z_axis = new IconBottonText(m_panel_rows, wxString("device_z_axis"), 16, wxString("0.002"), 12,
                                         wxString("device_z_dec"), wxString("push_button_arrow_dec_normal"));
    m_device_z_axis->setLimit(-5, 5);
    m_device_z_axis->setAdjustValue(0.025);
    m_device_z_axis->setPoint(3);
    m_device_cooling_fan = new IconBottonText(m_panel_rows, wxString("device_cooling_fan"), 16, wxString("100"), 12);
    m_device_cooling_fan->setLimit(0, 100);
    m_device_cooling_fan->setAdjustValue(10);

    auto rows_sizer = new wxBoxSizer(wxHORIZONTAL);
    rows_sizer->AddStretchSpacer();
    rows_sizer->Add(m_grid_sizer, 0, wxALIGN_CENTER_VERTICAL);
    rows_sizer->AddStretchSpacer();
    m_panel_rows->SetSizer(rows_sizer);
    updateGridSizer(AD5X);

    sizer->Add(m_panel_rows, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER | wxEXPAND, FromDIP(36));
#ifdef __WIN32__
    sizer->AddSpacer(FromDIP(34));
#elif defined(__APPLE__)
    sizer->AddSpacer(FromDIP(36));
#endif
    parent->SetSizer(sizer);
    parent->Layout();
}

void DeviceDetail::switchPage() 
{
    if (!FFUtils::isNozzlesPrinter(FFUtils::getPid(m_cur_id))) {
        if (m_device_speed) {
            m_device_speed->checkValue();
        }
    }
    if (m_device_z_axis) {
        m_device_z_axis->checkValue();
    }
    if (m_device_nozzle_fan) {
        m_device_nozzle_fan->checkValue();
    }
    if (m_device_cooling_fan) {
        m_device_cooling_fan->checkValue();
    }
}

void DeviceDetail::setMaterialName(wxString materialName)
{
    m_device_material->setText(materialName);
}

void DeviceDetail::setInitialSpeed(double initialSpeed)
{ 
    auto aInitialSpeed = static_cast<int>(initialSpeed);
    m_device_initial_speed->setText(wxString::Format("%d mm/s", aInitialSpeed));
}

void DeviceDetail::setSpeed(double speed) 
{ 
    auto aspeed = static_cast<int>(speed);
    if (!FFUtils::isNozzlesPrinter(FFUtils::getPid(m_cur_id))) {
        m_device_speed->setText(wxString::Format("%d", aspeed));
        m_device_speed->setCurValue(aspeed);
    } else {
        bool isFound = 0;
        for (int i = 0; i < m_device_speed_u1->combobox()->GetCount(); i++) {
            int data = static_cast<int>((uintptr_t)m_device_speed_u1->combobox()->GetClientData(i));
            if (aspeed == data) { // 对比结构体（依赖重载�?=�?
                m_device_speed_u1->combobox()->SetSelection(i);
                isFound = true;
                break;
            }
        }
        if (!isFound) {
            m_device_speed_u1->combobox()->ChangeValue(wxString::Format("%d%%", aspeed));
        }
    }
}

void DeviceDetail::setZAxis(double value) 
{
    auto aValue = wxString::Format("%.3f", value);
    m_device_z_axis->setText(aValue);
    m_device_z_axis->setCurValue(value);
}

void DeviceDetail::setLayer(int printLayer, int targetLayer)
{
    wxString str_printerLayer = wxString::Format("%d", printLayer);
    wxString str_targetLayer  = wxString::Format("%d", targetLayer);
    str_printerLayer.Append("/");
    str_printerLayer.Append(str_targetLayer);
    m_device_layer->setText(str_printerLayer);
}

void DeviceDetail::setFillRate(double fillRate) 
{
    auto aFillRate = static_cast<int>(fillRate);
    m_device_fill_rate->setText(wxString::Format("%d %%", aFillRate));
}

void DeviceDetail::setCoolingFanSpeed(double fanSpeed) 
{ 
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_nozzle_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_nozzle_fan->setCurValue(aFanSpeed);
}

void DeviceDetail::setCoolingFanShow(bool show) { m_device_cooling_fan->Show(show); }

void DeviceDetail::setChamberFanSpeed(double fanSpeed) 
{ 
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_cooling_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_cooling_fan->setCurValue(aFanSpeed);
}

void DeviceDetail::updateGridSizer(int pid) 
{
    if (pid == OTHER) {
        return;
    }

    m_layout_pid = pid;
    m_grid_sizer->Clear();
    auto addGrid = [=](wxWindow* window, bool show = true) { 
        if (show) {
            window->Show();
            m_grid_sizer->Add(window, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL, 0);
        } else {
            window->Hide();
        }
    };
    addGrid(m_device_material);
    addGrid(m_device_layer);
    addGrid(m_device_initial_speed);
    addGrid(m_device_fill_rate);
    addGrid(m_device_speed, !FFUtils::isNozzlesPrinter(pid));
    addGrid(m_device_speed_u1, FFUtils::isNozzlesPrinter(pid));
    addGrid(m_device_nozzle_fan);
    addGrid(m_device_z_axis, !FFUtils::isNozzlesPrinter(pid));
    addGrid(m_device_cooling_fan, FFUtils::isPrinterSupportCoolingFan(pid));
    m_grid_sizer->Layout();
    m_panel_rows->Layout();
    Layout();
    SetMinSize(wxSize(-1, -1));
    InvalidateBestSize();
    SetMinSize(wxSize(-1, std::max(FromDIP(277), GetBestSize().GetHeight())));
    logLayoutState("grid-updated");
    CallAfter([this] { logLayoutState("grid-settled"); });
}

void DeviceDetail::logLayoutState(const char* reason) const
{
    if (!m_grid_sizer || !m_panel_rows) {
        return;
    }

    const auto log_size = [](const wxSize& size) {
        return std::to_string(size.GetWidth()) + "x" + std::to_string(size.GetHeight());
    };
    const auto log_point = [](const wxPoint& point) {
        return std::to_string(point.x) + "," + std::to_string(point.y);
    };
    auto* mutable_this = const_cast<DeviceDetail*>(this);
    const wxSize panel_size = GetClientSize();
    const wxSize rows_size  = m_panel_rows->GetClientSize();
    const wxSize grid_min   = m_grid_sizer->GetMinSize();
    std::ostringstream summary;
    summary << "reason=" << reason << " pid=0x" << std::hex << m_layout_pid << std::dec << " panel=" << log_size(panel_size)
            << " panel_best=" << log_size(GetBestSize()) << " panel_min=" << log_size(GetMinSize()) << " rows=" << log_size(rows_size)
            << " grid_min=" << log_size(grid_min) << " cols=" << m_grid_sizer->GetCols() << " items=" << m_grid_sizer->GetItemCount();
    log_device_detail_layout(summary.str());

    const auto log_window = [&](const char* name, const wxWindow* window) {
        if (!window) {
            return;
        }
        const wxPoint screen_pos = window->ClientToScreen(wxPoint(0, 0));
        const wxPoint detail_pos = mutable_this->ScreenToClient(screen_pos);
        std::ostringstream child;
        child << "child=" << name << " shown=" << window->IsShown() << " size=" << log_size(window->GetSize())
              << " best=" << log_size(window->GetBestSize()) << " min=" << log_size(window->GetMinSize())
              << " parent_pos=" << log_point(window->GetPosition()) << " detail_pos=" << log_point(detail_pos)
              << " screen_pos=" << log_point(screen_pos);
        log_device_detail_layout(child.str());
    };
    log_window("material", m_device_material);
    log_window("layer", m_device_layer);
    log_window("initial_speed", m_device_initial_speed);
    log_window("fill_rate", m_device_fill_rate);
    log_window("speed", m_device_speed);
    log_window("speed_mode", m_device_speed_u1);
    log_window("nozzle_fan", m_device_nozzle_fan);
    log_window("z_axis", m_device_z_axis);
    log_window("cooling_fan", m_device_cooling_fan);

    // Log every element (any wxWindow) under the panel, not just wxPanels.
    std::function<void(wxWindow*, const std::string&, int)> log_elements;
    log_elements = [&](wxWindow* parent, const std::string& parent_path, int depth) {
        int child_index = 0;
        for (wxWindowList::compatibility_iterator node = parent->GetChildren().GetFirst(); node; node = node->GetNext()) {
            auto* child = node->GetData();
            if (!child) {
                continue;
            }

            const std::string class_name = wxString(child->GetClassInfo()->GetClassName()).ToStdString();
            const std::string child_path = parent_path + "/" + class_name + "[" + std::to_string(child_index++) + "]";
            const wxPoint screen_pos = child->ClientToScreen(wxPoint(0, 0));
            const wxPoint detail_pos = mutable_this->ScreenToClient(screen_pos);
            std::ostringstream element;
            element << "element_path=" << child_path << " depth=" << depth << " class=" << class_name
                    << " id=" << child->GetId() << " shown=" << child->IsShown()
                    << " parent_pos=" << log_point(child->GetPosition())
                    << " detail_pos=" << log_point(detail_pos) << " screen_pos=" << log_point(screen_pos)
                    << " size=" << log_size(child->GetSize()) << " best=" << log_size(child->GetBestSize())
                    << " min=" << log_size(child->GetMinSize());
            log_device_detail_layout(element.str());

            log_elements(child, child_path, depth + 1);
        }
    };
    log_elements(mutable_this, "DeviceDetail", 1);
}

G3UDetail::G3UDetail(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

void G3UDetail::setCurId(int curId) 
{
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
}

void G3UDetail::create_panel(wxWindow* parent) 
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* bSizer_confirm_row  = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_confirm_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    auto confirm_push_btn = new FFPushButton(m_panel_confirm_row, wxID_ANY, "push_button_confirm_normal", "push_button_confirm_hover",
                                             "push_button_confirm_press", "push_button_confirm_normal");
    confirm_push_btn->SetBackgroundColour(wxColour(255, 255, 255));
    confirm_push_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& event) {
        event.Skip();
        double speed;
        double z_axis;
        double right_nozzle_fan;
        double left_nozzle_fan;
        double cooling_fan;
        switchPage();
        if (m_device_speed) {
            wxString str_speed = m_device_speed->getTextValue();
            str_speed.ToDouble(&speed);
            double ss = speed;
        }
        if (m_device_z_axis) {
            m_device_z_axis->getTextValue().ToDouble(&z_axis);
        }
        if (m_device_right_nozzle_fan) {
            m_device_right_nozzle_fan->getTextValue().ToDouble(&right_nozzle_fan);
        }
        if (m_device_left_nozzle_fan) {
            m_device_left_nozzle_fan->getTextValue().ToDouble(&left_nozzle_fan);
        }
        if (m_device_cooling_fan) {
            m_device_cooling_fan->getTextValue().ToDouble(&cooling_fan);
        }
        ComPrintCtrl* printCtrl = new ComPrintCtrl(z_axis, speed, right_nozzle_fan, left_nozzle_fan, cooling_fan);
        if (m_cur_id >= 0) {
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, printCtrl);
        }
    });

    bSizer_confirm_row->AddSpacer(FromDIP(410));
    bSizer_confirm_row->Add(confirm_push_btn, 0, wxTOP, FromDIP(10));
    // bSizer_confirm_row->AddSpacer(FromDIP(25));

    m_panel_confirm_row->SetSizer(bSizer_confirm_row);
    m_panel_confirm_row->Layout();
    bSizer_confirm_row->Fit(m_panel_confirm_row);

    sizer->Add(m_panel_confirm_row, 0, wxALL, 0);

    //
    auto m_panel_separotor10 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor10->SetBackgroundColour(wxColour(255, 255, 255));
#ifdef __WIN32__
    m_panel_separotor10->SetMinSize(wxSize(-1, FromDIP(10)));
#elif defined(__APPLE__)
    m_panel_separotor10->SetMinSize(wxSize(-1, FromDIP(10)));
#endif


    sizer->Add(m_panel_separotor10);
    //
    wxBoxSizer* bSizer_h           = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_separotor0 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor0->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor0->SetMinSize(wxSize(FromDIP(28), -1));

    bSizer_h->Add(m_panel_separotor0);

    wxBoxSizer* bSizer_first_row  = new wxBoxSizer(wxVERTICAL);
    auto        m_panel_first_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);
    m_device_material = new IconText(m_panel_first_row, wxString("device_material"), 20, wxString("PLA-12345678901234567890"), 12);
    bSizer_first_row->Add(m_device_material, 0, wxALL, 0);

    wxBoxSizer* b1Sizer_h = new wxBoxSizer(wxHORIZONTAL);
    b1Sizer_h->AddSpacer(FromDIP(30));  
    m_rightMaterial = new wxStaticText(m_panel_first_row, wxID_ANY, _L(""), wxDefaultPosition, wxDefaultSize);
    
    b1Sizer_h->Add(m_rightMaterial);
    bSizer_first_row->Add(b1Sizer_h);
#ifdef __WIN32__
    bSizer_first_row->AddSpacer(FromDIP(14));
#elif defined(__APPLE__)
    bSizer_first_row->AddSpacer(FromDIP(12));
#endif

    m_device_initial_speed = new IconText(m_panel_first_row, wxString("device_initial_speed"), 20, wxString("1000000mm/s"), 12);
    bSizer_first_row->Add(m_device_initial_speed, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(18));

    m_device_speed = new IconBottonText(m_panel_first_row, wxString("device_speed"), 20, wxString("90"), 12);
    m_device_speed->setLimit(50, 150);
    m_device_speed->setAdjustValue(10);
    bSizer_first_row->Add(m_device_speed, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(18));

    m_device_z_axis = new IconBottonText(m_panel_first_row, wxString("device_z_axis"), 20, wxString("0.002"), 12, wxString("device_z_dec"),
                                         wxString("push_button_arrow_dec_normal"));
    m_device_z_axis->setLimit(-5, 5);
    m_device_z_axis->setAdjustValue(0.025);
    m_device_z_axis->setPoint(3);
    bSizer_first_row->Add(m_device_z_axis, 0, wxALL, 0);
    bSizer_first_row->AddSpacer(FromDIP(18));

    m_device_cooling_fan = new IconBottonText(m_panel_first_row, wxString("device_cooling_fan"), 20, wxString("100"), 12);
    m_device_cooling_fan->setLimit(0, 100);
    m_device_cooling_fan->setAdjustValue(10);
    bSizer_first_row->Add(m_device_cooling_fan, 0, wxALL, 0);

    bSizer_first_row->AddStretchSpacer();

    m_panel_first_row->SetSizer(bSizer_first_row);
    m_panel_first_row->Layout();
    bSizer_first_row->Fit(m_panel_first_row);

    bSizer_h->Add(m_panel_first_row, 0, wxALL, 0);

    //
    wxBoxSizer* bSizer_second_row  = new wxBoxSizer(wxVERTICAL);
    auto        m_panel_second_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);

    m_device_layer = new IconText(m_panel_second_row, wxString("device_layer"), 20, wxString("1000000/15000000"), 12);
    bSizer_second_row->Add(m_device_layer, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(18));

    bSizer_second_row->AddSpacer(FromDIP(14));

    m_device_fill_rate = new IconText(m_panel_second_row, wxString("device_fill_rate"), 20, wxString("200%"), 12);
    bSizer_second_row->Add(m_device_fill_rate, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(18));

    m_device_right_nozzle_fan = new IconBottonText(m_panel_second_row, wxString("device_right_nozzle_fan"), 20, wxString("50"), 12);
    m_device_right_nozzle_fan->setLimit(0, 100);
    m_device_right_nozzle_fan->setAdjustValue(10);
    bSizer_second_row->Add(m_device_right_nozzle_fan, 0, wxALL, 0);
    bSizer_second_row->AddSpacer(FromDIP(18));

    m_device_left_nozzle_fan = new IconBottonText(m_panel_second_row, wxString("device_left_nozzle_fan"), 20, wxString("100"), 12);
    m_device_left_nozzle_fan->setLimit(0, 100);
    m_device_left_nozzle_fan->setAdjustValue(10);
    bSizer_second_row->Add(m_device_left_nozzle_fan, 0, wxALL, 0);
    bSizer_second_row->AddStretchSpacer();

    m_panel_second_row->SetSizer(bSizer_second_row);
    m_panel_second_row->Layout();
    bSizer_second_row->Fit(m_panel_second_row);

    bSizer_h->Add(m_panel_second_row, 0, wxALL, 0);

    sizer->Add(bSizer_h, 0, wxALL, 0);
#ifdef __WIN32__
    sizer->AddSpacer(FromDIP(43));
#elif defined(__APPLE__)
    sizer->AddSpacer(FromDIP(46));
#endif

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit();
}

void G3UDetail::switchPage()
{
    if (m_device_speed) {
        m_device_speed->checkValue();
    }
    if (m_device_z_axis) {
        m_device_z_axis->checkValue();
    }
    if (m_device_right_nozzle_fan) {
        m_device_right_nozzle_fan->checkValue();
    }
    if (m_device_left_nozzle_fan) {
        m_device_left_nozzle_fan->checkValue();
    }
    if (m_device_cooling_fan) {
        m_device_cooling_fan->checkValue();
    }
}

void G3UDetail::setMaterialName(wxString materialName) 
{ 
    m_device_material->setText(materialName); 
}

void G3UDetail::setRightMaterialName(wxString materialName) 
{
    m_rightMaterial->SetForegroundColour(wxColour(50, 141, 251));
    m_rightMaterial->SetLabelText(materialName);
}

void G3UDetail::setInitialSpeed(double initialSpeed)
{
    auto aInitialSpeed = static_cast<int>(initialSpeed);
    m_device_initial_speed->setText(wxString::Format("%d mm/s", aInitialSpeed));
}

void G3UDetail::setSpeed(double speed)
{
    auto aspeed = static_cast<int>(speed);
    m_device_speed->setText(wxString::Format("%d", aspeed));
    m_device_speed->setCurValue(aspeed);
}

void G3UDetail::setZAxis(double value)
{
    auto aValue = wxString::Format("%.3f", value);
    m_device_z_axis->setText(aValue);
    m_device_z_axis->setCurValue(value);
}

void G3UDetail::setLayer(int printLayer, int targetLayer)
{
    wxString str_printerLayer = wxString::Format("%d", printLayer);
    wxString str_targetLayer  = wxString::Format("%d", targetLayer);
    str_printerLayer.Append("/");
    str_printerLayer.Append(str_targetLayer);
    m_device_layer->setText(str_printerLayer);
}

void G3UDetail::setFillRate(double fillRate)
{
    auto aFillRate = static_cast<int>(fillRate);
    m_device_fill_rate->setText(wxString::Format("%d %%", aFillRate));
}

void G3UDetail::setCoolingFanSpeed(double fanSpeed)
{
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_right_nozzle_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_right_nozzle_fan->setCurValue(aFanSpeed);
}

void G3UDetail::setLeftCoolingFanSpeed(double leftFanSpeed) 
{
    auto aFanSpeed = static_cast<int>(leftFanSpeed);
    m_device_left_nozzle_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_left_nozzle_fan->setCurValue(aFanSpeed);
}

void G3UDetail::setChamberFanSpeed(double fanSpeed)
{
    auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_cooling_fan->setText(wxString::Format("%d", aFanSpeed));
    m_device_cooling_fan->setCurValue(aFanSpeed);
}

wxDEFINE_EVENT(EVT_FILE_ITEM_CLICKED, wxCommandEvent);

FileItem::FileItem(wxWindow* parent, const FileData& data) 
    : wxPanel(parent), 
      m_data(data) 
{
    //create_panel(parent);
    const int height = FromDIP(44);
    SetMinSize(wxSize(0, height));
    Bind(wxEVT_ENTER_WINDOW, &FileItem::on_mouse_enter, this);
    Bind(wxEVT_LEAVE_WINDOW, &FileItem::on_mouse_leave, this);
    Bind(wxEVT_LEFT_UP, &FileItem::on_mouse_left_up, this);
    Bind(wxEVT_PAINT, &FileItem::OnPaint, this);
}

bool FileItem::IsPressed() const 
{
    return m_pressed;
}

void FileItem::SetPressed(bool pressed)
{
    m_pressed = pressed; 
}

void FileItem::create_panel(wxWindow* parent)
{
    int width = FromDIP(450), height = FromDIP(33);
    SetMinSize(wxSize(width, height));
    SetMaxSize(wxSize(width, height));
    SetSize(wxSize(width, height));

    m_iconPanel      = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_iconSizer      = new wxBoxSizer(wxVERTICAL);
    m_thumbnailPanel = new ThumbnailPanel(m_iconPanel);
    m_thumbnailPanel->SetSize(wxSize(height, height));
    m_thumbnailPanel->SetMinSize(wxSize(height, height));
    m_thumbnailPanel->SetMaxSize(wxSize(height, height));
    m_iconSizer->Add(m_thumbnailPanel, 0, wxEXPAND, 0);
    m_iconPanel->SetSizer(m_iconSizer);
    m_iconPanel->Layout();

    //set material pic
    // m_thumbnailPanel->set_thumbnail(iter->second);

    //fix file name tip
    m_fileLbl = new wxStaticText(this, wxID_ANY, _L("file name:"), wxDefaultPosition, wxDefaultSize);
    m_fileLbl->SetMinSize(wxSize(FromDIP(70), height));
    m_fileLbl->SetMaxSize(wxSize(FromDIP(70), height));
    m_fileLbl->SetSize(wxSize(FromDIP(70), height));
    m_fileLbl->SetForegroundColour(wxColor("#333333"));

    //file name
    m_nameLbl = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize);
    m_nameLbl->SetMinSize(wxSize(FromDIP(200), height));
    m_nameLbl->SetMaxSize(wxSize(FromDIP(200), height));
    m_nameLbl->SetSize(wxSize(FromDIP(200), height));
    m_nameLbl->SetForegroundColour(wxColor("#333333"));

    wxString elide_str = FFUtils::elideString(m_nameLbl, m_data.wxName, FromDIP(200));
    m_nameLbl->SetLabel(elide_str);
    //m_nameLbl->Wrap(name_width);
    //m_nameLbl->Fit();

    m_mainSizer = new wxBoxSizer(wxHORIZONTAL);
    // m_mainSizer->AddSpacer(FromDIP(5));
    m_mainSizer->Add(m_iconPanel, 0, wxALIGN_CENTER_VERTICAL);
    m_mainSizer->Add(m_fileLbl, 0, wxALIGN_CENTER_VERTICAL);
    m_mainSizer->Add(m_nameLbl ,1, wxALIGN_CENTER_VERTICAL);

    SetSizer(m_mainSizer);

    Layout();
    Fit();
}

wxImage FileItem::getImageByType(const std::string& type) 
{
    std::string imageName;
    if (type.compare("3mf") == 0) {
        imageName = "flielist_3MF" ;
    } else if (type.compare("gcode") == 0) {
        imageName = "filelist_gcode";
    } else if (type.compare("g") == 0) {
        imageName = "filelist_g";
    } else if (type.compare("gx") == 0) {
        imageName = "filelist_gx";
    }
    return wxImage(imageName);
}

std::string FileItem::getImageNameByType(const std::string& type) 
{
    std::string imageName;
    if (type.compare("3mf") == 0) {
        imageName = "flielist_3MF";
    } else if (type.compare("gcode") == 0) {
        imageName = "filelist_gcode";
    } else if (type.compare("g") == 0) {
        imageName = "filelist_g";
    } else if (type.compare("gx") == 0) {
        imageName = "filelist_gx";
    } else {
        imageName = "flielist_3MF";
    }
    return imageName;
}

void FileItem::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    doRender(dc);
}

void FileItem::render(wxDC& dc)
{
#ifdef __WXMSW__
    wxSize     size = GetSize();
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});

    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void FileItem::doRender(wxDC& dc)
{
    auto   left = 20;
    wxSize size = GetSize();
    dc.SetPen(*wxTRANSPARENT_PEN);

    if (m_pressed) {
        dc.SetBrush(m_bg_color);
        dc.SetPen(m_border_presse_color);
        dc.DrawRectangle(0, 0, size.x, size.y);
    }

    if (m_hovered) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(m_border_hover_color, 1, wxPENSTYLE_SOLID));
        dc.DrawRectangle(0, 0, size.x, size.y);
    }

    if (m_data.scaledImage.IsOk()) {
        wxBitmap bitmap = m_data.scaledImage;
        dc.DrawBitmap(bitmap, wxPoint(left, (size.y - m_data.scaledImage.GetHeight()) / 2));
        left += m_data.scaledImage.GetWidth() + 8;
    } else {
        std::string name = m_data.gcodeData.fileName;
        std::string suffix = name.substr(name.find_last_of(".") + 1);
        std::string imageName = getImageNameByType(suffix);

        ScalableBitmap dwbitmap = ScalableBitmap(this, imageName, 43);
        wxImage defaultImage = dwbitmap.bmp().ConvertToImage();
        defaultImage.Rescale(FILELIST_PIC_WIDTH, FILELIST_PIC_HEIGHT);
        wxBitmap bitmap = defaultImage;
        dc.DrawBitmap(bitmap, wxPoint(left, (size.y - defaultImage.GetHeight()) / 2));
        left += dwbitmap.GetBmpSize().x + 8;
    }
    
    dc.SetFont(Label::Body_13);
    dc.SetBackgroundMode(wxTRANSPARENT);
    dc.SetTextForeground(StateColor::darkModeColorFor(wxColour(38, 46, 48)));

    auto sizet = dc.GetTextExtent(m_data.wxName);
    wxString elide_str = FFUtils::elideString(this, m_data.wxName, FromDIP(200));
    dc.DrawText(elide_str, wxPoint(left, (size.y - sizet.y) / 2));
}

void FileItem::on_mouse_enter(wxMouseEvent& evt)
{
    m_hovered = true;
    Refresh();
}

void FileItem::on_mouse_leave(wxMouseEvent& evt)
{
    m_hovered = false;
    Refresh();
}

void FileItem::on_mouse_left_up(wxMouseEvent& evt) 
{
    wxCommandEvent event(EVT_FILE_ITEM_CLICKED, GetId());
    event.SetEventObject(this);
    wxPostEvent(this, event);

    m_pressed = true;
    Refresh();
}

SingleDeviceState::SingleDeviceState(wxWindow* parent, wxWindowID id, const wxPoint& pos, 
        const wxSize& size, long style, const wxString& name)
        : wxScrolledWindow(parent, id, pos, size, wxVSCROLL)
        , m_cur_printing_ctrl(0)
        , m_download_tool(5, 15000)
        , m_download_title_image_task_id(FFDownloadTool::InvalidTaskId)
{
    this->SetScrollRate(0, 30);
    // Keep the vertical scrollbar gutter reserved. At display sizes such as
    // 1920x1080 the time-lapse panel can put the page exactly on the scrolling
    // threshold: showing the scrollbar narrows the camera, the camera then gets
    // shorter, and the scrollbar disappears again. A stable gutter breaks this
    // width/height feedback loop and prevents the video panel from oscillating.
    ShowScrollbars(wxSHOW_SB_DEFAULT, wxSHOW_SB_ALWAYS);
    this->SetBackgroundColour(wxColour(240, 240, 240));
    setupLayout();
    connectEvent();
    reInit();
    setDevicePageState(DevicePageState::NoDevice);
}

SingleDeviceState::~SingleDeviceState()
{
    m_download_tool.wait(true);
}

void SingleDeviceState::setCurId(int curId)
{ 
    if (curId < 0) {
        setDevicePageState(DevicePageState::NoDevice);
        return;
    }
    if (curId != m_cur_id) {
        reInitMaterialPic();
        clearFileList();
        m_curId_first_Click_fileList = true;
        m_status_check_message_show_time = 0;
        m_status_check_error_code.clear();
    }
    {
        if (m_idle_tempMixDevice && !m_idle_tempMixDevice->IsShown()) {
            m_panel_print_btn->Hide();
            m_offline_info_page_gap->Hide();
            m_scrolledWindow->Hide();
            m_FileList_split_line->Hide();
            m_timeLapseVideoPnl->Hide();

            m_printBtn->Enable(false);
            if (m_curSelectedFileItem) {
                m_curSelectedFileItem->SetPressed(false);
                m_curSelectedFileItem = nullptr;
            }
            m_idle_tempMixDevice->Show();
        }
    }
    m_cur_id = curId;
    m_device_page_state = DevicePageState::Connected;
    // 切换设备：把摄像头视频流切换到当前设备（首次绑定亦走此处，comId != m_camera_cur_id 即触发）�?
    switchCameraStreamTo(curId);
    m_busy_device_detial->setCurId(curId);
    m_busy_G3U_detail->setCurId(curId);
    m_busy_circula_filter->setCurId(curId);
    m_idle_tempMixDevice->setCurId(curId);
    m_idle_lamp_bar->SetCurId(curId);
    m_busy_lamp_bar->SetCurId(curId);
    m_tempCtrl_panel->ReInitTempature(curId);
    m_timeLapseVideoPnl->setComId(curId);
    reInitProductState();
    m_idle_tempMixDevice->reInitProductState();

    //query device data by id
    bool  valid = false;
    const com_dev_data_t &data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
    if (!valid) {
        setDevicePageState(DevicePageState::NoDevice);
        return;
    }
    unsigned short curr_pid = 0;
    if (data.connectMode == COM_CONNECT_LAN) {
        m_cur_serial_number = data.lanDevInfo.serialNumber;
        curr_pid            = data.lanDevInfo.pid;
        //m_fileListbutton->SetMinSize((wxSize(FromDIP(680 + 5), FromDIP(69))));
        m_timeLapseVideoBtn->Show(false);
    } else if (data.connectMode == COM_CONNECT_WAN) {
        m_cur_serial_number = data.wanDevInfo.serialNumber;
        curr_pid            = data.devDetail->pid;
        //m_fileListbutton->SetMinSize((wxSize(FromDIP(680 / 2), FromDIP(69))));
        m_timeLapseVideoBtn->Show(data.devDetail->camera == 1);
    }

    // 根据机型判断是否支持四色打印，并设置currID
    std::string modelId = FFUtils::getPrinterModelId(curr_pid);
    bool        isPrinterSupportAms          = FFUtils::isPrinterSupportAms(curr_pid);
    bool        isPrinterSupportDeviceFilter = FFUtils::isPrinterSupportDeviceFilter(curr_pid);
    m_isNozzlesPrinter                       = FFUtils::isNozzlesPrinter(curr_pid);
    MaterialDialog::set_printer_type(curr_pid);
    if (m_isNozzlesPrinter) {
        m_print_check_bmp->Show();
        m_print_check_label->Show();
        m_nozzles->SetCurId(m_cur_id);
    } else {
        m_print_check_bmp->Hide();
        m_print_check_label->Hide();
        // m_material_station->show_material_panel(curr_pid);
        if (isPrinterSupportAms) {
            m_material_station->show_material_panel(curr_pid);
            m_material_station->setCurId(m_cur_id);
            m_material_station->Show();
        } else{
            m_material_station->Hide();
        }
    }
    m_busy_device_detial->updateGridSizer(curr_pid);
    // Basic device information (model, nozzle, firmware, serial number, etc.)
    // is available for every connected printer, so its entry must never be
    // replaced by the optional filter entry.
    m_idle_device_info_button->Show();

    // Filter control is an additional capability. Show it only for printers
    // that support it, while keeping the device-information entry available.
    m_filter_button->Show(isPrinterSupportDeviceFilter);
    m_filter_button->Enable(isPrinterSupportDeviceFilter);
    if (isPrinterSupportDeviceFilter) {
        m_filter_button->SetIcon("device_filter");
    }
    changeMachineType(data.devDetail->pid);
    m_idle_tempMixDevice->changeMachineType(data.devDetail->pid);
    reInitPage();
    onDevStateChanged(data.devDetail->status, data);
    fillValue(data);
    Layout();
    reInitData();
}

void SingleDeviceState::reInit() 
{
    reInitData();
    reInitUI();
}

void SingleDeviceState::clearCurrentDevice()
{
    m_cur_id = ComInvalidId;
    m_cur_serial_number.clear();
    m_cur_dev_name.clear();
    m_cur_dev_location.clear();
    m_cur_dev_state.clear();
    m_cur_print_file_name.clear();
    m_status_check_error_code.clear();
    m_status_check_message_show_time = 0;

    if (m_staticText_device_name) {
        m_staticText_device_name->SetLabel(wxEmptyString);
        m_staticText_device_name->UnsetToolTip();
    }
    if (m_staticText_device_position) {
        m_staticText_device_position->SetLabel(wxEmptyString);
        m_staticText_device_position->UnsetToolTip();
    }
    if (m_busy_device_info) {
        m_busy_device_info->SetDeviceInfo(wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString,
                                          wxEmptyString, wxEmptyString, wxEmptyString, wxEmptyString);
        m_busy_device_info->Hide();
    }

    reInitMaterialPic();
    clearFileList();
    m_curSelectedFileItem = nullptr;
    m_curId_first_Click_fileList = true;

    if (m_busy_device_detial) m_busy_device_detial->setCurId(ComInvalidId);
    if (m_busy_G3U_detail) m_busy_G3U_detail->setCurId(ComInvalidId);
    if (m_busy_circula_filter) m_busy_circula_filter->setCurId(ComInvalidId);
    if (m_idle_tempMixDevice) m_idle_tempMixDevice->setCurId(ComInvalidId);
    if (m_idle_lamp_bar) m_idle_lamp_bar->SetCurId(ComInvalidId);
    if (m_busy_lamp_bar) m_busy_lamp_bar->SetCurId(ComInvalidId);
    if (m_timeLapseVideoPnl) m_timeLapseVideoPnl->setComId(ComInvalidId);
    if (m_tempCtrl_panel) m_tempCtrl_panel->ReInitTempature(ComInvalidId);

    if (m_print_check_bmp) m_print_check_bmp->Hide();
    if (m_print_check_label) m_print_check_label->Hide();
    if (m_filter_button) m_filter_button->Hide();
}

void SingleDeviceState::setDevicePageState(DevicePageState state)
{
    m_device_page_state = state;
    if (state != DevicePageState::NoDevice) return;

    Freeze();
    clearCurrentDevice();
    reInitUI();
    // m_staticText_device_tip->SetLabel(_L("No device connected"));
    m_machine_idle_info_panel->Hide();
    m_machine_ctrl_info_panel->Hide();
    m_machine_idle_panel->Hide();
    m_machine_ctrl_panel->Hide();
    Layout();
    Thaw();
}

void SingleDeviceState::reInitData() 
{ 
    m_last_speed               = 0.00001;
    m_last_z_axis_compensation = 0.00001;
    m_last_cooling_fan_speed   = 0.00001;
    m_last_left_cooling_fan_speed = 0.00001;
    m_last_chamber_fan_speed   = 0.00001;
    m_right_target_temp        = 0.00001;
    m_plat_target_temp         = 0.00001;
    // 注意：m_camera_stream_url 现在跟随“摄像头绑定设备�?m_camera_cur_id)，与页面选中设备解耦，
    // 不能�?reInitData（每�?setCurId/离线都会调）里清空，否则会打断“切设备画面保持不变”�?
    // 仅在点摄像头按钮重绑设备时（activateCameraForCurrentDevice）才清空�?
    m_file_pic_url.clear();
    m_file_pic_name.clear();
    m_cur_dev_state.clear();
    m_cur_print_file_name.clear();
}

void SingleDeviceState::reInitUI() 
{
    m_staticText_device_tip->SetLabel(_L("Offline"));
    m_staticText_device_tip->SetForegroundColour(wxColour("#999999"));
    if (m_panel_idle_text) {
        m_panel_idle_text->Hide();
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_timeLapseVideoPnl->Hide();
        m_panel_separotor8->Hide();
        m_busyState_top_gap->Hide();
        m_busyState_bottom_gap->Show();
        m_offline_info_page_gap->Show();
    }
    m_staticText_device_info->Hide();
    m_clear_button->Hide();
    setIdlePrinterText(true);
    m_idle_tempMixDevice->modifyTemp("/", "/", "/");
    m_idle_tempMixDevice->setState(0);
    m_idle_lamp_bar->SetLampState(true, false);
    m_idle_lamp_bar->SetCameraState(true);
    m_busy_lamp_bar->SetLampState(true, false);
    m_busy_lamp_bar->SetCameraState(true);
    m_cur_printing_ctrl = 0;
    Layout();
}

void SingleDeviceState::reInitMaterialPic() 
{
   if (m_material_picture) {
     m_file_pic_url.clear();
     m_file_pic_name.clear();
     m_material_picture->SetImage(wxImage());
   }
}

void SingleDeviceState::reInitPage() 
{ 
    if (m_material_station) {
        showMaterialStation(true);
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->Hide();
    }

    if (m_busy_temp_brn) {
        m_busy_temp_brn->Hide();
    }
    if (m_busy_device_detial) {
        m_busy_device_detial->Hide();
    }
    if (m_busy_G3U_detail) {
        m_busy_G3U_detail->Hide();
    }
    if (m_busy_circula_filter) {
        m_busy_circula_filter->Hide();
    }
    if (m_device_info_button) {
        m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
    if (m_filter_button) {
        m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
}

void SingleDeviceState::changeMachineType(unsigned short pid)
{
    //switch (pid) {
    //case ADVENTURER_5M:  //"adventurer_5m"
    //case ADVENTURER_5M_PRO: //"adventurer_5m_pro"
    //case ADVENTURER_A5: //"adventurer_a5"
    //    m_tempCtrl_top->SetNormalIcon("device_top_temperature");
    //    m_tempCtrl_top->SetIconNormal();
    //    m_tempCtrl_bottom->SetNormalIcon("device_bottom_temperature");
    //    m_tempCtrl_bottom->SetIconNormal();
    //    m_tempCtrl_mid->SetNormalIcon("device_mid_temperature");
    //    m_tempCtrl_mid->SetIconNormal();
    //    m_tempCtrl_mid->SetReadOnly(true);
    //    break;
    //case GUIDER_3_ULTRA: //"guider_3_ultra"
    //    m_tempCtrl_top->SetNormalIcon("device_right_temperature");
    //    m_tempCtrl_top->SetIconNormal();
    //    m_tempCtrl_bottom->SetNormalIcon("device_left_temperature");
    //    m_tempCtrl_bottom->SetIconNormal();
    //    m_tempCtrl_mid->SetNormalIcon("device_bottom_temperature");
    //    m_tempCtrl_mid->SetIconNormal();
    //    m_tempCtrl_mid->SetReadOnly(false);
    //    break;
    //}
}

void SingleDeviceState::setDevProductAuthority(const fnet_dev_product_t &data) 
{
    bool lightCtrl = data.lightCtrlState == 0 ? false : true;
    bool fanCtrl   = data.internalFanCtrlState == 0 ? false : true;
    /*if (!lightCtrl) {
        m_lamp_control_button->SetIcon("device_lamp_offline");
        m_lamp_control_button->Enable(false);
    }*/
    if (!fanCtrl) {
        m_filter_button->SetIcon("device_filter_offline");
        m_filter_button->Enable(false);
        if (m_busy_circula_filter) {
            m_busy_circula_filter->Hide();
        }
    }
}

void SingleDeviceState::setG3UProductAuthority(const fnet_dev_product_t& data) 
{
    bool lightCtrl = data.lightCtrlState == 0 ? false : true;
    /*if (!lightCtrl) {
        m_lamp_control_button->SetIcon("device_lamp_offline");
        m_lamp_control_button->Enable(false);
    }*/
}

void SingleDeviceState::reInitProductState()
{ 
    //m_lamp_control_button->SetIcon("device_lamp_control");
    m_filter_button->SetIcon("device_filter");
    m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_filter_button->SetBorderColor(wxColour(255, 255, 255));
    m_idle_lamp_bar->SetLampState(false, false);
    m_busy_lamp_bar->SetLampState(false, false);
    m_filter_button->Enable(true);
}

std::string SingleDeviceState::getCurDevSerialNumber() 
{
    return m_cur_serial_number;
}

void SingleDeviceState::lostFocusmodifyTemp()
{
 //   double top_temp;
 //   double bottom_temp;
 //   double mid_temp;
 //   bool   bTop    = m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
 //   bool   bBottom = m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
 //   bool   bMid    = m_tempCtrl_mid->GetTagTemp().ToDouble(&mid_temp);
 //   switch (m_pid) {
 //   case ADVENTURER_5M:
 //   case ADVENTURER_5M_PRO: 
	//case U1:
 //   case ADVENTURER_A5: {
 //       //"Flashforge-Adventurer-5M";
 //       //"Flashforge-Adventurer-A5";
 //       //"Flashforge-Adventurer-5M-Pro";
 //       if (!bTop || top_temp < 0) {
 //           m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
 //           top_temp = m_right_target_temp;
 //       }
 //       if (top_temp > 280) {
 //           top_temp = 280;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       } else if (top_temp < 0) {
 //           top_temp = 0;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       }
 //       if (!bBottom || bottom_temp < 0) {
 //           m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
 //           bottom_temp = m_plat_target_temp;
 //       }
 //       if (bottom_temp > 110) {
 //           bottom_temp = 110;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       } else if (bottom_temp < 0) {
 //           bottom_temp = 0;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       }
 //       Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
 //       Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
 //       break;
 //   }
 //   case GUIDER_4: 
 //   case GUIDER_4_PRO: {
 //       //"Flashforge-Guider4";
 //       //"Flashforge-Guider4-Pro"
 //       if (!bTop || top_temp < 0) {
 //           m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
 //           top_temp = m_right_target_temp;
 //       }
 //       if (top_temp > 320) {
 //           top_temp = 320;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       } else if (top_temp < 0) {
 //           top_temp = 0;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       }
 //       if (!bMid || mid_temp < 0) {
 //           m_tempCtrl_mid->SetTagTemp(m_chamber_target_temp, true);
 //           mid_temp = m_chamber_target_temp;
 //       }
 //       if (mid_temp > 60) {
 //           mid_temp = 60;
 //           m_tempCtrl_mid->SetTagTemp(mid_temp, true);
 //           m_chamber_target_temp = mid_temp;
 //       } else if (mid_temp < 0) {
 //           mid_temp = 0;
 //           m_tempCtrl_mid->SetTagTemp(mid_temp, true);
 //           m_chamber_target_temp = mid_temp;
 //       }
 //       if (!bBottom || bottom_temp < 0) {
 //           m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
 //           bottom_temp = m_plat_target_temp;
 //       }
 //       if (bottom_temp > 120) {
 //           bottom_temp = 120;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       } else if (bottom_temp < 0) {
 //           bottom_temp = 0;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       }
 //       Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
 //       Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
 //       break;
 //   }
 //   case AD5X: {
 //       //"Flashforge-AD5X";
 //       if (!bTop || top_temp < 0) {
 //           m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
 //           top_temp = m_right_target_temp;
 //       }
 //       if (top_temp > 300) {
 //           top_temp = 300;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       } else if (top_temp < 0) {
 //           top_temp = 0;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       }
 //       if (!bBottom || bottom_temp < 0) {
 //           m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
 //           bottom_temp = m_plat_target_temp;
 //       }
 //       if (bottom_temp > 110) {
 //           bottom_temp = 110;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;;
 //       } else if (bottom_temp < 0) {
 //           bottom_temp = 0;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       }
 //       Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
 //       Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
 //       break;
 //   }
 //   case GUIDER_3_ULTRA: {
 //       //"Flashforge-Guider-3-Ultra";
 //       // right
 //       if (!bTop || top_temp < 0) {
 //           m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
 //           top_temp = m_right_target_temp;
 //       }
 //       if (top_temp > 350) {
 //           top_temp = 350;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       } else if (top_temp < 0) {
 //           top_temp = 0;
 //           m_tempCtrl_top->SetTagTemp(top_temp, true);
 //           m_right_target_temp = top_temp;
 //       }
 //       // left
 //       if (!bBottom || bottom_temp < 0) {
 //           m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
 //           bottom_temp = m_plat_target_temp;
 //       }
 //       if (bottom_temp > 350) {
 //           bottom_temp = 350;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       } else if (bottom_temp < 0) {
 //           bottom_temp = 0;
 //           m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
 //           m_plat_target_temp = bottom_temp;
 //       }
 //       // bottom
 //       if (!bMid || mid_temp < 0) {
 //           m_tempCtrl_mid->SetTagTemp(m_chamber_target_temp, true);
 //           mid_temp = m_chamber_target_temp;
 //       }
 //       if (mid_temp > 120) {
 //           mid_temp = 120;
 //           m_tempCtrl_mid->SetTagTemp(mid_temp, true);
 //           m_chamber_target_temp = mid_temp;
 //       } else if (mid_temp < 0) {
 //           mid_temp = 0;
 //           m_tempCtrl_mid->SetTagTemp(mid_temp, true);
 //           m_chamber_target_temp = mid_temp;
 //       }

 //       Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(mid_temp, top_temp, bottom_temp, 0);
 //       Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);

 //       break;
 //   }
 //   }

}

void SingleDeviceState::openPrintCheck() 
{ 
    wxLaunchDefaultBrowser("https://www.obico.io/", wxBROWSER_NEW_WINDOW);
}

void SingleDeviceState::setDisabledExtruderCtrl(bool b) 
{
    /*m_vSizer3->Show(!b);
    m_extruderLine->Show(!b);
    m_extruderSperator->Show(!b);
    m_extruderSperator1->Show(!b);
    m_plateSperator->Show(b);*/
}

wxBoxSizer* SingleDeviceState::create_machine_info_page() 
{ 
    wxBoxSizer* bSizer_right = new wxBoxSizer(wxVERTICAL);
    // Keep both columns inside a 1920x1080 client area at 125% Windows scaling.
    // The horizontal sizer still expands this column on wider displays.
    constexpr int machine_column_min_width = 580;
    bSizer_right->SetMinSize(wxSize(FromDIP(machine_column_min_width), -1));
    // 标题：信息与控制
    auto panel_control_title = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(49)), wxTAB_TRAVERSAL);
    panel_control_title->SetBackgroundColour(wxColour(248, 248, 248));

    wxBoxSizer* bSizer_control_title = new wxBoxSizer(wxHORIZONTAL);
    auto        staticText_control   = new Label(panel_control_title, _L("Info and Control"));
    staticText_control->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_control_title->Add(staticText_control, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(14));
    bSizer_control_title->AddStretchSpacer();
    ScalableBitmap check_map(panel_control_title, "print_check", 19);
    m_print_check_bmp   = new wxStaticBitmap(panel_control_title, wxID_ANY, check_map.bmp(), wxDefaultPosition, 
        wxSize(FromDIP(20), FromDIP(19)));
    m_print_check_bmp->Bind(wxEVT_LEFT_DOWN, [=](wxMouseEvent& event) { 
        openPrintCheck();
    });
    m_print_check_label = new Label(panel_control_title, Label::Body_12, _L("Print Inspection"));
    m_print_check_label->SetForegroundColour(wxColour("#419488"));
    m_print_check_label->SetBackgroundColour(wxColour(248, 248, 248));
    m_print_check_label->Bind(wxEVT_LEFT_DOWN, [=](wxMouseEvent& event) { openPrintCheck(); });
    m_print_check_bmp->Hide();
    m_print_check_label->Hide();
    bSizer_control_title->Add(m_print_check_bmp, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(5));
    bSizer_control_title->Add(m_print_check_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, FromDIP(19));
    panel_control_title->SetSizer(bSizer_control_title);
    panel_control_title->Layout();
    bSizer_control_title->Fit(panel_control_title);

    // 添加标题
    bSizer_right->Add(panel_control_title, 0, wxALL | wxEXPAND, 0);
    // 忙碌状�?
    m_machine_ctrl_info_panel = new wxPanel(this);
    m_machine_ctrl_info_panel->SetBackgroundColour(*wxWHITE);
    m_machine_ctrl_info_panel->SetDoubleBuffered(true);
    wxBoxSizer* bSizer_v_busy = new wxBoxSizer(wxVERTICAL);
    setupLayoutBusyInfoPage(bSizer_v_busy, m_machine_ctrl_info_panel);

    m_machine_ctrl_info_panel->SetSizer(bSizer_v_busy);
    m_machine_ctrl_info_panel->Layout();
    bSizer_v_busy->Fit(m_machine_ctrl_info_panel);

    // 空闲状�?
    m_machine_idle_info_panel = new wxPanel(this);
    // m_machine_idle_panel->SetBackgroundColour(*wxWHITE);
    m_machine_idle_info_panel->SetDoubleBuffered(true);
    wxBoxSizer* bSizer_v_idle = new wxBoxSizer(wxVERTICAL);
    setupLayoutIdleInfoPage(bSizer_v_idle, m_machine_idle_info_panel);

    m_machine_idle_info_panel->SetSizer(bSizer_v_idle);
    m_machine_idle_info_panel->Layout();
    bSizer_v_idle->Fit(m_machine_idle_info_panel);
    //***
    bSizer_right->Add(m_machine_ctrl_info_panel, 0, wxEXPAND, 0);
    bSizer_right->Add(m_machine_idle_info_panel, 0, wxEXPAND, 0);
    // m_machine_idle_panel->Hide();
    m_machine_ctrl_info_panel->Hide();
    return bSizer_right;
}

wxBoxSizer* SingleDeviceState::create_monitoring_page(wxPanel* parent)
{
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    ////摄像头布局
    //m_panel_monitoring_title = new wxPanel(parent, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(35)), wxTAB_TRAVERSAL);
    //m_panel_monitoring_title->SetBackgroundColour(wxColour(248,248,248));

    ////“摄像头”文字布局
    //wxBoxSizer *bSizer_monitoring_title;
    //bSizer_monitoring_title = new wxBoxSizer(wxHORIZONTAL);

    //m_staticText_monitoring = new Label(m_panel_monitoring_title, Label::Body_14, _L("Camera"));
    //m_staticText_monitoring->SetForegroundColour(wxColour(51,51,51));

    //bSizer_monitoring_title->AddSpacer(FromDIP(6));
    //bSizer_monitoring_title->Add(m_staticText_monitoring, 0, wxEXPAND | wxALL, FromDIP(8));
    //bSizer_monitoring_title->AddStretchSpacer();

    //m_panel_monitoring_title->SetSizer(bSizer_monitoring_title);
    //m_panel_monitoring_title->Layout();
    //bSizer_monitoring_title->Fit(m_panel_monitoring_title);

    ////行与行之间的间距使用 wxPanel 进行填充
    //sizer->Add(m_panel_monitoring_title, 0, wxEXPAND | wxALL, 0);

    //播放控件
    // 宽度随左栏横�?EXPAND（视频右边缘与下方“信息与控制”窗口对齐）；高度不再固定为 400�?
    // 而是�?FFRTMPVideoCtrl::OnSize 按“实际宽�?/ 摄像头宽高比 + 状态条高”动态设定，
    // 使“画面区”正好等于摄像头比例 —�?画面既不裁也不留黑边，画面与状态条纵向拼接�?
    // 设备遥测刷新不改变左栏宽度，故高度稳定、不会抖动�?
    m_camera_panel = new FFRTMPVideoCtrl(parent);
    m_camera_panel->setSize(wxSize(-1, FromDIP(466)));  // 解除宽度上限，允许横�?EXPAND
    //m_camera_panel->Hide();
    //if (m_idle_lamp_bar) {
    //    m_idle_lamp_bar->BindCamera(m_camera_panel);
    //}
    //if (m_busy_lamp_bar) {
    //    m_busy_lamp_bar->BindCamera(m_camera_panel);
    //}
    sizer->Add(m_camera_panel, 0, wxALL | wxEXPAND, 0);
    return sizer;
}

wxBoxSizer* SingleDeviceState::create_machine_control_title()
{
    wxBoxSizer *bSizer_v_title = new wxBoxSizer(wxVERTICAL);
    bSizer_v_title->AddSpacer(FromDIP(6));

    //设备提示信息
    wxBoxSizer *bSizer_h_title = new wxBoxSizer(wxHORIZONTAL);
    auto panel_top_right_info = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(28)), wxTAB_TRAVERSAL);
    //panel_top_right_info = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    panel_top_right_info->SetBackgroundColour(wxColour(240, 240, 240));

    //显示报错信息
    //m_staticText_device_info = new Label(panel_top_right_info, ("error Info"), wxALIGN_CENTER);
    //m_staticText_device_info->Wrap(-1);
    ////m_staticText_device_info->SetFont(wxFont(wxFontInfo(16)));
    //m_staticText_device_info->SetBackgroundColour(wxColour(246,203,198));
    //m_staticText_device_info->SetForegroundColour(wxColour(251, 71, 71));

    //bSizer_h_title->Add(m_staticText_device_info, wxSizerFlags(1).Expand());
    //bSizer_h_title->AddSpacer(FromDIP(6));

    m_staticText_device_info = new FFScrollButton(panel_top_right_info, wxID_ANY, ("error Info"), 0);
    m_staticText_device_info->SetFont(Label::Body_14);
    m_staticText_device_info->Enable(false);
    //m_staticText_device_info->SetBackgroundColour(*wxWHITE);
    m_staticText_device_info->SetBGDisableColor(wxColour("#F6CBC6"));
    m_staticText_device_info->SetFontDisableColor(wxColour("#FB4747"));
    m_staticText_device_info->SetMinSize(wxSize(FromDIP(432), FromDIP(45)));

    bSizer_h_title->Add(m_staticText_device_info, 1, wxALL, 0);
    bSizer_h_title->AddSpacer(FromDIP(14));

    //显示清除按钮
    m_clear_button = new Button(panel_top_right_info, _L("clear"), "", 0, 16);
    m_clear_button->SetPureText(true);
    m_clear_button->SetBorderWidth(1);
    m_clear_button->SetBackgroundColor(wxColour(240,240,240));
    m_clear_button->SetBorderColor(wxColour(50,141,251));
    m_clear_button->SetTextColor(wxColour(50,141,251));
    //m_clear_button->SetMinSize((wxSize(FromDIP(34), FromDIP(23))));
    m_clear_button->SetCornerRadius(0);
    m_clear_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { 
        e.Skip();
        ComStateCtrl* stateCtrl = new ComStateCtrl(CLEAR_PLATFORM);
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, stateCtrl);
        Layout();
    });

    bSizer_h_title->Add(m_clear_button, 0, wxALIGN_CENTER_VERTICAL | wxALL , 0);

    panel_top_right_info->SetSizer(bSizer_h_title);
    panel_top_right_info->Layout();
    bSizer_h_title->Fit(panel_top_right_info);

    bSizer_v_title->Add(panel_top_right_info, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(panel_top_right_info);

    //设备信息与信息之间的间隔
    auto m_panel_separotor3 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor3->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor3->SetMinSize(wxSize(-1, FromDIP(8)));
    m_panel_separotor3->SetMaxSize(wxSize(-1, FromDIP(8)));

    bSizer_v_title->Add(m_panel_separotor3, 0, wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor3);
    return bSizer_v_title;
}

wxBoxSizer* SingleDeviceState::create_machine_control_page()
{
    wxBoxSizer *bSizer_right = new wxBoxSizer(wxVERTICAL);
    //bSizer_right->SetMinSize(wxSize(FromDIP(621), -1));

    auto panel_control_title2 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(49)), wxTAB_TRAVERSAL);
    panel_control_title2->SetBackgroundColour(wxColour(248, 248, 248));

    wxBoxSizer* bSizer_control_title = new wxBoxSizer(wxHORIZONTAL);
    auto        staticText_control2  = new Label(panel_control_title2, _L("Temperature & Control"));
    staticText_control2->Wrap(-1);
    staticText_control2->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_control_title->Add(staticText_control2, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(14));
    bSizer_control_title->AddStretchSpacer();

    panel_control_title2->SetSizer(bSizer_control_title);
    panel_control_title2->Layout();

    // 添加标题
    bSizer_right->Add(panel_control_title2, 0, wxALL | wxEXPAND, 0);

    m_tempCtrl_panel = new NewTempInputPanel(this);
    bSizer_right->Add(m_tempCtrl_panel, 0, wxALL | wxEXPAND, 0);

    // 相机垂直布局中的材料�?
    m_material_station = new MaterialStation(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(255)));
    m_nozzles = new FFNozzles(this);
    auto mat_sizer     = new wxBoxSizer(wxVERTICAL);
    mat_sizer->Add(m_material_station, 0, wxTOP | wxEXPAND, FromDIP(14));
    mat_sizer->Add(m_nozzles, 0, wxTOP | wxEXPAND, FromDIP(14));
    bSizer_right->Add(mat_sizer, 0, wxEXPAND, 0);
    m_nozzles->Hide();

    //***添加材料布局和灯布局之间的间�?
    auto m_panel_separotor6 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor6->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor6->SetMinSize(wxSize(-1, FromDIP(14)));
    m_panel_separotor6->SetMaxSize(wxSize(-1, FromDIP(14)));
    bSizer_right->Add(m_panel_separotor6, 0, wxEXPAND, 0);

    //忙碌状�?
    m_machine_ctrl_panel = new wxPanel(this);
    m_machine_ctrl_panel->SetBackgroundColour(*wxWHITE);
    m_machine_ctrl_panel->SetDoubleBuffered(true);
    wxBoxSizer* bSizer_v_busy = new wxBoxSizer(wxVERTICAL);
    setupLayoutBusyCtrlPage(bSizer_v_busy, m_machine_ctrl_panel);

    m_machine_ctrl_panel->SetSizer(bSizer_v_busy);
    m_machine_ctrl_panel->Layout();
        
    //空闲状�?
    m_machine_idle_panel = new wxPanel(this);
    //m_machine_idle_panel->SetBackgroundColour(*wxWHITE);
    m_machine_idle_panel->SetDoubleBuffered(true);
    wxBoxSizer* bSizer_v_idle = new wxBoxSizer(wxVERTICAL);
    setupLayoutIdleCtrlPage(bSizer_v_idle, m_machine_idle_panel);

    m_machine_idle_panel->SetSizer(bSizer_v_idle);
    m_machine_idle_panel->Layout();
//***
    bSizer_right->Add(m_machine_ctrl_panel,  0, wxEXPAND, 0);
    bSizer_right->Add(m_machine_idle_panel, 0, wxEXPAND, 0);
    //m_machine_idle_panel->Hide();
    m_machine_ctrl_panel->Hide();
    return bSizer_right;
}

void SingleDeviceState::showMaterialStation(bool show)
{
    if (show) {
        if (m_isNozzlesPrinter) {
            m_nozzles->Show();
            m_material_station->Hide();
        } else {
            m_nozzles->Hide();
            
            if (FFUtils::isPrinterSupportAms(FFUtils::getPid(m_cur_id))) {
                m_material_station->Show();
            } else {
                m_material_station->Hide();
            }
        }
    } else {
        m_nozzles->Hide();
        m_material_station->Hide();
    }
}

void SingleDeviceState::trackBtnClick(std::string str)
{
    std::string eventType = "widget";
    std::string eventName = "widget_click";
    std::string uuid      = boost::uuids::to_string(boost::uuids::random_generator()());
    uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
    std::string timestamp                     = FFUtils::getTimestampMsStr();

    std::string uid, did, sid;
    wxGetApp().get_uds_id(uid, did, sid);
    com_tracking_common_data_t commonData;
    commonData.uid = uid;
    commonData.did = did;
    commonData.sid = sid;

    com_tracking_event_data_t eventData;
    eventData.eventType = eventType;
    eventData.eventId   = (boost::format("%s_%s_%s") % eventName % timestamp % uuid).str();
    eventData.eventName = eventName;
    eventData.pageId    = "device_monitor";
    eventData.objectId  = str;
    eventData.timestamp = timestamp;

    MultiComHelper::inst()->reportTrackingData(commonData, eventData, ComTimeoutWanB);
}

void SingleDeviceState::setupLayout()
{
    //最外层框架布局
    wxBoxSizer *bSizer_status = new wxBoxSizer(wxVERTICAL);

    //顶部背景条（作为间距�?
    auto m_panel_separotor_top_back = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(14)), wxTAB_TRAVERSAL);
    m_panel_separotor_top_back->SetBackgroundColour(wxColour(240,240,240));
    bSizer_status->Add(m_panel_separotor_top_back, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_separotor_top_back);

    //
    wxBoxSizer *bSizer_status_below = new wxBoxSizer(wxHORIZONTAL);
    //左侧空白
    auto m_panel_separotor_left = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor_left->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor_left->SetMinSize(wxSize(FromDIP(19), -1));

    bSizer_status_below->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);
    m_idleWnd.push_back(m_panel_separotor_left);
        
    //中间垂直布局
    wxBoxSizer *bSizer_left = new wxBoxSizer(wxVERTICAL);

    //机器上方状态栏
    auto m_machine_status = create_machine_status_page();
    bSizer_left->Add(m_machine_status, 0, wxALL | wxEXPAND, 0);

    // 相机布局
    m_monitor_panel = new wxPanel(this);
    auto m_monitoring_sizer = create_monitoring_page(m_monitor_panel);
    m_monitor_panel->SetSizer(m_monitoring_sizer);
    m_monitor_panel->Layout();
    bSizer_left->Add(m_monitor_panel, 0, wxALL | wxEXPAND, 0);

    // 信息与控制详情页
    auto m_machine_control = create_machine_info_page();
    bSizer_left->Add(m_machine_control, 0, wxTOP | wxEXPAND, FromDIP(13));

    bSizer_status_below->Add(bSizer_left, 1, wxALL, 0);

    //中间间隔
    auto m_panel_separator_middle = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL);
    m_panel_separator_middle->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separator_middle->SetMinSize(wxSize(FromDIP(14), -1));

    bSizer_status_below->Add(m_panel_separator_middle, 0, wxEXPAND | wxALL, 0);

    //右侧信息
    //标题�?
    auto m_machine_title = create_machine_control_title();
    
    //设备控制
    auto m_machine_ctrl = create_machine_control_page();

    m_machine_title->Add(m_machine_ctrl, 0, wxALL | wxEXPAND, 0);
    bSizer_status_below->Add(m_machine_title, 1, wxALL | wxEXPAND, 0);
    //水平布局最右侧间隔
    auto panel_separator_right = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(13), -1), wxTAB_TRAVERSAL);
    panel_separator_right->SetBackgroundColour(wxColour(240, 240, 240));

    bSizer_status_below->Add(panel_separator_right, 0, wxEXPAND | wxALL, 0);

    bSizer_status->Add(bSizer_status_below, 1, wxALL | wxEXPAND, 0);

    //底部间距
    auto panel_separotor_bottom = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(8)), wxTAB_TRAVERSAL);
    panel_separotor_bottom->SetBackgroundColour(wxColour(240, 240, 240));

    bSizer_status->Add(panel_separotor_bottom, 0, wxEXPAND | wxALL, 0);
    this->SetSizer(bSizer_status);
    this->Layout();
    Bind(wxEVT_SIZE, &SingleDeviceState::OnScrollWinSize, this);
    UpdateScrollVirtualSize();
}

wxBoxSizer* SingleDeviceState::create_machine_status_page()
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    // 水平布局
    wxBoxSizer* bSizer_title_label = new wxBoxSizer(wxHORIZONTAL);
    auto        panel_top_title    = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(18)), wxTAB_TRAVERSAL);
    panel_top_title->SetBackgroundColour(wxColour(240, 240, 240));
    // 显示设备名称
    m_staticText_device_name = new Label(panel_top_title, Label::Body_13, (""));
    m_staticText_device_name->SetMinSize(wxSize(FromDIP(220), -1));
    m_staticText_device_name->SetMaxSize(wxSize(FromDIP(220), -1));
    m_staticText_device_name->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_title_label->Add(m_staticText_device_name, 0, wxALIGN_LEFT | wxALL, 0);
    bSizer_title_label->AddStretchSpacer();

    // 显示设备所在货�?
    m_staticText_device_position = new Label(panel_top_title, Label::Body_13, (""));
    m_staticText_device_position->SetMinSize(wxSize(FromDIP(220), -1));
    m_staticText_device_position->SetMaxSize(wxSize(FromDIP(220), -1));
    m_staticText_device_position->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_title_label->Add(m_staticText_device_position, 0, wxALIGN_LEFT | wxALL, 0);
    bSizer_title_label->AddStretchSpacer();

    // 显示提示内容
    m_staticText_device_tip = new Label(panel_top_title, Label::Body_13, _L("error"));
    m_staticText_device_tip->SetForegroundColour(wxColour(251, 71, 71));

    bSizer_title_label->Add(m_staticText_device_tip, 0, wxALIGN_RIGHT | wxALL, 0);
    // bSizer_title_label->AddStretchSpacer();

    panel_top_title->SetSizer(bSizer_title_label);
    panel_top_title->Layout();
    bSizer_title_label->Fit(panel_top_title);

    sizer->AddSpacer(FromDIP(10));
    sizer->Add(panel_top_title, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(3));

    // 添加白色分割�?
    auto m_panel_separotor_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(3)), wxTAB_TRAVERSAL);
    m_panel_separotor_top->SetBackgroundColour(wxColour(255, 255, 255));
    sizer->Add(m_panel_separotor_top, 0, wxEXPAND | wxALL, 0);

    // 第二段水平布局，相机垂直布局的顶部间�?
    auto m_panel_separotor0 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor0->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor0->SetMinSize(wxSize(-1, FromDIP(8)));
    m_panel_separotor0->SetMaxSize(wxSize(-1, FromDIP(8)));
    sizer->Add(m_panel_separotor0, 0, wxEXPAND, 0);
    // sizer->AddSpacer(FromDIP(6));

    return sizer;
}

void SingleDeviceState::setupLayoutBusyInfoPage(wxBoxSizer* busySizer, wxPanel* parent) 
{
    ///********
    // 先垂直布局（添加空白间距）
    // 再水平布局�?、添加左侧空白间距；2、添加中间垂直布局�?、添加右侧垂直布局�?
    // 最终添加至整体布局

    wxBoxSizer* bSizer_file_info = new wxBoxSizer(wxVERTICAL);
    m_panel_control_info         = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(286)), wxTAB_TRAVERSAL);
    m_panel_control_info->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_control_info->SetMinSize(wxSize(-1, FromDIP(286)));

    //***顶部白条（分隔）
    auto m_panel_separotor_top = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(27)), wxTAB_TRAVERSAL);
    m_panel_separotor_top->SetBackgroundColour(wxColour(255, 255, 255));

    bSizer_file_info->Add(m_panel_separotor_top, 0, wxEXPAND | wxALL, 0);

    // 信息部分水平布局
    wxBoxSizer* bSizer_control_info = new wxBoxSizer(wxHORIZONTAL);

    //***水平布局添加最左侧空白
    auto m_panel_separotor_left = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(36), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_left->SetBackgroundColour(wxColour(255, 255, 255));
    // m_panel_separotor_left->SetMinSize(wxSize(FromDIP(10), -1));

    bSizer_control_info->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);

    // 添加垂直布局（显示文件内容）
    wxBoxSizer* bSizer_control_file_info = new wxBoxSizer(wxVERTICAL);
    // auto m_panel_control_file_info = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(145)), wxTAB_TRAVERSAL);
    auto m_panel_control_file_info = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_control_file_info->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_control_file_info->SetMinSize(wxSize(FromDIP(PRINT_FILE_INFO_WIDTH), -1));

    // 文件信息
    wxBoxSizer* bSizer_control_file_name = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_file_name = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_control_file_name->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_control_file_name->SetMinSize(wxSize(FromDIP(PRINT_FILE_INFO_WIDTH), -1));

    // 显示文件标题
    m_staticText_file_head = new Label(m_panel_control_file_name, _L("file: "));
    m_staticText_file_head->SetForegroundColour(wxColour(51, 51, 51));
    m_staticText_file_head->SetFont(Label::sysFont(16, false));
    const int file_head_width = m_staticText_file_head->GetTextExtent(_L("file: ")).x + FromDIP(PRINT_FILE_HEAD_WIDTH_PADDING);
    m_staticText_file_head->SetMinSize(wxSize(file_head_width, -1));
    m_staticText_file_head->SetMaxSize(wxSize(file_head_width, -1));

    // 显示文件名称
    m_staticText_file_name = new wxStaticText(m_panel_control_file_name, wxID_ANY, "", wxDefaultPosition,
                                              wxSize(FromDIP(PRINT_FILE_NAME_WIDTH), -1));
    m_staticText_file_name->SetForegroundColour(wxColour(51, 51, 51));
    m_staticText_file_name->SetFont(Label::sysFont(16, false));
    m_staticText_file_name->SetMinSize(wxSize(FromDIP(PRINT_FILE_NAME_WIDTH), -1));
    m_staticText_file_name->SetMaxSize(wxSize(FromDIP(PRINT_FILE_NAME_WIDTH), -1));

    bSizer_control_file_name->Add(m_staticText_file_head, 0, wxALIGN_CENTER_VERTICAL);
    bSizer_control_file_name->Add(m_staticText_file_name, 0, wxALIGN_CENTER_VERTICAL);
    m_panel_control_file_name->SetSizer(bSizer_control_file_name);
    m_panel_control_file_name->Layout();

    //***添加材料质量
    auto material_weight_pic = create_scaled_bitmap("device_material_weight", this, FromDIP(11));
    m_material_weight_staticbitmap = new wxStaticBitmap(m_panel_control_file_info, wxID_ANY, material_weight_pic);
    m_material_weight_label        = new Label(m_panel_control_file_info, Label::sysFont(13, false), ("234g"));
    wxBoxSizer* hbox = new wxBoxSizer(wxHORIZONTAL);
    hbox->SetMinSize(wxSize(FromDIP(56), -1));
    hbox->Add(m_material_weight_staticbitmap, 0, wxALIGN_CENTER | wxALL, 0);
    hbox->AddSpacer(FromDIP(4));
    hbox->Add(m_material_weight_label, wxALIGN_CENTER | wxALL, 0);

    //显示云切片帮助文本
    m_staticText_cloud_text = new Label(m_panel_control_file_info, Label::sysFont(22, false), _L("Cloud slicing queued..."));
    m_staticText_cloud_text->Wrap(-1);
    m_staticText_cloud_text->SetForegroundColour(wxColour(50, 141, 251));

    //显示云切片排队状�?
    auto cloud_sizer              = new wxBoxSizer(wxHORIZONTAL);
    m_staticText_cloud_queue_text = new Label(m_panel_control_file_info, Label::sysFont(16, false), _L("Jobs ahead"));
    m_staticText_cloud_queue_text->SetForegroundColour(wxColour("#999999"));
    m_staticText_cloud_queue_count = new Label(m_panel_control_file_info, Label::sysFont(22, false), _L("0"));
    m_staticText_cloud_queue_count->SetForegroundColour(wxColour(50, 141, 251));
    cloud_sizer->Add(m_staticText_cloud_queue_text, 0, wxALIGN_BOTTOM, 0); 
    cloud_sizer->Add(m_staticText_cloud_queue_count, 0, wxALIGN_BOTTOM | wxLEFT, FromDIP(5));

    //显示排队后提示文�?
    m_staticText_cloud_queue_tip = new Label(m_panel_control_file_info, Label::sysFont(16, false), _L("Starts slicing and printing once the queue clears."));
    m_staticText_cloud_queue_tip->Wrap(-1);
    m_staticText_cloud_queue_tip->SetForegroundColour(wxColour("#999999"));
    
    // 显示倒计�?
    wxString time           = "0" + _L("h ") + "0" + _L("min ");
    m_staticText_count_time = new Label(m_panel_control_file_info, Label::sysFont(22, false), time);
    m_staticText_count_time->Wrap(-1);
    // m_staticText_count_time->SetFont(wxFont(wxFontInfo(16)));
    m_staticText_count_time->SetForegroundColour(wxColour(50, 141, 251));

    // 显示剩余时间标签
    m_staticText_time_label = new Label(m_panel_control_file_info, ::Label::sysFont(14, false), _L("Remaining Time"));
    m_staticText_time_label->SetForegroundColour(wxColour(153, 153, 153));

    auto m_panel_separotor_mid = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(20)),
                                             wxTAB_TRAVERSAL);
    m_panel_separotor_mid->SetBackgroundColour(wxColour(255, 255, 255));

    // 显示进度�?
    m_progress_bar = new ProgressBar(m_panel_control_file_info, wxID_ANY, 100, wxDefaultPosition, wxSize(FromDIP(384), FromDIP(19)), true);
    m_progress_bar->SetProgress(30);
    m_progress_bar->SetVerticalSpace(FromDIP(5));
    m_progress_bar->SetFont(Label::Body_14);
    m_progress_bar->SetProgressBackgroundColour(wxColour(50, 141, 251));

    bSizer_control_file_info->Add(m_panel_control_file_name, 0, wxEXPAND | wxBOTTOM, FromDIP(3));
    bSizer_control_file_info->AddSpacer(FromDIP(24));
    bSizer_control_file_info->Add(hbox, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 0);
    bSizer_control_file_info->AddStretchSpacer(1)->SetMinSize(-1, FromDIP(30));
    bSizer_control_file_info->Add(m_panel_separotor_mid, 1, wxEXPAND | wxALL, 0);

    wxBoxSizer* bottomSizer = new wxBoxSizer(wxVERTICAL);
    bottomSizer->Add(m_staticText_cloud_text, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(3));
    bottomSizer->Add(cloud_sizer, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(3));
    bottomSizer->Add(m_staticText_cloud_queue_tip, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(3));
    bottomSizer->Add(m_staticText_count_time, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(3));
    bottomSizer->Add(m_staticText_time_label, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(3));
    bottomSizer->Add(m_panel_separotor_mid, 0, wxEXPAND | wxALL, 0);
    bottomSizer->Add(m_progress_bar, 0, wxALIGN_CENTER_VERTICAL, FromDIP(3));
    bSizer_control_file_info->Add(bottomSizer, 0, wxEXPAND | wxALIGN_BOTTOM | wxBOTTOM, FromDIP(30));
    m_panel_control_file_info->SetSizer(bSizer_control_file_info);
    m_panel_control_file_info->Layout();
    //bSizer_control_file_info->Fit(m_panel_control_file_info);

    bSizer_control_info->Add(m_panel_control_file_info, 0, wxEXPAND | wxALL, 0);
    /*
    //***水平布局添加中间空白间距
        auto m_panel_separotor_right = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(58), -1),
    wxTAB_TRAVERSAL); m_panel_separotor_right->SetBackgroundColour(wxColour(255,255,255));
     
        bSizer_control_info->Add(m_panel_separotor_right, 0, wxEXPAND | wxALL, 0);
    */
    bSizer_control_info->AddStretchSpacer(1);

    //***添加右侧垂直布局
    wxBoxSizer* bSizer_control_material = new wxBoxSizer(wxVERTICAL);
    m_panel_control_material = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(170)), wxTAB_TRAVERSAL);
    m_panel_control_material->SetBackgroundColour(wxColour(255, 255, 255));

    m_busy_lamp_bar = new LampToolBar(m_panel_control_material);
    auto lamp_bar_sizer = new wxBoxSizer(wxHORIZONTAL);
    lamp_bar_sizer->AddStretchSpacer(1);
    lamp_bar_sizer->Add(m_busy_lamp_bar, 0, wxALL, 0);
    bSizer_control_material->Add(lamp_bar_sizer, 1, wxRIGHT | wxEXPAND, 0);
    m_material_picture = new MaterialImagePanel(m_panel_control_material);
    m_material_picture->SetMinSize(wxSize(FromDIP(102), FromDIP(102)));
    //bSizer_control_material->Add(hbox, 0, /*wxALIGN_CENTER*/ wxRIGHT | wxALL, 0);
    bSizer_control_material->AddSpacer(FromDIP(17));
    bSizer_control_material->Add(m_material_picture, 0, /*wxALIGN_CENTER*/ wxRIGHT, 0);

    m_panel_control_material->SetSizer(bSizer_control_material);
    m_panel_control_material->Layout();

    bSizer_control_info->Add(m_panel_control_material);

    //***水平布局添加最右侧空白
    auto m_panel_separotor_right2 = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor_right2->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor_right2->SetMinSize(wxSize(FromDIP(27), -1));

    bSizer_control_info->Add(m_panel_separotor_right2, 0, wxEXPAND | wxALL, 0);

    bSizer_file_info->Add(bSizer_control_info, 1, wxEXPAND | wxALL, 0);

    //**信息与控制布局添加至垂直布局
    m_panel_control_info->SetSizer(bSizer_file_info);
    m_panel_control_info->Layout();
    //bSizer_file_info->Fit(m_panel_control_info);

    busySizer->Add(m_panel_control_info, 1, wxALL | wxEXPAND, 0);

    //****添加暂停打印、取消打�?
    // 设备信息与打印按钮之间的间隔
    auto m_panel_separotor4 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor4->SetBackgroundColour(wxColour(248, 248, 248));
    m_panel_separotor4->SetMinSize(wxSize(-1, FromDIP(8)));
    m_panel_separotor4->SetMaxSize(wxSize(-1, FromDIP(8)));
    busySizer->Add(m_panel_separotor4, 0, wxEXPAND, 0);

    //***打印布局
    wxBoxSizer* bSizer_control_print  = new wxBoxSizer(wxHORIZONTAL);
    m_panel_control_print = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(69)), wxTAB_TRAVERSAL);
    m_panel_control_print->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_control_print->SetMinSize(wxSize(FromDIP(680), FromDIP(69)));
    m_panel_control_print->SetMaxSize(wxSize(FromDIP(680), FromDIP(69)));

    // 显示继续打印按钮
    m_print_button = new Button(m_panel_control_print, _L("pause print"), "device_pause_print", 0, 16);
    m_print_button->SetFont(Label::sysFont(14, false));
    m_print_button->SetFlashForge(true);
    m_print_button->SetBorderWidth(0);
    m_print_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_print_button->SetBorderColor(wxColour(255, 255, 255));
    m_print_button->SetTextColor(wxColour(51, 51, 51));
    //m_print_button->SetMinSize(FromDIP(wxSize(336, 69)));
    //  m_print_button->SetMinSize((wxSize(FromDIP(158), FromDIP(29))));
    m_print_button->SetCornerRadius(0);
    m_print_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        e.Skip();
        auto        detail = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail;
        std::string status = detail->status;
        if (/*!m_print_button_pressed_down*/ status == "printing") {
            m_cur_printing_ctrl    = 1;
            std::string printState = PAUSE;
            std::string jobId      = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
            ComJobCtrl* jobCtrl    = new ComJobCtrl(jobId, printState);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
            /*m_print_button->SetLabel(_L("continue print"));
            m_print_button->SetIcon("device_continue_print");
            m_print_button->Refresh();*/
        } else if (status == PAUSE || status == P_PAUSING) {
            m_cur_printing_ctrl    = 2;
            std::string printState = CONTINUE;
            std::string jobId      = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
            ComJobCtrl* jobCtrl    = new ComJobCtrl(jobId, printState);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
            /*m_print_button->SetLabel(_L("pause print"));
            m_print_button->SetIcon("device_pause_print");
            m_print_button->Refresh();*/
        }
        // m_print_button_pressed_down = !m_print_button_pressed_down;
    });

    // bSizer_control_print->Add(m_print_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_print->Add(m_print_button, 1, wxLEFT | wxRIGHT | wxEXPAND, 0);

    auto m_panel_separotor_print = new wxPanel(m_panel_control_print, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(5), -1), wxTAB_TRAVERSAL);
    m_panel_separotor_print->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor_print->SetMinSize(wxSize(FromDIP(5), -1));

    bSizer_control_print->Add(m_panel_separotor_print, 0, wxALL | wxEXPAND, 0);

    // 显示取消打印按钮
    m_cancel_button = new Button(m_panel_control_print, _L("cancel print"), "device_cancel_print", 0, 16);
    m_cancel_button->SetFont(Label::sysFont(14, false));
    m_cancel_button->SetFlashForge(true);
    m_cancel_button->SetBorderWidth(0);
    m_cancel_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_cancel_button->SetBorderColor(wxColour(255, 255, 255));
    m_cancel_button->SetTextColor(wxColour(51, 51, 51));
    //m_cancel_button->SetMinSize(FromDIP(wxSize(336, 69)));
    //  m_cancel_button->SetMinSize((wxSize(FromDIP(158), FromDIP(29))));
    m_cancel_button->SetCornerRadius(0);
    m_cancel_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        // e.Skip();
        if (!m_cancel_confirm_page) {
            m_cancel_confirm_page = new CancelPrint(_L("Whether Cancel Printing"), _L("yes"), _L("no"), _L("Cancel print"));
            m_cancel_confirm_page->Bind(EVT_CANCEL_PRINT_CLICKED, &SingleDeviceState::onCancelPrint, this);
            m_cancel_confirm_page->Bind(EVT_CONTINUE_PRINT_CLICKED, &SingleDeviceState::onContinuePrint, this);
        }
        m_cancel_confirm_page->ShowModal();
        trackBtnClick("cancel_print");
    });

    // bSizer_control_print->Add(m_cancel_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_print->Add(m_cancel_button, 1, wxLEFT | wxRIGHT | wxEXPAND, 0);

    //***继续打印布局添加至垂直布局
    m_panel_control_print->SetSizer(bSizer_control_print);
    m_panel_control_print->Layout();

    busySizer->Add(m_panel_control_print, 0, wxLEFT | wxRIGHT | wxEXPAND, 0);

    wxBoxSizer* cloud_btn_sizer  = new wxBoxSizer(wxHORIZONTAL);
    m_panel_control_cloud = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(69)), wxTAB_TRAVERSAL);
    m_panel_control_cloud->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_control_cloud->SetMinSize(wxSize(-1, FromDIP(69)));
    m_panel_control_cloud->SetMaxSize(wxSize(-1, FromDIP(69)));

    m_retry_print_button = new Button(m_panel_control_cloud, _L("Reprint"), "refresh", 0, 16);
    m_retry_print_button->SetFont(Label::sysFont(14, false));
    m_retry_print_button->SetFlashForge(true);
    m_retry_print_button->SetBorderWidth(0);
    m_retry_print_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_retry_print_button->SetBorderColor(wxColour(255, 255, 255));
    m_retry_print_button->SetTextColor(wxColour(51, 51, 51));
    //m_retry_print_button->SetMinSize(FromDIP(wxSize(336, 69)));
    m_retry_print_button->SetCornerRadius(0);
    m_retry_print_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        e.Skip();
        auto        detail = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail;
        if (!detail) {
            return;
        }
        std::string status = detail->status;
        if (status != P_READY && status != P_CLOUD_SLICING) {
            ErrorDialog dlg(wxGetApp().GetMainTopWindow(), _L("The printer is not idle"), false);
            dlg.ShowModal();
            wxGetApp().mainframe->jump_to_monitor(EVT_SWITCH_TO_DEVICE_LIST);
        } else {
            MultiComHelper::inst()->retrySliceTask(m_slice_task_id, ComTimeoutWanB);
        }
        trackBtnClick("reprint");
        // m_print_button_pressed_down = !m_print_button_pressed_down;
    });
    m_cancel_queue_button = new Button(m_panel_control_cloud, _L("Cancel Queue"), "device_cancel_print", 0, 16);
    m_cancel_queue_button->SetFont(Label::sysFont(14, false));
    m_cancel_queue_button->SetFlashForge(true);
    m_cancel_queue_button->SetBorderWidth(0);
    m_cancel_queue_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_cancel_queue_button->SetBorderColor(wxColour(255, 255, 255));
    m_cancel_queue_button->SetTextColor(wxColour(51, 51, 51));
    //m_cancel_queue_button->SetMinSize(FromDIP(wxSize(680, 69)));
    m_cancel_queue_button->SetCornerRadius(0);
    m_cancel_queue_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        e.Skip();
        auto        title = m_cancel_queue_button->GetLabel();
        wxString    info;
        std::string trackStr;
        if (title == _L("Cancel Queue")) {
            info = _L("Cancel queue?");
            trackStr = "cancel_queue";
        } else if (title == _L("Cancel Slicing")) {
            info = _L("Cancel slicing?");
            trackStr = "cancel_slice";
        }
        CancelPrint* dlg = new CancelPrint(info, _L("yes"), _L("no"), title);
        dlg->Bind(EVT_CANCEL_PRINT_CLICKED, [=](wxCommandEvent& event) {
            MultiComHelper::inst()->cancelSliceTask(m_slice_task_id, ComTimeoutWanB); 
            dlg->Close();
            dlg->Destroy();
        });
        dlg->Bind(EVT_CONTINUE_PRINT_CLICKED, [=](wxCommandEvent& event) { 
            dlg->Close();
            dlg->Destroy();
        });
        dlg->ShowModal();
        if (!trackStr.empty())
            trackBtnClick(trackStr);
    });
    m_cancel_slice_button = new Button(m_panel_control_cloud, _L("Cancel Slicing"), "device_cancel_print", 0, 16);
    m_cancel_slice_button->SetFont(Label::sysFont(14, false));
    m_cancel_slice_button->SetFlashForge(true);
    m_cancel_slice_button->SetBorderWidth(0);
    m_cancel_slice_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_cancel_slice_button->SetBorderColor(wxColour(255, 255, 255));
    m_cancel_slice_button->SetTextColor(wxColour(51, 51, 51));
    //m_cancel_slice_button->SetMinSize(FromDIP(wxSize(336, 69)));
    m_cancel_slice_button->SetCornerRadius(0);
    m_cancel_slice_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        e.Skip();
        CancelPrint* dlg = new CancelPrint(_L("Cancel slicing?"), _L("yes"), _L("no"), _L("Cancel Slicing"));
        dlg->Bind(EVT_CANCEL_PRINT_CLICKED, [=](wxCommandEvent& event) {
            MultiComHelper::inst()->cancelSliceTask(m_slice_task_id, ComTimeoutWanB);
            dlg->Close();
            dlg->Destroy();
        });
        dlg->Bind(EVT_CONTINUE_PRINT_CLICKED, [=](wxCommandEvent& event) {
            dlg->Close();
            dlg->Destroy();
        });
        dlg->ShowModal();
    });

    m_cancel_slice_button->Hide();
    m_retry_print_button->Hide();
    cloud_btn_sizer->Add(m_cancel_queue_button, 1, wxLEFT | wxRIGHT | wxEXPAND, 0);
    cloud_btn_sizer->Add(m_retry_print_button, 1, wxRIGHT | wxEXPAND, FromDIP(8));
    cloud_btn_sizer->Add(m_cancel_slice_button, 1, wxLEFT | wxRIGHT | wxEXPAND, 0);

    m_panel_control_cloud->SetSizer(cloud_btn_sizer);
    m_panel_control_cloud->Layout();
    m_panel_control_cloud->Hide();
    busySizer->Add(m_panel_control_cloud, 0, wxLEFT | wxRIGHT | wxEXPAND, 0);

    //***添加打印控制和温度布局之间的间�?
    auto m_panel_separotor5 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor5->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor5->SetMinSize(wxSize(-1, FromDIP(22)));
    m_panel_separotor5->SetMaxSize(wxSize(-1, FromDIP(22)));
    busySizer->Add(m_panel_separotor5, 0, wxALL | wxEXPAND, 0);
}

void SingleDeviceState::setupLayoutIdleInfoPage(wxBoxSizer* idleSizer, wxPanel* parent) 
{
    m_busyState_top_gap = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_busyState_top_gap->SetBackgroundColour(wxColour(255, 255, 255));
    m_busyState_top_gap->SetMinSize(wxSize(-1, FromDIP(22)));
    idleSizer->Add(m_busyState_top_gap, 0, wxALL | wxEXPAND, 0);
    m_busyState_top_gap->Hide();

    // 水平布局，机器图 + 文字
    wxBoxSizer* bSizer_h_device_tip  = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* bSizer_v_device_text = new wxBoxSizer(wxVERTICAL);
    bSizer_v_device_text->SetMinSize(-1, FromDIP(205));
    m_panel_idle                     = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_idle->SetBackgroundColour(*wxWHITE);
    m_panel_idle->SetMinSize(wxSize(-1, FromDIP(280)));

    auto idle_device_pic       = create_scaled_bitmap("adventurer_5m", 0, 200);
    m_idle_device_staticbitmap = new wxStaticBitmap(m_panel_idle, wxID_ANY, idle_device_pic);
    m_staticText_idle = new Label(
        m_panel_idle, Label::sysFont(16, false), _L(HAS_NO_PRINTING), wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    m_staticText_idle->SetForegroundColour(wxColour(51, 51, 51));
    m_staticText_idle->SetBackgroundColour(wxColour(255, 255, 255));
    m_idle_lamp_bar = new LampToolBar(m_panel_idle);
    auto lamp_bar_sizer = new wxBoxSizer(wxHORIZONTAL);
    lamp_bar_sizer->AddStretchSpacer(1);
    lamp_bar_sizer->Add(m_idle_lamp_bar, 0, wxALL, 0);
    bSizer_h_device_tip->Add(m_idle_device_staticbitmap, 0, wxALL | wxEXPAND, 0);
    bSizer_h_device_tip->AddSpacer(FromDIP(22));
    bSizer_v_device_text->AddSpacer(FromDIP(27));
    bSizer_v_device_text->Add(lamp_bar_sizer, 0, wxRIGHT | wxEXPAND, FromDIP(27));
    bSizer_v_device_text->AddSpacer(FromDIP(64));
    bSizer_v_device_text->Add(m_staticText_idle, 0, wxALL, 0);
    bSizer_v_device_text->AddStretchSpacer();
    bSizer_h_device_tip->Add(bSizer_v_device_text, 1, wxALL | wxEXPAND, 0);

    m_panel_idle->SetSizer(bSizer_h_device_tip);
    m_panel_idle->Layout();

    idleSizer->Add(m_panel_idle, 0, wxALL | wxEXPAND, 0);

    // 添加空白间距
    auto m_panel_separotor1 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor1->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor1->SetMinSize(wxSize(-1, FromDIP(6)));
    
    m_busyState_bottom_gap = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_busyState_bottom_gap->SetBackgroundColour(wxColour(255, 255, 255));
    m_busyState_bottom_gap->SetMinSize(wxSize(-1, FromDIP(69)));
    idleSizer->Add(m_busyState_bottom_gap, 0, wxALL | wxEXPAND, 0);
    m_offline_info_page_gap = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_offline_info_page_gap->SetBackgroundColour(wxColour(255, 255, 255));
    m_offline_info_page_gap->SetMinSize(wxSize(-1, FromDIP(22)));    
    idleSizer->Add(m_panel_separotor1, 0, wxALL | wxEXPAND, 0);
    idleSizer->Add(m_offline_info_page_gap, 0, wxALL | wxEXPAND, 0);
    
    m_busyState_bottom_gap->Hide();
    m_offline_info_page_gap->Hide();

    //*** 设备空闲和文件列表间�?
    // 添加空白间距
    auto m_panel_separotor2 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor2->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor2->SetMinSize(wxSize(-1, FromDIP(8)));

    idleSizer->Add(m_panel_separotor2, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor2);

    m_panel_idle_text = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(69)), wxTAB_TRAVERSAL);
    m_fileListbutton  = new Button(m_panel_idle_text, _L("Local File List"), "local_file_list", 0, 16);
    //m_fileListbutton->SetMinSize((wxSize(FromDIP(680 / 2), FromDIP(69))));
    m_fileListbutton->SetFont(Label::sysFont(14, false));
    m_fileListbutton->SetFlashForge(true);
    m_fileListbutton->SetBorderWidth(0);
    m_fileListbutton->SetBackgroundColor(wxColour(255, 255, 255));
    m_fileListbutton->SetBorderColor(wxColour(255, 255, 255));
    m_fileListbutton->SetTextColor(wxColour(51, 51, 51));
    m_fileListbutton->SetCornerRadius(0);

    m_timeLapseVideoBtn = new Button(m_panel_idle_text, _L("Time-Lapse Video"), "time_lapse_video", 0, 16);
    //m_timeLapseVideoBtn->SetMinSize((wxSize(FromDIP(680 / 2), FromDIP(69))));
    m_timeLapseVideoBtn->SetFont(Label::sysFont(14, false));
    m_timeLapseVideoBtn->SetFlashForge(true);
    m_timeLapseVideoBtn->SetBorderWidth(0);
    m_timeLapseVideoBtn->SetBackgroundColor(wxColour(255, 255, 255));
    m_timeLapseVideoBtn->SetBorderColor(wxColour(255, 255, 255));
    m_timeLapseVideoBtn->SetTextColor(wxColour(51, 51, 51));
    m_timeLapseVideoBtn->SetCornerRadius(0);

    wxBoxSizer* bSizer_h_idle_text = new wxBoxSizer(wxHORIZONTAL);
    bSizer_h_idle_text->Add(m_fileListbutton, 1, wxEXPAND | wxALL, 0);
    bSizer_h_idle_text->Add(m_timeLapseVideoBtn, 1, wxEXPAND | wxLEFT, FromDIP(5));

    m_panel_idle_text->SetSizer(bSizer_h_idle_text);
    idleSizer->Add(m_panel_idle_text, 0, wxALL | wxEXPAND, 0);

    //*** 文件列表和列表内容间�?
    // 添加空白间距
    m_panel_separotor8 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor8->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor8->SetMinSize(wxSize(-1, FromDIP(22)));

    idleSizer->Add(m_panel_separotor8, 0, wxALL | wxEXPAND, 0);
    m_idleWnd.push_back(m_panel_separotor8);
    // m_panel_separotor8->Hide();

    m_scrolledWindow = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    // m_scrolledWindow->SetBackgroundColour(/**wxWHITE*/ wxColour("#fafafa"));
    m_scrolledWindow->SetBackgroundColour(*wxWHITE);
    m_scrolledWindow->SetMinSize(wxSize(0, FromDIP(198)));
    m_scrolledWindow->SetMaxSize(wxSize(-1, FromDIP(198)));
    m_scrolledWindow->SetScrollRate(0, 30);
    m_sizer_my_devices = new wxBoxSizer(wxVERTICAL);
    m_scrolledWindow->SetSizer(m_sizer_my_devices);
    m_scrolledWindow->Layout();

    idleSizer->Add(m_scrolledWindow, 0, wxALL | wxEXPAND, 0);

    m_scrolledWindow->Hide();
    // split line
    m_FileList_split_line = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)), wxTAB_TRAVERSAL);
    m_FileList_split_line->SetBackgroundColour(wxColor("#F0F0F0"));
    idleSizer->Add(m_FileList_split_line, 0, wxALL | wxEXPAND, 0);
    m_FileList_split_line->Hide();

    // print btn
    wxBoxSizer* bSizer_v_print = new wxBoxSizer(wxVERTICAL);
    m_panel_print_btn          = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(56)), wxTAB_TRAVERSAL);
    m_panel_print_btn->SetMinSize(wxSize(-1, FromDIP(56)));
    m_panel_print_btn->SetMaxSize(wxSize(-1, FromDIP(56)));
    m_panel_print_btn->SetBackgroundColour(wxColor("#FFFFFF"));

    // wxHORIZONTAL
    wxBoxSizer* bSizer_h_print = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_control_print = new wxPanel(m_panel_print_btn, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(40)), wxTAB_TRAVERSAL);
    m_panel_control_print->SetBackgroundColour(wxColour("#FFFFFF"));

    m_printBtn = new FFButton(m_panel_control_print, wxID_ANY, _L("print"));
    m_printBtn->SetMinSize(wxSize(FromDIP(91), FromDIP(40)));
    m_printBtn->SetFont(Label::sysFont(14, false));
    m_printBtn->SetFontHoverColor(wxColour(255, 255, 255));
    m_printBtn->SetBGHoverColor(wxColour(149, 197, 255));
    m_printBtn->SetBorderHoverColor(wxColour(149, 197, 255));

    m_printBtn->SetFontPressColor(wxColour(255, 255, 255));
    m_printBtn->SetBGPressColor(wxColour(17, 111, 223));
    m_printBtn->SetBorderPressColor(wxColour(17, 111, 223));

    m_printBtn->SetFontColor(wxColour(255, 255, 255));
    m_printBtn->SetBorderColor(wxColour(50, 141, 251));
    m_printBtn->SetBGColor(wxColour(50, 141, 251));
    m_printBtn->Enable(false);
    m_printBtn->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onFileListPrintBtnClicked, this);

    bSizer_h_print->Add(m_printBtn, 0, wxALIGN_CENTER, 0);

    m_refreshBtn = new Button(m_panel_control_print, "", "file_list_refresh", 0, 24);
    // m_refreshBtn->SetMinSize((wxSize(FromDIP(450), FromDIP(45))));
    // m_refreshBtn->SetBorderWidth(0);
    m_refreshBtn->SetFlashForge(true);
    m_refreshBtn->SetBackgroundColor(wxColour(255, 255, 255));
    m_refreshBtn->SetBorderColor(wxColour(255, 255, 255));
    m_refreshBtn->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onFileListRefreshBtnClicked, this);

    bSizer_h_print->AddSpacer(FromDIP(71));
    bSizer_h_print->Add(m_refreshBtn, 0, wxALIGN_CENTER, 0);

    m_panel_control_print->SetSizer(bSizer_h_print);
    m_panel_control_print->Layout();
    bSizer_h_print->Fit(m_panel_control_print);

    bSizer_v_print->AddStretchSpacer();
    bSizer_v_print->Add(m_panel_control_print, 0, wxALIGN_CENTER, 0);
    bSizer_v_print->AddStretchSpacer();

    m_panel_print_btn->SetSizer(bSizer_v_print);
    m_panel_print_btn->Layout();
    bSizer_v_print->Fit(m_panel_print_btn);

    idleSizer->Add(m_panel_print_btn, 0, wxALL | wxEXPAND, 0);
    m_panel_print_btn->Hide();

    // 延迟视频
    m_timeLapseVideoPnl = new TimeLapseVideoPanel(parent);
    m_timeLapseVideoPnl->SetBackgroundColour(*wxWHITE);
    m_timeLapseVideoPnl->SetMinSize(wxSize(-1, FromDIP(255)));
    m_timeLapseVideoPnl->Hide();
    idleSizer->Add(m_timeLapseVideoPnl, 0, wxALL | wxEXPAND, 0);
}

void SingleDeviceState::setupLayoutBusyCtrlPage(wxBoxSizer* busySizer, wxPanel* parent) 
{
    // 新建水平布局
    wxBoxSizer* midSizer          = new wxBoxSizer(wxHORIZONTAL);
    ////***温度控件
    //wxBoxSizer* bSizer_temperature  = new wxBoxSizer(wxVERTICAL);
    //auto        m_panel_temperature = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(491), FromDIP(286)));
    //m_panel_temperature->SetBackgroundColour(*wxWHITE);
    //wxWindowID top_id = wxWindow::NewControlId();
    //m_tempCtrl_top         = new TempInput(m_panel_temperature, top_id, wxString("--"), wxString("--"), wxString("device_top_temperature"),
    //                                  wxString("device_top_temperature"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    //m_tempCtrl_top->SetMinTemp(20);
    //m_tempCtrl_top->SetMaxTemp(120);
    //m_tempCtrl_top->SetMinSize((wxSize(-1, FromDIP(58))));
    //m_tempCtrl_top->SetBorderWidth(0);
    //StateColor tempinput_text_colour(std::make_pair(wxColour(51, 51, 51), (int) StateColor::Disabled),
    //                                 std::make_pair(wxColour(48, 58, 60), (int) StateColor::Normal));
    //m_tempCtrl_top->SetTextColor(tempinput_text_colour);
    //StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int) StateColor::Disabled),
    //                                   std::make_pair(wxColour(0, 150, 136), (int) StateColor::Focused),
    //                                   std::make_pair(wxColour(0, 150, 136), (int) StateColor::Hovered),
    //                                   std::make_pair(*wxWHITE, (int) StateColor::Normal));
    //m_tempCtrl_top->SetBorderColor(tempinput_border_colour);
    //m_tempCtrl_top->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
    //    event.Skip();
    //    lostFocusmodifyTemp();
    //});
    //m_tempCtrl_top->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& event) {
    //    event.Skip();
    //    lostFocusmodifyTemp();
    //});

    //bSizer_temperature->AddSpacer(FromDIP(16));
    //bSizer_temperature->Add(m_tempCtrl_top, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(28));

    //wxWindowID bottom_id = wxWindow::NewControlId();
    //m_tempCtrl_bottom = new TempInput(m_panel_temperature, bottom_id, wxString("--"), wxString("--"), wxString("device_bottom_temperature"),
    //                             wxString("device_bottom_temperature"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    //m_tempCtrl_bottom->SetMinTemp(20);
    //m_tempCtrl_bottom->SetMaxTemp(120);
    //m_tempCtrl_bottom->SetMinSize((wxSize(-1, FromDIP(58))));
    //m_tempCtrl_bottom->SetBorderWidth(0);
    //m_tempCtrl_bottom->SetBorderColor(tempinput_border_colour);
    //m_tempCtrl_bottom->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
    //    event.Skip();
    //    lostFocusmodifyTemp();
    //});
    //m_tempCtrl_bottom->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& event) {
    //    event.Skip();
    //    lostFocusmodifyTemp();
    //});
    //bSizer_temperature->AddSpacer(FromDIP(16));
    //bSizer_temperature->Add(m_tempCtrl_bottom, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(28));

    //wxWindowID bottom_mid = wxWindow::NewControlId();
    //m_tempCtrl_mid = new TempInput(m_panel_temperature, bottom_mid, wxString("--"), wxString("--"), wxString("device_mid_temperature"),
    //                          wxString("device_mid_temperature"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    //m_tempCtrl_mid->SetMinTemp(20);
    //m_tempCtrl_mid->SetMaxTemp(120);
    //m_tempCtrl_mid->SetMinSize((wxSize(-1, FromDIP(58))));
    //m_tempCtrl_mid->SetBorderWidth(0);
    //m_tempCtrl_mid->SetReadOnly(true);
    //// m_tempCtrl_mid->SetTextBindInput();
    //m_tempCtrl_mid->SetBorderColor(tempinput_border_colour);
    //m_tempCtrl_mid->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
    //    event.Skip();
    //    lostFocusmodifyTemp();
    //});
    //m_tempCtrl_mid->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& event) {
    //    event.Skip();
    //    lostFocusmodifyTemp();
    //});
    //bSizer_temperature->AddSpacer(FromDIP(16));
    //bSizer_temperature->Add(m_tempCtrl_mid, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(28));
    //bSizer_temperature->AddStretchSpacer();
    //bSizer_temperature->Fit(m_panel_temperature);
    //m_panel_temperature->SetSizer(bSizer_temperature);
    //m_panel_temperature->Layout();
    //busySizer->Add(m_panel_temperature, 0, wxALL, 0);

    //***灯控制布局
    wxBoxSizer* bSizer_control_lamp  = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_control_lamp = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(69)), wxTAB_TRAVERSAL);
    m_panel_control_lamp->SetBackgroundColour(wxColour(255, 255, 255));

    // 显示文件信息按钮
    m_device_info_button = new Button(m_panel_control_lamp, wxString(""), "device_file_info", 0, 24);
    m_device_info_button->SetBorderWidth(0);
    m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_device_info_button->SetBorderColor(wxColour(255, 255, 255));
    // m_device_info_button->SetTextColor(wxColour(51,51,51));
    m_device_info_button->SetMinSize((wxSize(FromDIP(158), FromDIP(69))));
    m_device_info_button->SetCornerRadius(0);
    // bSizer_control_lamp->Add(m_device_info_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_device_info_button, wxSizerFlags(1).Expand());
    //bSizer_control_lamp->AddSpacer(FromDIP(8));

    // 显示过滤按钮
    m_filter_button = new Button(m_panel_control_lamp, wxString(""), "device_filter", 0, 24);
    m_filter_button->SetBorderWidth(0);
    m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_filter_button->SetBorderColor(wxColour(255, 255, 255));
    // m_filter_button->SetTextColor(wxColour(51,51,51));
    m_filter_button->SetMinSize((wxSize(FromDIP(158), FromDIP(69))));
    m_filter_button->SetCornerRadius(0);
    // bSizer_control_lamp->Add(m_filter_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_filter_button, wxSizerFlags(1).Expand());

    // 显示信息按钮
    m_idle_device_info_button = new Button(m_panel_control_lamp, wxString(""), "device_idle_file_info", 0, 24);
    m_idle_device_info_button->SetBorderWidth(0);
    m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_idle_device_info_button->SetBorderColor(wxColour(255, 255, 255));
    m_idle_device_info_button->SetMinSize((wxSize(FromDIP(158), FromDIP(69))));
    m_idle_device_info_button->SetCornerRadius(0);
    bSizer_control_lamp->Add(m_idle_device_info_button, wxSizerFlags(1).Expand());
    m_idle_device_info_button->Hide();

    //***灯控制布局添加至垂直布局
    m_panel_control_lamp->SetSizer(bSizer_control_lamp);
    m_panel_control_lamp->Layout();
    bSizer_control_lamp->Fit(m_panel_control_lamp);

    busySizer->Add(m_panel_control_lamp, 0, wxALL | wxEXPAND, 0);
    // busy button slot
    m_device_info_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        // m_device_info_button->SetIcon("device_idle_file_info");
        m_device_info_button->Refresh();
        {
            std::ostringstream selection;
            selection << "device-info-click pid=0x" << std::hex << m_pid << std::dec
                      << " selected=" << (m_pid == GUIDER_3_ULTRA ? "m_busy_G3U_detail" : "m_busy_device_detial");
            log_device_detail_layout(selection.str());
        }
        if (m_pid == 0x001F && m_busy_G3U_detail) {
            if (m_busy_device_detial) {
                m_busy_device_detial->Hide();
            }
            bool bShow = !m_busy_G3U_detail->IsShown();
            m_busy_G3U_detail->Show(bShow);
            m_busy_G3U_detail->Layout();
            if (bShow) {
                m_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
            } else {
                m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
            }
        } else {
            if (m_busy_device_detial) {
                if (m_busy_G3U_detail) {
                    m_busy_G3U_detail->Hide();
                }
                bool bShow = !m_busy_device_detial->IsShown();
                m_busy_device_detial->Show(bShow);
                m_busy_device_detial->Layout();
                log_device_detail_layout(std::string("m_busy_device_detial shown=") + (bShow ? "true" : "false"));
                if (bShow) {
                    m_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
                } else {
                    m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
                }
            }
        }

        if (m_busy_device_info) {
            m_busy_device_info->Hide();
        }

        if (m_busy_circula_filter) {
            m_busy_circula_filter->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if (m_filter_button) {
            m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
        }

        if (m_idle_device_info_button) {
            m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        }
        Layout();
    });

    m_filter_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        wxCommandEvent* event = new wxCommandEvent(EVT_SWITCH_TO_FILETER);
        wxQueueEvent(m_busy_device_detial, event);
        if (m_pid == 0x001F) {
            m_clear_fan_pressed_down = !m_clear_fan_pressed_down;
            if (m_clear_fan_pressed_down) {
                ComClearFanCtrl* clearFan = new ComClearFanCtrl(OPEN);
                Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, clearFan);
                m_filter_button->SetIcon("device_filter");
            } else {
                ComClearFanCtrl* clearFan = new ComClearFanCtrl(CLOSE);
                Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, clearFan);
                m_filter_button->SetIcon("device_filter_offline");
            }
            if (m_busy_circula_filter) {
                m_busy_circula_filter->Hide();
            }
        } else {
            if (m_busy_circula_filter) {
                bool bShow = !m_busy_circula_filter->IsShown();
                m_busy_circula_filter->Show(bShow);
                m_busy_circula_filter->Layout();
                if (bShow) {
                    m_filter_button->SetBackgroundColor(wxColour(217, 234, 255));
                } else {
                    m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
                }
            }
        }

        if (m_busy_device_detial) {
            m_busy_device_detial->Hide();
        }
        if (m_busy_G3U_detail) {
            m_busy_G3U_detail->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if (m_device_info_button) {
            m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        }
        Layout();
    });

    m_idle_device_info_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
        if (!m_busy_device_info) return;

        const bool bShow = !m_busy_device_info->IsShown();
        // The data is populated by device events before the click. Only switch
        // mutually exclusive windows here; do not rebuild the whole device page.
        if (m_busy_device_detial) {
            m_busy_device_detial->Hide();
        }
        if (m_busy_G3U_detail) {
            m_busy_G3U_detail->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if (m_device_info_button) {
            m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        }

        m_busy_device_info->Show(bShow);
        m_idle_device_info_button->SetBackgroundColor(bShow ? wxColour(217, 234, 255) : wxColour(255, 255, 255));

        // Layout only the direct container. Calling SingleDeviceState::Layout()
        // here also lays out the camera and every other section, which made this
        // already-populated panel appear unnecessarily late.
        if (wxSizer *sizer = m_busy_device_info->GetContainingSizer()) {
            sizer->Layout();
        }
        if (wxWindow *container = m_busy_device_info->GetParent()) {
            container->Refresh(false);
            container->Update();
        }
    });

    // 添加设备详情
    m_busy_device_detial = new DeviceDetail(parent);
    m_busy_device_detial->SetMinSize(wxSize(-1, FromDIP(277)));
    m_busy_device_detial->Bind(EVT_SWITCH_TO_FILETER, [this](wxCommandEvent& event) {
        event.Skip();
        m_busy_device_detial->switchPage();
    });
    busySizer->Add(m_busy_device_detial, 0, wxALL | wxEXPAND, 0);
    m_busy_device_detial->Hide();

    m_busy_G3U_detail = new G3UDetail(parent);
    m_busy_G3U_detail->SetMinSize(wxSize(-1, FromDIP(277)));
    m_busy_G3U_detail->Bind(EVT_SWITCH_TO_FILETER, [this](wxCommandEvent& event) {
        event.Skip();
        m_busy_G3U_detail->switchPage();
    });
    busySizer->Add(m_busy_G3U_detail, 0, wxALL | wxEXPAND, 0);
    m_busy_G3U_detail->Hide();

    // 添加循环过滤
    m_busy_circula_filter = new StartFilter(parent);
    busySizer->Add(m_busy_circula_filter, 0, wxALL | wxEXPAND, 0);
    m_busy_circula_filter->Hide();

    //***添加设备信息布局
    m_busy_device_info = new DeviceInfoPanel(parent, wxSize(-1, FromDIP(277)));
    busySizer->Add(m_busy_device_info, 0, wxALL | wxEXPAND, 0);
    m_busy_device_info->Hide();

    // 添加温度修改确认页面
    m_busy_temp_brn = new ModifyTemp(parent);
    m_busy_temp_brn->Bind(EVT_MODIFY_TEMP_CLICKED, &SingleDeviceState::onModifyTempClicked, this);
    m_busy_temp_brn->Bind(EVT_MODIFY_TEMP_CANCEL_CLICKED, [this](wxCommandEvent& event) {
        /*event.Skip();
        m_busy_temp_brn->Hide();
        m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
        m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
        m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        Layout();*/
    });
    busySizer->Add(m_busy_temp_brn, 0, wxALL | wxEXPAND, 0);
    m_busy_temp_brn->Hide();
}

void SingleDeviceState::setupLayoutIdleCtrlPage(wxBoxSizer* idleSizer, wxPanel* parent) 
{
    // 新增温度-设备控件
    m_idle_tempMixDevice = new TempMixDevice(parent, false);
    idleSizer->Add(m_idle_tempMixDevice, 0, wxALL | wxEXPAND, 0);
}

void SingleDeviceState::msw_rescale()
{
	int test = 9;
}

void SingleDeviceState::connectEvent()
{
    //device info modify
    //外网数据更新
   MultiComMgr::inst()->Bind(COM_WAN_DEV_INFO_UPDATE_EVENT, &SingleDeviceState::onConnectWanDevInfoUpdate, this);
   //局域网数据更新
   MultiComMgr::inst()->Bind(COM_DEV_DETAIL_UPDATE_EVENT, &SingleDeviceState::onComDevDetailUpdate, this);
   //局域网连接更新
   MultiComMgr::inst()->Bind(COM_CONNECTION_READY_EVENT, &SingleDeviceState::onComConnectReady, this);
   //连接断开
   MultiComMgr::inst()->Bind(COM_CONNECTION_EXIT_EVENT, &SingleDeviceState::onConnectExit, this);
   //账号登录/登出维护（登出时关闭视频弹窗�?
   MultiComMgr::inst()->Bind(COM_WAN_DEV_MAINTAIN_EVENT, &SingleDeviceState::onComWanDevMaintain, this);
   //get file list info
   MultiComMgr::inst()->Bind(COM_GET_DEV_GCODE_LIST_EVENT, &SingleDeviceState::onFileListUpdate, this);
   //file list file send finished
   MultiComMgr::inst()->Bind(COM_START_JOB_EVENT, &SingleDeviceState::onFileSendFinished, this);
   //lan network download file finished
   MultiComMgr::inst()->Bind(COM_GET_GCODE_THUMB_EVENT, &SingleDeviceState::onLanThumbDownloadFinished, this);
   //云切片状态更�?
   MultiComMgr::inst()->Bind(COM_CONN_CLOUD_SLICE_EVENT, &SingleDeviceState::onComCloudSliceUpdate, this);
   //云任务状态更�?
   MultiComMgr::inst()->Bind(COM_CONN_JOB_INFO_EVENT, &SingleDeviceState::onComJobInfoUpdate, this);

   //local file list
   m_fileListbutton->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onFileListClicked, this);

   // time lapse video
   m_timeLapseVideoBtn->Bind(wxEVT_LEFT_DOWN, &SingleDeviceState::onTimeLapseVideoBtnClicked, this);

   //m_lamp_control_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
   //    if (m_lamp_control_button->GetFlashForgeSelected()) {
   //        // 关灯
   //        ComLightCtrl *lightctrl = new ComLightCtrl(CLOSE);
   //        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
   //        m_lamp_control_button->SetIcon("device_lamp_control");
   //        m_lamp_control_button->Refresh();
   //        m_lamp_control_button->SetFlashForgeSelected(false);
   //    } else {
   //        //开�?
   //        ComLightCtrl *lightctrl = new ComLightCtrl(OPEN);
   //        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
   //        m_lamp_control_button->SetIcon("device_lamp_control_press");
   //        m_lamp_control_button->Refresh();
   //        m_lamp_control_button->SetFlashForgeSelected(true);
   //    }
   //});
   m_check_printer_status_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &e) {
       checkPrinterStatus();
   });
   m_check_printer_status_timer.Start(1000);

   m_download_tool.Bind(EVT_FF_DOWNLOAD_FINISHED, &SingleDeviceState::onDownloadImageFinished, this);
}


void SingleDeviceState::UpdateScrollVirtualSize()
{
    wxSizer* sizer = GetSizer();
    if (!sizer)
        return;

    //设置摄像头缩放大�?
    // Camera height is owned by FFRTMPVideoCtrl::OnSize() (4:3 video plus the
    // status bar). Changing it here to width * 0.75 removes the status-bar
    // height, after which OnSize restores it and causes a layout oscillation.

    // 获取内容需要的最小尺寸（核心：取sizer计算出的最小尺寸）
    wxSize minContentSize = sizer->CalcMin();
    // 获取滚动窗口当前可视区域大小
    wxSize clientSize = GetClientSize();

    // This page is horizontally responsive and only scrolls vertically.
    // Printing controls can slightly increase CalcMin() (often by the
    // scrollbar gutter width), but that must not widen the virtual canvas.
    int virtW = clientSize.x;
    int virtH = wxMax(minContentSize.y, clientSize.y);

    // 设置滚动窗口虚拟画布大小
    SetVirtualSize(virtW, virtH);
}

void SingleDeviceState::OnScrollWinSize(wxSizeEvent& evt)
{
    UpdateScrollVirtualSize();
    evt.Skip();
}

void SingleDeviceState::onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event) 
{
    event.Skip();
    // 摄像头绑定设备的拉流地址更新（可能与当前选中设备不同）�?
    // 注意：这里只在有新地址时切流，绝不因设�?MQTT)离线断开摄像头（视频流走独立服务器）�?
    if (event.id == m_camera_cur_id) {
        refreshCameraStream();
    }
    if (m_cur_id == event.id) {
        //当前选中的设备，获取相应数据，更新界面显�?
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
        // 离线判断
        std::string status = wan_device_display_status(data);
        camdbg_log("onConnectWanDevInfoUpdate wanStatus=" + status);
        if (status.compare("offline") == 0) {
            setPageOffline();
        } else {
            fillValue(data,true);
        }
        return;
    }
    
    if (-1 == m_cur_id) {
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
        if (data.wanDevInfo.serialNumber.compare(m_cur_serial_number) == 0 && is_wan_device_online(data)) {
            m_cur_id = event.id;
            setCurId(m_cur_id);
        }
        return;
    }
}

void SingleDeviceState::onComDevDetailUpdate(ComDevDetailUpdateEvent &event) 
{
    event.Skip();
    if (m_cur_id == event.id) {
        // 当前选中的设备，获取相应数据，更新界面显�?
        bool  valid = false;
        const com_dev_data_t& data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
        fillValue(data);
    }
    // 摄像头绑定设备的视频流刷新（绑定设备可能与当前选中设备不同）�?
    if (event.id == m_camera_cur_id) {
        refreshCameraStream();
    }
}

void SingleDeviceState::onComCloudSliceUpdate(ComCloudSliceUpdateEvent& event)
{
    event.Skip();
    if (m_cur_id == event.id) {
        fillCloudValue(*event.state);
    }
}

void SingleDeviceState::onComJobInfoUpdate(ComJobInfoUpdateEvent& event)
{
    event.Skip();
    if (m_cur_id == event.id) {
        fillJobValue(*event.info);
    }
}

void SingleDeviceState::onComConnectReady(ComConnectionReadyEvent &event) 
{
    event.Skip();
    // MultiComMgr has stored devDetail before broadcasting this event. Refresh
    // the selected device immediately instead of waiting for another update.
    if (m_cur_id == event.id) {
        bool valid = false;
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id, &valid);
        if (valid && data.devDetail) {
            updateDeviceInfoPanel(data);
        }
    }
    if (-1 == m_cur_id) {
        const com_dev_data_t &data  = MultiComMgr::inst()->devData(event.id);
        std::string           lan_serial_number = data.lanDevInfo.serialNumber;
        if (!lan_serial_number.empty()  && lan_serial_number.compare(m_cur_serial_number) == 0) {
            m_cur_id = event.id;
            setCurId(m_cur_id);
        }
        return;
    }
}

// void SingleDeviceState::onConnectExit(ComConnectionExitEvent &event)
// {
//     event.Skip();
//     unbind_ts("A. onConnectExit ENTER id=" + std::to_string(event.id)
//               + " cur_id=" + std::to_string(m_cur_id)
//               + " cam_id=" + std::to_string(m_camera_cur_id));
//     if (event.id == m_cur_id) {
//         unbind_ts("B1. setPageOffline BEGIN (整页离线重建)");
//         setPageOffline();
//         unbind_ts("B2. setPageOffline END");
//     }
//     // 摄像头“绑定设备”的连接彻底退出（解绑/移除/连接丢失，设备已�?MultiComMgr 移除）→
//     // 该设备已不存在，视频不可能再继续：先关闭视频播放窗口、停止视频流，再解绑摄像头�?
//     // 注意：这�?MQTT 上报的临�?offline 不同——后者连接仍在、流可能正常，不在此断开
//     // （沿用既有“设备离线与摄像头解耦”）；此处是连接真正消失，必须收起视频�?
//     if (event.id == m_camera_cur_id) {
//         // 先同步解绑，避免后续事件再次进入本分支�?
//         m_camera_cur_id = -1;
//         m_camera_stream_url.clear();
//         // 分两步、分不同 tick 执行，优先保证产品体验：
//         //   tick1：关闭视频弹�?+ 画面切到“断开连接”并隐藏 —�?�?UI、非阻塞�?
//         //          让用户立刻看到“设备已解绑、摄像头画面已隐藏”�?
//         //   tick2：真正回收解码线程（可能�?DNS/连接/关闭阻塞）—�?放到视觉反馈之后�?
//         //          即便回收有瞬时卡顿，也不影响解绑画面的即时呈现�?
//         // （另外，解绑正在播放的设备时本事件往往还会触发 setPageOffline 整页重建�?
//         //   把摄像头拆除分到后续 tick 也能避免与其叠加成一次长卡顿。）
//         unbind_ts("C. onConnectExit 调度 camera tick1 (CallAfter)");
//         CallAfter([this]() {
//             if (!m_camera_panel) return;
//             unbind_ts("D. camera tick1 ENTER");
//             m_camera_panel->closePopup();            // 关闭视频播放窗口（若打开�?
//             unbind_ts("E. closePopup END");
//             m_camera_panel->showOfflineImmediate();  // 立即隐藏画面 + “断开连接”占位（非阻塞）
//             unbind_ts("F. showOfflineImmediate END (画面已隐�? 用户此刻应看到解�?");
//             CallAfter([this]() {
//                 if (!m_camera_panel) return;
//                 unbind_ts("H. camera tick2 ENTER -> reapStoppedStream BEGIN");
//                 m_camera_panel->reapStoppedStream();  // 后台回收解码线程
//                 unbind_ts("I. reapStoppedStream END");
//             });
//             unbind_ts("G. camera tick1 EXIT (已调�?tick2)");
//         });
//     }
// }

void SingleDeviceState::onConnectExit(ComConnectionExitEvent &event)
{
    event.Skip();

    if (m_camera_panel) m_camera_panel->logPanelState("SingleDeviceState::onConnectExit enter");

    if (event.id == m_cur_id) {
        setPageOffline();   // 页面拆除(不含摄像�?二者解�?
        setDevicePageState(DevicePageState::NoDevice);
    }

    // 连接彻底退�?设备已从 MultiComMgr 移除,流不可能再继�?必须显式收起视频�?
    // (这与 MQTT 临时 offline 不同——那种情况连接还在、由流健康度自行决定,故不在此断�?
    if (event.id == m_camera_cur_id) {
        m_camera_cur_id = -1;        // 先同步解�?防止后续事件重入本分�?
        m_camera_stream_url.clear();

        CallAfter([this]() {          // 延后一 tick,避免与整页重建叠加成长卡�?
            if (!m_camera_panel) return;
            m_camera_panel->logPanelState("onConnectExit CallAfter before camera offline");
            // Device unbind/removal has a dedicated path: invalidate the old
            // URL/comId and keep the current camera surface visible offline.
            m_camera_panel->handleDeviceUnbound();
            m_camera_panel->logPanelState("onConnectExit CallAfter after camera disconnected");
            CallAfter([this]() {
                if (!m_camera_panel) return;
                m_camera_panel->reapStoppedStream();  // 回收解码线程
            });
        });
    }
}

void SingleDeviceState::onComWanDevMaintain(ComWanDevMaintainEvent &event)
{
    event.Skip();
    if (event.sessionGeneration != 0
        && event.sessionGeneration != MultiComMgr::inst()->wanSessionGeneration()) {
        return;
    }
    if (m_camera_panel) m_camera_panel->logPanelState("onComWanDevMaintain enter");
    if (event.serviceState == WanServiceState::Reconnecting) {
        camdbg_log("WAN service reconnecting; keep device page and last device state");
        return;
    }
    if (event.serviceState == WanServiceState::ApiUnavailable) {
        camdbg_log("WAN API unavailable; keep device page and distinguish from printer offline");
        return;
    }
    // 账号登出（login=false）：�?WAN)设备全部被移除�?
    if (!event.login) {
        // 刷新并清空设备页到离线态：仅当当前页展示的正是云设备（或该设备已被移除）时才清页；
        // 本地(LAN)设备不受账号登出影响，保持原样不动�?
        bool valid = false;
        const com_dev_data_t &data = MultiComMgr::inst()->devData(m_cur_id, &valid);
        if (!valid || data.connectMode == COM_CONNECT_WAN) {
            setPageOffline();   // 整页离线重建（清空并刷新一次）；摄像头窗口与此解耦、不受影�?
            setDevicePageState(DevicePageState::NoDevice);
        }
        // 摄像头：停流并解绑，但保留内联窗口，退回“缺省图 + 不可播放按钮”的初始不可用状态�?
        if (m_camera_panel) {
            // 登出后保留摄像头布局区域，并显示缺省图。该标志也会阻止并发�?
            // onConnectExit 延迟回调再次隐藏窗口�?
            m_camera_panel->setOffline(true);
            m_camera_panel->logPanelState("onComWanDevMaintain after setOffline(true)");
            // m_camera_panel->closePopup();
            m_camera_cur_id = -1;
            m_camera_stream_url.clear();
        }
    }
}

void SingleDeviceState::onTargetTempModify(wxCommandEvent &event)
{
    /*event.Skip();
    wxTextCtrl *click_btn = dynamic_cast<wxTextCtrl *>(event.GetEventObject());
    if (event.GetEventType() == wxEVT_TEXT) {
        if (click_btn && m_tempCtrl_top && m_tempCtrl_bottom && m_tempCtrl_mid) {
               m_busy_temp_brn->Show();
               m_busy_temp_brn->Layout();
               if (m_busy_circula_filter) {
               m_busy_circula_filter->Hide();
            }
            if (m_busy_device_detial) {
                m_busy_device_detial->Hide();
            }
            if (m_filter_button) {
                m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
            }
            if (m_device_info_button) {
                m_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
            }
            Layout();
        }
        double top_temp;
        double bottom_temp;
        m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
        m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
        if (top_temp > 280) {
            m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            m_tempCtrl_top->SetTagTemp(280, true);
            m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        }
        if (bottom_temp > 110) {
            m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            m_tempCtrl_bottom->SetTagTemp(110, true);
            m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        }
    }*/
}

void SingleDeviceState::onModifyTempClicked(wxCommandEvent &event) 
{
    /*event.Skip();
    m_busy_temp_brn->Hide();
    Layout();
    double top_temp;
    double bottom_temp;
    double mid_temp; 
    bool bTop = m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
    if (!bTop) {
       m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
       m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
       top_temp = m_right_target_temp;
       m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this); 
    }
    bool bBottom = m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
    if (!bBottom) {
       m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
       m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
       bottom_temp = m_plat_target_temp;
       m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
    }
    bool bMid = m_tempCtrl_mid->GetTagTemp().ToDouble(&mid_temp);
    ComTempCtrl* tempCtrl = new ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);*/
}

void SingleDeviceState::onDevStateChanged(std::string devState, const com_dev_data_t& data)
{
    std::string state = devState; // 状�?
    //state             = P_CLOUD_SLICING;
    if (data.devDetail->pid == 0x001F) {
        setG3UProductAuthority(*data.devProduct);
    } else {
        setDevProductAuthority(*data.devProduct);
    }

    // 设备遥测每帧都会走到这里，仅当状态真正变化时才需要重排整页布局�?
    // 否则每帧一�?SingleDeviceState::Layout() 会与摄像头持续重绘叠加，
    // 导致喷头面板等控件“时隐时现”。这里记录状态是否变化，末尾据此决定是否 Layout()�?
    // 说明：分支内�?Show()/Hide() 对已处于目标状态的窗口是幂等空操作，不会触发重绘，
    // 因此保留每帧执行不影响性能；连续数值（温度/进度/灯状态等）仍需每帧更新�?
    bool devStateChanged = (m_cur_dev_state != state);
    m_cur_dev_state = state;
    //m_panel_control_print->Show();
    //m_panel_control_cloud->Hide();
    //m_staticText_cloud_text->Hide();
    //m_staticText_cloud_queue_count->Hide();
    //m_staticText_cloud_queue_text->Hide();
    //m_staticText_cloud_queue_tip->Hide();
    if (state == P_READY) {
        
    } else if (state == P_CLOUD_SLICING || state == P_DOWNLOADING || state == P_UNZIPPING || state == P_SENDING) {
        m_print_button->Hide();
        m_cancel_button->Hide();
        m_staticText_time_label->Hide();
        m_staticText_count_time->Hide();
        m_panel_control_print->Hide();
        if (state == P_CLOUD_SLICING) {
            m_panel_control_cloud->Show();
        } else {
            m_panel_control_cloud->Hide();
        }
        if (state == P_SENDING) {
            m_staticText_cloud_text->SetForegroundColour(wxColour(50, 141, 251));
            m_staticText_cloud_text->SetLabel(_L("Sending task..."));
            m_staticText_cloud_text->Show();
            m_progress_bar->Hide();
        }
    } else {
        m_isCloudState = false;
        m_print_button->Show();
        m_cancel_button->Show();
        m_staticText_time_label->Show();
        m_staticText_count_time->Show();
        m_panel_control_print->Show();
        m_panel_control_cloud->Hide();
        m_staticText_cloud_queue_count->Hide();
        m_staticText_cloud_queue_text->Hide();
        m_staticText_cloud_queue_tip->Hide();
        m_staticText_cloud_text->Hide();
    }

    if (state == P_READY) {
        if (m_isCloudState) {
            Layout();
            return;
        }
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(false);
        m_machine_idle_panel->Show();
        m_machine_idle_info_panel->Show();
        m_machine_ctrl_info_panel->Hide();
        m_machine_ctrl_panel->Hide();
        m_busyState_top_gap->Hide();
        m_busyState_bottom_gap->Hide();
        m_offline_info_page_gap->Hide();
        m_panel_idle_text->Show();
        m_panel_separotor8->Show();
        m_nozzles->SetCurState(true);
        wxString idle_state = _L("idle");
        setTipMessage(idle_state, "#00CD6D", "", false);
        std::string lightStatus = data.devDetail->lightStatus;
        m_idle_tempMixDevice->setState(1, lightStatus.compare(CLOSE));

        m_cur_print_file_name.clear();
        setIdlePrinterText();
        m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        m_idle_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
        m_busy_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
        reInitMaterialPic();
    } else if (state == P_COMPLETED || state == CANCEL) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        if (m_machine_idle_panel->IsShown()) {
            //showMaterialStation(true);
            m_scrolledWindow->Hide();
            m_FileList_split_line->Hide();
            m_panel_print_btn->Hide();
            m_timeLapseVideoPnl->Hide();
            m_panel_idle_text->Hide();
        }
        m_machine_ctrl_panel->Show();
        m_machine_ctrl_info_panel->Show();
        m_machine_idle_panel->Hide();
        m_machine_idle_info_panel->Hide();
        m_print_button->SetTextColor(wxColor("#999999"));
        m_cancel_button->SetTextColor(wxColor("#999999"));
        m_print_button->Enable(false);
        m_cancel_button->Enable(false);
        m_print_button->SetIcon("device_pause_print_disable");
        m_cancel_button->SetIcon("device_cancel_print_disable");
        m_nozzles->SetCurState(false);
        wxString compelete_state = _L("completed");
        wxString compelete_info  = _L("Print completed,clean platform!");
        setTipMessage(compelete_state, "#328DFB", compelete_info, true, true);

        m_staticText_time_label->SetLabel(_L("Total Time"));

        double totalTime = data.devDetail->printDuration; // 本次打印耗时
        m_staticText_count_time->SetLabel(convertSecondsToHMS(totalTime));
    } else if (state == P_BUSY) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_timeLapseVideoPnl->Hide();
        m_idle_tempMixDevice->Show();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        m_machine_idle_panel->Show();
        m_machine_idle_info_panel->Show();
        m_machine_ctrl_info_panel->Hide();
        m_machine_ctrl_panel->Hide();
        m_panel_idle_text->Hide();
        m_panel_separotor8->Hide();
        m_busyState_top_gap->Show();
        m_busyState_bottom_gap->Show();
        m_offline_info_page_gap->Hide();
        m_nozzles->SetCurState(false);
        wxString busy_state = _L("busy");
        wxString busy_info  = _L("Print cancelled,in cache command");
        setTipMessage(busy_state, "#F9B61C", busy_info, false, false);
        std::string lightStatus = data.devDetail->lightStatus;
        m_idle_tempMixDevice->setState(1, lightStatus.compare(CLOSE));

        // splitIdleTextLabel();
        setIdlePrinterText();
        m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        m_idle_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
        m_busy_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
    } else if (state == P_CALIBRATE || state == P_LOADING) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        m_machine_idle_panel->Show();
        m_machine_idle_info_panel->Show();
        m_machine_ctrl_info_panel->Hide();
        m_machine_ctrl_panel->Hide();
        m_nozzles->SetCurState(false);
        wxString busy_state = _L("busy");
        wxString busy_info  = _L("");
        setTipMessage(busy_state, "#F9B61C", busy_info, false, false);
        std::string lightStatus = data.devDetail->lightStatus;
        m_idle_tempMixDevice->setState(1, lightStatus.compare(CLOSE));
        // splitIdleTextLabel();
        setIdlePrinterText();
        m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        m_idle_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
        m_busy_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
    } else if (state == P_ERROR) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        m_machine_idle_panel->Show();
        m_machine_idle_info_panel->Show();
        m_machine_ctrl_info_panel->Hide();
        m_machine_ctrl_panel->Hide();
        m_panel_separotor8->Hide();
        m_busyState_top_gap->Show();
        m_busyState_bottom_gap->Show();
        m_offline_info_page_gap->Hide();
        m_panel_idle_text->Hide();
        m_nozzles->SetCurState(false);
        wxString    error_state = _L("error");
        std::string error_info  = data.devDetail->errorCode;
        wxString    trans_error = FFUtils::converDeviceError(error_info);
        setTipMessage(error_state, "#FB4747", trans_error, !trans_error.empty(), false);
        m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        std::string lightStatus = data.devDetail->lightStatus;
        m_idle_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
        m_busy_lamp_bar->SetLampState(data.devProduct->lightCtrlState == 0, lightStatus.compare(CLOSE));
    } else if (state == PAUSE) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        if (m_machine_idle_panel->IsShown()) {
            //showMaterialStation(true);
            m_scrolledWindow->Hide();
            m_FileList_split_line->Hide();
            m_panel_print_btn->Hide();
            m_timeLapseVideoPnl->Hide();
            m_panel_idle_text->Hide();
        }
        m_machine_ctrl_panel->Show();
        m_machine_ctrl_info_panel->Show();
        m_machine_idle_panel->Hide();
        m_machine_idle_info_panel->Hide();
        m_nozzles->SetCurState(false);
        wxString print_state = _L("pause");
        setTipMessage(print_state, "#982187");

        m_print_button->Enable(true);
        m_cancel_button->Enable(true);
        m_print_button->SetIcon("device_pause_print");
        m_cancel_button->SetIcon("device_cancel_print");
        m_print_button->SetTextColor(wxColour(51, 51, 51));
        m_cancel_button->SetTextColor(wxColour(51, 51, 51));
        m_print_button->SetLabel(_L("continue print"));
        m_print_button->SetIcon("device_continue_print");
        m_print_button->Refresh();

        m_staticText_time_label->SetLabel(_L("Remaining Time"));
        double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
        m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
    } else if (state == P_PAUSING || state == P_HEATING) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        if (m_machine_idle_panel->IsShown()) {
            //showMaterialStation(true);
            m_scrolledWindow->Hide();
            m_panel_print_btn->Hide();
            m_FileList_split_line->Hide();
            m_timeLapseVideoPnl->Hide();
            m_panel_idle_text->Hide();
        }
        m_machine_ctrl_panel->Show();
        m_machine_ctrl_info_panel->Show();
        m_machine_idle_panel->Hide();
        m_machine_idle_info_panel->Hide();
        m_nozzles->SetCurState(false);
        wxString print_state = _L("pausing");
        if (state == P_HEATING) {
            print_state = _L("heating");
        }
        setTipMessage(print_state, "#982187");
        m_print_button->SetTextColor(wxColor("#999999"));
        m_cancel_button->SetTextColor(wxColor("#999999"));
        m_print_button->Enable(false);
        m_cancel_button->Enable(false);
        m_print_button->SetIcon("device_pause_print_disable");
        m_cancel_button->SetIcon("device_cancel_print_disable");
        m_print_button->SetLabel(_L("continue print"));
        m_print_button->SetIcon("device_continue_print");
        m_print_button->Refresh();

        m_staticText_time_label->SetLabel(_L("Remaining Time"));
        double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
        m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
    } else if (state == P_CLOUD_SLICING || state == P_DOWNLOADING || state == P_UNZIPPING || state == P_SENDING) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(false);
        if (m_machine_idle_panel->IsShown()) {
            //showMaterialStation(true);
            m_scrolledWindow->Hide();
            m_FileList_split_line->Hide();
            m_panel_print_btn->Hide();
            m_timeLapseVideoPnl->Hide();
            m_panel_idle_text->Hide();
        }
        m_machine_ctrl_panel->Show();
        m_machine_ctrl_info_panel->Show();
        m_machine_idle_panel->Hide();
        m_machine_idle_info_panel->Hide();
        m_nozzles->SetCurState(false);
        wxString print_state = _L("busy");
        setTipMessage(print_state, "#F9B61C");
    } else {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
        m_tempCtrl_panel->SwitchTargetTemp(true);
        if (m_machine_idle_panel->IsShown()) {
            //showMaterialStation(true);
            m_scrolledWindow->Hide();
            m_panel_print_btn->Hide();
            m_FileList_split_line->Hide();
            m_timeLapseVideoPnl->Hide();
            m_panel_idle_text->Hide();
        }
        m_machine_ctrl_panel->Show();
        m_machine_ctrl_info_panel->Show();
        m_machine_idle_panel->Hide();
        m_machine_idle_info_panel->Hide();
        m_nozzles->SetCurState(false);
        wxString print_state = _L("printing");
        setTipMessage(print_state, "#4D54FF");

        m_print_button->Enable(true);
        m_cancel_button->Enable(true);
        m_print_button->SetIcon("device_pause_print");
        m_cancel_button->SetIcon("device_cancel_print");
        m_print_button->SetTextColor(wxColour(51, 51, 51));
        m_cancel_button->SetTextColor(wxColour(51, 51, 51));
        m_print_button->SetLabel(_L("pause print"));
        m_print_button->SetIcon("device_pause_print");
        m_print_button->Refresh();
        m_staticText_time_label->SetLabel(_L("Remaining Time"));
        double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
        m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
    }
    // 仅在设备状态真正切换时重排布局，避免每�?Layout() 造成的抖�?闪烁
    if (devStateChanged) {
        Layout();
    }
    UpdateScrollVirtualSize();
}

void SingleDeviceState::onCancelPrint(wxCommandEvent &event)
{
    //event.Skip();
    m_cancel_confirm_page->Close();
    m_cur_printing_ctrl    = 3;
    std::string printState = CANCEL;
    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(m_cur_id, &valid);
    if (valid) {
         std::string jobId   = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
         ComJobCtrl *jobCtrl = new ComJobCtrl(jobId, printState);
         Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
         // m_cancel_confirm_page->Hide();
    }
}

void SingleDeviceState::onContinuePrint(wxCommandEvent &event)
{
    //event.Skip();
    m_cancel_confirm_page->Close();
    //m_cancel_confirm_page->Hide();
}

void SingleDeviceState::onFileListClicked(wxMouseEvent& event) 
{
    Freeze();
    if (m_scrolledWindow && m_scrolledWindow->IsShown()) {
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();

        m_printBtn->Enable(false);
        if (m_curSelectedFileItem) {
            m_curSelectedFileItem->SetPressed(false);
            m_curSelectedFileItem = nullptr;
        }
        m_timeLapseVideoPnl->Hide();
        //showMaterialStation(true);
        Layout();
        UpdateScrollVirtualSize();
        Thaw();
        return;
    }

    if (m_curId_first_Click_fileList) {
         ComGetDevGcodeList* devGcodeList = new ComGetDevGcodeList();
         Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, devGcodeList);
         m_curId_first_Click_fileList = false;
    }

    if (m_scrolledWindow && !m_scrolledWindow->IsShown()) {
         //showMaterialStation(false);
         m_timeLapseVideoPnl->Hide();
         m_scrolledWindow->Scroll(0, 0);
         m_scrolledWindow->Refresh();
         m_scrolledWindow->Show();
         m_panel_print_btn->Show();
         m_FileList_split_line->Show();
    }
    Layout();
    UpdateScrollVirtualSize();
    Thaw();
}

void SingleDeviceState::onFileListRefreshBtnClicked(wxMouseEvent& event)
{
    m_printBtn->Enable(false);
    m_curSelectedFileItem = nullptr;
    ComGetDevGcodeList* devGcodeList = new ComGetDevGcodeList();
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, devGcodeList);
}

void SingleDeviceState::onFileListUpdate(ComGetDevGcodeListEvent& event)
{
    auto setGcodeData = [](FileItem::FileData &fileData, const fnet_gcode_data_t &gcodeData) {
        fileData.wxName = wxString::FromUTF8(gcodeData.fileName);
        fileData.gcodeData.fileName = gcodeData.fileName;
        fileData.gcodeData.thumbUrl = gcodeData.thumbUrl;
        fileData.gcodeData.printingTime = gcodeData.printingTime;
        fileData.gcodeData.totalFilamentWeight = gcodeData.totalFilamentWeight;
        fileData.gcodeData.useMatlStation = gcodeData.useMatlStation;
        fileData.gcodeData.gcodeToolDatas.resize(gcodeData.gcodeToolCnt);
        for (size_t j = 0; j < fileData.gcodeData.gcodeToolDatas.size(); ++j) {
            const fnet_gcode_tool_data_t &gcodeToolData = gcodeData.gcodeToolDatas[j];
            fileData.gcodeData.gcodeToolDatas[j].toolId = gcodeToolData.toolId;
            fileData.gcodeData.gcodeToolDatas[j].slotId = gcodeToolData.slotId;
            fileData.gcodeData.gcodeToolDatas[j].materialName = gcodeToolData.materialName;
            fileData.gcodeData.gcodeToolDatas[j].materialColor = gcodeToolData.materialColor;
            fileData.gcodeData.gcodeToolDatas[j].filemanetWeight = gcodeToolData.filemanetWeight;
        }
    };
    event.Skip();
    if (m_cur_id == event.id) {
        if (event.wanGcodeList.gcodeCnt != 0) {
             const com_gcode_list_t &gcodeList = event.wanGcodeList;
             std::vector<FileItem::FileData> fileDataList(gcodeList.gcodeCnt);
             for (size_t i = 0; i < fileDataList.size(); ++i) {
                 setGcodeData(fileDataList[i], gcodeList.gcodeDatas[i]);
             }
             if (!m_fileItemList.empty()) {
                clearFileList();
                m_scrolledWindow->Scroll(0, 0);
             }
             initFileList(fileDataList);
         }
        if (event.lanGcodeList.gcodeCnt != 0) {
             const com_gcode_list_t &gcodeList = event.lanGcodeList;
             std::vector<FileItem::FileData> fileDataList(gcodeList.gcodeCnt);
             for (int i = 0; i < fileDataList.size(); ++i) {
                 setGcodeData(fileDataList[i], gcodeList.gcodeDatas[i]);
             }
             if (!m_fileItemList.empty()) {
                clearFileList();
                m_scrolledWindow->Scroll(0, 0);
             }
             initFileList(fileDataList);
        }
        if (event.wanGcodeList.gcodeCnt == 0 && event.lanGcodeList.gcodeCnt == 0) {
            if (!m_fileItemList.empty()) {
                clearFileList();
                m_scrolledWindow->Scroll(0, 0);
            }
        }
    }
}

void SingleDeviceState::onFileListPrintBtnClicked(wxMouseEvent& event) 
{
    event.Skip();
    if (m_curSelectedFileItem == nullptr) {
         return;
    }
    PrintDevLocalFileDlg printDevLocalFileDlg(wxGetApp().mainframe);
    wxImage *image;
    wxImage defultImage;
    if (m_curSelectedFileItem->m_data.srcImage.IsOk()) {
        image = &m_curSelectedFileItem->m_data.srcImage;
    } else {
        std::string name = m_curSelectedFileItem->m_data.gcodeData.fileName;
        std::string suffix = name.substr(name.find_last_of(".") + 1);
        ScalableBitmap bmp(&printDevLocalFileDlg, FileItem::getImageNameByType(suffix), FromDIP(117));
        defultImage = bmp.bmp().ConvertToImage();
        image = &defultImage;
    }
    const com_gcode_data_t &gcodeData = m_curSelectedFileItem->m_data.gcodeData;
    com_local_job_data_t jobData;
    jobData.fileName = gcodeData.fileName;
    jobData.printNow = true;
    if (!printDevLocalFileDlg.setupData(m_cur_id, gcodeData, *image)) {
        return;
    }
    if (printDevLocalFileDlg.ShowModal(jobData) != wxID_OK) {
        return;
    }
    ComStartJob* startJob = new ComStartJob(jobData);
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, startJob);
    m_printBtn->Enable(false);
    m_refreshBtn->Enable(false);
}

void SingleDeviceState::onFileSendFinished(ComStartJobEvent& event)
{
    event.Skip();
    BOOST_LOG_TRIVIAL(info) << "SingleDeviceState:onFileSendFinished, com_id: " << event.id << ", " << event.ret;
    m_printBtn->Enable(true);
    m_refreshBtn->Enable(true);
}

void SingleDeviceState::onLanThumbDownloadFinished(ComGetGcodeThumbEvent& event) 
{
    event.Skip(); 
    if (event.ret == COM_OK) {
         wxMemoryInputStream stream(event.thumbData.data(), event.thumbData.size());
         wxImage  image(stream, wxBITMAP_TYPE_ANY);
         for (const auto& item : m_fileItemList) {
             if (item->m_data.commandId == event.commandId) {
                item->m_data.srcImage = image;
                item->m_data.scaledImage = image.Rescale(FILELIST_PIC_WIDTH, FILELIST_PIC_HEIGHT);
                break;
             } else {
                continue;
             }
         }
    } else {
         BOOST_LOG_TRIVIAL(info) << "SingleDeviceState:onLanThumbDownloadFinished, com_id: " << event.id << ", " << event.ret;
    }
}

void SingleDeviceState::onTimeLapseVideoBtnClicked(wxMouseEvent& event)
{
    Freeze();
    if (m_timeLapseVideoPnl->IsShown()) {   
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->Hide();
        //showMaterialStation(true);
    } else {
        //showMaterialStation(false);
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->updateVideoList();
        m_timeLapseVideoPnl->Show();
    }
    Layout();
    UpdateScrollVirtualSize();
    Thaw();
}

void SingleDeviceState::onDownloadImageFinished(FFDownloadFinishedEvent& event)
{
    if (!event.succeed) {
        return;
    }
    if (event.taskId == m_download_title_image_task_id) {
        if (m_material_picture != nullptr) {
            wxMemoryInputStream stream(event.data.data(), event.data.size());
            wxImage image(stream, wxBITMAP_TYPE_ANY);
            if (image.IsOk()) {
                m_material_picture->SetImage(image.Rescale(MATERIAL_PIC_WIDTH, MATERIAL_PIC_HEIGHT));
            }
        }
    }
    auto it = m_download_file_list_image_map.find(event.taskId);
    if (it != m_download_file_list_image_map.end()) {
        wxMemoryInputStream stream(event.data.data(), event.data.size());
        wxImage image(stream, wxBITMAP_TYPE_ANY);
        if (image.IsOk()) {
            it->second->m_data.srcImage = image;
            it->second->m_data.scaledImage = image.Rescale(FILELIST_PIC_WIDTH, FILELIST_PIC_HEIGHT);
        }
    }
}

void SingleDeviceState::setTipMessage(const wxString& title, const std::string& titleColor, const wxString& info, bool showInfo, bool showBtn)
{
    m_staticText_device_tip->SetLabel(title); 
    m_staticText_device_tip->SetForegroundColour(wxColour(titleColor));
    m_staticText_device_info->SetLabel(info);
    m_staticText_device_info->SetMinSize(wxSize(FromDIP(432), FromDIP(45)));

    m_staticText_device_info->Show(showInfo);
    m_clear_button->Show(showBtn);
    Layout();
}

void SingleDeviceState::checkPrinterStatus()
{
    if (m_cur_id < 0 || m_block_status_check) {
        return;
    }
    bool valid;
    const fnet_dev_detail_t *devDetail = MultiComMgr::inst()->devData(m_cur_id, &valid).devDetail;
    if (!valid || strcmp(devDetail->status, "error") != 0) {
        return;
    }
    if (PrinterErrorMsgDlg::isErrorCodeHandled(devDetail->errorCode)) {
        time_t elapsedTime = time(nullptr) - m_status_check_message_show_time;
        if (elapsedTime > 20 || devDetail->errorCode != m_status_check_error_code) {
            m_status_check_error_code = devDetail->errorCode; // 进入事件循环后之前获取的devDetail可能失效
            m_block_status_check = true;
            PrinterErrorMsgDlg(wxGetApp().mainframe, m_cur_id, devDetail->errorCode).ShowModal();
            m_block_status_check = false;
            m_status_check_message_show_time = time(nullptr);
        }
    }
}

wxString SingleDeviceState::convertSecondsToHMS(int totalSeconds)
{
    int hours   = totalSeconds / 3600;
    int remainingSeconds = totalSeconds % 3600;
    int minutes          = remainingSeconds / 60;
    int secs             = remainingSeconds % 60; 

    wxString hoursStr   = wxString::Format("%02d", hours);
    wxString minutesStr = wxString::Format("%02d", minutes);  

    wxString stream = hoursStr.append(_L("h "));
    stream.append(minutesStr);
    stream.append(_L("min "));
    return stream;
    /*    std::ostringstream stream;
        stream << std::setfill('0') << std::setw(1) << hours << _L("h ") << std::setfill('0') << std::setw(2) << minutes << _L("min ");
        return stream.str(); */
}

static std::string hlsUrlToFlv(const std::string &hls)
{
    if (hls.empty())
        return hls;
    std::string url = hls;
    // 去掉查询参数
    std::string::size_type q = url.find('?');
    if (q != std::string::npos)
        url.erase(q);
    // 将结尾的 .m3u8 替换�?.flv
    const std::string ext = ".m3u8";
    if (url.size() >= ext.size() && url.compare(url.size() - ext.size(), ext.size(), ext) == 0)
        url.replace(url.size() - ext.size(), ext.size(), ".flv");
    return url;
}

void SingleDeviceState::updateDeviceInfoPanel(const com_dev_data_t& data)
{
    if (!data.devDetail || !m_busy_device_info) {
        return;
    }

    const std::string machine_type = FFUtils::getPrinterName(data.devDetail->pid);
    const std::string nozzle_model = data.devDetail->nozzleModel;
    std::string       measure      = data.devDetail->measure;
    if (!measure.empty()) measure.append("mm");

    const std::string firmware_version = data.devDetail->firmwareVersion;
    const std::string serial_number = data.connectMode == COM_CONNECT_LAN
                                          ? data.lanDevInfo.serialNumber
                                          : data.wanDevInfo.serialNumber;
    const wxString cumulative_print_time = format_cumulative_print_time(*data.devDetail, data.connectMode);
    const wxString cumulative_filament = format_cumulative_filament(data.devDetail->cumulativeFilament, data.connectMode);

    m_busy_device_info->SetDeviceInfo(machine_type, nozzle_model, measure, firmware_version, serial_number,
                                      cumulative_print_time, cumulative_filament, data.devDetail->ipAddr);
}

void SingleDeviceState::fillValue(const com_dev_data_t& data,bool wanDev)
{
    if (!data.devDetail) {
        return;
    }
    // Model, nozzle, firmware and SN are valid for both WAN and LAN. Display
    // them as soon as devDetail exists, independently of dynamic WAN handling.
    updateDeviceInfoPanel(data);
    std::string state = data.devDetail->status; // 状�?
    if (wanDev) {
        state = wan_device_display_status(data);
    }
    onDevStateChanged(state, data);
    if (state.compare("printing") == 0 && !m_isCloudState) {
        double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
        m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
    }

    m_busy_lamp_bar->SetCameraState(false);
    m_idle_lamp_bar->SetCameraState(false);
    // 视频拉流不再跟随“当前选中设备”：改由“摄像头绑定设备�?m_camera_cur_id) 驱动�?
    // 统一�?refreshCameraStream() 中处理，并由设备更新事件�?event.id==m_camera_cur_id 触发
    // （切设备时画面保持旧设备，点摄像头按钮才切流，见 activateCameraForCurrentDevice）�?
    std::string device_name = data.devDetail->name;  //设备�?
    if (m_cur_dev_name != device_name && !device_name.empty()) {
        m_cur_dev_name  = device_name;
        wxString u8_dev_name = wxString::FromUTF8(device_name);
        wxGCDC dc(this);
        wxString clipName = FFUtils::trimString(dc, u8_dev_name, FromDIP(190));
        m_staticText_device_name->SetLabelText(clipName);
        m_staticText_device_name->SetToolTip(u8_dev_name);
    }

    std::string device_location = data.devDetail->location;//位置
    if (m_cur_dev_location != device_location && !device_location.empty()) {
        m_cur_dev_location = device_location;
        wxString u8_dev_location = wxString::FromUTF8(device_location);
        wxGCDC   dc(this);
        wxString clipName = FFUtils::trimString(dc, u8_dev_location, FromDIP(150));
        m_staticText_device_position->SetLabel(clipName);
        m_staticText_device_position->SetToolTip(u8_dev_location);
    } 

    if (!m_isCloudState) {
        std::string printFileName = data.devDetail->printFileName; // 文件�?
        setMaterialName(printFileName);
        setMaterialPic(data);                                       // 图片地址
        double total_weight = data.devDetail->estimatedRightWeight; // 材料重量
        char   weight[64];
        ::sprintf(weight, "  %.2f g", total_weight);
        m_material_weight_label->SetLabel(weight);
    }

    // Temperature telemetry is present in devDetail for both LAN and WAN.
    // WAN updates call fillValue(data, true), so this must be a common path.
    setTempurature(data);

    if (!wanDev) {
        if (!m_isCloudState) {
            double printProgress = data.devDetail->printProgress; // 打印进度
            m_progress_bar->SetProgress(printProgress * 100);
            m_progress_bar->Show();
        }

        std::string lightStatus = data.devDetail->lightStatus; // 灯状�?
        /*if (data.devProduct->lightCtrlState == 1 && lightStatus.compare(CLOSE) == 0) {
            m_lamp_control_button->SetIcon("device_lamp_control");
            m_lamp_control_button->Refresh();
            m_lamp_control_button->SetFlashForgeSelected(false);
            m_idle_tempMixDevice->modifyDeviceLampState(false);
        } else if (data.devProduct->lightCtrlState == 1 && lightStatus.compare(OPEN) == 0) {
            m_lamp_control_button->SetIcon("device_lamp_control_press");
            m_lamp_control_button->Refresh();
            m_lamp_control_button->SetFlashForgeSelected(true);
            m_idle_tempMixDevice->modifyDeviceLampState(true);
        }*/
        std::string internalFanStatus = data.devDetail->internalFanStatus; // 内循环状�?
        bool        internal_open     = internalFanStatus.compare(OPEN) ? false : true;
        std::string externalFanStatus = data.devDetail->externalFanStatus; // 外循环状�?
        bool        external_open     = externalFanStatus.compare(OPEN) ? false : true;
        m_busy_circula_filter->setAirFilterState(internal_open, external_open);
        m_idle_tempMixDevice->modifyDeviceFilterState(internal_open, external_open);
        m_idle_tempMixDevice->modifyDevicePositonState(data.devDetail->coordinate[0], data.devDetail->coordinate[1],
                                                       data.devDetail->coordinate[2]);
        if (data.devDetail->pid == 0x001F) {
            std::string clearStatus = data.devDetail->clearFanStatus;
            bool  clear_fan_open = clearStatus.compare(OPEN) ? false : true;
            m_clear_fan_pressed_down   = clear_fan_open;
            if (clear_fan_open) {
                m_filter_button->SetIcon("device_filter");
            } else {
                m_filter_button->SetIcon("device_filter_offline");
            }
            m_idle_tempMixDevice->modifyG3UClearFanState(clear_fan_open);
        }

        std::string rightFilamentType = data.devDetail->rightFilamentType; // 右喷头材料类�?
        m_busy_device_detial->setMaterialName(rightFilamentType);
        std::string leftFilamentType = data.devDetail->leftFilamentType; // 左喷头材料类�?
        std::string leftFilament     = "L/2: ";
        m_busy_G3U_detail->setMaterialName(leftFilament + leftFilamentType);
        std::string rightFilament = "R/1: ";
        m_busy_G3U_detail->setRightMaterialName(rightFilament + rightFilamentType);
        double currentPrintSpeed = data.devDetail->currentPrintSpeed; // 初始打印速度
        m_busy_device_detial->setInitialSpeed(currentPrintSpeed);
        m_busy_G3U_detail->setInitialSpeed(currentPrintSpeed);
        double printSpeedAdjust = data.devDetail->printSpeedAdjust; // 速度
        if (data.devDetail->printSpeedAdjust != m_last_speed) {
            m_last_speed = data.devDetail->printSpeedAdjust;
            m_busy_device_detial->setSpeed(printSpeedAdjust);
            m_busy_G3U_detail->setSpeed(printSpeedAdjust);
        }
        double zAxisCompensation = data.devDetail->zAxisCompensation; // z轴坐�?
        if (data.devDetail->zAxisCompensation != m_last_z_axis_compensation) {
            m_last_z_axis_compensation = data.devDetail->zAxisCompensation;
            m_busy_device_detial->setZAxis(zAxisCompensation);
            m_busy_G3U_detail->setZAxis(zAxisCompensation);
        }
        int printLayer       = data.devDetail->printLayer;       // 当前�?
        int targetPrintLayer = data.devDetail->targetPrintLayer; // 目标层数
        m_busy_device_detial->setLayer(printLayer, targetPrintLayer);
        m_busy_G3U_detail->setLayer(printLayer, targetPrintLayer);
        double fillAmount = data.devDetail->fillAmount; // 填充�?
        m_busy_device_detial->setFillRate(fillAmount);
        m_busy_G3U_detail->setFillRate(fillAmount);
        double coolingFanSpeed = data.devDetail->coolingFanSpeed; // 喷头风扇
        if (data.devDetail->coolingFanSpeed != m_last_cooling_fan_speed) {
            m_last_cooling_fan_speed = data.devDetail->coolingFanSpeed;
            m_busy_device_detial->setCoolingFanSpeed(coolingFanSpeed);
            m_busy_G3U_detail->setCoolingFanSpeed(coolingFanSpeed);
        }
        double leftCoolingFanSpeed = data.devDetail->coolingFanLeftSpeed; // 左喷头风�?
        if (data.devDetail->coolingFanLeftSpeed != m_last_left_cooling_fan_speed) {
            m_last_left_cooling_fan_speed = data.devDetail->coolingFanLeftSpeed;
            m_busy_G3U_detail->setLeftCoolingFanSpeed(leftCoolingFanSpeed);
        }
        double chamberFanSpeed = data.devDetail->chamberFanSpeed; // 冷却风扇（腔体风扇）
        if (data.devDetail->chamberFanSpeed != m_last_chamber_fan_speed) {
            m_last_chamber_fan_speed = data.devDetail->chamberFanSpeed;
            m_busy_device_detial->setChamberFanSpeed(chamberFanSpeed);
            m_busy_G3U_detail->setChamberFanSpeed(chamberFanSpeed);
        }

        map<int, bool> temp_pid_show_datas;
        for (auto it : FFUtils::printer_preset_map) {
            temp_pid_show_datas[it.first] = false;
        }
        temp_pid_show_datas[GUIDER_4_PRO] = true;
        temp_pid_show_datas[GUIDER_3_ULTRA] = true;
        if (m_pid != data.devDetail->pid) {
            for (auto& elem : temp_pid_show_datas) {
                if (data.devDetail->pid == elem.first) {
                    //m_tempCtrl_mid->SetReadOnly(!elem.second);
                    //m_tempCtrl_mid->Enable(elem.second);
                    m_pid = data.devDetail->pid;
                    auto bitmap_name = FFUtils::getBitmapFileName(m_pid);
                    if (bitmap_name) {
                        m_idle_device_staticbitmap->SetBitmap(create_scaled_bitmap(bitmap_name.ToStdString(), 0, 250));
                    }
                    break;
                }
            }
        }
#if 0
        if (m_pid != data.devDetail->pid) {
            m_pid = data.devDetail->pid;
            m_idle_device_staticbitmap->SetBitmap(create_scaled_bitmap(FFUtils::getBitmapFileName(data.devDetail->pid).ToStdString(), 0, 165));
        }
#endif
        std::string machineType = FFUtils::getPrinterName(data.devDetail->pid);
        std::string nozzleModel = data.devDetail->nozzleModel; // 喷嘴型号
        std::string measure     = data.devDetail->measure;     // 打印尺寸
        measure.append("mm");
        std::string firmwareVersion    = data.devDetail->firmwareVersion; // 固件版本
        std::string serialNubmer       = data.connectMode == 0 ? data.lanDevInfo.serialNumber : data.wanDevInfo.serialNumber; // 序列�?
        wxString cumulativePrintTime = format_cumulative_print_time(*data.devDetail, data.connectMode);
        wxString strCumulativeFilament = format_cumulative_filament(data.devDetail->cumulativeFilament, data.connectMode);
        std::string ipAddr = data.devDetail->ipAddr; // ip地址
        m_idle_tempMixDevice->modifyDeviceInfo(machineType, nozzleModel, measure, firmwareVersion, serialNubmer, cumulativePrintTime,
                                               strCumulativeFilament, ipAddr);
        m_idle_tempMixDevice->setDisabledMoveCtrl(data.devDetail->moveCtrl != 1);
        m_idle_tempMixDevice->setDisabledExtruderCtrl(data.devDetail->extrudeCtrl != 1);
        setDisabledExtruderCtrl(data.devDetail->extrudeCtrl != 1);
    }
}

void SingleDeviceState::fillCloudValue(const fnet_slice_state_t& data)
{
    // fnet_slice_state �?char* 字段可能�?null（云端某些状态不�?fileName/thumb 等）�?
    // 直接�?null 构�?std::string 会崩（strlen(null)）。这里统一归一化为安全�?std::string�?
    std::string cloud_id        = data.id ? data.id : "";
    std::string cloud_status    = data.status ? data.status : "";
    std::string cloud_file_name = data.fileName ? data.fileName : "";
    std::string cloud_thumb     = data.thumbImagePath ? data.thumbImagePath : "";

    m_isCloudState = true;
    m_staticText_count_time->Hide();
    m_staticText_time_label->Hide();
    m_staticText_device_info->Hide();
    m_print_button->Hide();
    m_cancel_button->Hide();
    m_staticText_time_label->Hide();
    m_staticText_count_time->Hide();
    m_panel_control_print->Hide();
    m_panel_control_cloud->Show();
    m_clear_button->Hide();
    m_tempCtrl_panel->SwitchTargetTemp(false);
    if (m_machine_idle_panel->IsShown()) {
        showMaterialStation(true);
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->Hide();
        m_panel_idle_text->Hide();
    }
    m_machine_ctrl_panel->Show();
    m_machine_ctrl_info_panel->Show();
    m_machine_idle_panel->Hide();
    m_machine_idle_info_panel->Hide();
    m_nozzles->SetCurState(false);
    wxString print_state = _L("busy");
    setTipMessage(print_state, "#F9B61C");
    m_slice_task_id = cloud_id;
    setMaterialName(cloud_file_name);
    m_file_pic_url                 = cloud_thumb;
    m_file_pic_name                = "";
    m_download_title_image_task_id = m_download_tool.downloadMem(m_file_pic_url, 30000, 60000);
    double total_weight = data.weight;
    char   weight[64];
    ::sprintf(weight, "  %.2f g", total_weight);
    m_material_weight_label->SetLabel(weight);
    if (cloud_status == "QUEUE") {
        m_staticText_cloud_text->SetForegroundColour(wxColour(50, 141, 251));
        m_staticText_cloud_text->SetLabel(_L("Cloud slicing queued..."));
        m_staticText_cloud_text->Show();
        m_staticText_cloud_queue_tip->Show();
        m_staticText_cloud_queue_count->SetLabel(wxString::Format("%d", data.waitingCount));
        m_staticText_cloud_queue_count->Show();
        m_staticText_cloud_queue_text->Show();
        m_progress_bar->SetProgress(0);
        m_progress_bar->Show();
        m_cancel_queue_button->Show();
        m_cancel_queue_button->SetLabel(_L("Cancel Queue"));
        m_cancel_slice_button->Hide();
        m_retry_print_button->Hide();
    } else if (cloud_status == "SLICING") {
        m_staticText_cloud_text->SetForegroundColour(wxColour(50, 141, 251));
        m_staticText_cloud_text->SetLabel(_L("Cloud task is slicing..."));
        m_staticText_cloud_text->Show();
        m_staticText_cloud_queue_tip->Hide();
        m_staticText_cloud_queue_count->Hide();
        m_staticText_cloud_queue_text->Hide();
        m_progress_bar->SetProgress(data.percentage);
        m_progress_bar->Show();
        m_cancel_queue_button->Show();
        m_cancel_queue_button->SetLabel(_L("Cancel Slicing"));
        m_cancel_slice_button->Hide();
        m_retry_print_button->Hide();
    } else if (cloud_status == "FAILED") {
        m_staticText_cloud_text->SetLabel(_L("Failed. Please try printing again."));
        m_staticText_cloud_text->SetForegroundColour(wxColour(251, 71, 71));
        m_staticText_cloud_text->Show();
        m_staticText_cloud_queue_tip->Hide();
        m_staticText_cloud_queue_count->Hide();
        m_staticText_cloud_queue_text->Hide();
        m_progress_bar->SetProgress(0);
        m_progress_bar->Show();
        m_cancel_queue_button->Hide();
        m_cancel_slice_button->Show();
        m_retry_print_button->Show();
    } else if (cloud_status == "CANCELED") {
        m_isCloudState = false;
    }
    Layout();
}

void SingleDeviceState::fillJobValue(const fnet_job_info_t& info)
{ 
    m_isCloudState = true;
    m_staticText_count_time->Hide();
    m_staticText_time_label->Hide();
    m_staticText_device_info->Hide();
    m_clear_button->Hide();
    m_tempCtrl_panel->SwitchTargetTemp(false);
    m_print_button->Hide();
    m_cancel_button->Hide();
    m_staticText_time_label->Hide();
    m_staticText_count_time->Hide();
    m_panel_control_print->Hide();
    m_panel_control_cloud->Hide();
    if (m_machine_idle_panel->IsShown()) {
        showMaterialStation(true);
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_panel_print_btn->Hide();
        m_timeLapseVideoPnl->Hide();
        m_panel_idle_text->Hide();
    }
    m_machine_ctrl_panel->Show();
    m_machine_ctrl_info_panel->Show();
    m_machine_idle_panel->Hide();
    m_machine_idle_info_panel->Hide();
    m_nozzles->SetCurState(false);
    wxString print_state = _L("busy");
    setTipMessage(print_state, "#F9B61C");
    m_staticText_cloud_text->SetForegroundColour(wxColour(50, 141, 251));
    m_panel_control_cloud->Hide();
    m_staticText_cloud_queue_tip->Hide();
    m_staticText_cloud_queue_count->Hide();
    m_staticText_cloud_queue_text->Hide();

    if (std::string(info.status) == "downloading") {
        m_staticText_cloud_text->SetLabel(_L("Downloading cloud task..."));
        m_staticText_cloud_text->Show();
        m_progress_bar->SetProgress(info.percentage);
        m_progress_bar->Show();
    } else {
        m_isCloudState = false;
    }
    if (std::string(info.status) == "unzipping") {
        m_staticText_cloud_text->SetLabel(_L("Extracting files, please wait..."));
        m_staticText_cloud_text->Show();
        m_progress_bar->Hide();
    } else {
        m_isCloudState = false;
    }
    
    Layout();
}

void SingleDeviceState::setPageOffline()
{
    m_device_page_state = DevicePageState::Offline;
    if (m_camera_panel) m_camera_panel->logPanelState("setPageOffline enter");
   camdbg_log("setPageOffline called -> device offline (MQTT), tearing down page only; camera left intact (decoupled, driven by stream health)");
   // 离线
    m_cur_id = -1;
    // 批量 Show/Hide/Layout 期间冻结重绘，避免多次中间重绘造成卡顿（解绑正在播放的
    // 当前设备时，本函数会与视频拆除在同一 UI 事件里执行，冻结可显著减轻卡顿感）�?
    Freeze();
    if (m_isNozzlesPrinter) {
        m_nozzles->SetCurId(m_cur_id);
        m_nozzles->SetOffline();
    } else {
        m_material_station->show_material_panel(false);
        m_material_station->setCurId(m_cur_id);
    }
    if (m_panel_idle_text) {
        m_panel_idle_text->Hide();
        m_panel_print_btn->Hide();
        m_scrolledWindow->Hide();
        m_FileList_split_line->Hide();
        m_timeLapseVideoPnl->Hide();
        m_panel_separotor8->Show();
        m_busyState_top_gap->Show();
        m_busyState_bottom_gap->Show();
        m_offline_info_page_gap->Show();
    }
    unbind_ts("B1a. setPageOffline: nozzles/material + Show/Hide 完成");
    m_tempCtrl_panel->ReInitTempature(-1);
    unbind_ts("B1b. setPageOffline: ReInitTempature 完成");
    m_idle_tempMixDevice->Show();
    m_machine_idle_panel->Show();
    m_machine_idle_info_panel->Show();
    m_machine_ctrl_info_panel->Hide();
    m_machine_ctrl_panel->Hide();
    // 设备离线与摄像头离线解耦：不再因整机离线（�?20s 心跳超时的误�?短暂 flap�?
    // 就强制把摄像头断开。摄像头的在�?断开由它自身的流健康度决定—�?
    // 拉流正常就继续显示，拉流真正失败时由 FFRTMPVideoCtrl 的重�?占位逻辑自行处理�?
    // m_camera_panel->setOffline();   // 解耦：移除对摄像头的强制断开
    unbind_ts("B1c. setPageOffline: reInit BEGIN");
    reInit();
    if (m_camera_panel) m_camera_panel->logPanelState("setPageOffline after reInit before Thaw");
    unbind_ts("B1d. setPageOffline: reInit END; Thaw BEGIN (Thaw 会触发真正重�?");
    Thaw();  // 与上面的 Freeze() 配对，统一刷新一�?
    if (m_camera_panel) m_camera_panel->logPanelState("setPageOffline after Thaw");
    unbind_ts("B1e. setPageOffline: Thaw END");
}

void SingleDeviceState::refreshCameraStream()
{
    // 按“摄像头绑定设备�?m_camera_cur_id) 驱动视频流，与页面选中设备(m_cur_id)解耦�?
    // 关键：设备状态走 MQTT，视频流走的是另一台流媒体服务器，两者相互独立�?
    // 因此设备(MQTT)离线并不代表视频流断——只要流正常就保持画面不变，绝不因设备状�?
    // 主动 setOffline。真正的流中断由 FFRTMPVideoCtrl 自身的重�?占位逻辑处理�?
    if (!m_camera_panel || m_camera_cur_id < 0) {
        camera_capability_log(
            "refresh skipped: panel=" + std::string(m_camera_panel ? "present" : "null")
            + " cameraComId=" + std::to_string(m_camera_cur_id)
            + " currentComId=" + std::to_string(m_cur_id));
        return;
    }
    bool valid = false;
    const com_dev_data_t &data = MultiComMgr::inst()->devData(m_camera_cur_id, &valid);
    if (!valid) {
        camera_capability_log(
            "refresh skipped: device data invalid, cameraComId="
            + std::to_string(m_camera_cur_id)
            + " currentComId=" + std::to_string(m_cur_id));
        return;  // 设备数据暂不可用：保持当前视频流不变
    }
    if (data.devDetail == nullptr) {
        camera_capability_log(
            "refresh skipped: devDetail=null, cameraComId="
            + std::to_string(m_camera_cur_id)
            + " currentComId=" + std::to_string(m_cur_id)
            + " connectMode=" + std::to_string(static_cast<int>(data.connectMode))
            + " detailUpdated=" + std::string(data.devDetailUpdated ? "true" : "false"));
        return;
    }

    const char *camera_stream_url = data.devDetail->cameraStreamUrl;
    const bool stream_url_present = camera_stream_url != nullptr && camera_stream_url[0] != '\0';
    const bool uses_stream_url_capability =
        data.connectMode == COM_CONNECT_LAN
        && (data.devDetail->pid == ADVENTURER_5M || data.devDetail->pid == ADVENTURER_5M_PRO);
    // Adventurer 5M series uses the presence of a stream URL as the availability
    // signal in LAN mode. All other cases use the explicit camera field.
    bool camera_available = uses_stream_url_capability ? stream_url_present : data.devDetail->camera == 1;

    if (data.devDetail->pid == GUIDER_3_ULTRA) {
        camera_available = true;
    }

    camera_capability_log(
        "refresh: cameraComId=" + std::to_string(m_camera_cur_id)
        + " currentComId=" + std::to_string(m_cur_id)
        + " connectMode=" + std::to_string(static_cast<int>(data.connectMode))
        + " detailUpdated=" + std::string(data.devDetailUpdated ? "true" : "false")
        + " usesStreamUrlCapability=" + std::string(uses_stream_url_capability ? "true" : "false")
        + " cameraRaw=" + std::to_string(data.devDetail->camera)
        + " cameraAvailable=" + std::string(camera_available ? "true" : "false")
        + " streamUrlPresent=" + std::string(stream_url_present ? "true" : "false")
        + " streamUrl=" + (camera_stream_url != nullptr ? camera_stream_url : "<null>"));

    m_camera_panel->setCameraAvailable(camera_available);
    if (!camera_available) {
        m_camera_stream_url.clear();
        return;
    }
    if (!stream_url_present) {
        return;
    }

    const std::string source_url = camera_stream_url;
    // The LAN detail response already contains the address expected by the
    // printer's local camera service. Keep it unchanged; protocol conversion
    // is only applicable to remote/WAN stream addresses.

    std::string stream_url = source_url;

    if(data.connectMode == COM_CONNECT_WAN) {
        stream_url = switch_camera_stream_url(source_url, g_camera_stream_protocol);
    } else if(data.connectMode == COM_CONNECT_LAN) {

    }

    if (!stream_url.empty() && m_camera_stream_url != stream_url) {
        m_camera_stream_url = stream_url;
        m_camera_panel->setStreamUrl(m_camera_stream_url);
    }
}

void SingleDeviceState::switchCameraStreamTo(int comId)
{
    // 把摄像头视频流切换到指定设备（切设备/点摄像头按钮均走此处）�?
    if (!m_camera_panel || comId < 0 || comId == m_camera_cur_id) {
        return;
    }
    m_camera_cur_id = comId;
    m_camera_stream_url.clear();          // 强制 refreshCameraStream 按新设备重新 setStreamUrl
    m_camera_panel->setCurComId(comId);
    // 切设备后与“首次进入设备页”保持一致：新设备起流默认进入暂�?缺省�?播放按钮)�?
    // 不沿用上一台设备的播放/暂停状态�?
    m_camera_panel->setStartPausedPreference(true);
    refreshCameraStream();                // 立即按新设备拉流（地址未到时由后续遥测事件补触发）
}

void SingleDeviceState::activateCameraForCurrentDevice()
{
    if (!m_camera_panel) {
        return;
    }
    // 点摄像头按钮：确保绑定到当前选中设备（通常切设备时已切好，这里兜底），再弹窗显示�?
    switchCameraStreamTo(m_cur_id);
    m_camera_panel->showPopup();
}

std::string SingleDeviceState::getCurLanguage() 
{
    AppConfig *app_config = wxGetApp().app_config; 
    return  app_config->get("language");
}

void SingleDeviceState::setMaterialName(const std::string& printFileName)
{
    if (m_cur_print_file_name != printFileName) {
        m_cur_print_file_name = printFileName;
        wxString wxPrintFileName = wxString::FromUTF8(printFileName);
        wxString elideString = FFUtils::elideString(m_staticText_file_name, wxPrintFileName, FromDIP(PRINT_FILE_NAME_WIDTH));
        m_staticText_file_name->SetLabel(elideString);
        m_staticText_file_name->SetToolTip(wxPrintFileName);
        m_staticText_file_name->Show();
        m_staticText_file_name->Layout();
        if (wxWindow* file_name_panel = m_staticText_file_name->GetParent()) {
            file_name_panel->Layout();
            if (wxWindow* file_info_panel = file_name_panel->GetParent()) {
                file_info_panel->Layout();
            }
        }
        Layout();
    }
}

void SingleDeviceState::setMaterialPic(const com_dev_data_t &data)
{
    std::string file_pic_path = data.devDetail->printFileThumbUrl; // 图片地址
    std::string file_pic_name = data.devDetail->printFileName;     // 图片名称

    if (data.connectMode == COM_CONNECT_LAN) {
       if (m_file_pic_name == file_pic_name || file_pic_name.empty() || file_pic_path.empty())
            return;
    }
 else if (data.connectMode == COM_CONNECT_WAN) {
     if (m_file_pic_url == file_pic_path || file_pic_path.empty())
         return;
    }
 else {
     return;
    }

    m_file_pic_url = file_pic_path;
    m_file_pic_name = file_pic_name;
    m_download_title_image_task_id = m_download_tool.downloadMem(m_file_pic_url, 30000, 60000);
}

void SingleDeviceState::setTempurature(const com_dev_data_t& data)
{
    m_tempCtrl_panel->UpdateTempatrue(data);
    //if (m_pid == GUIDER_3_ULTRA) {
    //   // 右喷头温�?
    //   double rightTemp = data.devDetail->rightTemp;
    //   m_tempCtrl_top->SetCurrTemp(rightTemp, true);
    //   // 右喷头目标温�?
    //   double rightTargetTemp = data.devDetail->rightTargetTemp;
    //   if (m_right_target_temp != rightTargetTemp) {
    //        m_right_target_temp = rightTargetTemp;
    //        m_tempCtrl_top->SetTagTemp(rightTargetTemp, true);
    //   }

    //   // 左喷头温�?
    //   double leftTemp = data.devDetail->leftTemp;
    //   m_tempCtrl_bottom->SetCurrTemp(leftTemp, true);
    //   // 左喷头目标温�?
    //   double leftTargetTemp = data.devDetail->leftTargetTemp; 
    //   if (m_plat_target_temp != leftTargetTemp) {
    //        m_plat_target_temp = leftTargetTemp;
    //        m_tempCtrl_bottom->SetTagTemp(leftTargetTemp, true);
    //   }

    //   // 底板温度
    //   double platTemp = data.devDetail->platTemp; 

    //   m_tempCtrl_mid->SetCurrTemp(platTemp, true);
    //   // 底板目标温度
    //   double platTempTargetTemp = data.devDetail->platTargetTemp; 
    //   if (m_chamber_target_temp != platTempTargetTemp) {
    //        m_chamber_target_temp = platTempTargetTemp;
    //        m_tempCtrl_mid->SetTagTemp(platTempTargetTemp, true);
    //   }

    //   auto     aRightTemp          = static_cast<int>(rightTemp);
    //   auto     aPlatTemp           = static_cast<int>(leftTemp);
    //   auto     aChamberTemp        = static_cast<int>(platTemp);
    //   wxString modify_nozzle_temp  = wxString::Format("%d", aRightTemp);
    //   wxString modify_plat_temp    = wxString::Format("%d", aPlatTemp);
    //   wxString modify_chamber_temp = wxString::Format("%d", aChamberTemp);

    //   m_idle_tempMixDevice->modifyTemp(modify_nozzle_temp, modify_plat_temp, modify_chamber_temp, rightTargetTemp, leftTargetTemp,
    //                                    platTempTargetTemp);

    //} else {
    //   double rightTemp = data.devDetail->rightTemp; // 右喷头温�?
    //   m_tempCtrl_top->SetCurrTemp(rightTemp, true);
    //   double rightTargetTemp = data.devDetail->rightTargetTemp; // 右喷头目标温�?
    //   if (m_right_target_temp != rightTargetTemp) {
    //        m_right_target_temp = rightTargetTemp;
    //        m_tempCtrl_top->SetTagTemp(rightTargetTemp, true);
    //   }

    //   double platTemp = data.devDetail->platTemp; // 平台温度
    //   m_tempCtrl_bottom->SetCurrTemp(platTemp, true);
    //   double platTargetTemp = data.devDetail->platTargetTemp; // 平台目标温度
    //   if (m_plat_target_temp != platTargetTemp) {
    //        m_plat_target_temp = platTargetTemp;
    //        m_tempCtrl_bottom->SetTagTemp(platTargetTemp, true);
    //   }

    //   double chamberTemp = data.devDetail->chamberTemp; // 腔体温度

    //   m_tempCtrl_mid->SetCurrTemp(chamberTemp, true);
    //   double chamberTargetTemp = data.devDetail->chamberTargetTemp; // 腔体目标温度
    //   if (m_chamber_target_temp != chamberTargetTemp) {
    //        m_tempCtrl_mid->SetTagTemp(chamberTargetTemp, true);
    //   }
    //   auto     aRightTemp          = static_cast<int>(rightTemp);
    //   auto     aPlatTemp           = static_cast<int>(platTemp);
    //   auto     aChamberTemp        = static_cast<int>(chamberTemp);
    //   wxString modify_nozzle_temp  = wxString::Format("%d", aRightTemp);
    //   wxString modify_plat_temp    = wxString::Format("%d", aPlatTemp);
    //   wxString modify_chamber_temp = wxString::Format("%d", aChamberTemp);

    //   m_idle_tempMixDevice->modifyTemp(modify_nozzle_temp, modify_plat_temp, modify_chamber_temp, rightTargetTemp, platTargetTemp,
    //                                    chamberTargetTemp);
    //}
}

void SingleDeviceState::setIdlePrinterText(bool isOffline)
{
    wxString str;
    if (isOffline) {
        str = "Device offline";
    }
    else {
        if (FFUtils::isNozzlesPrinter(FFUtils::getPid(m_cur_id))) {
            str = HAS_NO_PRINTING_U1;
        } else {
            str = HAS_NO_PRINTING;
        }
    }
    m_staticText_idle->SetLabel(_L(str));
    wxClientDC idle_text_dc(m_staticText_idle);
    idle_text_dc.SetFont(m_staticText_idle->GetFont());
    wxSize idle_text_size = idle_text_dc.GetMultiLineTextExtent(_L(str));
    m_staticText_idle->SetMinSize(wxSize(idle_text_size.x + FromDIP(4), idle_text_size.y));
    if (m_staticText_idle->GetParent()) {
        m_staticText_idle->GetParent()->Layout();
    }
}

void SingleDeviceState::splitIdleTextLabel()
{
    wxGCDC   dc(this);
    wxString multiText;
    Label::split_lines(dc, FromDIP(IDLE_NAME_LENGTH), _L(HAS_NO_PRINTING), multiText);
    m_staticText_idle->SetLabel(multiText);
}

void SingleDeviceState::clearFileList()
{
    for (auto item : m_fileItemList) {
        item->Destroy();
    }
    m_fileItemList.clear();
    m_download_file_list_image_map.clear();
    m_sizer_my_devices->Layout();
}

void SingleDeviceState::initFileList(const std::vector<FileItem::FileData>& fileDataList)
{ 
    for (const auto& fileData : fileDataList) {
       auto mitem = new FileItem(m_scrolledWindow, fileData);
       bool  valid = false;
       const com_dev_data_t& data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
       if (data.connectMode == 0) {
            std::string fileName = mitem->m_data.gcodeData.fileName;
            ComGetGcodeThumb* devGcodeThumb = new ComGetGcodeThumb(fileName);
            mitem->m_data.commandId = devGcodeThumb->commandId();
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, devGcodeThumb);
       } else if (data.connectMode == 1) {
           int taskId = m_download_tool.downloadMem(mitem->m_data.gcodeData.thumbUrl, 30000, 60000);
           m_download_file_list_image_map.emplace(taskId, mitem);
       }
       mitem->Bind(EVT_FILE_ITEM_CLICKED, [mitem, this](wxCommandEvent& event) {
           m_printBtn->Enable(true);
           if (m_curSelectedFileItem == nullptr) {
               m_curSelectedFileItem = mitem;
           } else {
               if (mitem == m_curSelectedFileItem) {
                   return;
               } else {
                   m_curSelectedFileItem->SetPressed(false);
                   m_curSelectedFileItem = mitem;
               }
           }
       });
       m_sizer_my_devices->Add(mitem, 0, wxEXPAND, 0);
       mitem->SetToolTip(fileData.wxName);
       m_fileItemList.emplace_back(mitem);
    }
    int visual_height = fileDataList.size() * FromDIP(45);
    m_scrolledWindow->SetVirtualSize(FromDIP(46), visual_height);
    m_sizer_my_devices->Layout();
}

LampToolBar::LampToolBar(wxWindow* parent) : 
    wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize) 
{
    m_lamp_btn = new Button(this, "", "device_lamp_control", 0, 24);
    m_lamp_btn->SetBorderWidth(0);
    m_lamp_btn->SetBackgroundColor(wxColour("#F8F8F8"));
    m_lamp_btn->SetBorderColor(wxColour(255, 255, 255));
    m_lamp_btn->SetMinSize((wxSize(FromDIP(51), FromDIP(51))));
    m_lamp_btn->SetCornerRadius(0);
    m_lamp_btn->Bind(wxEVT_LEFT_DOWN, &LampToolBar::lamp_btn_clicked, this);

    m_camera_btn = new Button(this, "", "camera_button_offline", 0, 24);
    m_camera_btn->SetBorderWidth(0);
    m_camera_btn->SetBackgroundColor(wxColour("#F8F8F8"));
    m_camera_btn->SetBorderColor(wxColour(255, 255, 255));
    m_camera_btn->SetMinSize((wxSize(FromDIP(51), FromDIP(51))));
    m_camera_btn->SetCornerRadius(0);
    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_lamp_btn, 0, wxALL, 0);
    sizer->AddSpacer(FromDIP(10));
    sizer->Add(m_camera_btn, 0, wxALL, 0);
    m_camera_btn->Hide();
    SetSizer(sizer);
    Layout();
}

void LampToolBar::lamp_btn_clicked(wxMouseEvent& event)
{
     event.Skip();
     if (m_lamp_btn->GetFlashForgeSelected()) {
         // 关灯
         Slic3r::GUI::ComLightCtrl *lightctrl = new Slic3r::GUI::ComLightCtrl(CLOSE);
         // 测试，临时将id写死
         if (m_cur_id >= 0) {
             bool ok = Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
             camdbg_log("light CLOSE putCommand=" + std::string(ok ? "ok" : "rejected")
                        + " id=" + std::to_string(m_cur_id));
         }
         m_lamp_btn->SetIcon("device_lamp_control");
         m_lamp_btn->Refresh();
         m_lamp_btn->SetFlashForgeSelected(false);
     } else {
         // 开�?
         Slic3r::GUI::ComLightCtrl *lightctrl = new Slic3r::GUI::ComLightCtrl(OPEN);
         // 测试，临时将id写死
         if (m_cur_id >= 0) {
             bool ok = Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
             camdbg_log("light OPEN putCommand=" + std::string(ok ? "ok" : "rejected")
                        + " id=" + std::to_string(m_cur_id));
         }
         m_lamp_btn->SetIcon("device_lamp_control_press");
         m_lamp_btn->Refresh();
         m_lamp_btn->SetFlashForgeSelected(true);
     }
}

void LampToolBar::SetCurId(com_id_t curId) 
{ 
    m_cur_id = curId; 
}

void LampToolBar::BindCamera(FFRTMPVideoCtrl* camera, std::function<void()> onActivate)
{
    if (camera == nullptr) {
        return;
    }
}

void LampToolBar::SetLampState(bool isOffline, bool isOpen) 
{
    if (isOffline) {
        m_lamp_btn->SetIcon("device_lamp_offline");
        m_lamp_btn->Enable(false);
        m_lamp_btn->Refresh();
        return;
    }
    m_lamp_btn->Enable(true);
    if (isOpen) {
        m_lamp_btn->SetIcon("device_lamp_control_press");
        m_lamp_btn->Refresh();
        m_lamp_btn->SetFlashForgeSelected(true);
    } else {
        m_lamp_btn->SetIcon("device_lamp_control");
        m_lamp_btn->Refresh();
        m_lamp_btn->SetFlashForgeSelected(false);
    }
}

void LampToolBar::SetCameraVisible(bool visible)
{
    /*m_camera_btn->Show(visible);
    Layout();*/
}

void LampToolBar::SetCameraState(bool isOffline) 
{ 
    /*m_camera_btn->Enable(!isOffline);
    if (isOffline) {
        m_camera_btn->SetIcon("camera_button_offline");
    } else {
        m_camera_btn->SetIcon("camera_button");
    }
    m_camera_btn->Refresh();*/
}

}}
