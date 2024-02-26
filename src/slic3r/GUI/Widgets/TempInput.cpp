#include "TempInput.hpp"
#include "Label.hpp"
#include "PopupWindow.hpp"
#include "../I18N.hpp"
#include <wx/dcgraph.h>
#include "../GUI.hpp"
#include "../GUI_App.hpp"

wxDEFINE_EVENT(wxCUSTOMEVT_SET_TEMP_FINISH, wxCommandEvent);

BEGIN_EVENT_TABLE(TempInput, wxPanel)
EVT_MOTION(TempInput::mouseMoved)
EVT_ENTER_WINDOW(TempInput::mouseEnterWindow)
EVT_LEAVE_WINDOW(TempInput::mouseLeaveWindow)
EVT_KEY_DOWN(TempInput::keyPressed)
EVT_KEY_UP(TempInput::keyReleased)
EVT_MOUSEWHEEL(TempInput::mouseWheelMoved)
EVT_PAINT(TempInput::paintEvent)
END_EVENT_TABLE()


TempInput::TempInput()
    : label_color(std::make_pair(wxColour(0xAC,0xAC,0xAC), (int) StateColor::Disabled),std::make_pair(0x323A3C, (int) StateColor::Normal))
    , text_color(std::make_pair(wxColour(0xAC,0xAC,0xAC), (int) StateColor::Disabled), std::make_pair(0x6B6B6B, (int) StateColor::Normal))
{
    hover  = false;
    radius = 0;
    border_color = StateColor(std::make_pair(*wxWHITE, (int) StateColor::Disabled), std::make_pair(0x009688, (int) StateColor::Focused), std::make_pair(0x009688, (int) StateColor::Hovered),
                 std::make_pair(*wxWHITE, (int) StateColor::Normal));
    background_color = StateColor(std::make_pair(*wxWHITE, (int) StateColor::Disabled), std::make_pair(*wxWHITE, (int) StateColor::Normal));
    SetFont(Label::Body_12);
}

TempInput::TempInput(wxWindow *parent, int type, wxString text, wxString label, wxString normal_icon, wxString actice_icon, const wxPoint &pos, const wxSize &size, long style)
    : TempInput()
{
    actice = false;
    temp_type = type;
    Create(parent, text, label, normal_icon, actice_icon, pos, size, style);
}

void TempInput::Create(wxWindow *parent, wxString text, wxString label, wxString normal_icon, wxString actice_icon, const wxPoint &pos, const wxSize &size, long style)
{
    StaticBox::Create(parent, wxID_ANY, pos, size, style);
    wxWindow::SetLabel(label);
    style &= ~wxALIGN_CENTER_HORIZONTAL;
    state_handler.attach({&label_color, &text_color});
    state_handler.update_binds();
    text_ctrl = new wxTextCtrl(this, wxID_ANY, text, {5, 5}, wxDefaultSize, wxTE_PROCESS_ENTER | wxBORDER_NONE, wxTextValidator(wxFILTER_NUMERIC), wxTextCtrlNameStr);
    text_ctrl->SetBackgroundColour(StateColor::darkModeColorFor(*wxWHITE));
    text_ctrl->SetMaxLength(3);
    state_handler.attach_child(text_ctrl);
    text_ctrl->Bind(wxEVT_SET_FOCUS, [this](auto &e) {
        e.SetId(GetId());
        ProcessEventLocally(e);
        e.Skip();
        if (m_read_only) return;
        // enter input mode
        auto temp = text_ctrl->GetValue();
        if (temp.length() > 0 && temp[0] == (0x5f)) { 
            text_ctrl->SetValue(wxEmptyString);
        }
        if (wdialog != nullptr) { wdialog->Dismiss(); }
    });
    text_ctrl->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) {
        if (m_read_only) { SetCursor(wxCURSOR_ARROW); }
    });
    text_ctrl->Bind(wxEVT_KILL_FOCUS, [this](auto &e) {
        e.SetId(GetId());
        ProcessEventLocally(e);
        e.Skip();
        OnEdit();
        auto temp = text_ctrl->GetValue();
        if (temp.ToStdString().empty()) {
            text_ctrl->SetValue(wxString("_"));
            return;
        }

        if (!AllisNum(temp.ToStdString())) return;
        if (max_temp <= 0) return;

       /* auto tempint = std::stoi(temp.ToStdString());
         if ((tempint > max_temp || tempint < min_temp) && !warning_mode) {
             if (tempint > max_temp)
                 Warning(true, WARNING_TOO_HIGH);
             else if (tempint < min_temp)
                 Warning(true, WARNING_TOO_LOW);
             return;
         } else {
             Warning(false);
         }*/
        SetFinish();
    });
    text_ctrl->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &e) {
        OnEdit();
        auto temp = text_ctrl->GetValue();
        if (temp.ToStdString().empty()) return;
        if (!AllisNum(temp.ToStdString())) return;
        if (max_temp <= 0) return;

        auto tempint = std::stoi(temp.ToStdString());
        if (tempint > max_temp) {
            Warning(true, WARNING_TOO_HIGH);
            return;
        } else {
            Warning(false, WARNING_TOO_LOW);
        }
        SetFinish();
        Slic3r::GUI::wxGetApp().GetMainTopWindow()->SetFocus();
    });
    text_ctrl->Bind(wxEVT_RIGHT_DOWN, [this](auto &e) {}); // disable context menu
    text_ctrl->Bind(wxEVT_LEFT_DOWN, [this](auto &e) {
        if (m_read_only) { 
            return;
        } else {
            e.Skip();
        }
    });
    text_ctrl->SetFont(Label::Body_13);
    text_ctrl->SetForegroundColour(text_color.colorForStates(StateColor::Normal));
    if (!normal_icon.IsEmpty()) { this->normal_icon = ScalableBitmap(this, normal_icon.ToStdString(), 16); }
    if (!actice_icon.IsEmpty()) { this->actice_icon = ScalableBitmap(this, actice_icon.ToStdString(), 16); }
    this->degree_icon = ScalableBitmap(this, "degree", 16);
    messureSize();
}


