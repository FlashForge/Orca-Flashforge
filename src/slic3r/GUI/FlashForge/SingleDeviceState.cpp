#include "SingleDeviceState.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/BitmapCache.hpp"
#include "slic3r/GUI/GUI.hpp"
#include <slic3r/GUI/Widgets/WebView.hpp>
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"
#include "slic3r/GUI/FlashForge/MultiComUtils.hpp"
#include <nlohmann/json.hpp>
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/FFUtils.hpp"
#include <wx/dcgraph.h>
using namespace std::literals;
using json   = nlohmann::json;
namespace pt = boost::property_tree;

namespace Slic3r {
namespace GUI {
wxDEFINE_EVENT(EVT_SWITCH_TO_FILETER, wxCommandEvent);

const std::string CLOSE = "close";
const std::string OPEN  = "open";
const std::string CANCEL ="cancel";
const std::string PAUSE  = "pause";
const std::string CONTINUE = "continue";

const std::string OPENSTREAM = "streamCtrl_cmd";

const std::string P_READY = "ready"; 
const std::string P_COMPLETED = "completed";
const std::string P_ERROR     = "error";
const std::string P_PAUSING  = "pausing";
const std::string P_BUSY      = "busy";
const std::string P_HEATING   = "heating";
const std::string P_CALIBRATE  = "calibrate_doing";

const wxString    TEMPERATURE = _L("Temperature");
const wxString    TEMP_CANCEL  = _L("cancel");
const wxString    TEMP_CONFIRM = _L("confirm");

const int TEXT_LENGTH = 20;
const int MATERIAL_PIC_WIDTH  = 80;
const int MATERIAL_PIC_HEIGHT = 80;

MaterialImagePanel::MaterialImagePanel(wxWindow *parent, const wxSize &size /*=wxDefaultSize*/)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
{
    Bind(wxEVT_PAINT, &MaterialImagePanel::OnPaint, this);
    // Bind(wxEVT_SIZE, &RoundImagePanel::OnSize, this);
}

void MaterialImagePanel::SetImage(const wxImage &image)
{
    m_image = image;
    Refresh();
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

    if (!img.HasAlpha()) {
        img.InitAlpha();
    }

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

MaterialPanel::MaterialPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
    {
        SetBackgroundColour(*wxWHITE);
        create_panel(this);
    }

MaterialPanel::~MaterialPanel()
{

}

void MaterialPanel::create_panel(wxWindow* parent)
{
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *bSizer_printing_title = new wxBoxSizer(wxHORIZONTAL);

        m_panel_printing_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
        m_panel_printing_title->SetBackgroundColour(wxColour(248,248,248));

        //材料站标题
        m_staticText_printing = new wxStaticText(m_panel_printing_title, wxID_ANY ,_L("Material Station"));
        m_staticText_printing->Wrap(-1);
        //m_staticText_printing->SetFont(wxFont(wxFontInfo(16)));
        //m_staticText_printing->SetForegroundColour(wxColour(51,51,51));
        m_staticText_printing->SetForegroundColour(wxColour(248, 248, 248));

        bSizer_printing_title->Add(m_staticText_printing, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
        bSizer_printing_title->Add(0, 0, 1, wxEXPAND, 0);
        m_panel_printing_title->SetSizer(bSizer_printing_title);
        m_panel_printing_title->Layout();
        bSizer_printing_title->Fit(m_panel_printing_title);

        //材料站内容
        wxBoxSizer *bSizer_task_name_hor = new wxBoxSizer(wxHORIZONTAL);
        wxPanel*    task_name_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(208)), wxTAB_TRAVERSAL);
        m_staticText_subtask_value = new wxStaticText(task_name_panel, wxID_ANY, _L("Unconnected"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER | wxST_ELLIPSIZE_END);
        m_staticText_subtask_value->Wrap(-1);
        //m_staticText_subtask_value->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_subtask_value->SetForegroundColour(wxColour(255, 255, 255));

        bSizer_task_name_hor->Add(m_staticText_subtask_value, 0, wxALIGN_CENTER, 0);

        task_name_panel->SetSizer(bSizer_task_name_hor);
        task_name_panel->Layout();
        bSizer_task_name_hor->Fit(task_name_panel);

        sizer->Add(m_panel_printing_title, 0, wxEXPAND | wxALL, 0);
        //sizer->AddStretchSpacer();
        sizer->Add(task_name_panel, 0, wxALIGN_CENTER, 0);
        //sizer->AddStretchSpacer();

        parent->SetSizer(sizer);
        parent->Layout();
        parent->Fit();
}

StartFilter::StartFilter(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
        SetBackgroundColour(*wxWHITE);
        create_panel(this);
}

StartFilter::~StartFilter()
{
        ;
}

void StartFilter::create_panel(wxWindow* parent)
{
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *bSizer_filtering_title = new wxBoxSizer(wxHORIZONTAL);

        auto m_panel_filtering_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_filtering_title->SetBackgroundColour(wxColour(248,248,248));
        m_panel_filtering_title->SetMinSize(wxSize(FromDIP(450), FromDIP(36)));

        //过滤标题
        auto m_staticText_filtering = new wxStaticText(m_panel_filtering_title, wxID_ANY ,_L("Start Filtering"));
        m_staticText_filtering->Wrap(-1);
        //m_staticText_filtering->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_filtering->SetForegroundColour(wxColour(51,51,51));

        bSizer_filtering_title->Add(m_staticText_filtering, 0, wxLEFT | wxCENTER, FromDIP(17));
        m_panel_filtering_title->SetSizer(bSizer_filtering_title);
        m_panel_filtering_title->Layout();
        bSizer_filtering_title->Fit(m_panel_filtering_title);

        //内循环过滤
        wxBoxSizer *bSizer_internal_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
        wxPanel*    internal_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
        auto m_staticText_internal_circulate = new wxStaticText(internal_circulate_panel, wxID_ANY, _L("Internal Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        m_staticText_internal_circulate->Wrap(-1);
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

        //外循环过滤
        wxBoxSizer *bSizer_external_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
        wxPanel*    external_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
        auto m_staticText_external_circulate = new wxStaticText(external_circulate_panel, wxID_ANY, _L("External Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        m_staticText_external_circulate->Wrap(-1);
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

        sizer->Add(m_panel_filtering_title, 0, wxALL, 0);
        sizer->AddSpacer(FromDIP(12));
        sizer->Add(internal_circulate_panel, 0, wxLEFT, 0);
        sizer->AddSpacer(FromDIP(12));
        sizer->Add(external_circulate_panel, 0, wxLEFT, 0);
#ifdef __WIN32__
        sizer->AddSpacer(FromDIP(142));
#else if __APPLE__
        sizer->AddSpacer(FromDIP(151));
#endif
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
    wxBoxSizer *sizer                  = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *bSizer_temp_title = new wxBoxSizer(wxHORIZONTAL);

    auto m_panel_temp_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    m_panel_temp_title->SetBackgroundColour(wxColour(248, 248, 248));

    // 温度标题
    m_staticText_title = new wxStaticText(m_panel_temp_title, wxID_ANY, TEMPERATURE);
    m_staticText_title->Wrap(-1);
    //m_staticText_title->SetFont(wxFont(wxFontInfo(16)));
    m_staticText_title->SetForegroundColour(wxColour(51, 51, 51));

    bSizer_temp_title->Add(m_staticText_title, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_temp_title->Add(0, 0, 1, wxEXPAND, 0);
    m_panel_temp_title->SetSizer(bSizer_temp_title);
    m_panel_temp_title->Layout();
    bSizer_temp_title->Fit(m_panel_temp_title);

    // 确认、取消按钮
    //垂直布局、水平布局
    wxBoxSizer *bSizer_operate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel    *operate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(109)), wxTAB_TRAVERSAL);
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
}
// DeviceDetail::~DeviceDetail()
// {
//         ;
// }

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
        auto m_panel_confirm_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        auto confirm_push_btn = new FFPushButton(m_panel_confirm_row, wxID_ANY, "push_button_confirm_normal", "push_button_confirm_hover","push_button_confirm_press", "push_button_confirm_normal");
        confirm_push_btn->SetBackgroundColour(wxColour(255, 255, 255));
        confirm_push_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) {
            event.Skip();
            double speed;
            double z_axis;
            double nozzle_fan;
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
            if (m_device_nozzle_fan) {
                m_device_nozzle_fan->getTextValue().ToDouble(&nozzle_fan);
            }
            if (m_device_cooling_fan) {
                m_device_cooling_fan->getTextValue().ToDouble(&cooling_fan);
            }
            ComPrintCtrl *printCtrl = new ComPrintCtrl(z_axis, speed, nozzle_fan,cooling_fan);
            if (m_cur_id >= 0) {
                Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, printCtrl);
            }
        });

        bSizer_confirm_row->AddSpacer(FromDIP(410));
        bSizer_confirm_row->Add(confirm_push_btn, 0, wxTOP, FromDIP(17));
        //bSizer_confirm_row->AddSpacer(FromDIP(25));

        m_panel_confirm_row->SetSizer(bSizer_confirm_row);
        m_panel_confirm_row->Layout();
        bSizer_confirm_row->Fit(m_panel_confirm_row);

        sizer->Add(m_panel_confirm_row, 0, wxALL, 0);

//
        auto m_panel_separotor10 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor10->SetBackgroundColour(wxColour(255, 255, 255));
        m_panel_separotor10->SetMinSize(wxSize(-1,FromDIP(18)));

        sizer->Add(m_panel_separotor10);
//
        wxBoxSizer *bSizer_h = new wxBoxSizer(wxHORIZONTAL);
        auto m_panel_separotor0 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor0->SetBackgroundColour(wxColour(255, 255, 255));
        m_panel_separotor0->SetMinSize(wxSize(FromDIP(28),-1));

        bSizer_h->Add(m_panel_separotor0);

        wxBoxSizer *bSizer_first_row = new wxBoxSizer(wxVERTICAL);
        auto m_panel_first_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);
        m_device_material = new IconText(m_panel_first_row, wxString("device_material"), 20, wxString("PLA-12345678901234567890"), 12);
        bSizer_first_row->Add(m_device_material, 0, wxALL, 0);
        bSizer_first_row->AddSpacer(FromDIP(30));

        m_device_initial_speed = new IconText(m_panel_first_row, wxString("device_initial_speed"), 20, wxString("1000000mm/s"), 12);
        bSizer_first_row->Add(m_device_initial_speed, 0,wxALL, 0);
        bSizer_first_row->AddSpacer(FromDIP(30));

        m_device_speed = new IconBottonText(m_panel_first_row, wxString("device_speed"), 20, wxString("90"), 12);
        m_device_speed->setLimit(10, 150);
        m_device_speed->setAdjustValue(10);
        bSizer_first_row->Add(m_device_speed, 0,wxALL, 0);
        bSizer_first_row->AddSpacer(FromDIP(30));