bool TempInput::AllisNum(std::string str)
{
    for (int i = 0; i < str.size(); i++) {
        int tmp = (int) str[i];
        if (tmp >= 48 && tmp <= 57) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

void TempInput::SetFinish()
{
    wxCommandEvent event(wxCUSTOMEVT_SET_TEMP_FINISH);
    event.SetInt(temp_type);
    wxPostEvent(this->GetParent(), event);
}

wxString TempInput::erasePending(wxString &str)
{
    wxString tmp   = str;
    int      index = tmp.size() - 1;
    while (index != -1) {
        if (tmp[index] < '0' || tmp[index] > '9') {
            tmp.erase(index, 1);
            index--;
        } else {
            break;
        }
    }
    return tmp;
}

void TempInput::SetTagTemp(int temp)
{
    text_ctrl->SetValue(wxString::Format("%d", temp));
    messureSize();
    Refresh();
}

void TempInput::SetTagTemp(wxString temp) 
{ 
    text_ctrl->SetValue(temp);
    messureSize();
    Refresh();
}

void TempInput::SetCurrTemp(int temp) 
{ 
    SetLabel(wxString::Format("%d", temp)); 
}

void TempInput::SetCurrTemp(wxString temp) 
{
    SetLabel(temp);
}

void TempInput::Warning(bool warn, WarningType type)
{
    warning_mode = warn;
    //Refresh();

    if (warning_mode) {
        if (wdialog == nullptr) {
            wdialog = new PopupWindow(this);
            wdialog->SetBackgroundColour(wxColour(0xFFFFFF));

            wdialog->SetSizeHints(wxDefaultSize, wxDefaultSize);

            wxBoxSizer *sizer_body = new wxBoxSizer(wxVERTICAL);

            auto body = new wxPanel(wdialog, wxID_ANY, wxDefaultPosition, {this->GetSize().x - 4, -1}, wxTAB_TRAVERSAL);
            body->SetBackgroundColour(wxColour(0xFFFFFF));


            wxBoxSizer *sizer_text;
            sizer_text = new wxBoxSizer(wxHORIZONTAL);

           

            warning_text = new wxStaticText(body, wxID_ANY, 
                                            wxEmptyString, 
                                            wxDefaultPosition, wxDefaultSize,
                                            wxALIGN_CENTER_HORIZONTAL);
            warning_text->SetFont(::Label::Body_12);
            warning_text->SetForegroundColour(wxColour(255, 111, 0));
            warning_text->Wrap(-1);
            sizer_text->Add(warning_text, 1, wxEXPAND | wxTOP | wxBOTTOM, 2);

            body->SetSizer(sizer_text);
            body->Layout();
            sizer_body->Add(body, 0, wxEXPAND, 0);

            wdialog->SetSizer(sizer_body);
            wdialog->Layout();
            sizer_body->Fit(wdialog);
        }

        wxPoint pos = this->ClientToScreen(wxPoint(2, 0));
        pos.y += this->GetRect().height - (this->GetSize().y - this->text_ctrl->GetSize().y) / 2 - 2;
        wdialog->SetPosition(pos);

        wxString warning_string;
        if (type == WarningType::WARNING_TOO_HIGH)
             warning_string = _L("The maximum temperature cannot exceed" + wxString::Format("%d", max_temp));
        else if (type == WarningType::WARNING_TOO_LOW)
             warning_string = _L("The minmum temperature should not be less than " + wxString::Format("%d", max_temp));

        warning_text->SetLabel(warning_string);
        wdialog->Popup();
    } else {
        if (wdialog)
            wdialog->Dismiss();
    }
}

void TempInput::SetIconActive()
{
    actice = true;
    Refresh();
}

void TempInput::SetIconNormal()
{
    actice = false;
    Refresh();
}

void TempInput::SetMaxTemp(int temp) { max_temp = temp; }

void TempInput::SetMinTemp(int temp) { min_temp = temp; }

void TempInput::SetLabel(const wxString &label)
{
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
}

void TempInput::SetTextColor(StateColor const &color)
{
    text_color = color;
    state_handler.update_binds();
}

void TempInput::SetLabelColor(StateColor const &color)
{
    label_color = color;
    state_handler.update_binds();
}

void TempInput::Rescale()
{
    if (this->normal_icon.bmp().IsOk()) this->normal_icon.msw_rescale();
    if (this->degree_icon.bmp().IsOk()) this->degree_icon.msw_rescale();
    messureSize();
}

bool TempInput::Enable(bool enable)
{
    bool result = wxWindow::Enable(enable);
    if (result) {
        wxCommandEvent e(EVT_ENABLE_CHANGED);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);
    }
    return result;
}

void TempInput::SetMinSize(const wxSize &size)
{
    wxSize size2 = size;
    if (size2.y < 0) {
#ifdef __WXMAC__
        if (GetPeer()) // peer is not ready in Create on mac
#endif
            size2.y = GetSize().y;
    }
    wxWindow::SetMinSize(size2);
    messureMiniSize();
}

void TempInput::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
    if (sizeFlags & wxSIZE_USE_EXISTING) return;

    auto       left = padding_left;
    wxClientDC dc(this);
    if (normal_icon.bmp().IsOk()) {
        wxSize szIcon = normal_icon.GetBmpSize();
        left += szIcon.x;
    }

    // interval
    left += 9;

    // label
    dc.SetFont(::Label::Head_14);
    labelSize = dc.GetMultiLineTextExtent(wxWindow::GetLabel());
    left += labelSize.x;

    // interval
    left += 10;

    // separator
    dc.SetFont(::Label::Body_12);
    auto sepSize = dc.GetMultiLineTextExtent(wxString("/"));
    left += sepSize.x;

    // text text
    auto textSize = text_ctrl->GetTextExtent(wxString("0000"));
    text_ctrl->SetSize(textSize);
    text_ctrl->SetPosition({left, (GetSize().y - text_ctrl->GetSize().y) / 2});
}