        m_device_z_axis = new IconBottonText(m_panel_first_row, wxString("device_z_axis"), 20, wxString("0.002"), 12,
                                             wxString("device_z_dec"), wxString("push_button_arrow_dec_normal"));
        m_device_z_axis->setLimit(-5, 5);
        m_device_z_axis->setAdjustValue(0.025);
        m_device_z_axis->setPoint(3);
        bSizer_first_row->Add(m_device_z_axis, 0, wxALL, 0);
        bSizer_first_row->AddStretchSpacer();

        m_panel_first_row->SetSizer(bSizer_first_row);
        m_panel_first_row->Layout();
        bSizer_first_row->Fit(m_panel_first_row);

        bSizer_h->Add(m_panel_first_row, 0, wxALL, 0);
        //bSizer_h->AddStretchSpacer();
        //bSizer_h->AddSpacer(FromDIP(20));

//
        wxBoxSizer *bSizer_second_row  = new wxBoxSizer(wxVERTICAL);
        auto m_panel_second_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), wxTAB_TRAVERSAL);

        m_device_layer = new IconText(m_panel_second_row, wxString("device_layer"), 20, wxString("1000000/15000000"), 12);
        bSizer_second_row->Add(m_device_layer, 0, wxALL, 0);
        bSizer_second_row->AddSpacer(FromDIP(30));

        m_device_fill_rate = new IconText(m_panel_second_row, wxString("device_fill_rate"), 20, wxString("200%"), 12);
        bSizer_second_row->Add(m_device_fill_rate, 0, wxALL, 0);
        bSizer_second_row->AddSpacer(FromDIP(30));

        m_device_nozzle_fan = new IconBottonText(m_panel_second_row, wxString("device_nozzle_fan"), 20, wxString("50"), 12);
        m_device_nozzle_fan->setLimit(0, 100);
        m_device_nozzle_fan->setAdjustValue(10);
        bSizer_second_row->Add(m_device_nozzle_fan, 0, wxALL, 0);
        bSizer_second_row->AddSpacer(FromDIP(30));

        m_device_cooling_fan = new IconBottonText(m_panel_second_row, wxString("device_cooling_fan"), 20, wxString("100"), 12);
        m_device_cooling_fan->setLimit(0, 100);
        m_device_cooling_fan->setAdjustValue(10);
        bSizer_second_row->Add(m_device_cooling_fan, 0, wxALL, 0);
        bSizer_second_row->AddStretchSpacer();

        m_panel_second_row->SetSizer(bSizer_second_row);
        m_panel_second_row->Layout();
        bSizer_second_row->Fit(m_panel_second_row);

        bSizer_h->Add(m_panel_second_row, 0, wxALL, 0);

        //auto m_panel_separotor1 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        //m_panel_separotor1->SetBackgroundColour(wxColour(255, 255, 255));
        //m_panel_separotor1->SetMinSize(wxSize(FromDIP(25), -1));

        //bSizer_h->Add(m_panel_separotor1);

        sizer->Add(bSizer_h, 0, wxALL, 0);
#ifdef __WIN32__
        sizer->AddSpacer(FromDIP(17));
#else if __APPLE__
        sizer->AddSpacer(FromDIP(3));
#endif

        parent->SetSizer(sizer);
        parent->Layout();
        parent->Fit(); 
}

void DeviceDetail::switchPage() 
{
    if (m_device_speed) {
        m_device_speed->checkValue();
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
    m_device_speed->setText(wxString::Format("%d", aspeed));
    m_device_speed->setCurValue(aspeed);
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

void DeviceDetail::setChamberFanSpeed(double fanSpeed) 
{ 
     auto aFanSpeed = static_cast<int>(fanSpeed);
    m_device_cooling_fan->setText(wxString::Format("%d", aFanSpeed));
     m_device_cooling_fan->setCurValue(aFanSpeed);
}

SingleDeviceState::SingleDeviceState(wxWindow* parent, wxWindowID id, const wxPoint& pos, 
        const wxSize& size, long style, const wxString& name)
        : wxScrolledWindow(parent, id, pos, size, wxHSCROLL | wxVSCROLL), 
    m_cur_printing_ctrl(0)
{
        this->SetScrollRate(30, 30);
        this->SetBackgroundColour(wxColour(240,240,240));
        setupLayout();
        connectEvent();
        reInit();
}

SingleDeviceState::~SingleDeviceState()
{
    if(m_pic_thread){
        MultiComUtils::killAsyncCall(m_pic_thread);
    }    
}

void SingleDeviceState::setCurId(int curId)
{ 
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
    m_busy_device_detial->setCurId(curId);
    m_busy_circula_filter->setCurId(curId);
    m_idle_tempMixDevice->setCurId(curId);

    reInitProductState();
    m_idle_tempMixDevice->reInitProductState();

    //query device data by id
    bool  valid = false;
    const com_dev_data_t &data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
    if (!valid) {
        setPageOffline();
        return;
    }
    if (data.connectMode == 0) {
        m_cur_serial_number = data.lanDevInfo.serialNumber;
    } else if (data.connectMode == 1) {
        m_cur_serial_number = data.wanDevInfo.serialNumber;
    }
    reInitMaterialPic();
    reInitPage();
    onDevStateChanged(data.devDetail->status, data);
    fillValue(data);
    Layout();
    reInitData();
}

void SingleDeviceState::modifyVideoPlayerAddress(const std::string &urlAddress)
{
    std::string cur_language = getCurLanguage();
   std::string jsonStr = R"({"command": "modify_rtsp_player_address","address": "http://115.231.29.48:1370/ffspace/SNMMOC98989898.m3u8","sequence_id": "10001"})";
   // 将JSON字符串解析为JSON对象
   json jsonObj = json::parse(jsonStr);
   if (!urlAddress.empty()) {
      jsonObj["address"] = urlAddress;
   } else {
      return;
   }
   jsonObj["language"] = cur_language;
   // 将JSON对象转换为字符串
   std::string newJsonStr = jsonObj.dump();
   wxString    strJS      = wxString::Format("window.postMessage(%s)", wxString::FromUTF8(newJsonStr));
   if (m_browser) {
     WebView::RunScript(m_browser, strJS);
   }
}

void SingleDeviceState::notifyWebDevOffline() 
{
   std::string cur_language = getCurLanguage();
   std::string jsonStr = R"({"command" : "close_rtsp", "sequence_id" : "10001"})";
   json        jsonObj      = json::parse(jsonStr);
   jsonObj["language"]      = cur_language;
   std::string newJsonStr   = jsonObj.dump();
   wxString    strJS        = wxString::Format("window.postMessage(%s)", wxString::FromUTF8(newJsonStr));
   if (m_browser) {
     WebView::RunScript(m_browser, strJS);
   }
}

void SingleDeviceState::reInit() 
{
   reInitData();
   reInitUI();
}

void SingleDeviceState::reInitData() 
{ 
   m_last_speed               = 0.00001;
   m_last_z_axis_compensation = 0.00001;
   m_last_cooling_fan_speed   = 0.00001;
   m_last_chamber_fan_speed   = 0.00001;
   m_right_target_temp        = 0.00;
   m_plat_target_temp         = 0.00;
   m_camera_stream_url.clear();
   m_file_pic_url.clear();
   m_cur_dev_state.clear();
   m_cur_print_file_name.clear();
}

void SingleDeviceState::reInitUI() 
{
   m_staticText_device_tip->SetLabel(_L("Offline"));
   m_staticText_device_tip->SetForegroundColour(wxColour("#999999"));
   m_staticText_device_info->Hide();
   m_clear_button->Hide();
   m_staticText_idle->SetLabel(_L("Device offline"));
   m_idle_tempMixDevice->modifyTemp("/", "/", "/");
   m_idle_tempMixDevice->setState(0);
   m_cur_printing_ctrl = 0;
   Layout();
}

void SingleDeviceState::reInitMaterialPic() 
{
   if (m_material_picture) {
     if (m_material_image) {
            delete m_material_image;
            m_material_image = nullptr;
     }
     m_file_pic_url.clear();
     m_last_pic_data.clear();
     std::string name = "monitor_item_prediction_0";
     wxImage     image;
     m_material_image = new wxImage(image);
     m_material_picture->SetImage(*m_material_image);
   }
}

void SingleDeviceState::reInitPage() 
{ 
    if (m_busy_temp_brn) {
        m_busy_temp_brn->Hide();
    }
    if (m_busy_device_detial) {
        m_busy_device_detial->Hide();
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

void SingleDeviceState::setDevProductAuthority(const fnet_dev_product_t &data) 
{
   bool lightCtrl = data.lightCtrlState == 0 ? false : true;
   bool fanCtrl   = data.internalFanCtrlState == 0 ? false : true;
   if (!lightCtrl) {
     m_lamp_control_button->SetIcon("device_lamp_offline");
     m_lamp_control_button->Enable(false);
   }
   if (!fanCtrl) {
     m_filter_button->SetIcon("device_filter_offline");
     m_filter_button->Enable(false);
     if (m_busy_circula_filter) {
            m_busy_circula_filter->Hide();
     }
   }
}

void SingleDeviceState::reInitProductState()
{ 
    m_lamp_control_button->SetIcon("device_lamp_control");
    m_filter_button->SetIcon("device_filter");
    m_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_filter_button->SetBorderColor(wxColour(255, 255, 255));
    m_lamp_control_button->Enable(true); 
    m_filter_button->Enable(true);
}

std::string SingleDeviceState::getCurDevSerialNumber() 
{
    return m_cur_serial_number;
}

void SingleDeviceState::lostFocusmodifyTemp() 
{
    double top_temp;
    double bottom_temp;
    double mid_temp;
    bool   bTop = m_tempCtrl_top->GetTagTemp().ToDouble(&top_temp);
    if (!bTop) {
        m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
        top_temp = m_right_target_temp;
    }
    if (top_temp > 280) {
        top_temp = 280;
        m_tempCtrl_top->SetTagTemp(top_temp, true);
        m_right_target_temp = top_temp;
    } else if (top_temp < 0) {
        top_temp = 0;
        m_tempCtrl_top->SetTagTemp(top_temp, true);
        m_right_target_temp = top_temp;
    }
    bool bBottom = m_tempCtrl_bottom->GetTagTemp().ToDouble(&bottom_temp);
    if (!bBottom) {
        m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
        bottom_temp = m_plat_target_temp;
    }
    if (bottom_temp > 110) {
        bottom_temp = 110;
        m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
        m_plat_target_temp = bottom_temp;
    } else if (bottom_temp < 0) {
        bottom_temp = 0;
        m_tempCtrl_bottom->SetTagTemp(bottom_temp, true);
        m_plat_target_temp = bottom_temp;
    }
    bool  bMid = m_tempCtrl_mid->GetTagTemp().ToDouble(&mid_temp);
    ComTempCtrl *tempCtrl = new ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
}

wxBoxSizer* SingleDeviceState::create_monitoring_page()
{
        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

        //水平布局
        wxBoxSizer *bSizer_title_label = new wxBoxSizer(wxHORIZONTAL);
        m_panel_top_title = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(22)), wxTAB_TRAVERSAL);
        m_panel_top_title->SetBackgroundColour(wxColour(240,240,240));
        //显示设备名称
        m_staticText_device_name = new Label(m_panel_top_title, ("      "));
        m_staticText_device_name->Wrap(-1);
        //m_staticText_device_name->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_device_name->SetForegroundColour(wxColour(51,51,51));

        bSizer_title_label->Add(m_staticText_device_name, 0, wxALIGN_LEFT | wxEXPAND | wxALL, 0);
        bSizer_title_label->AddStretchSpacer();

        //显示设备所在货架
        m_staticText_device_position = new Label(m_panel_top_title, ("      "));
        m_staticText_device_position->Wrap(-1);
        //m_staticText_device_position->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_device_position->SetForegroundColour(wxColour(51,51,51));

        bSizer_title_label->Add(m_staticText_device_position, 0, wxALIGN_CENTER | wxEXPAND | wxALL, 0);
        bSizer_title_label->AddStretchSpacer();

        //显示提示内容
        m_staticText_device_tip = new Label(m_panel_top_title, _L("error"));
        m_staticText_device_tip->Wrap(-1);
        //m_staticText_device_tip->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_device_tip->SetForegroundColour(wxColour(251,71,71));

        bSizer_title_label->Add(m_staticText_device_tip, 0, wxALIGN_RIGHT | wxEXPAND | wxALL, 0);
        //bSizer_title_label->AddStretchSpacer();

        m_panel_top_title->SetSizer(bSizer_title_label);
        m_panel_top_title->Layout();
        bSizer_title_label->Fit(m_panel_top_title);

        sizer->AddSpacer(FromDIP(12));
        sizer->Add(m_panel_top_title, 0, wxEXPAND | wxALL, 0);
        sizer->AddSpacer(FromDIP(4));
        
        //添加白色分割条
        auto m_panel_separotor_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(3)), wxTAB_TRAVERSAL);
        m_panel_separotor_top->SetBackgroundColour(wxColour(255,255,255));
        sizer->Add(m_panel_separotor_top, 0, wxEXPAND | wxALL, 0);

        //第二段水平布局，相机垂直布局的顶部间隔
        auto m_panel_separotor0 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor0->SetBackgroundColour(wxColour(240, 240, 240));
        m_panel_separotor0->SetMinSize(wxSize(-1, FromDIP(10)));
        m_panel_separotor0->SetMaxSize(wxSize(-1, FromDIP(10)));
        sizer->Add(m_panel_separotor0, 0, wxEXPAND, 0);
        //sizer->AddSpacer(FromDIP(6));

        //摄像头布局
        m_panel_monitoring_title = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
        m_panel_monitoring_title->SetBackgroundColour(wxColour(248,248,248));

        //“摄像头”文字布局
        wxBoxSizer *bSizer_monitoring_title;
        bSizer_monitoring_title = new wxBoxSizer(wxHORIZONTAL);

        m_staticText_monitoring = new Label(m_panel_monitoring_title, _L("Camera"));
        m_staticText_monitoring->Wrap(-1);
        //m_staticText_monitoring->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_monitoring->SetForegroundColour(wxColour(51,51,51));

        bSizer_monitoring_title->AddSpacer(FromDIP(13));
        bSizer_monitoring_title->Add(m_staticText_monitoring, 0, wxTOP, FromDIP(6));
        bSizer_monitoring_title->AddStretchSpacer();

        m_panel_monitoring_title->SetSizer(bSizer_monitoring_title);
        m_panel_monitoring_title->Layout();
        bSizer_monitoring_title->Fit(m_panel_monitoring_title);

        //行与行之间的间距使用 wxPanel 进行填充
        sizer->Add(m_panel_monitoring_title, 0, wxEXPAND | wxALL, 0);

        //播放控件
        m_camera_play_url = wxString::Format("file://%s/web/orca/missing_connection.html?lang=http://192.168.4.64:8080/?action=stream", from_u8(resources_dir()));

        m_browser = WebView::CreateWebView(this,m_camera_play_url);

        wxEvtHandler::Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED, &SingleDeviceState::onScriptMessage, this);
        m_browser->Bind(wxEVT_WEBVIEW_NAVIGATED, &SingleDeviceState::on_navigated, this);
        if(m_browser == nullptr){
            BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << boost::format("load web view of SingleDeviceState url failed");
            return sizer;
        }
        m_browser->SetMinSize(wxSize(FromDIP(730), FromDIP(364)));
        //m_browser->SetMaxSize(wxSize(FromDIP(786), FromDIP(364)));
        m_browser->EnableContextMenu(true);
        //sizer->Add(m_browser, 1, wxEXPAND | wxALL, 0);
        sizer->Add(m_browser, 0, wxALL, 0);



        return sizer;
}