void TempInput::DoSetToolTipText(wxString const &tip)
{
    wxWindow::DoSetToolTipText(tip);
    text_ctrl->SetToolTip(tip);
}

void TempInput::paintEvent(wxPaintEvent &evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void TempInput::render(wxDC &dc)
{
    StaticBox::render(dc);
    int    states      = state_handler.states();
    wxSize size        = GetSize();
    bool   align_right = GetWindowStyle() & wxRIGHT;

    if (warning_mode) {
        border_color = wxColour(255, 111, 0);
    } else {
        border_color = StateColor(std::make_pair(*wxWHITE, (int) StateColor::Disabled), std::make_pair(0x009688, (int) StateColor::Focused),
                                  std::make_pair(0x009688, (int) StateColor::Hovered), std::make_pair(*wxWHITE, (int) StateColor::Normal));
    }

    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    // start draw
    wxPoint pt = {padding_left, 0};
    if (actice_icon.bmp().IsOk() && actice) {
        wxSize szIcon = actice_icon.GetBmpSize();
        pt.y          = (size.y - szIcon.y) / 2;
        dc.DrawBitmap(actice_icon.bmp(), pt);
        pt.x += szIcon.x + 9;
    } else {
        actice = false;
    }

    if (normal_icon.bmp().IsOk() && !actice) {
        wxSize szIcon = normal_icon.GetBmpSize();
        pt.y          = (size.y - szIcon.y) / 2;
        dc.DrawBitmap(normal_icon.bmp(), pt);
        pt.x += szIcon.x + 9;
    }

    // label
    auto text = wxWindow::GetLabel();
    dc.SetFont(::Label::Head_14);
    labelSize = dc.GetMultiLineTextExtent(wxWindow::GetLabel());
    
    if (!IsEnabled()) {
        dc.SetTextForeground(wxColour(0xAC, 0xAC, 0xAC));
        dc.SetTextBackground(background_color.colorForStates((int) StateColor::Disabled));
    } 
    else {
        dc.SetTextForeground(wxColour(0x32, 0x3A, 0x3D));
        dc.SetTextBackground(background_color.colorForStates((int) states));
    }
        

    /*if (!text.IsEmpty()) {
        
    }*/
    wxSize textSize = text_ctrl->GetSize();
    if (align_right) {
        if (pt.x + labelSize.x > size.x) text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, size.x - pt.x);
        pt.y = (size.y - labelSize.y) / 2;
    } else {
        pt.y = (size.y - labelSize.y) / 2;
    }

    dc.SetTextForeground(StateColor::darkModeColorFor("#323A3C"));
    dc.DrawText(text, pt);

    // separator
    dc.SetFont(::Label::Body_12);
    auto sepSize = dc.GetMultiLineTextExtent(wxString("/"));
    dc.SetTextForeground(text_color.colorForStates(states));
    dc.SetTextBackground(background_color.colorForStates(states));
    pt.x += labelSize.x + 10;
    pt.y = (size.y - sepSize.y) / 2;
    dc.DrawText(wxString("/"), pt);

    // flag
    if (degree_icon.bmp().IsOk()) {
        auto   pos    = text_ctrl->GetPosition();
        wxSize szIcon = degree_icon.GetBmpSize();
        pt.y          = (size.y - szIcon.y) / 2;
        pt.x          = pos.x + text_ctrl->GetSize().x;
        dc.DrawBitmap(degree_icon.bmp(), pt);
    }
}


void TempInput::messureMiniSize()
{
    wxSize size = GetMinSize();

    auto width  = 0;
    auto height = 0;

    wxClientDC dc(this);
    if (normal_icon.bmp().IsOk()) {
        wxSize szIcon = normal_icon.GetBmpSize();
        width += szIcon.x;
        height = szIcon.y;
    }

    // interval
    width += 9;

    // label
    dc.SetFont(::Label::Head_14);
    labelSize = dc.GetMultiLineTextExtent(wxWindow::GetLabel());
    width += labelSize.x;
    height = labelSize.y > height ? labelSize.y : height;

    // interval
    width += 10;

    // separator
    dc.SetFont(::Label::Body_12);
    auto sepSize = dc.GetMultiLineTextExtent(wxString("/"));
    width += sepSize.x;
    height = sepSize.y > height ? sepSize.y : height;

    // text text
    auto textSize = text_ctrl->GetTextExtent(wxString("0000"));
    width += textSize.x;
    height = textSize.y > height ? textSize.y : height;

    // flag flag
    auto flagSize = degree_icon.GetBmpSize();
    width += flagSize.x;
    height = flagSize.y > height ? flagSize.y : height;

    if (size.x < width) {
        size.x = width;
    } else {
        padding_left = (size.x - width) / 2;
    }

    if (size.y < height) size.y = height;

    SetSize(size);
}