wxBoxSizer* SingleDeviceState::create_machine_control_title()
{
        wxBoxSizer *bSizer_v_title = new wxBoxSizer(wxVERTICAL);
        bSizer_v_title->AddSpacer(FromDIP(12));

        //设备提示信息
        wxBoxSizer *bSizer_h_title = new wxBoxSizer(wxHORIZONTAL);
        m_panel_top_right_info = new wxPanel(this, wxID_ANY,wxDefaultPosition, wxSize(-1, FromDIP(30)), wxTAB_TRAVERSAL);
        //m_panel_top_right_info = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_top_right_info->SetBackgroundColour(wxColour(240,240,240));

        //显示报错信息
        //m_staticText_device_info = new Label(m_panel_top_right_info, ("error Info"), wxALIGN_CENTER);
        //m_staticText_device_info->Wrap(-1);
        ////m_staticText_device_info->SetFont(wxFont(wxFontInfo(16)));
        //m_staticText_device_info->SetBackgroundColour(wxColour(246,203,198));
        //m_staticText_device_info->SetForegroundColour(wxColour(251, 71, 71));

        //bSizer_h_title->Add(m_staticText_device_info, wxSizerFlags(1).Expand());
        //bSizer_h_title->AddSpacer(FromDIP(6));

        m_staticText_device_info = new FFButton(m_panel_top_right_info, wxID_ANY, ("error Info"), 0);
        m_staticText_device_info->Enable(false);
        //m_staticText_device_info->SetBackgroundColour(*wxWHITE);
        m_staticText_device_info->SetBGDisableColor(wxColour("#F6CBC6"));
        m_staticText_device_info->SetFontDisableColor(wxColour("#FB4747"));
        m_staticText_device_info->SetMinSize(wxSize(FromDIP(394), FromDIP(56)));

        bSizer_h_title->Add(m_staticText_device_info, 0, wxALL | wxEXPAND);
        bSizer_h_title->AddSpacer(FromDIP(6));

        //显示清除按钮
        m_clear_button = new Button(m_panel_top_right_info, _L("clear"), "", 0, FromDIP(20));
        m_clear_button->SetPureText(true);
        //m_clear_button->SetFont(wxFont(wxFontInfo(16)));
        m_clear_button->SetBorderWidth(1);
        m_clear_button->SetBackgroundColor(wxColour(240,240,240));
        m_clear_button->SetBorderColor(wxColour(50,141,251));
        m_clear_button->SetTextColor(wxColour(50,141,251));
        //m_clear_button->SetMinSize((wxSize(FromDIP(34), FromDIP(23))));
        m_clear_button->SetCornerRadius(0);
        m_clear_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { 
            e.Skip();
            m_staticText_device_info->Hide();
            m_clear_button->Hide();
            //m_staticText_file_name->SetLabel("333555666777888999");
            Layout();
        });

        bSizer_h_title->Add(m_clear_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(0));

        m_panel_top_right_info->SetSizer(bSizer_h_title);
        m_panel_top_right_info->Layout();
        bSizer_h_title->Fit(m_panel_top_right_info);

        bSizer_v_title->Add(m_panel_top_right_info, 0, wxALL | wxEXPAND, 0);

        //设备信息与信息之间的间隔
        auto m_panel_separotor3 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor3->SetBackgroundColour(wxColour(240, 240, 240));
        m_panel_separotor3->SetMinSize(wxSize(-1, FromDIP(10)));
        m_panel_separotor3->SetMaxSize(wxSize(-1, FromDIP(10)));

        bSizer_v_title->Add(m_panel_separotor3, 0, wxEXPAND, 0);
        return bSizer_v_title;
}

wxBoxSizer* SingleDeviceState::create_machine_control_page()
{
        wxBoxSizer *bSizer_right = new wxBoxSizer(wxVERTICAL);
        bSizer_right->SetMinSize(wxSize(FromDIP(450),-1));

        //忙碌状态
        m_machine_ctrl_panel = new wxPanel(this);
        m_machine_ctrl_panel->SetBackgroundColour(*wxWHITE);
        m_machine_ctrl_panel->SetDoubleBuffered(true);
        wxBoxSizer* bSizer_v_busy = new wxBoxSizer(wxVERTICAL);
        setupLayoutBusyPage(bSizer_v_busy,m_machine_ctrl_panel);

        m_machine_ctrl_panel->SetSizer(bSizer_v_busy);
        m_machine_ctrl_panel->Layout();
        bSizer_v_busy->Fit(m_machine_ctrl_panel);
        
        //空闲状态
        m_machine_idle_panel = new wxPanel(this);
        //m_machine_idle_panel->SetBackgroundColour(*wxWHITE);
        m_machine_idle_panel->SetDoubleBuffered(true);
        wxBoxSizer* bSizer_v_idle = new wxBoxSizer(wxVERTICAL);
        setupLayoutIdlePage(bSizer_v_idle,m_machine_idle_panel);

        m_machine_idle_panel->SetSizer(bSizer_v_idle);
        m_machine_idle_panel->Layout();
        bSizer_v_idle->Fit(m_machine_idle_panel);
//***
        bSizer_right->Add(m_machine_ctrl_panel,  0, wxEXPAND, 0);
        bSizer_right->Add(m_machine_idle_panel, 0, wxEXPAND, 0);
        //m_machine_idle_panel->Hide();
        m_machine_ctrl_panel->Hide();
        return bSizer_right;
}

void SingleDeviceState::setupLayout()
{
 //最外层框架布局
        wxBoxSizer *bSizer_status = new wxBoxSizer(wxVERTICAL);

        //顶部背景条（作为间距）
        auto m_panel_separotor_top_back = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(18)), wxTAB_TRAVERSAL);
        m_panel_separotor_top_back->SetBackgroundColour(wxColour(240,240,240));
        bSizer_status->Add(m_panel_separotor_top_back, 0, wxEXPAND | wxALL, 0);

        //第二段水平布局
        wxBoxSizer *bSizer_status_below = new wxBoxSizer(wxHORIZONTAL);
        //第二段水平布局，左侧空白
        auto m_panel_separotor_left = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor_left->SetBackgroundColour(wxColour(240, 240, 240));
        m_panel_separotor_left->SetMinSize(wxSize(FromDIP(24), -1));

        bSizer_status_below->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);
        
        //第二段水平布局，中间垂直布局
        wxBoxSizer *bSizer_left = new wxBoxSizer(wxVERTICAL);

        //第二段水平布局，相机布局
        auto m_monitoring_sizer = create_monitoring_page();
        bSizer_left->Add(m_monitoring_sizer, 0, wxALL, 0);

        //第二段水平布局，相机垂直布局的中间间隔
        auto m_panel_separotor1 = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor1->SetBackgroundColour(wxColour(240, 240, 240));
        m_panel_separotor1->SetMinSize(wxSize(-1, FromDIP(20)));
        m_panel_separotor1->SetMaxSize(wxSize(-1, FromDIP(20)));
        m_monitoring_sizer->Add(m_panel_separotor1, 0, wxALL, 0);

        //第二段水平布局，相机垂直布局中的材料站
        m_material_panel = new MaterialPanel(this);
        m_monitoring_sizer->Add(m_material_panel, 0, wxALL | wxEXPAND , 0);
        bSizer_status_below->Add(bSizer_left, 0, wxALL | wxEXPAND, 0);

        //第二段水平布局，中间间隔
        auto m_panel_separator_middle = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxTAB_TRAVERSAL);
        m_panel_separator_middle->SetBackgroundColour(wxColour(240, 240, 240));
        m_panel_separator_middle->SetMinSize(wxSize(FromDIP(20), -1));

        bSizer_status_below->Add(m_panel_separator_middle, 0, wxEXPAND | wxALL, 0);

        //第二段水平布局，右侧信息
        //标题栏
        auto m_machine_title = create_machine_control_title();
        //信息与控制详情页

        auto m_machine_control = create_machine_control_page();
/*
        m_machine_ctrl_panel->SetSizer(m_machine_control);
        m_machine_ctrl_panel->Layout();
        m_machine_control->Fit(m_machine_ctrl_panel);

        bSizer_status_below->Add(m_machine_ctrl_panel, 0, wxALL, 0);
*/
        m_machine_title->Add(m_machine_control, 0, wxALL, 0);
        bSizer_status_below->Add(m_machine_title, 0, wxALL, 0);
        //水平布局最右侧间隔
        m_panel_separator_right = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(35), -1), wxTAB_TRAVERSAL);
        m_panel_separator_right->SetBackgroundColour(wxColour(240, 240, 240));

        bSizer_status_below->Add(m_panel_separator_right, 0, wxEXPAND | wxALL, 0);

        bSizer_status->Add(bSizer_status_below, 1, wxALL | wxEXPAND, 0);

        //底部间距
        m_panel_separotor_bottom = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(10)), wxTAB_TRAVERSAL);
        m_panel_separotor_bottom->SetBackgroundColour(wxColour(240, 240, 240));

        bSizer_status->Add(m_panel_separotor_bottom, 0, wxEXPAND | wxALL, 0);
        this->SetSizerAndFit(bSizer_status);
        this->Layout();
}

void SingleDeviceState::setupLayoutBusyPage(wxBoxSizer* busySizer,wxPanel* parent)
{
        //标题：信息与控制
        m_panel_control_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
        m_panel_control_title->SetBackgroundColour(wxColour(248,248,248));

        wxBoxSizer *bSizer_control_title = new wxBoxSizer(wxHORIZONTAL);
        m_staticText_control = new Label(m_panel_control_title,_L("Info and Control"));
        m_staticText_control->Wrap(-1);
        //m_staticText_control->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_control->SetForegroundColour(wxColour(51,51,51));

        bSizer_control_title->Add(m_staticText_control, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
        bSizer_control_title->AddStretchSpacer();

        m_panel_control_title->SetSizer(bSizer_control_title);
        m_panel_control_title->Layout();
        bSizer_control_title->Fit(m_panel_control_title);

        //添加标题
        busySizer->Add(m_panel_control_title, 0, wxALL | wxEXPAND, 0);
///********
        //先垂直布局（添加空白间距）
        //再水平布局（1、添加左侧空白间距；2、添加中间垂直布局，3、添加右侧垂直布局）
        //最终添加至整体布局

        wxBoxSizer *bSizer_file_info = new wxBoxSizer(wxVERTICAL);
        m_panel_control_info = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(167)), wxTAB_TRAVERSAL);
        m_panel_control_info->SetBackgroundColour(wxColour(255,255,255));

//***顶部白条（分隔）
        auto m_panel_separotor_top = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(8)), wxTAB_TRAVERSAL);
        m_panel_separotor_top->SetBackgroundColour(wxColour(255,255,255));

        bSizer_file_info->Add(m_panel_separotor_top, 0, wxEXPAND | wxALL, 0);

        //信息部分水平布局
        wxBoxSizer *bSizer_control_info = new wxBoxSizer(wxHORIZONTAL);

//***水平布局添加最左侧空白
        auto m_panel_separotor_left = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(17), -1),wxTAB_TRAVERSAL);
        m_panel_separotor_left->SetBackgroundColour(wxColour(255,255,255));
        //m_panel_separotor_left->SetMinSize(wxSize(FromDIP(10), -1));

        bSizer_control_info->Add(m_panel_separotor_left, 0, wxEXPAND | wxALL, 0);

//添加垂直布局（显示文件内容）
        wxBoxSizer *bSizer_control_file_info = new wxBoxSizer(wxVERTICAL);
        //auto m_panel_control_file_info = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(145)), wxTAB_TRAVERSAL);
        auto m_panel_control_file_info = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxDefaultSize,wxTAB_TRAVERSAL);
        m_panel_control_file_info->SetBackgroundColour(wxColour(255,255,255));

 //文件信息
        wxBoxSizer *bSizer_control_file_name = new wxBoxSizer(wxHORIZONTAL);
        auto m_panel_control_file_name = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_control_file_name->SetBackgroundColour(wxColour(255, 255, 255));

        //显示文件标题
        m_staticText_file_head = new Label(m_panel_control_file_name, _L("file: "));
        m_staticText_file_head->Wrap(-1);
        //m_staticText_file_head->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_file_head->SetForegroundColour(wxColour(51, 51, 51));

        //显示文件名称
        m_staticText_file_name = new Label(m_panel_control_file_name, "123456123456123456");
        m_staticText_file_name->Wrap(-1);
        //m_staticText_file_name->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_file_name->SetForegroundColour(wxColour(51, 51, 51));

        bSizer_control_file_name->Add(m_staticText_file_head);
        bSizer_control_file_name->Add(m_staticText_file_name);
        m_panel_control_file_name->SetSizer(bSizer_control_file_name);
        m_panel_control_file_name->Layout();
        bSizer_control_file_name->Fit(m_panel_control_file_name);


        bSizer_control_file_info->Add(m_panel_control_file_name, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
 /*       
        //显示设备状态
        m_staticText_device_state = new Label(m_panel_control_file_info, _L("pause"));
        m_staticText_device_state->Wrap(-1);
        m_staticText_device_state->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_device_state->SetForegroundColour(wxColour(50,141,251));

        bSizer_control_file_info->Add(m_staticText_device_state, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
*/

////文件中间白条（分隔）
//        auto m_panel_separotor_mid = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(24)), wxTAB_TRAVERSAL);
//        m_panel_separotor_mid->SetBackgroundColour(wxColour(255,255,255));
//
//        bSizer_control_file_info->Add(m_panel_separotor_mid, 0, wxEXPAND | wxALL, 0);
        //bSizer_control_file_info->AddStretchSpacer();
        bSizer_control_file_info->AddSpacer(FromDIP(40));

        //显示倒计时
        wxString time = "0" + _L("h ") + "0" + _L("min ");
        m_staticText_count_time = new Label(m_panel_control_file_info, ::Label::Body_14, time);
        m_staticText_count_time->Wrap(-1);
        //m_staticText_count_time->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_count_time->SetForegroundColour(wxColour(50,141,251));

        bSizer_control_file_info->Add(m_staticText_count_time, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_file_info->AddSpacer(FromDIP(10));

        //显示剩余时间标签
        m_staticText_time_label = new Label(m_panel_control_file_info, ::Label::Body_12, _L("Remaining Time"));
        m_staticText_time_label->Wrap(-1);
        //m_staticText_time_label->SetFont(wxFont(wxFontInfo(10)));
        m_staticText_time_label->SetForegroundColour(wxColour(153,153,153));

        bSizer_control_file_info->Add(m_staticText_time_label, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));

        auto m_panel_separotor_mid = new wxPanel(m_panel_control_file_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(11)),wxTAB_TRAVERSAL);
        m_panel_separotor_mid->SetBackgroundColour(wxColour(255, 255, 255));

        bSizer_control_file_info->Add(m_panel_separotor_mid, 0, wxEXPAND | wxALL, 0);

        //显示进度条
        m_progress_bar = new ProgressBar(m_panel_control_file_info,wxID_ANY,100,wxDefaultPosition,wxSize(FromDIP(201),FromDIP(5)),true);
        m_progress_bar->SetProgress(30);
        m_progress_bar->SetProgressBackgroundColour(wxColour(50, 141, 251));
        bSizer_control_file_info->Add(m_progress_bar, 0, wxALIGN_CENTER_VERTICAL, FromDIP(4));
        //bSizer_control_file_info->AddSpacer(FromDIP(6));

        m_panel_control_file_info->SetSizer(bSizer_control_file_info);
        m_panel_control_file_info->Layout();
        bSizer_control_file_info->Fit(m_panel_control_file_info);
        
        bSizer_control_info->Add(m_panel_control_file_info, 0, wxEXPAND | wxALL, 0);
/*
//***水平布局添加中间空白间距
        auto m_panel_separotor_right = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(58), -1), wxTAB_TRAVERSAL);
        m_panel_separotor_right->SetBackgroundColour(wxColour(255,255,255));

        bSizer_control_info->Add(m_panel_separotor_right, 0, wxEXPAND | wxALL, 0);
*/
           bSizer_control_info->AddStretchSpacer();

//***添加右侧垂直布局
        wxBoxSizer *bSizer_control_material = new wxBoxSizer(wxVERTICAL);
        m_panel_control_material = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(145)), wxTAB_TRAVERSAL);
        m_panel_control_material->SetBackgroundColour(wxColour(255,255,255));

        //***添加右侧材料
        static Slic3r::GUI::BitmapCache cache;
        m_material_weight_pic = create_scaled_bitmap("device_material_weight", this, 16);
        
        m_material_weight_staticbitmap = new wxStaticBitmap(m_panel_control_material, wxID_ANY,m_material_weight_pic);

        m_material_picture = new MaterialImagePanel(m_panel_control_material);
        m_material_picture->SetMinSize(wxSize(FromDIP(80), FromDIP(80)));

        m_material_weight_label = new Label(m_panel_control_material,("234g"));
        
        wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
        hbox->SetMinSize(wxSize(60,-1));
        hbox->Add(m_material_weight_staticbitmap,0, wxALIGN_CENTER| wxALL,0);
        hbox->AddSpacer(FromDIP(5));
        hbox->Add(m_material_weight_label, wxALIGN_CENTER| wxALL,0);

        bSizer_control_material->Add(hbox, 0, /*wxALIGN_CENTER*/ wxRIGHT | wxALL, 0);
        bSizer_control_material->AddStretchSpacer();
        bSizer_control_material->Add(m_material_picture, 0, /*wxALIGN_CENTER*/ wxRIGHT, 0);

        m_panel_control_material->SetSizer(bSizer_control_material);
        m_panel_control_material->Layout();
        bSizer_control_material->Fit(m_panel_control_material);
        
        bSizer_control_info->Add(m_panel_control_material);