void TempInput::messureSize()
{
    wxSize size = GetSize();

    auto width  = 0;
    auto height = 0;

    wxClientDC dc(this);
    if (normal_icon.bmp().IsOk()) {
        wxSize szIcon = normal_icon.GetBmpSize();
        width += szIcon.x;
        height = szIcon.y;
    }

    // interval
    width += 9;

    // label
    dc.SetFont(::Label::Head_14);
    labelSize = dc.GetMultiLineTextExtent(wxWindow::GetLabel());
    width += labelSize.x;
    height = labelSize.y > height ? labelSize.y : height;

    // interval
    width += 10;

    // separator
    dc.SetFont(::Label::Body_12);
    auto sepSize = dc.GetMultiLineTextExtent(wxString("/"));
    width += sepSize.x;
    height = sepSize.y > height ? sepSize.y : height;

    // text text
    auto textSize = text_ctrl->GetTextExtent(wxString("0000"));
    width += textSize.x;
    height = textSize.y > height ? textSize.y : height;

    // flag flag
    auto flagSize = degree_icon.GetBmpSize();
    width += flagSize.x;
    height = flagSize.y > height ? flagSize.y : height;

    if (size.x < width) {
        size.x = width;
    } else {
        padding_left = (size.x - width) / 2;
    }

    if (size.y < height) size.y = height;

    wxSize minSize = size;
    minSize.x      = GetMinWidth();
    SetMinSize(minSize);
    SetSize(size);
}

void TempInput::mouseEnterWindow(wxMouseEvent &event)
{
    if (!hover) {
        hover = true;
        Refresh();
    }
}

void TempInput::mouseLeaveWindow(wxMouseEvent &event)
{
    if (hover) {
        hover = false;
        Refresh();
    }
}

// currently unused events
void TempInput::mouseMoved(wxMouseEvent &event) {}
void TempInput::mouseWheelMoved(wxMouseEvent &event) {}
void TempInput::keyPressed(wxKeyEvent &event) {}
void TempInput::keyReleased(wxKeyEvent &event) {}

IconText::IconText(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize,const wxPoint &pos,const wxSize & size,long style)
                : wxPanel(parent, wxID_ANY,pos, size, style)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this,icon,iconSize,text,textSize);
}

 IconText::~IconText() 
 { 
 }

void IconText::create_panel(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_page = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxDefaultSize,wxBORDER_NONE);
    //m_panel_page->SetMinSize(wxSize(-1,-1));
    m_icon = create_scaled_bitmap(icon.ToStdString(), parent, iconSize);
    auto icon_static = new wxStaticBitmap(m_panel_page, wxID_ANY, m_icon);
    //icon_static->SetMinSize(wxSize(FromDIP(15),FromDIP(15)));
    //icon_static->SetMinSize(wxSize(47,50));
    icon_static->SetBackgroundColour(*wxWHITE);

    m_text_ctrl = new Label(m_panel_page, text);
    m_text_ctrl->Wrap(-1);
    m_text_ctrl->SetFont(wxFont(wxFontInfo(textSize)));
    m_text_ctrl->SetForegroundColour(wxColour(50, 141, 251));
    m_text_ctrl->SetBackgroundColour(*wxWHITE);
    m_text_ctrl->SetMinSize(wxSize(FromDIP(70), -1));

    //sizer->AddSpacer(FromDIP(10));
    sizer->AddStretchSpacer();
    sizer->Add(icon_static,0, wxALIGN_CENTER | wxALL | wxEXPAND ,0);
    sizer->AddSpacer(FromDIP(5));
    sizer->Add(m_text_ctrl, 0, wxALIGN_CENTER | wxALL | wxEXPAND, 0);
    sizer->AddStretchSpacer();

    m_panel_page->SetSizer(sizer);
    m_panel_page->Layout();
    sizer->Fit(m_panel_page); 

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit(); 
}

void IconText::setText(wxString text)
{
    m_text_ctrl->SetLabel(text);
    Refresh();
}

void IconText::setTextColor(wxColour colour)
{
    m_text_ctrl->SetBackgroundColour(colour);
    Refresh();
}

IconBottonText::IconBottonText(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize,wxString secondIcon,wxString thirdIcon,const wxPoint &pos,const wxSize & size,long style)
                : wxPanel(parent, wxID_ANY,pos, size, style)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this,icon,iconSize,text,textSize,secondIcon,thirdIcon);
}

//IconBottonText::~IconBottonText()
//{
//}