//***水平布局添加最右侧空白
        auto m_panel_separotor_right2 = new wxPanel(m_panel_control_info, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor_right2->SetBackgroundColour(wxColour(255,255,255));
        m_panel_separotor_right2->SetMinSize(wxSize(FromDIP(26), -1));

        bSizer_control_info->Add(m_panel_separotor_right2, 0, wxEXPAND | wxALL, 0);

        bSizer_file_info->Add(bSizer_control_info, 0, wxEXPAND | wxALL, 0);

//**信息与控制布局添加至垂直布局
        m_panel_control_info->SetSizer(bSizer_file_info);
        m_panel_control_info->Layout();
        bSizer_file_info->Fit(m_panel_control_info);

        busySizer->Add(m_panel_control_info, 0, wxALL | wxEXPAND, 0);

//****添加暂停打印、取消打印
        //设备信息与打印按钮之间的间隔
        auto m_panel_separotor4 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor4->SetBackgroundColour(wxColour(248,248,248));
        m_panel_separotor4->SetMinSize(wxSize(-1, FromDIP(13)));
        m_panel_separotor4->SetMaxSize(wxSize(-1, FromDIP(13)));
        busySizer->Add(m_panel_separotor4, 0, wxEXPAND, 0);


//***打印布局
        wxBoxSizer *bSizer_control_print = new wxBoxSizer(wxHORIZONTAL);
        auto m_panel_control_print = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
        m_panel_control_print->SetBackgroundColour(wxColour(255,255,255));

        //显示继续打印按钮
        m_print_button = new Button(m_panel_control_print, _L("pause print"), "device_pause_print", 0, 16);
        m_print_button->SetFlashForge(true);
        //m_print_button->SetFont(wxFont(wxFontInfo(16)));
        m_print_button->SetBorderWidth(0);
        m_print_button->SetBackgroundColor(wxColour(255,255,255));
        m_print_button->SetBorderColor(wxColour(255,255,255));
        m_print_button->SetTextColor(wxColour(51,51,51));
//        m_print_button->SetMinSize((wxSize(FromDIP(158), FromDIP(29))));
        m_print_button->SetCornerRadius(0);
        m_print_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            e.Skip();
            auto detail = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail;
            std::string status = detail->status;
            if (/*!m_print_button_pressed_down*/ status == "printing") {
                m_cur_printing_ctrl    = 1;
                std::string printState = PAUSE;
                std::string jobId      = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
                ComJobCtrl *jobCtrl    = new ComJobCtrl(jobId, printState);
                Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
                /*m_print_button->SetLabel(_L("continue print"));
                m_print_button->SetIcon("device_continue_print");
                m_print_button->Refresh();*/
            } else if (status == PAUSE || status == P_PAUSING) {
                m_cur_printing_ctrl    = 2;
                std::string printState = CONTINUE;
                std::string jobId      = Slic3r::GUI::MultiComMgr::inst()->devData(m_cur_id).devDetail->jobId;
                ComJobCtrl *jobCtrl = new ComJobCtrl(jobId, printState);
                Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, jobCtrl);
                /*m_print_button->SetLabel(_L("pause print"));
                m_print_button->SetIcon("device_pause_print");
                m_print_button->Refresh();*/
            }
            //m_print_button_pressed_down = !m_print_button_pressed_down;
        });

        //bSizer_control_print->Add(m_print_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_print->Add(m_print_button, wxSizerFlags(1).Expand());

        auto m_panel_separotor_print = new wxPanel(m_panel_control_print, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1), wxTAB_TRAVERSAL);
        m_panel_separotor_print->SetBackgroundColour(wxColour(240,240,240));

        bSizer_control_print->Add(m_panel_separotor_print, 0, wxEXPAND | wxALL, 0);

        //显示取消打印按钮
        m_cancel_button = new Button(m_panel_control_print, _L("cancel print"), "device_cancel_print", 0, 16);
        m_cancel_button->SetFlashForge(true);
        //m_cancel_button->SetFont(wxFont(wxFontInfo(16)));
        m_cancel_button->SetBorderWidth(0);
        m_cancel_button->SetBackgroundColor(wxColour(255,255,255));
        m_cancel_button->SetBorderColor(wxColour(255,255,255));
        m_cancel_button->SetTextColor(wxColour(51,51,51));
//        m_cancel_button->SetMinSize((wxSize(FromDIP(158), FromDIP(29))));
        m_cancel_button->SetCornerRadius(0);
        m_cancel_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) { 
            //e.Skip();
            if (!m_cancel_confirm_page) {
                m_cancel_confirm_page = new CancelPrint(_L("Whether Cancel Printing"), _L("yes"), _L("no"));
                m_cancel_confirm_page->Bind(EVT_CANCEL_PRINT_CLICKED, &SingleDeviceState::onCancelPrint,this);
                m_cancel_confirm_page->Bind(EVT_CONTINUE_PRINT_CLICKED, &SingleDeviceState::onContinuePrint,this);
            }
            m_cancel_confirm_page->ShowModal();
        });

        //bSizer_control_print->Add(m_cancel_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_print->Add(m_cancel_button, wxSizerFlags(1).Expand());

//***继续打印布局添加至垂直布局
        m_panel_control_print->SetSizer(bSizer_control_print);
        m_panel_control_print->Layout();
        bSizer_control_print->Fit(m_panel_control_print);

        busySizer->Add(m_panel_control_print,0, wxALL | wxEXPAND, 0);

//***添加打印控制和温度布局之间的间隔
        auto m_panel_separotor5 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor5->SetBackgroundColour(wxColour(240,240,240));
        m_panel_separotor5->SetMinSize(wxSize(-1, FromDIP(13)));
        m_panel_separotor5->SetMaxSize(wxSize(-1, FromDIP(13)));
        busySizer->Add(m_panel_separotor5, 0, wxEXPAND, 0);

//***温度布局
        wxBoxSizer *bSizer_control_temperature = new wxBoxSizer(wxHORIZONTAL);
        auto m_panel_control_temperature = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
        m_panel_control_temperature->SetBackgroundColour(wxColour(255,255,255));

        //显示顶部温度控件
        wxWindowID top_id = wxWindow::NewControlId();
        m_tempCtrl_top = new TempInput(m_panel_control_temperature, top_id, wxString("--"), wxString("--"), wxString("device_top_temperature"), wxString("device_top_temperature"), wxDefaultPosition,
                                        wxDefaultSize, wxALIGN_CENTER);
        m_tempCtrl_top->SetMinTemp(20);
        m_tempCtrl_top->SetMaxTemp(120);
        m_tempCtrl_top->SetMinSize((wxSize(FromDIP(106), FromDIP(29))));
        m_tempCtrl_top->SetBorderWidth(0);
        StateColor tempinput_text_colour(std::make_pair(wxColour(51,51,51), (int) StateColor::Disabled), std::make_pair(wxColour(48,58,60), (int) StateColor::Normal));
        m_tempCtrl_top->SetTextColor(tempinput_text_colour);
        StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int)StateColor::Disabled), std::make_pair(wxColour(0, 150, 136), (int)StateColor::Focused),
                                        std::make_pair(wxColour(0, 150, 136), (int)StateColor::Hovered),std::make_pair(*wxWHITE, (int)StateColor::Normal));
        m_tempCtrl_top->SetBorderColor(tempinput_border_colour);
        m_tempCtrl_top->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
            event.Skip();
            lostFocusmodifyTemp();
        });
        m_tempCtrl_top->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
            event.Skip();
            lostFocusmodifyTemp();
        });
        //m_tempCtrl_top->Bind(wxEVT_TEXT,&SingleDeviceState::onTargetTempModify, this);  

        //bSizer_control_temperature->Add(m_tempCtrl_top, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_temperature->Add(m_tempCtrl_top, wxSizerFlags(1).Expand());

        //间距
        auto m_panel_separotor_temp = new wxPanel(m_panel_control_temperature, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1), wxTAB_TRAVERSAL);
        m_panel_separotor_temp->SetBackgroundColour(wxColour(240,240,240));
        bSizer_control_temperature->Add(m_panel_separotor_temp, 0, wxEXPAND | wxALL, 0);

        //显示底部温度控件
        wxWindowID bottom_id = wxWindow::NewControlId();
        m_tempCtrl_bottom = new TempInput(m_panel_control_temperature, bottom_id, wxString("--"), wxString("--"), wxString("device_bottom_temperature"), wxString("device_bottom_temperature"), wxDefaultPosition,
                                        wxDefaultSize, wxALIGN_CENTER);

        m_tempCtrl_bottom->SetMinTemp(20);
        m_tempCtrl_bottom->SetMaxTemp(120);
        m_tempCtrl_bottom->SetMinSize((wxSize(FromDIP(106), FromDIP(29))));
        m_tempCtrl_bottom->SetBorderWidth(0);
        //StateColor tempinput_text_colour(std::make_pair(wxColour(171, 172, 172), (int) StateColor::Disabled), std::make_pair(wxColour(48,58,60), (int) StateColor::Normal));
        //m_tempCtrl_bottom->SetTextColor(tempinput_text_colour);
        //StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int)StateColor::Disabled), std::make_pair(wxColour(0, 150, 136), (int)StateColor::Focused),
        //                                std::make_pair(wxColour(0, 150, 136), (int)StateColor::Hovered),std::make_pair(*wxWHITE, (int)StateColor::Normal));
        m_tempCtrl_bottom->SetBorderColor(tempinput_border_colour);
        m_tempCtrl_bottom->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
            event.Skip();
            lostFocusmodifyTemp();
        });
        m_tempCtrl_bottom->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
            event.Skip();
            lostFocusmodifyTemp();
        });
        //m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);  
        //bSizer_control_temperature->Add(m_tempCtrl_bottom, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_temperature->Add(m_tempCtrl_bottom, wxSizerFlags(1).Expand());

        //间距
        auto m_panel_separotor_temp1 = new wxPanel(m_panel_control_temperature, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1), wxTAB_TRAVERSAL);
        m_panel_separotor_temp1->SetBackgroundColour(wxColour(240,240,240));
        bSizer_control_temperature->Add(m_panel_separotor_temp1, 0, wxEXPAND | wxALL, 0);

        //显示中间温度控件
        wxWindowID bottom_mid = wxWindow::NewControlId();
        m_tempCtrl_mid = new TempInput(m_panel_control_temperature, bottom_mid, wxString("--"), wxString("--"), wxString("device_mid_temperature"), wxString("device_mid_temperature"), wxDefaultPosition,
                                        wxDefaultSize, wxALIGN_CENTER);

        m_tempCtrl_mid->SetMinTemp(20);
        m_tempCtrl_mid->SetMaxTemp(120);
        m_tempCtrl_mid->SetMinSize((wxSize(FromDIP(106), FromDIP(29))));
        m_tempCtrl_mid->SetBorderWidth(0);
        m_tempCtrl_mid->SetReadOnly(true);
        m_tempCtrl_mid->SetTextBindInput();
        //StateColor tempinput_text_colour(std::make_pair(wxColour(171, 172, 172), (int) StateColor::Disabled), std::make_pair(wxColour(48,58,60), (int) StateColor::Normal));
        //m_tempCtrl_mid->SetTextColor(tempinput_text_colour);
        //StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int)StateColor::Disabled), std::make_pair(wxColour(0, 150, 136), (int)StateColor::Focused),
        //                                std::make_pair(wxColour(0, 150, 136), (int)StateColor::Hovered),std::make_pair(*wxWHITE, (int)StateColor::Normal));
        m_tempCtrl_mid->SetBorderColor(tempinput_border_colour);
        //m_tempCtrl_mid->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
        m_tempCtrl_mid->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
            event.Skip();
            lostFocusmodifyTemp();
        });
        m_tempCtrl_mid->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
            event.Skip();
            lostFocusmodifyTemp();
        });
        //bSizer_control_temperature->Add(m_tempCtrl_mid, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_temperature->Add(m_tempCtrl_mid, wxSizerFlags(1).Expand());

//***温度布局添加至垂直布局
        m_panel_control_temperature->SetSizer(bSizer_control_temperature);
        m_panel_control_temperature->Layout();
        bSizer_control_temperature->Fit(m_panel_control_temperature);

        busySizer->Add(m_panel_control_temperature,0, wxALL | wxEXPAND, 0);

//***添加温度布局和灯布局之间的间隔
        auto m_panel_separotor6 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor6->SetBackgroundColour(wxColour(240,240,240));
        m_panel_separotor6->SetMinSize(wxSize(-1, FromDIP(13)));
        m_panel_separotor6->SetMaxSize(wxSize(-1, FromDIP(13)));
        busySizer->Add(m_panel_separotor6, 0, wxEXPAND, 0);

//***灯控制布局
        wxBoxSizer *bSizer_control_lamp = new wxBoxSizer(wxHORIZONTAL);
        auto m_panel_control_lamp = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
        m_panel_control_lamp->SetBackgroundColour(wxColour(255,255,255));

        //显示文件信息按钮
        m_device_info_button = new Button(m_panel_control_lamp, wxString(""), "device_file_info", 0, FromDIP(18));
        //m_device_info_button->SetFont(wxFont(wxFontInfo(16)));
        m_device_info_button->SetBorderWidth(0);
        m_device_info_button->SetBackgroundColor(wxColour(255,255,255));
        m_device_info_button->SetBorderColor(wxColour(255,255,255));
        //m_device_info_button->SetTextColor(wxColour(51,51,51));
        m_device_info_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
        m_device_info_button->SetCornerRadius(0);
        //bSizer_control_lamp->Add(m_device_info_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_lamp->Add(m_device_info_button, wxSizerFlags(1).Expand());
        bSizer_control_lamp->AddSpacer(FromDIP(35));

        //显示灯控制按钮
        m_lamp_control_button = new Button(m_panel_control_lamp, wxString(""), "device_lamp_control", 0, FromDIP(18));
        //m_lamp_control_button->SetFont(wxFont(wxFontInfo(16)));
        m_lamp_control_button->SetBorderWidth(0);
        m_lamp_control_button->SetBackgroundColor(wxColour(255,255,255));
        m_lamp_control_button->SetBorderColor(wxColour(255,255,255));
        //m_lamp_control_button->SetTextColor(wxColour(51,51,51));
        m_lamp_control_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
        m_lamp_control_button->SetCornerRadius(0);
        //bSizer_control_lamp->Add(m_lamp_control_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_lamp->Add(m_lamp_control_button, wxSizerFlags(1).Expand());
        bSizer_control_lamp->AddSpacer(FromDIP(35));

        //显示过滤按钮
        m_filter_button = new Button(m_panel_control_lamp, wxString(""), "device_filter", 0, FromDIP(18));
        //m_filter_button->SetFont(wxFont(wxFontInfo(16)));
        m_filter_button->SetBorderWidth(0);
        m_filter_button->SetBackgroundColor(wxColour(255,255,255));
        m_filter_button->SetBorderColor(wxColour(255,255,255));
        //m_filter_button->SetTextColor(wxColour(51,51,51));
        m_filter_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
        m_filter_button->SetCornerRadius(0);
        //bSizer_control_lamp->Add(m_filter_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
        bSizer_control_lamp->Add(m_filter_button, wxSizerFlags(1).Expand());

//***灯控制布局添加至垂直布局
        m_panel_control_lamp->SetSizer(bSizer_control_lamp);
        m_panel_control_lamp->Layout();
        bSizer_control_lamp->Fit(m_panel_control_lamp);

        busySizer->Add(m_panel_control_lamp,0, wxALL | wxEXPAND, 0);

//**灯控制布局和设备详情之间间隔
        auto m_panel_separotor7 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor7->SetBackgroundColour(wxColour(240,240,240));
        m_panel_separotor7->SetMinSize(wxSize(-1, FromDIP(21)));
        m_panel_separotor7->SetMaxSize(wxSize(-1, FromDIP(21)));
        busySizer->Add(m_panel_separotor7, 0, wxEXPAND, 0);

//添加设备详情
        m_busy_device_detial = new DeviceDetail(parent);
        m_busy_device_detial->Bind(EVT_SWITCH_TO_FILETER, [this](wxCommandEvent &event) {
         event.Skip();
            m_busy_device_detial->switchPage();
        });
        busySizer->Add(m_busy_device_detial, 0, wxALL, 0);
        m_busy_device_detial->Hide();
//添加循环过滤
        m_busy_circula_filter = new StartFilter(parent);
        busySizer->Add(m_busy_circula_filter, 0, wxALL, 0);
        m_busy_circula_filter->Hide();
//添加温度修改确认页面
        m_busy_temp_brn = new ModifyTemp(parent);
        m_busy_temp_brn->Bind(EVT_MODIFY_TEMP_CLICKED, &SingleDeviceState::onModifyTempClicked,this);
        m_busy_temp_brn->Bind(EVT_MODIFY_TEMP_CANCEL_CLICKED, [this](wxCommandEvent &event) {
            event.Skip();
            m_busy_temp_brn->Hide();
            m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            m_tempCtrl_top->SetTagTemp(m_right_target_temp, true);
            m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this); 
            m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            m_tempCtrl_bottom->SetTagTemp(m_plat_target_temp, true);
            m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);
            Layout();
        });
        busySizer->Add(m_busy_temp_brn, 0, wxALL | wxEXPAND, 0);
        m_busy_temp_brn->Hide();
}

void SingleDeviceState::setupLayoutIdlePage(wxBoxSizer* idleSizer,wxPanel* parent)
{
        //标题：信息与控制
        m_panel_control_title2 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
        m_panel_control_title2->SetBackgroundColour(wxColour(248,248,248));

        wxBoxSizer *bSizer_control_title = new wxBoxSizer(wxHORIZONTAL);
        m_staticText_control2             = new Label(m_panel_control_title2,_L("Info and Control"));
        m_staticText_control2->Wrap(-1);
        m_staticText_control2->SetForegroundColour(wxColour(51,51,51));

        bSizer_control_title->Add(m_staticText_control2, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
        bSizer_control_title->AddStretchSpacer();

        m_panel_control_title2->SetSizer(bSizer_control_title);
        m_panel_control_title2->Layout();
        bSizer_control_title->Fit(m_panel_control_title2);

        //添加标题
        idleSizer->Add(m_panel_control_title2, 0, wxALL | wxEXPAND, 0);

        //添加空白间距
        auto m_panel_separotor0 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor0->SetBackgroundColour(wxColour(255,255,255));
        m_panel_separotor0->SetMinSize(wxSize(-1, FromDIP(14)));

        idleSizer->Add(m_panel_separotor0, 0, wxALL | wxEXPAND, 0);

        //水平布局，机器图 + 文字
        wxBoxSizer *bSizer_h_device_tip = new wxBoxSizer(wxHORIZONTAL);
        wxBoxSizer *bSizer_v_device_text = new wxBoxSizer(wxVERTICAL);
        m_panel_idle = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_idle->SetBackgroundColour(*wxWHITE);
        //m_panel_idle->SetMinSize(wxSize(-1,FromDIP(178)));
        
        //m_idle_device_pic = create_scaled_bitmap("adventurer_5m", this, 112);
        m_idle_device_pic = create_scaled_bitmap("adventurer_5m", 0, 165);
        m_idle_device_staticbitmap = new wxStaticBitmap(m_panel_idle, wxID_ANY, m_idle_device_pic);
        m_staticText_idle = new Label(m_panel_idle, _L("The Current Device has no Printing Projects"));
        m_staticText_idle->Wrap(-1);
        m_staticText_idle->SetForegroundColour(wxColour(51,51,51));
        m_staticText_idle->SetBackgroundColour(wxColour(255,255,255));
        m_staticText_idle->SetWindowStyleFlag(wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL); 

        bSizer_h_device_tip->Add(m_idle_device_staticbitmap,0, wxALL | wxEXPAND, 0);
        bSizer_v_device_text->Add(m_staticText_idle,0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 0);
        bSizer_h_device_tip->Add(bSizer_v_device_text,0,wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL);
        bSizer_h_device_tip->AddStretchSpacer();

        m_panel_idle->SetSizer(bSizer_h_device_tip);
        m_panel_idle->Layout();
        bSizer_h_device_tip->Fit(m_panel_idle);

        idleSizer->Add(m_panel_idle, 0, wxALL | wxEXPAND, 0);

        //添加空白间距
        auto m_panel_separotor1 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor1->SetBackgroundColour(wxColour(255,255,255));
        m_panel_separotor1->SetMinSize(wxSize(-1, FromDIP(44)));

        idleSizer->Add(m_panel_separotor1, 0, wxALL | wxEXPAND, 0);

//*** 设备空闲和文件列表间距
        //添加空白间距
        auto m_panel_separotor2 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor2->SetBackgroundColour(wxColour(240,240,240));
        m_panel_separotor2->SetMinSize(wxSize(-1, FromDIP(16)));

        idleSizer->Add(m_panel_separotor2, 0, wxALL | wxEXPAND, 0);

#if 0
        //垂直布局：文件列表
        wxBoxSizer *bSizer_v_device_file_list = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *bSizer_h_device_file_list = new wxBoxSizer(wxHORIZONTAL);
        m_panel_idle_text = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_idle_text->SetBackgroundColour(*wxWHITE);
//        m_panel_idle_text->SetMinSize(wxSize(FromDIP(400),-1));
        m_panel_idle_text->SetSizer(bSizer_h_device_file_list);

        m_idle_file_list_pic = create_scaled_bitmap("local_file_list", m_panel_idle_text, 18);
        m_idle_file_list_staticbitmap = new wxStaticBitmap(m_panel_idle_text, wxID_ANY, m_idle_file_list_pic);

        m_staticText_file_list = new Label(m_panel_idle_text, _L("Local File List"));
        //m_staticText_file_list->Wrap(-1);
        m_staticText_file_list->SetFont(wxFont(wxFontInfo(13)));
        m_staticText_file_list->SetForegroundColour(wxColour(51,51,51));
        m_staticText_file_list->SetBackgroundColour(wxColour(255,255,255));
        //m_staticText_file_list->SetWindowStyleFlag(wxALIGN_CENTER_VERTICAL); 

        bSizer_h_device_file_list->AddSpacer(FromDIP(123));
        bSizer_h_device_file_list->Add(m_idle_file_list_staticbitmap,0, wxALL | wxEXPAND |wxALIGN_CENTER | wxBOTTOM | wxTop, FromDIP(8));
        //bSizer_h_device_file_list->AddSpacer(FromDIP(4));
        bSizer_h_device_file_list->Add(m_staticText_file_list,0, wxALL | wxEXPAND |wxALIGN_CENTER | wxBOTTOM | wxTop, FromDIP(8));

 //       m_panel_idle_text->SetSizer(bSizer_h_device_file_list);
        m_panel_idle_text->Layout();
        bSizer_h_device_file_list->Fit(m_panel_idle_text);

        bSizer_v_device_file_list->Add(m_panel_idle_text,0, wxALL | wxEXPAND | wxALIGN_CENTER, 0);


        idleSizer->Add(bSizer_v_device_file_list, 0, wxALL | wxEXPAND, 0);

//*** 文件列表和设备状态间距
        //添加空白间距
        auto m_panel_separotor3 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        m_panel_separotor3->SetBackgroundColour(wxColour(240,240,240));
        m_panel_separotor3->SetMinSize(wxSize(-1, FromDIP(6)));

        idleSizer->Add(m_panel_separotor3,0, wxALL | wxEXPAND, 0);
#endif
//新增温度-设备控件
        m_idle_tempMixDevice = new TempMixDevice(parent,false);
        idleSizer->Add(m_idle_tempMixDevice, 0, wxALL | wxEXPAND , 0);
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
#if 0
//local file list
   m_staticText_file_list->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e){
        if(m_idle_tempMixDevice && m_idle_tempMixDevice->IsShown()){
            m_idle_tempMixDevice->Hide();
        }
        else if(m_idle_tempMixDevice && !m_idle_tempMixDevice->IsShown()){
            m_idle_tempMixDevice->Show();
        }  
   });