void IconBottonText::create_panel(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize,wxString secondIcon,wxString thirdIcon)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_page = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxDefaultSize,wxBORDER_NONE);
    m_panel_page->SetSize(wxSize(-1,-1));
    m_icon = create_scaled_bitmap(icon.ToStdString(), parent, iconSize);

    text_ctrl = new Label(m_panel_page, text);
    text_ctrl->Wrap(-1);
    text_ctrl->SetFont(wxFont(wxFontInfo(textSize)));
    text_ctrl->SetForegroundColour(wxColour(51,51,51));
    text_ctrl->SetBackgroundColour(wxColour(255,255,255));
    text_ctrl->SetMinSize(wxSize(FromDIP(35),-1));


    auto icon_static = new wxStaticBitmap(m_panel_page, wxID_ANY, m_icon);
    //icon_static->SetMinSize(wxSize(FromDIP(15),FromDIP(15)));

    //auto dec_icon = create_scaled_bitmap(wxString("spin_dec").ToStdString(), parent, iconSize);
    //auto dec_icon_static = new wxStaticBitmap(m_panel_page, wxID_ANY, dec_icon);
    wxString second_icon_name = secondIcon.IsEmpty() ? "device_dec" : secondIcon;
    m_dec_btn = new Button(m_panel_page, "", second_icon_name, wxBORDER_NONE, 16);
    m_dec_btn->SetBackgroundColour(*wxWHITE);

    //auto inc_icon = create_scaled_bitmap(wxString("spin_inc").ToStdString(), parent, iconSize);
    //auto inc_icon_static = new wxStaticBitmap(m_panel_page, wxID_ANY, inc_icon);
    wxString third_icon_name = thirdIcon.IsEmpty() ? "device_inc" : secondIcon;
    m_inc_btn = new Button(m_panel_page, "", third_icon_name, wxBORDER_NONE, 16);
    m_inc_btn->SetBackgroundColour(*wxWHITE);

    sizer->Add(icon_static,0, wxALIGN_CENTER | wxALL | wxEXPAND ,0);
    sizer->AddSpacer(FromDIP(5));
    sizer->Add(m_dec_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
    sizer->AddSpacer(FromDIP(5));
    sizer->Add(text_ctrl,0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND,0);
    sizer->AddSpacer(FromDIP(5));
    sizer->Add(m_inc_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
    //sizer->AddStretchSpacer();

    m_panel_page->SetSizer(sizer);
    m_panel_page->Layout();
    sizer->Fit(m_panel_page); 

    parent->SetSizer(sizer);
    parent->Layout();
    parent->Fit(); 
}

StartFiltering::StartFiltering(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
        SetBackgroundColour(*wxWHITE);
        create_panel(this);
}

//StartFiltering::~StartFiltering()
//{
//}

void StartFiltering::create_panel(wxWindow* parent)
{
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *bSizer_filtering_title = new wxBoxSizer(wxHORIZONTAL);

        auto m_panel_filtering_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
        m_panel_filtering_title->SetBackgroundColour(wxColour(248,248,248));

        //过滤标题
        auto m_staticText_filtering = new wxStaticText(m_panel_filtering_title, wxID_ANY ,("Start Filtering"));
        m_staticText_filtering->Wrap(-1);
        m_staticText_filtering->SetFont(wxFont(wxFontInfo(16)));
        m_staticText_filtering->SetForegroundColour(wxColour(51,51,51));

        bSizer_filtering_title->Add(m_staticText_filtering, 0, wxLEFT, FromDIP(17));
        bSizer_filtering_title->Add(0, 0, 1, wxEXPAND, 0);
        m_panel_filtering_title->SetSizer(bSizer_filtering_title);
        m_panel_filtering_title->Layout();
        bSizer_filtering_title->Fit(m_panel_filtering_title);

        //内循环过滤
        wxBoxSizer *bSizer_internal_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
        wxPanel*    internal_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
        auto m_staticText_internal_circulate = new wxStaticText(internal_circulate_panel, wxID_ANY, ("Internal Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        m_staticText_internal_circulate->Wrap(-1);
        m_staticText_internal_circulate->SetFont(wxFont(wxFontInfo(16)));
        m_internal_circulate_switch = new SwitchButton(internal_circulate_panel);
        m_internal_circulate_switch->SetBackgroundColour(*wxWHITE);

        bSizer_internal_circulate_hor->AddSpacer(FromDIP(17));
        bSizer_internal_circulate_hor->Add(m_staticText_internal_circulate, 0, wxALL | wxEXPAND, 0);
        bSizer_internal_circulate_hor->AddSpacer(FromDIP(7));
        bSizer_internal_circulate_hor->Add(m_internal_circulate_switch, 0, wxALL | wxEXPAND, 0);

        internal_circulate_panel->SetSizer(bSizer_internal_circulate_hor);
        internal_circulate_panel->Layout();
        bSizer_internal_circulate_hor->Fit(internal_circulate_panel);

        //外循环过滤
        wxBoxSizer *bSizer_external_circulate_hor = new wxBoxSizer(wxHORIZONTAL);
        wxPanel*    external_circulate_panel      = new wxPanel(parent, wxID_ANY, wxDefaultPosition,wxDefaultSize, wxTAB_TRAVERSAL);
        auto m_staticText_external_circulate = new wxStaticText(external_circulate_panel, wxID_ANY, ("External Circulate"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        m_staticText_external_circulate->Wrap(-1);
        m_staticText_external_circulate->SetFont(wxFont(wxFontInfo(16)));
        m_external_circulate_switch = new SwitchButton(external_circulate_panel);
        m_external_circulate_switch->SetBackgroundColour(*wxWHITE);

        bSizer_external_circulate_hor->AddSpacer(FromDIP(17));
        bSizer_external_circulate_hor->Add(m_staticText_external_circulate, 0, wxALL | wxEXPAND, 0);
        bSizer_external_circulate_hor->AddSpacer(FromDIP(7));
        bSizer_external_circulate_hor->Add(m_external_circulate_switch, 0, wxALL | wxEXPAND, 0);

        external_circulate_panel->SetSizer(bSizer_external_circulate_hor);
        external_circulate_panel->Layout();
        bSizer_external_circulate_hor->Fit(external_circulate_panel);

        sizer->Add(m_panel_filtering_title, 0, wxEXPAND | wxALL, 0);
        sizer->Add(0, FromDIP(12), 0);
        sizer->Add(internal_circulate_panel, 0, wxEXPAND | wxALIGN_CENTER, 0);
        sizer->AddSpacer(FromDIP(14));
        sizer->Add(external_circulate_panel, 0, wxEXPAND | wxALIGN_CENTER, 0);

        parent->SetSizer(sizer);
        parent->Layout();
        parent->Fit();  
}

TempMixDevice::TempMixDevice(wxWindow* parent,bool idle, wxString nozzleTemp, wxString platformTemp, wxString cavityTemp,const wxPoint &pos,const wxSize & size,long style)
             : wxPanel(parent, wxID_ANY,pos, size, style)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this,idle,nozzleTemp,platformTemp,cavityTemp);
    connectEvent();
}

 //TempMixDevice::~TempMixDevice()
 //{
 //}

void TempMixDevice::create_panel(wxWindow* parent,bool idle, wxString nozzleTemp,wxString platformTemp,wxString cavityTemp)
{
    //新建垂直布局
    wxBoxSizer* idleSizer = new wxBoxSizer(wxVERTICAL);
//
//***温度控件

    wxBoxSizer *bSizer_temperature  = new wxBoxSizer(wxHORIZONTAL);
    //auto m_panel_temperature = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(30)), wxTAB_TRAVERSAL);
    auto m_panel_temperature = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_temperature->SetBackgroundColour(*wxWHITE);

    int temperature = 25;
    wxString temperatureString = wxString::Format("%d", temperature);
    wxString degreeSymbol = wxString::FromUTF8("\xE2\x84\x83");
    temperatureString.Append(degreeSymbol);
    IconText* tempCtrl_top = new IconText(m_panel_temperature,wxString("device_top_temperature"),20,temperatureString,20);
    tempCtrl_top->SetMinSize(wxSize(FromDIP(104), FromDIP(29)));

    bSizer_temperature->Add(tempCtrl_top,0, wxALIGN_CENTER | wxALL | wxEXPAND, FromDIP(9));

    auto m_panel_temperature_separotor0 = new wxPanel(m_panel_temperature, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_temperature_separotor0->SetBackgroundColour(wxColour(240,240,240));
    m_panel_temperature_separotor0->SetMinSize(wxSize(FromDIP(6),-1));

    bSizer_temperature->Add(m_panel_temperature_separotor0,0, wxEXPAND, 0);

    int temperature1 = 80;
    wxString temperatureString1 = wxString::Format("%d", temperature1);
    wxString degreeSymbol1 = wxString::FromUTF8("\xE2\x84\x83");
    temperatureString1.Append(degreeSymbol1);
    IconText* tempCtrl_top1 = new IconText(m_panel_temperature,wxString("device_bottom_temperature"),20,temperatureString1,20);
    tempCtrl_top1->SetMinSize(wxSize(FromDIP(104), FromDIP(29)));

    bSizer_temperature->Add(tempCtrl_top1,0, wxALIGN_CENTER | wxALL | wxEXPAND, FromDIP(9));

    auto m_panel_temperature_separotor1 = new wxPanel(m_panel_temperature, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_temperature_separotor1->SetBackgroundColour(wxColour(240,240,240));
    m_panel_temperature_separotor1->SetMinSize(wxSize(FromDIP(6),-1));

    bSizer_temperature->Add(m_panel_temperature_separotor1,0, wxEXPAND, 0);

    int temperature2 = 100;
    wxString temperatureString2 = wxString::Format("%d", temperature2);
    wxString degreeSymbol2 = wxString::FromUTF8("\xE2\x84\x83");
    temperatureString2.Append(degreeSymbol2);
    IconText* tempCtrl_top2 = new IconText(m_panel_temperature,wxString("device_mid_temperature"),20,temperatureString2,20);
    tempCtrl_top2->SetMinSize(wxSize(FromDIP(104), FromDIP(29)));

    bSizer_temperature->Add(tempCtrl_top2,0, wxALIGN_CENTER | wxALL | wxEXPAND, FromDIP(9));


    m_panel_temperature->SetSizer(bSizer_temperature);
    m_panel_temperature->Layout();
    bSizer_temperature->Fit(m_panel_temperature);

    idleSizer->Add(m_panel_temperature);

    //添加空白间距
    auto m_panel_separotor5 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor5->SetBackgroundColour(wxColour(240,240,240));
    m_panel_separotor5->SetMinSize(wxSize(-1, FromDIP(6)));

    idleSizer->Add(m_panel_separotor5,0, wxALL | wxEXPAND, 0);

//创建设备状态切换控件

    m_panel_idle_device_state = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_idle_device_state->SetBackgroundColour(*wxWHITE);
    wxBoxSizer *deviceStateSizer = new wxBoxSizer(wxVERTICAL);
    setupLayoutIdleDeviceState(deviceStateSizer, m_panel_idle_device_state,idle);
    m_panel_idle_device_state->SetSizer(deviceStateSizer);
    m_panel_idle_device_state->Layout();
    deviceStateSizer->Fit(m_panel_idle_device_state);

    idleSizer->Add(m_panel_idle_device_state);
//*** 设备状态和设备状态信息间距
    // 添加空白间距
     auto m_panel_separotor4 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor4->SetBackgroundColour(wxColour(240, 240, 240));
     m_panel_separotor4->SetMinSize(wxSize(-1, FromDIP(6)));

    idleSizer->Add(m_panel_separotor4, 0, wxALL | wxEXPAND, 0);
//***添加设备信息布局
    m_panel_idle_device_info = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_idle_device_info->SetBackgroundColour(*wxWHITE);
    wxBoxSizer* deviceInfoSizer = new wxBoxSizer(wxHORIZONTAL);
    setupLayoutDeviceInfo(deviceInfoSizer, m_panel_idle_device_info);
    m_panel_idle_device_info->SetSizer(deviceInfoSizer);
    m_panel_idle_device_info->Layout();
    deviceInfoSizer->Fit(m_panel_idle_device_info);
//***添加循环过滤信息布局
    m_panel_circula_filter = new StartFiltering(parent);

    idleSizer->Add(m_panel_idle_device_info, 0, wxALL | wxEXPAND , 0);
    idleSizer->Add(m_panel_circula_filter, 0, wxALL | wxEXPAND , 0);
    //m_panel_idle_device_info->Hide();
    m_panel_circula_filter->Hide();

    parent->SetSizer(idleSizer);
    parent->Layout();
    parent->Fit(); 
}

void TempMixDevice::setupLayoutIdleDeviceState(wxBoxSizer *deviceStateSizer, wxPanel *parent,bool idle) 
{
//***灯控制布局
    wxBoxSizer *bSizer_control_lamp  = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_control_lamp = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(30)), wxTAB_TRAVERSAL);
    m_panel_control_lamp->SetBackgroundColour(wxColour(255, 255, 255));

    // 显示文件信息按钮
    wxString file_pic = idle ? "device_idle_file_info" :"device_file_info";
    m_idle_device_info_button = new Button(m_panel_control_lamp, wxString(""), file_pic, 0, FromDIP(18));
    m_idle_device_info_button->SetFont(wxFont(wxFontInfo(16)));
    m_idle_device_info_button->SetBorderWidth(0);
//    m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_idle_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
    m_idle_device_info_button->SetBorderColor(wxColour(255, 255, 255));
    // m_idle_device_info_button->SetTextColor(wxColour(51,51,51));
    m_idle_device_info_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
    m_idle_device_info_button->SetCornerRadius(0);
    bSizer_control_lamp->Add(m_idle_device_info_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM , FromDIP(4));
    bSizer_control_lamp->AddSpacer(FromDIP(6));

    // 显示灯控制按钮
    m_idle_lamp_control_button = new Button(m_panel_control_lamp, wxString(""), "device_lamp_control", 0, FromDIP(18));
    m_idle_lamp_control_button->SetFont(wxFont(wxFontInfo(16)));
    m_idle_lamp_control_button->SetBorderWidth(0);
    m_idle_lamp_control_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_idle_lamp_control_button->SetBorderColor(wxColour(255, 255, 255));
    // m_idle_lamp_control_button->SetTextColor(wxColour(51,51,51));
    m_idle_lamp_control_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
    m_idle_lamp_control_button->SetCornerRadius(0);
    bSizer_control_lamp->Add(m_idle_lamp_control_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->AddSpacer(FromDIP(6));

    // 显示过滤按钮
    m_idle_filter_button = new Button(m_panel_control_lamp, wxString(""), "device_filter", 0, FromDIP(18));
    m_idle_filter_button->SetFont(wxFont(wxFontInfo(16)));
    m_idle_filter_button->SetBorderWidth(0);
    m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_idle_filter_button->SetBorderColor(wxColour(255, 255, 255));
    // m_idle_filter_button->SetTextColor(wxColour(51,51,51));
    m_idle_filter_button->SetMinSize((wxSize(FromDIP(108), FromDIP(29))));
    m_idle_filter_button->SetCornerRadius(0);
    bSizer_control_lamp->Add(m_idle_filter_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));

    //***灯控制布局添加至垂直布局
    m_panel_control_lamp->SetSizer(bSizer_control_lamp);
    m_panel_control_lamp->Layout();
    bSizer_control_lamp->Fit(m_panel_control_lamp);

    deviceStateSizer->Add(m_panel_control_lamp, 0, wxALL | wxEXPAND, 0);
}

void TempMixDevice::setupLayoutDeviceInfo(wxBoxSizer *deviceStateSizer, wxPanel *parent)
{
//水平布局中添加垂直布局
    //wxBoxSizer *bSizer_device_info  = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_device_info = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(180), -1), wxTAB_TRAVERSAL);
    m_panel_device_info->SetBackgroundColour(wxColour(255, 255, 255));

    //  添加空白间距
    auto m_panel_separotor0 = new wxPanel(m_panel_device_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor0->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor0->SetMinSize(wxSize(FromDIP(20), -1));

    deviceStateSizer->Add(m_panel_separotor0);

    wxBoxSizer *bSizer_device_name = new wxBoxSizer(wxVERTICAL);
    auto machine_type = new Label(m_panel_device_info, _L("Machine Type"));
    machine_type->Wrap(-1);
    machine_type->SetFont(wxFont(wxFontInfo(16)));
    machine_type->SetForegroundColour(wxColour(153, 153, 153));
    machine_type->SetBackgroundColour(wxColour(255, 255, 255));
    machine_type->SetMinSize(wxSize(FromDIP(120), -1));

    auto spray_nozzle = new Label(m_panel_device_info, _L("Spray Nozzle"));
    spray_nozzle->SetFont(wxFont(wxFontInfo(16)));
    spray_nozzle->SetForegroundColour(wxColour(153, 153, 153));
    spray_nozzle->SetBackgroundColour(wxColour(255, 255, 255));

    auto print_size = new Label(m_panel_device_info, _L("Print Size"));
    print_size->SetFont(wxFont(wxFontInfo(16)));
    print_size->SetForegroundColour(wxColour(153, 153, 153));
    print_size->SetBackgroundColour(wxColour(255, 255, 255));

    auto firmware_version = new Label(m_panel_device_info, _L("Firmware Version"));
    firmware_version->SetFont(wxFont(wxFontInfo(16)));
    firmware_version->SetForegroundColour(wxColour(153, 153, 153));
    firmware_version->SetBackgroundColour(wxColour(255, 255, 255));

    auto serial_number = new Label(m_panel_device_info, _L("Serial Number"));
    serial_number->SetFont(wxFont(wxFontInfo(16)));
    serial_number->SetForegroundColour(wxColour(153, 153, 153));
    serial_number->SetBackgroundColour(wxColour(255, 255, 255));

    auto private_material = new Label(m_panel_device_info, _L("Private Material Statistics"));
    private_material->SetFont(wxFont(wxFontInfo(16)));
    private_material->SetForegroundColour(wxColour(153, 153, 153));
    private_material->SetBackgroundColour(wxColour(255, 255, 255));

    bSizer_device_name->Add(machine_type, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->Add(spray_nozzle, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->Add(print_size, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->Add(firmware_version, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->Add(serial_number, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->Add(private_material, 0, wxALL | wxEXPAND, 0);

    m_panel_device_info->SetSizer(bSizer_device_name);
    m_panel_device_info->Layout();
    bSizer_device_name->Fit(m_panel_device_info);
    deviceStateSizer->Add(m_panel_device_info);

    //  添加空白间距
    auto m_panel_separotor1 = new wxPanel(m_panel_device_info, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor1->SetBackgroundColour(wxColour(255, 255, 255));
    m_panel_separotor1->SetMinSize(wxSize(FromDIP(20), -1));

    deviceStateSizer->Add(m_panel_separotor1);

    auto m_panel_device_data = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_device_data->SetBackgroundColour(wxColour(255, 255, 255));

    wxBoxSizer *bSizer_device_data = new wxBoxSizer(wxVERTICAL);
    auto        machine_type_data       = new Label(m_panel_device_data, _L("Adventurer 5M"));
    machine_type_data->Wrap(-1);
    machine_type_data->SetFont(wxFont(wxFontInfo(16)));
    machine_type_data->SetForegroundColour(wxColour(51, 51, 51));
    machine_type_data->SetBackgroundColour(wxColour(255, 255, 255));
    machine_type_data->SetMinSize(wxSize(FromDIP(120), -1));

    auto spray_nozzle_data = new Label(m_panel_device_data, _L("0.4mm"));
    spray_nozzle_data->SetFont(wxFont(wxFontInfo(16)));
    spray_nozzle_data->SetForegroundColour(wxColour(51, 51, 51));
    spray_nozzle_data->SetBackgroundColour(wxColour(255, 255, 255));

    auto print_size_data = new Label(m_panel_device_data, _L("220*220*220mm"));
    print_size_data->SetFont(wxFont(wxFontInfo(16)));
    print_size_data->SetForegroundColour(wxColour(51, 51, 51));
    print_size_data->SetBackgroundColour(wxColour(255, 255, 255));

    auto firmware_version_data = new Label(m_panel_device_data, _L("2.1.4-2.1.6"));
    firmware_version_data->SetFont(wxFont(wxFontInfo(16)));
    firmware_version_data->SetForegroundColour(wxColour(51, 51, 51));
    firmware_version_data->SetBackgroundColour(wxColour(255, 255, 255));

    auto serial_number_data = new Label(m_panel_device_data, _L("ABCDEFG"));
    serial_number_data->SetFont(wxFont(wxFontInfo(16)));
    serial_number_data->SetForegroundColour(wxColour(51, 51, 51));
    serial_number_data->SetBackgroundColour(wxColour(255, 255, 255));

    auto private_material_data = new Label(m_panel_device_data, _L("1600.9 m"));
    private_material_data->SetFont(wxFont(wxFontInfo(16)));
    private_material_data->SetForegroundColour(wxColour(51, 51, 51));
    private_material_data->SetBackgroundColour(wxColour(255, 255, 255));

    bSizer_device_data->Add(machine_type_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->Add(spray_nozzle_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->Add(print_size_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->Add(firmware_version_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->Add(serial_number_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->Add(private_material_data, 0, wxALL | wxEXPAND, 0);

    m_panel_device_data->SetSizer(bSizer_device_data);
    m_panel_device_data->Layout();
    bSizer_device_data->Fit(m_panel_device_data);

    deviceStateSizer->Add(m_panel_device_data);
}

void TempMixDevice::connectEvent()
{
    //idle button slot
    m_idle_device_info_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        if(m_panel_idle_device_info){
             m_panel_idle_device_info->Show();
            m_idle_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
        }
        if(m_panel_circula_filter){
          m_panel_circula_filter->Hide();
        }
        if (m_idle_filter_button) {
          m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
        }
        Layout();
    });

    m_idle_filter_button->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        if(m_panel_circula_filter){
            m_panel_circula_filter->Show();
            m_idle_filter_button->SetBackgroundColor(wxColour(217, 234, 255));
        }
        if(m_panel_idle_device_info){
          m_panel_idle_device_info->Hide();
        }
        if (m_idle_device_info_button) {
          m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        }
        Layout();
    });  

}

void TempMixDevice::setDeviceInfoBtnIcon(const wxString &icon) 
{ 
    m_idle_device_info_button->SetIcon(icon);
}