#endif
//busy button slot
   m_device_info_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e){
       //m_device_info_button->SetIcon("device_idle_file_info");
       m_device_info_button->Refresh();
        if(m_busy_device_detial){
            m_busy_device_detial->Show();
            m_busy_device_detial->Show(m_busy_device_detial->IsShown());
            m_busy_device_detial->Layout();
            m_device_info_button->SetBackgroundColor(wxColour(217,234,255));
        }
        if (m_busy_circula_filter) {
            m_busy_circula_filter->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if(m_filter_button){
            m_filter_button->SetBackgroundColor(wxColour(255,255,255));
        }
        Layout();
   });

   m_filter_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e){
       wxCommandEvent *event = new wxCommandEvent(EVT_SWITCH_TO_FILETER);
       wxQueueEvent(m_busy_device_detial, event);
        if(m_busy_circula_filter){
            m_busy_circula_filter->Show();
            m_busy_circula_filter->Show(m_busy_circula_filter->IsShown());
            m_busy_circula_filter->Layout();
            m_filter_button->SetBackgroundColor(wxColour(217,234,255));
        }
        if(m_busy_device_detial){
            m_busy_device_detial->Hide();
        }
        if (m_busy_temp_brn) {
            m_busy_temp_brn->Hide();
        }
        if(m_device_info_button){
            m_device_info_button->SetBackgroundColor(wxColour(255,255,255));
        }
        Layout();
   });

   m_lamp_control_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
       if (m_lamp_control_button->GetFlashForgeSelected()) {
           // 关灯
           ComLightCtrl *lightctrl = new ComLightCtrl(CLOSE);
           Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
           m_lamp_control_button->SetIcon("device_lamp_control");
           m_lamp_control_button->Refresh();
           m_lamp_control_button->SetFlashForgeSelected(false);
       } else {
           //开灯
           ComLightCtrl *lightctrl = new ComLightCtrl(OPEN);
           Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
           m_lamp_control_button->SetIcon("device_lamp_control_press");
           m_lamp_control_button->Refresh();
           m_lamp_control_button->SetFlashForgeSelected(true);
       }
   });
}

void SingleDeviceState::on_navigated(wxWebViewEvent &event) 
{
   wxString strInput = event.GetString();
}

void SingleDeviceState::onConnectWanDevInfoUpdate(ComWanDevInfoUpdateEvent &event) 
{
    event.Skip();
    if (m_cur_id == event.id) {
        //当前选中的设备，获取相应数据，更新界面显示
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
        // 离线判断
        std::string status = data.wanDevInfo.status;
        if (status.compare("offline") == 0) {
            setPageOffline();
        } else {
            fillValue(data);
        }
        return;
    }
    
    if (-1 == m_cur_id) {
        const com_dev_data_t &data = MultiComMgr::inst()->devData(event.id);
        if (data.wanDevInfo.serialNumber.compare(m_cur_serial_number) == 0) {
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
        // 当前选中的设备，获取相应数据，更新界面显示
        bool  valid = false;
        const com_dev_data_t& data  = MultiComMgr::inst()->devData(m_cur_id, &valid);
        fillValue(data);
   }
}

void SingleDeviceState::onComConnectReady(ComConnectionReadyEvent &event) 
{
   event.Skip();
   if (-1 == m_cur_id) {
        const com_dev_data_t &data  = MultiComMgr::inst()->devData(event.id);
        std::string           lan_serial_number = data.lanDevInfo.serialNumber;
        if (lan_serial_number.compare(m_cur_serial_number) == 0) {
            m_cur_id = event.id;
            setCurId(m_cur_id);
        }
        return;
   }
}

void SingleDeviceState::onConnectExit(ComConnectionExitEvent &event) 
{ 
    event.Skip(); 
    if (event.id == m_cur_id) {
        setPageOffline();
    }
}

void SingleDeviceState::onTargetTempModify(wxCommandEvent &event) 
{
   event.Skip();
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
   }
}

void SingleDeviceState::onModifyTempClicked(wxCommandEvent &event) 
{
    event.Skip();
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
    Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
}

void SingleDeviceState::onDevStateChanged(std::string devState, const com_dev_data_t &data)
{
    std::string state = data.devDetail->status; // 状态
    setDevProductAuthority(*data.devProduct);
    //if (m_cur_dev_state != state) {
        m_cur_dev_state = state;

        double total_weight = data.devDetail->estimatedRightWeight; //材料重量
        char   weight[64];
        ::sprintf(weight, "  %.2f g", total_weight);
        m_material_weight_label->SetLabel(weight);

        if (state == P_READY) {
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            std::string idle_state = _L("idle").ToStdString();
            setTipMessage(idle_state, "#00CD6D", "", false);
            m_idle_tempMixDevice->setState(1);
            m_staticText_idle->SetLabel(_L("The Current Device has no Printing Projects"));
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        } else if (state == P_COMPLETED) {
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            m_print_button->SetTextColor(wxColor("#999999"));
            m_cancel_button->SetTextColor(wxColor("#999999"));
            m_print_button->Enable(false);
            m_cancel_button->Enable(false);
            m_print_button->SetIcon("device_pause_print_disable");
            m_cancel_button->SetIcon("device_cancel_print_disable");
            std::string compelete_state = _L("completed").ToStdString();
            std::string compelete_info  = _L("Print completed,clean platform!").ToStdString();
            setTipMessage(compelete_state, "#328DFB", compelete_info, true);

            m_staticText_time_label->SetLabel(_L("Total Time"));

            double totalTime = data.devDetail->printDuration; // 本次打印耗时
            m_staticText_count_time->SetLabel(convertSecondsToHMS(totalTime));
        } else if (state == P_BUSY) {
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            std::string busy_state = _L("busy").ToStdString();
            std::string busy_info  = _L("Print cancelled,in cache command").ToStdString();
            setTipMessage(busy_state, "#F9B61C", busy_info, false);
            m_idle_tempMixDevice->setState(1);
            m_staticText_idle->SetLabel(_L("The Current Device has no Printing Projects"));
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        } else if (state == P_CALIBRATE) {
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            std::string busy_state = _L("busy").ToStdString();
            std::string busy_info  = _L("").ToStdString();
            setTipMessage(busy_state, "#F9B61C", busy_info, false);
            m_idle_tempMixDevice->setState(1);
            m_staticText_idle->SetLabel(_L("The Current Device has no Printing Projects"));
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
         } else if (state == P_ERROR) {
            m_machine_idle_panel->Show();
            m_machine_ctrl_panel->Hide();
            std::string error_state = _L("error").ToStdString();
            std::string error_info  = data.devDetail->errorCode;
            wxString trans_error = FFUtils::converDeviceError(error_info);
            setTipMessage(error_state, "#FB4747", trans_error.ToStdString(), true);
            m_idle_tempMixDevice->setDevProductAuthority(*data.devProduct);
        } else if (state == PAUSE) {
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            std::string print_state = _L("pause").ToStdString();
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
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            std::string print_state = _L("pausing").ToStdString();
            if (state == P_HEATING) {
                print_state = _L("heating").ToStdString();
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
        }else{
            m_machine_ctrl_panel->Show();
            m_machine_idle_panel->Hide();
            std::string print_state = _L("printing").ToStdString();
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
        Layout();
    //}
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

void SingleDeviceState::setTipMessage(const std::string& title, const std::string& titleColor,const std::string& info,bool showInfo)
{
   m_staticText_device_tip->SetLabel(title); 
   m_staticText_device_tip->SetForegroundColour(wxColour(titleColor));
   m_staticText_device_info->SetLabel(info);
   m_staticText_device_info->SetMinSize(wxSize(FromDIP(394), FromDIP(56)));
   if (!showInfo) {
        m_staticText_device_info->Hide();
        m_clear_button->Hide();
   } else {
        m_staticText_device_info->Show();
        m_clear_button->Show();
   }
   Layout();
}

std::string SingleDeviceState::convertSecondsToHMS(int totalSeconds)
{
   int hours   = totalSeconds / 3600;
   int remainingSeconds = totalSeconds % 3600;
   int minutes          = remainingSeconds / 60;
   int secs             = remainingSeconds % 60; 

   std::ostringstream stream;  
   stream << std::setfill('0') << std::setw(1) << hours << _L("h ") << std::setfill('0') << std::setw(2) << minutes << _L("min ");  
   return stream.str();  
}

void SingleDeviceState::fillValue(const com_dev_data_t &data)
{
   std::string state = data.devDetail->status; // 状态
   onDevStateChanged(state, data);
   if (state.compare("printing") == 0) {
        double estimatedTime = data.devDetail->estimatedTime; // 剩余时间
        m_staticText_count_time->SetLabel(convertSecondsToHMS(estimatedTime));
   }

   std::string stram_url = data.devDetail->cameraStreamUrl;
   if (!stram_url.empty() && m_camera_stream_url != data.devDetail->cameraStreamUrl) {
       if (0 == data.connectMode) {
            // 通知设备开流
            ComCameraStreamCtrl *cameraStreamCtrl = new ComCameraStreamCtrl(OPEN);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, cameraStreamCtrl);
        }

        m_camera_stream_url = data.devDetail->cameraStreamUrl;
        modifyVideoPlayerAddress(m_camera_stream_url);  
   } else if (stram_url.empty()) {
        notifyWebDevOffline();
   }
   std::string device_name = data.devDetail->name;  //设备名
   if (m_cur_dev_name != device_name && !device_name.empty()) {
        m_staticText_device_name->SetLabel(wxString::FromUTF8(device_name));
   }

   std::string device_location = data.devDetail->location;//位置
   if (m_cur_dev_location != device_location && !device_location.empty()) {
        m_staticText_device_position->SetLabel(device_location);
   } 

   std::string printFileName = data.devDetail->printFileName; // 文件名
   if (m_cur_print_file_name != printFileName && !printFileName.empty()) {
        m_cur_print_file_name       = printFileName;
        std::string truncatedString = FFUtils::truncateString(printFileName, TEXT_LENGTH);
        m_staticText_file_name->SetLabel(truncatedString);
        m_staticText_file_name->SetToolTip(wxString::FromUTF8(printFileName));
        m_staticText_file_name->Show();
        m_staticText_file_name->Layout();
        Layout();
   }

   std::string file_pic_path = data.devDetail->printFileThumbUrl;// 图片地址
   if (m_file_pic_url != file_pic_path && !file_pic_path.empty()) {
        m_file_pic_url = file_pic_path; 
        Bind(COM_ASYNC_CALL_FINISH_EVENT, [&](ComAsyncCallFinishEvent &event) {
            //event.Skip();
            if (event.ret == COM_OK) {
                if (!m_pic_data.empty()) {
                    //translate pic data from vector to wxImage object
                    wxMemoryInputStream stream(m_pic_data.data(), m_pic_data.size());
                    wxImage image(stream, wxBITMAP_TYPE_ANY);
                    image.Rescale(MATERIAL_PIC_WIDTH, MATERIAL_PIC_HEIGHT);
                    //translate pic data  from wxImage object to wxBitmap object
                    if (m_last_pic_data != m_pic_data) {
                        bool equal = false;
                    }
                    if (m_last_pic_data != m_pic_data) {
                        m_last_pic_data = m_pic_data;
                        if (m_material_image) {
                            delete m_material_image;
                            m_material_image = nullptr;
                        }
                        m_material_image = new wxImage(image);
                        m_material_picture->SetImage(*m_material_image);
                    } 
                }
            } else {
                m_file_pic_url.clear();
            }
        });
        m_pic_thread = MultiComUtils::asyncCall(this, [&]() {
            return MultiComUtils::downloadFile(m_file_pic_url, m_pic_data, 15000);
        });     
   }

   double printProgress = data.devDetail->printProgress; // 打印进度
   m_progress_bar->SetProgress(printProgress * 100);

   double rightTemp = data.devDetail->rightTemp; // 右喷头温度
   //m_tempCtrl_top->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);  
   m_tempCtrl_top->SetCurrTemp(rightTemp,true);
   double rightTargetTemp = data.devDetail->rightTargetTemp; // 右喷头目标温度
   if (m_right_target_temp != rightTargetTemp) {
        m_right_target_temp = rightTargetTemp;
        m_tempCtrl_top->SetTagTemp(rightTargetTemp, true);
   }
   //m_tempCtrl_top->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);  
   double platTemp = data.devDetail->platTemp; // 平台温度
   //m_tempCtrl_bottom->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this); 
   m_tempCtrl_bottom->SetCurrTemp(platTemp, true);
   double platTargetTemp = data.devDetail->platTargetTemp; // 平台目标温度
   if (m_plat_target_temp != platTargetTemp) {
        m_plat_target_temp = platTargetTemp;
        m_tempCtrl_bottom->SetTagTemp(platTargetTemp, true);
   }
   //m_tempCtrl_bottom->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);  
   double chamberTemp = data.devDetail->chamberTemp; // 腔体温度
   //m_tempCtrl_mid->Unbind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this); 
   m_tempCtrl_mid->SetCurrTemp(chamberTemp, true);
   double chamberTargetTemp = data.devDetail->chamberTargetTemp; // 腔体目标物温度
   m_tempCtrl_mid->SetTagTemp(chamberTargetTemp, true);
   //m_tempCtrl_mid->Bind(wxEVT_TEXT, &SingleDeviceState::onTargetTempModify, this);  

   auto aRightTemp = static_cast<int>(rightTemp);
   auto aPlatTemp  = static_cast<int>(platTemp);
   auto aChamberTemp = static_cast<int>(chamberTemp);
   wxString modify_nozzle_temp  = wxString::Format("%d", aRightTemp);
   wxString modify_plat_temp    = wxString::Format("%d", aPlatTemp);
   wxString modify_chamber_temp = wxString::Format("%d", aChamberTemp);

   m_idle_tempMixDevice->modifyTemp(modify_nozzle_temp, modify_plat_temp, modify_chamber_temp, rightTargetTemp, platTargetTemp,
                                    chamberTargetTemp);
   std::string lightStatus = data.devDetail->lightStatus; // 灯状态
   if (data.devProduct->lightCtrlState == 1 && lightStatus.compare(CLOSE) == 0) {
        m_lamp_control_button->SetIcon("device_lamp_control");
        m_lamp_control_button->Refresh();
        m_lamp_control_button->SetFlashForgeSelected(false);
        m_idle_tempMixDevice->modifyDeviceLampState(false);
   } else if (data.devProduct->lightCtrlState == 1 && lightStatus.compare(OPEN) == 0) {
        m_lamp_control_button->SetIcon("device_lamp_control_press");
        m_lamp_control_button->Refresh();
        m_lamp_control_button->SetFlashForgeSelected(true);
        m_idle_tempMixDevice->modifyDeviceLampState(true);
   }
   std::string internalFanStatus = data.devDetail->internalFanStatus; // 内循环状态
   bool        internal_open     = internalFanStatus.compare(OPEN) ? false: true;
   std::string externalFanStatus = data.devDetail->externalFanStatus; // 外循环状态
   bool        external_open     = externalFanStatus.compare(OPEN) ? false: true;
   m_busy_circula_filter->setAirFilterState(internal_open, external_open);
   m_idle_tempMixDevice->modifyDeviceFilterState(internal_open, external_open);
   std::string rightFilamentType = data.devDetail->rightFilamentType; // 材料类型
   m_busy_device_detial->setMaterialName(rightFilamentType);
   double currentPrintSpeed = data.devDetail->currentPrintSpeed; // 初始打印速度
   m_busy_device_detial->setInitialSpeed(currentPrintSpeed);
   double printSpeedAdjust = data.devDetail->printSpeedAdjust;// 速度
   if (data.devDetail->printSpeedAdjust != m_last_speed) {
        m_last_speed = data.devDetail->printSpeedAdjust;
        m_busy_device_detial->setSpeed(printSpeedAdjust);
   }
   double zAxisCompensation = data.devDetail->zAxisCompensation; // z轴坐标
   if (data.devDetail->zAxisCompensation != m_last_z_axis_compensation) {
        m_last_z_axis_compensation = data.devDetail->zAxisCompensation;
        m_busy_device_detial->setZAxis(zAxisCompensation);
   }
   int printLayer       = data.devDetail->printLayer;       // 当前层
   int targetPrintLayer = data.devDetail->targetPrintLayer; // 目标层数
   m_busy_device_detial->setLayer(printLayer, targetPrintLayer);
   double fillAmount = data.devDetail->fillAmount; // 填充率
   m_busy_device_detial->setFillRate(fillAmount);
   double coolingFanSpeed = data.devDetail->coolingFanSpeed; // 喷头风扇
   if (data.devDetail->coolingFanSpeed != m_last_cooling_fan_speed) {
        m_last_cooling_fan_speed = data.devDetail->coolingFanSpeed;
       m_busy_device_detial->setCoolingFanSpeed(coolingFanSpeed);
   }
   double chamberFanSpeed = data.devDetail->chamberFanSpeed; // 冷却风扇（腔体风扇）
   if (data.devDetail->chamberFanSpeed != m_last_chamber_fan_speed) {
       m_last_chamber_fan_speed = data.devDetail->chamberFanSpeed;
       m_busy_device_detial->setChamberFanSpeed(chamberFanSpeed);
   } 

   if (m_pid != data.devDetail->pid && data.devDetail->pid == 0x0024) {
       m_pid = data.devDetail->pid;
       m_idle_device_staticbitmap->SetBitmap(create_scaled_bitmap("adventurer_5m_pro", 0, 165));
   } else if (m_pid != data.devDetail->pid && data.devDetail->pid == 0x0023) {
       m_pid = data.devDetail->pid;
       m_idle_device_staticbitmap->SetBitmap(create_scaled_bitmap("adventurer_5m", 0, 165));
   }

   std::string machineType = data.devDetail->pid == 0x0024 ? "Adventurer 5M Pro" :(data.devDetail->pid == 0x0023 ? "Adventurer 5M" : "Unknow");
   std::string nozzleModel = data.devDetail->nozzleModel;//喷嘴型号
   std::string measure     = data.devDetail->measure;//打印尺寸
   measure.append("mm");
   std::string firmwareVersion = data.devDetail->firmwareVersion;//固件版本
   std::string serialNubmer = data.connectMode == 0 ? data.lanDevInfo.serialNumber : data.wanDevInfo.serialNumber; //序列号
   double cumulativeFilament = data.devDetail->cumulativeFilament; //丝料统计
   wxString strCumulativeFilament = wxString::Format("%.2f", cumulativeFilament);
   strCumulativeFilament.append("m");

   m_idle_tempMixDevice->modifyDeviceInfo(machineType, nozzleModel, measure, firmwareVersion, serialNubmer, strCumulativeFilament);
}

void SingleDeviceState::setPageOffline() 
{
   // 离线
   m_cur_id = -1;
   m_machine_idle_panel->Show();
   m_machine_ctrl_panel->Hide();
   notifyWebDevOffline();
   reInit();
}

std::string SingleDeviceState::getCurLanguage() 
{
    AppConfig *app_config = wxGetApp().app_config; 
    return  app_config->get("language");
}

void SingleDeviceState::onScriptMessage(wxWebViewEvent &evt)
{
   wxString          strInput = evt.GetString();
   std::string       cmd      = evt.GetString().ToUTF8().data();
   std::stringstream ss(cmd), oss;
   pt::ptree         root, response;
   pt::read_json(ss, root);
   if (root.empty())
            return;

   boost::optional<std::string> sequence_id = root.get_optional<std::string>("sequence_id");
   boost::optional<std::string> command     = root.get_optional<std::string>("command");
   if (command.has_value()) {
       std::string command_str = command.value();
       if (command_str.compare("rtsp_player_continue") == 0) {
            // 外网视频继续播放
            ComCameraStreamCtrl *cameraStreamCtrl = new ComCameraStreamCtrl(OPEN);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, cameraStreamCtrl);
       }
   }
}

}
}
