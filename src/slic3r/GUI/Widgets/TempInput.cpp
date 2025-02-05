#include "TempInput.hpp"
#include "Label.hpp"
#include "PopupWindow.hpp"
#include "../I18N.hpp"
#include <wx/dcgraph.h>
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "slic3r/GUI/FlashForge/MultiComMgr.hpp"

wxDEFINE_EVENT(wxCUSTOMEVT_SET_TEMP_FINISH, wxCommandEvent);
wxDEFINE_EVENT(EVT_CANCEL_PRINT_CLICKED, wxCommandEvent);
wxDEFINE_EVENT(EVT_CONTINUE_PRINT_CLICKED, wxCommandEvent);

BEGIN_EVENT_TABLE(TempInput, wxPanel)
EVT_MOTION(TempInput::mouseMoved)
EVT_ENTER_WINDOW(TempInput::mouseEnterWindow)
EVT_LEAVE_WINDOW(TempInput::mouseLeaveWindow)
EVT_KEY_DOWN(TempInput::keyPressed)
EVT_KEY_UP(TempInput::keyReleased)
EVT_MOUSEWHEEL(TempInput::mouseWheelMoved)
EVT_PAINT(TempInput::paintEvent)
END_EVENT_TABLE()

const std::string CLOSE = "close";
const std::string OPEN  = "open";

CancelPrint::CancelPrint(const wxString &info, const wxString &leftBtnTxt, const wxString &rightBtnTxt)
    : TitleDialog(static_cast<wxWindow *>(Slic3r::GUI::wxGetApp().GetMainTopWindow()), _L("Cancel print"), 6)
{
    m_sizer_main = MainSizer();
    m_sizer_main->SetMinSize(wxSize(FromDIP(370), FromDIP(154)));

    m_sizer_main->AddSpacer(FromDIP(31));
    m_info = new wxStaticText(this, wxID_ANY, info);
    m_sizer_main->Add(m_info, 0, wxALIGN_CENTER, 0);

    m_sizer_main->AddSpacer(FromDIP(18));

    // 确认、取消按钮
    wxBoxSizer *bSizer_operate_hor = new wxBoxSizer(wxHORIZONTAL);
    wxPanel    *operate_panel      = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_cancel_btn                   = new FFButton(operate_panel, wxID_ANY, leftBtnTxt);
    m_cancel_btn->SetMinSize(wxSize(FromDIP(76), FromDIP(34)));
    m_cancel_btn->SetFontHoverColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBGHoverColor(wxColour("#65A79E"));
    m_cancel_btn->SetBorderHoverColor(wxColour("#65A79E"));

    m_cancel_btn->SetFontPressColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBGPressColor(wxColour("#1A8676"));
    m_cancel_btn->SetBorderPressColor(wxColour("#1A8676"));

    m_cancel_btn->SetFontColor(wxColour(255, 255, 255));
    m_cancel_btn->SetBorderColor(wxColour("#419488"));
    m_cancel_btn->SetBGColor(wxColour("#419488"));
    m_cancel_btn->Bind(wxEVT_LEFT_DOWN, [this, operate_panel](wxMouseEvent &event) {
        event.Skip();
        wxCommandEvent ev(EVT_CANCEL_PRINT_CLICKED, GetId());
         ev.SetEventObject(this);
         wxPostEvent(this, ev);
    });

    bSizer_operate_hor->AddStretchSpacer();
    bSizer_operate_hor->Add(m_cancel_btn, 0, wxALIGN_CENTER, 0);
    bSizer_operate_hor->AddSpacer(FromDIP(43));

    m_confirm_btn = new FFButton(operate_panel, wxID_ANY, rightBtnTxt);
    m_confirm_btn->SetMinSize(wxSize(FromDIP(76), FromDIP(34)));
    m_confirm_btn->SetFontHoverColor(wxColour("#65A79E"));
    m_confirm_btn->SetBGHoverColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBorderHoverColor(wxColour("#65A79E"));

    m_confirm_btn->SetFontPressColor(wxColour("#1A8676"));
    m_confirm_btn->SetBGPressColor(wxColour(255, 255, 255));
    m_confirm_btn->SetBorderPressColor(wxColour("#1A8676"));

    m_confirm_btn->SetFontColor(wxColour("#333333"));
    m_confirm_btn->SetBorderColor(wxColour("#333333"));
    m_confirm_btn->SetBGColor(wxColour(255, 255, 255));
    m_confirm_btn->Bind(wxEVT_LEFT_DOWN, [this, operate_panel](wxMouseEvent &event) {
        event.Skip();
        wxCommandEvent ev(EVT_CONTINUE_PRINT_CLICKED, GetId());
        ev.SetEventObject(this);
        wxPostEvent(this, ev);
    });

    bSizer_operate_hor->Add(m_confirm_btn, 0, wxALIGN_CENTER, 0);
    bSizer_operate_hor->AddStretchSpacer();

    operate_panel->SetSizer(bSizer_operate_hor);
    operate_panel->Layout();
    bSizer_operate_hor->Fit(operate_panel);

    m_sizer_main->Add(operate_panel, 0, wxALL | wxALIGN_CENTER, 0);

    Fit();
    //Thaw();
    Centre(wxBOTH);
    Layout();
}

ShowTip::ShowTip(const wxString &info)
    : TitleDialog(static_cast<wxWindow *>(Slic3r::GUI::wxGetApp().GetMainTopWindow()), _L("Tip"), 6)
{
    m_sizer_main = MainSizer();
    m_sizer_main->SetMinSize(wxSize(FromDIP(360), FromDIP(160)));

    m_sizer_main->AddSpacer(FromDIP(50));
    m_info = new wxStaticText(this, wxID_ANY, info, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_info->SetForegroundColour(wxColor("#419488"));
    m_sizer_main->Add(m_info, 0, wxALIGN_CENTER);
    m_sizer_main->AddStretchSpacer();

    Fit();
    //Thaw();
    Centre(wxBOTH);
    Layout();
}

void ShowTip::SetLabel(const wxString &info) 
{ 
    m_info->SetLabel(info);
}

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
            text_ctrl->SetValue(wxString("--"));
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
        e.Skip();
        if (m_read_only) {
            return;
        }
        OnEdit();
        auto temp = text_ctrl->GetValue();
        if (temp.ToStdString().empty()) return;
        if (!AllisNum(temp.ToStdString())) return;
        if (max_temp <= 0) return;

        auto tempint = std::stoi(temp.ToStdString());
        if (tempint > max_temp) {
            tempint = max_temp;
            //Warning(true, WARNING_TOO_HIGH);
            Warning(false, WARNING_TOO_LOW);
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
    text_ctrl->SetForegroundColour(StateColor::darkModeColorFor(*wxBLACK));
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

void TempInput::SetTagTemp(int temp, bool notifyModify)
{
    if (notifyModify) {
        Freeze();
        text_ctrl->Freeze();
        text_ctrl->SetValue(wxString::Format("%d", temp));
        text_ctrl->Thaw();
        Thaw();
        messureSize();
        Refresh();
    } else {
        text_ctrl->SetValue(wxString::Format("%d", temp));
        messureSize();
        Refresh();
    }

}

void TempInput::SetCurrTemp(int temp) 
{ 
    SetLabel(wxString::Format("%d", temp)); 
}

void TempInput::SetCurrTemp(wxString temp) 
{
    SetLabel(temp);
}

void TempInput::SetCurrTemp(int temp, bool notifyModify)
{
    if (notifyModify) {
        Freeze();
        SetLabel(wxString::Format("%d", temp));
        Thaw();
    } else {
        SetLabel(wxString::Format("%d", temp));
    }
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

void TempInput::SetTextBindInput() 
{
    text_ctrl->Bind(wxEVT_CHAR, [&](wxKeyEvent &event) { 
        event.Skip(false);
    });
}

void TempInput::SetMaxTemp(int temp) { max_temp = temp; }

void TempInput::SetMinTemp(int temp) { min_temp = temp; }

void TempInput::SetNormalIcon(wxString normalIcon) 
{ 
    this->normal_icon = ScalableBitmap(this, normalIcon.ToStdString(), 16); 
}

void TempInput::SetTargetTempVis(bool visible) 
{
    if (visible) {
        target_temp_vis = true;
        text_ctrl->Show();
    } else {
        target_temp_vis = false;
        text_ctrl->Hide();
    }
    Refresh();
}

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
    dc.SetFont(::Label::Body_14);
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

    dc.SetTextForeground(StateColor::darkModeColorFor("#328DFB"));
    if (text.compare("--") == 0) {
        dc.SetTextForeground(StateColor::darkModeColorFor("#999999"));
    }

    dc.DrawText(text, pt);

    // separator
    dc.SetFont(::Label::Body_10);
    auto sepSize = dc.GetMultiLineTextExtent(wxString("/"));
    dc.SetTextForeground(wxColor(51, 51, 51));
    dc.SetTextBackground(background_color.colorForStates(states));
    pt.x += labelSize.x + 10;
    pt.y = (size.y - sepSize.y) / 2;
    if (target_temp_vis) {
        dc.DrawText(wxString("/"), pt);

        // flag
        if (degree_icon.bmp().IsOk()) {
            auto   pos    = text_ctrl->GetPosition();
            wxSize szIcon = degree_icon.GetBmpSize();
            pt.y          = (size.y - szIcon.y) / 2;
            pt.x          = pos.x + text_ctrl->GetSize().x;
            dc.DrawBitmap(degree_icon.bmp(), pt);
        }
    } else {
            // flag
        if (degree_icon.bmp().IsOk()) {
            dc.DrawBitmap(degree_icon.bmp(), pt);
        }
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
    padding_left = 26;
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
    padding_left = 26;
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

IconText::IconText() {}

IconText::IconText(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize,const wxPoint &pos,const wxSize & size,long style)
                //: wxPanel(parent, wxID_ANY,pos, size, style)
{
    Create(parent, wxID_ANY, pos, size);
    SetBackgroundColour(*wxWHITE);
    create_panel(this, icon, iconSize, text, textSize);
}

void IconText::create_panel(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_page = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    m_icon = create_scaled_bitmap(icon.ToStdString(), parent, iconSize);
    auto icon_static = new wxStaticBitmap(m_panel_page, wxID_ANY, m_icon);
    icon_static->SetBackgroundColour(*wxWHITE);

    wxSize size = icon_static->GetSize();
    icon_static->SetSize(size.GetWidth(), size.GetHeight());

    m_text_ctrl = new Label(m_panel_page, text);
    //m_text_ctrl->Wrap(-1);
    //m_text_ctrl->SetFont(wxFont(wxFontInfo(textSize)));
    m_text_ctrl->SetForegroundColour(wxColour(50, 141, 251));
    m_text_ctrl->SetBackgroundColour(*wxWHITE);
    //m_text_ctrl->SetMinSize(wxSize(FromDIP(70), -1));

    sizer->AddStretchSpacer();
    sizer->Add(icon_static, 0, wxALIGN_CENTER_HORIZONTAL | wxALL | wxEXPAND, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(m_text_ctrl, 0, wxALIGN_CENTER_HORIZONTAL | wxALL | wxEXPAND, 0);
    sizer->AddStretchSpacer();

    m_panel_page->SetSizer(sizer);
    m_panel_page->Layout();
    sizer->Fit(m_panel_page); 
}

void IconText::setText(wxString text)
{
    m_text_ctrl->SetLabel(text);
    Refresh();
}

void IconText::setTextForegroundColour(wxColour colour) 
{
    m_text_ctrl->SetForegroundColour(colour);
    Refresh();
}

void IconText::setTextBackgroundColor(wxColour colour)
{
    m_text_ctrl->SetBackgroundColour(colour);
    Refresh();
}

IconBottonText::IconBottonText(wxWindow* parent,wxString icon,int iconSize,wxString text,int textSize,wxString secondIcon,wxString thirdIcon,bool positiveOrder,const wxPoint &pos,const wxSize & size,long style)
                : wxPanel(parent, wxID_ANY,pos, size, style)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this, icon, iconSize, text, textSize, secondIcon, thirdIcon, positiveOrder);
}

void IconBottonText::create_panel(
    wxWindow *parent, wxString icon, int iconSize, wxString text, int textSize, wxString secondIcon, wxString thirdIcon, bool positiveOrder)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_page = new wxPanel(this, wxID_ANY, wxDefaultPosition,wxDefaultSize,wxBORDER_NONE);
    m_panel_page->SetSize(wxSize(-1,-1));
    m_icon = create_scaled_bitmap(icon.ToStdString(), parent, iconSize);

    m_text_ctrl = new wxTextCtrl(m_panel_page, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    //m_text_ctrl->SetFont(wxFont(wxFontInfo(textSize)));
    m_text_ctrl->SetForegroundColour(wxColour(51, 51, 51));
    m_text_ctrl->SetBackgroundColour(wxColour(255, 255, 255));
    m_text_ctrl->SetMinSize(wxSize(FromDIP(50), -1));
    //m_text_ctrl->Bind(wxEVT_TEXT, &IconBottonText::onTextChange, this);
    m_text_ctrl->Bind(wxEVT_KILL_FOCUS, &IconBottonText::onTextFocusOut, this);

    m_unitLabel = new wxStaticText(m_panel_page, wxID_ANY, "%");

    auto icon_static = new wxStaticBitmap(m_panel_page, wxID_ANY, m_icon);

    if (secondIcon.IsEmpty()) {
        m_dec_btn = new FFPushButton(m_panel_page, wxID_ANY, "push_button_dec_normal", "push_button_dec_hover", "push_button_dec_press","push_button_dec_disable",20);
    } else {
        m_dec_btn = new FFPushButton(m_panel_page, wxID_ANY, "push_button_arrow_inc_normal", "push_button_arrow_inc_hover",
                                     "push_button_arrow_inc_press", "push_button_arrow_inc_disable",20);
    }
    m_dec_btn->Bind(wxEVT_LEFT_UP, &IconBottonText::onDecBtnClicked, this);
    m_dec_btn->SetBackgroundColour(*wxWHITE);

    if (secondIcon.IsEmpty()) {
        m_inc_btn = new FFPushButton(m_panel_page, wxID_ANY, "push_button_inc_normal", "push_button_inc_hover", "push_button_inc_press","push_button_inc_disable",20);
    } else {
        m_inc_btn = new FFPushButton(m_panel_page, wxID_ANY, "push_button_arrow_dec_normal", "push_button_arrow_dec_hover",
                                     "push_button_arrow_dec_press", "push_button_arrow_dec_disable",20);
            
    }
    m_inc_btn->Bind(wxEVT_LEFT_UP, &IconBottonText::onIncBtnClicked, this);
    m_inc_btn->SetBackgroundColour(*wxWHITE);

    if (positiveOrder) {
        sizer->Add(icon_static, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
        sizer->AddSpacer(FromDIP(12));
        sizer->Add(m_dec_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
        sizer->AddSpacer(FromDIP(5));
        sizer->Add(m_text_ctrl, 0, wxALIGN_CENTER_HORIZONTAL, 0);
        sizer->Add(m_unitLabel, 0, wxALIGN_CENTER_HORIZONTAL, 0);
        sizer->AddSpacer(FromDIP(5));
        sizer->Add(m_inc_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
    } else {
        sizer->Add(icon_static, 0, wxALIGN_CENTER | wxALL | wxEXPAND, 0);
        sizer->AddSpacer(FromDIP(12));
        sizer->Add(m_inc_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
        sizer->AddSpacer(FromDIP(5));
        sizer->Add(m_text_ctrl, 0, wxALIGN_CENTER, 0);
        sizer->Add(m_unitLabel, 0, wxALIGN_CENTER, 0);
        sizer->AddSpacer(FromDIP(5));
        sizer->Add(m_dec_btn, 0, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 0);
    }


    m_panel_page->SetSizer(sizer);
    m_panel_page->Layout();
    sizer->Fit(m_panel_page); 
    if (!secondIcon.IsEmpty()) {
        m_unitLabel->Hide();
    }
}

void IconBottonText::setLimit(double min, double max)
{ 
    m_min = min;
    m_max = max;
}

void IconBottonText::setAdjustValue(double value) 
{
    m_adjust_value = value; 
}

wxString IconBottonText::getTextValue() 
{ 
    return m_text_ctrl->GetValue(); 
}

void IconBottonText::setText(wxString text) 
{
    m_text_ctrl->SetValue(text);
    Refresh();
}

void IconBottonText::setCurValue(double value) 
{
    m_cur_value = value;
}

void IconBottonText::setPoint(int value) 
{
    m_point = value;
}

void IconBottonText::onTextChange(wxCommandEvent &event) 
{
    long value;
    if (m_text_ctrl->GetValue().ToLong(&value)) {
        if (value < m_min) {
            wxString str_min = wxString::Format("%.2f", m_min);
            m_text_ctrl->ChangeValue(str_min);
        } else if (value > m_max) {
            wxString str_max = wxString::Format("%.2f", m_max);
            m_text_ctrl->ChangeValue(str_max);
        }
    }
}

void IconBottonText::checkValue() 
{
    wxString text = m_text_ctrl->GetValue();
    long     value;
    double   doubleValue;
    if (text.ToLong(&value)) {
        if (value < m_min) {
            wxString str_min = wxString::Format("%.0f", m_min);
            m_text_ctrl->ChangeValue(str_min);
        } else if (value > m_max) {
            wxString str_max = wxString::Format("%.0f", m_max);
            m_text_ctrl->ChangeValue(str_max);
        }
    } else if (text.ToDouble(&doubleValue)) {
        if (doubleValue < m_min) {
            wxString str_min = wxString::Format("%.3f", m_min);
            m_text_ctrl->ChangeValue(str_min);
        } else if (doubleValue > m_max) {
            wxString str_max = wxString::Format("%.3f", m_max);
            m_text_ctrl->ChangeValue(str_max);
        }
    } else {
        // 输入不合法
        if (m_point == 3) {
            wxString str_last = wxString::Format("%.3f", m_cur_value);
            m_text_ctrl->ChangeValue(str_last);
        } else {
            wxString str_last = wxString::Format("%.0f", m_cur_value);
            m_text_ctrl->ChangeValue(str_last);
        }
    }
}

void IconBottonText::onTextFocusOut(wxFocusEvent &event) 
{
    checkValue();
    event.Skip();
}

void IconBottonText::onDecBtnClicked(wxMouseEvent &event) 
{
    wxString text = m_text_ctrl->GetValue();
    long     value;
    double   doubleValue;
    if (text.ToLong(&value)) {
        double cur_value = (value - m_adjust_value) < m_min ? m_min : value - m_adjust_value;
        wxString str_cur   = wxString::Format("%.0f", cur_value);
        m_text_ctrl->ChangeValue(str_cur);
    } else if (text.ToDouble(&doubleValue)) {
        double cur_value = (doubleValue - m_adjust_value) < m_min ? m_min : doubleValue - m_adjust_value;
        wxString str_cur   = wxString::Format("%.3f", cur_value);
        m_text_ctrl->ChangeValue(str_cur);
    }
    event.Skip(); 
}

void IconBottonText::onIncBtnClicked(wxMouseEvent &event)
{
    wxString text = m_text_ctrl->GetValue();
    long     value;
    double   doubleValue;
    if (text.ToLong(&value)) {
        double cur_value = (value + m_adjust_value) > m_max ? m_max : value + m_adjust_value;
        wxString str_cur   = wxString::Format("%.0f", cur_value);
        m_text_ctrl->ChangeValue(str_cur);
    } else if (text.ToDouble(&doubleValue)) {
        double   cur_value = (doubleValue + m_adjust_value) > m_max ? m_max : doubleValue + m_adjust_value;
        wxString str_cur   = wxString::Format("%.3f", cur_value);
        m_text_ctrl->ChangeValue(str_cur);
    }
    event.Skip();
}

StartFiltering::StartFiltering(wxWindow* parent)
    : wxPanel(parent, wxID_ANY,wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
{
    SetBackgroundColour(*wxWHITE);
    create_panel(this);
}

void StartFiltering::setCurId(int curId)
{
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
}

void StartFiltering::create_panel(wxWindow* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *bSizer_filtering_title = new wxBoxSizer(wxHORIZONTAL);

    auto m_panel_filtering_title = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(36)), wxTAB_TRAVERSAL);
    m_panel_filtering_title->SetBackgroundColour(wxColour(248,248,248));

    //过滤标题
    auto m_staticText_filtering = new wxStaticText(m_panel_filtering_title, wxID_ANY ,_L("Start Filtering"));
    m_staticText_filtering->Wrap(-1);
    //m_staticText_filtering->SetFont(wxFont(wxFontInfo(16)));
    m_staticText_filtering->SetForegroundColour(wxColour(51,51,51));

    bSizer_filtering_title->Add(m_staticText_filtering, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(17));
    bSizer_filtering_title->Add(0, 1, wxEXPAND, 0);
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

    bSizer_internal_circulate_hor->AddSpacer(FromDIP(17));
    bSizer_internal_circulate_hor->Add(m_staticText_internal_circulate, 0, wxALL | wxEXPAND, 0);
    bSizer_internal_circulate_hor->AddSpacer(FromDIP(7));
    bSizer_internal_circulate_hor->Add(m_internal_circulate_switch, 0, wxALL | wxEXPAND, 0);
    m_internal_circulate_switch->Bind(wxEVT_TOGGLEBUTTON, &StartFiltering::onAirFilterToggled, this);

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
    m_external_circulate_switch->Bind(wxEVT_TOGGLEBUTTON, &StartFiltering::onAirFilterToggled, this);

    bSizer_external_circulate_hor->AddSpacer(FromDIP(17));
    bSizer_external_circulate_hor->Add(m_staticText_external_circulate, 0, wxALL | wxEXPAND, 0);
    bSizer_external_circulate_hor->AddSpacer(FromDIP(7));
    bSizer_external_circulate_hor->Add(m_external_circulate_switch, 0, wxALL | wxEXPAND, 0);

    external_circulate_panel->SetSizer(bSizer_external_circulate_hor);
    external_circulate_panel->Layout();
    bSizer_external_circulate_hor->Fit(external_circulate_panel);

    sizer->Add(m_panel_filtering_title, 0, wxEXPAND | wxALL, 0);
    sizer->AddSpacer(FromDIP(12));
    sizer->Add(internal_circulate_panel, 0, wxEXPAND | wxALIGN_CENTER, 0);
    sizer->AddSpacer(FromDIP(14));
    sizer->Add(external_circulate_panel, 0, wxEXPAND | wxALIGN_CENTER, 0);
#ifdef __WIN32__
    sizer->AddSpacer(FromDIP(140));
#else if __APPLE__
    sizer->AddSpacer(FromDIP(151));
#endif

    parent->SetSizer(sizer);
}

void StartFiltering::setBtnState(bool internalOpen, bool externalOpen) 
{ 
    m_internal_circulate_switch->SetValue(internalOpen);
    m_external_circulate_switch->SetValue(externalOpen);
}

void StartFiltering::onAirFilterToggled(wxCommandEvent &event) 
{
    event.Skip();
    SwitchButton *click_btn   = dynamic_cast<SwitchButton *>(event.GetEventObject());
    std::string   inter_state = CLOSE;
    std::string   exter_state = CLOSE;

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
    Slic3r::GUI::ComAirFilterCtrl *filterCtrl = new Slic3r::GUI::ComAirFilterCtrl(inter_state, exter_state);
    // 测试，临时将id写死
    if (m_cur_id >= 0) {
        Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, filterCtrl);
    }
}

TempMixDevice::TempMixDevice(wxWindow* parent,bool idle, wxString nozzleTemp, wxString platformTemp, wxString cavityTemp,const wxPoint &pos,const wxSize & size,long style)
             : wxPanel(parent, wxID_ANY,pos, size, style)
{
    //SetBackgroundColour(*wxWHITE);
    create_panel(this,idle,nozzleTemp,platformTemp,cavityTemp);
    connectEvent();
}

void TempMixDevice::setState(int state, bool lampState)
{ 
    if (0 == state) {   //offline
        //图标、解绑
        m_idle_device_info_button->SetIcon("device_file_offline");
        m_idle_lamp_control_button->SetIcon("device_lamp_offline");
        m_idle_filter_button->SetIcon("device_filter_offline");
        m_idle_device_info_button->Unbind(wxEVT_LEFT_DOWN, &TempMixDevice::onDevInfoBtnClicked, this);
        m_idle_lamp_control_button->Unbind(wxEVT_LEFT_DOWN, &TempMixDevice::onLampBtnClicked, this);
        m_idle_filter_button->Unbind(wxEVT_LEFT_DOWN, &TempMixDevice::onFilterBtnClicked, this);
        m_panel_idle_device_info->Hide();
        m_panel_circula_filter->Hide();
        m_idle_device_info_button->Enable(false);
        m_idle_lamp_control_button->Enable(false);
        m_idle_filter_button->Enable(false);
        m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
        Layout();
    } else if(1 == state){  //idle
        m_idle_device_info_button->SetIcon("device_idle_file_info");
        if (lampState) {
            m_idle_lamp_control_button->SetIcon("device_lamp_control_press");
        } else {
            m_idle_lamp_control_button->SetIcon("device_lamp_control");
        }
        if (m_g3uMachine && !m_clearFanPressed) {
            m_idle_filter_button->SetIcon("device_filter_offline");
        } else {
            m_idle_filter_button->SetIcon("device_filter");
        }
        m_idle_device_info_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onDevInfoBtnClicked, this);
        m_idle_lamp_control_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onLampBtnClicked, this);
        m_idle_filter_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onFilterBtnClicked, this);
        m_idle_device_info_button->Enable(true);
        m_idle_lamp_control_button->Enable(true);
        m_idle_filter_button->Enable(true);
        m_top_btn->SetTargetTempVis(false);
        m_bottom_btn->SetTargetTempVis(false);
        m_mid_btn->SetTargetTempVis(false);
    } else if (2 == state) {   // normal
        m_idle_device_info_button->SetIcon("device_file_info");
        m_idle_lamp_control_button->SetIcon("device_lamp_control");
        if (m_g3uMachine && !m_clearFanPressed) {
            m_idle_filter_button->SetIcon("device_filter_offline");
        } else {
            m_idle_filter_button->SetIcon("device_filter");
        }
        m_idle_device_info_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onDevInfoBtnClicked, this);
        m_idle_lamp_control_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onLampBtnClicked, this);
        m_idle_filter_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onFilterBtnClicked, this);
        m_idle_device_info_button->Enable(true);
        m_idle_lamp_control_button->Enable(true);
        m_idle_filter_button->Enable(true);
    }
}

void TempMixDevice::setCurId(int curId)
{
    if (curId < 0) {
        return;
    }
    m_cur_id = curId;
    m_panel_circula_filter->setCurId(curId);
    m_clearFanPressed = true;
    reInitPage();
}

void TempMixDevice::reInitProductState() 
{ 
    m_idle_lamp_control_button->SetIcon("device_lamp_control");
    m_idle_filter_button->SetIcon("device_filter");
    m_idle_lamp_control_button->Enable(true);
    m_idle_filter_button->Enable(true);
}

void TempMixDevice::reInitPage() 
{
    if (m_panel_idle_device_info) {
        m_panel_idle_device_info->Hide();
    }
    if (m_panel_circula_filter) {
        m_panel_circula_filter->Hide();
    }
    if (m_idle_device_info_button) {
        m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
    if (m_idle_filter_button) {
        m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
}

void TempMixDevice::setDevProductAuthority(const fnet_dev_product_t &data) 
{
    bool lightCtrl = data.lightCtrlState == 0 ? false : true;
    bool fanCtrl   = data.internalFanCtrlState == 0 ? false : true;
    if (!lightCtrl) {
        m_idle_lamp_control_button->SetIcon("device_lamp_offline");
        m_idle_lamp_control_button->Enable(false);
    }
    if (!m_g3uMachine  && !fanCtrl) {
        m_idle_filter_button->SetIcon("device_filter_offline");
        m_idle_filter_button->Enable(false);
        m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
        m_idle_filter_button->SetBorderColor(wxColour(255, 255, 255));
        if (m_panel_circula_filter) {
            m_panel_circula_filter->Hide();
        }
    }
}

void TempMixDevice::lostFocusmodifyTemp()
{
    double top_temp;
    double bottom_temp;
    double mid_temp;
    bool   bTop = m_top_btn->GetTagTemp().ToDouble(&top_temp);
    bool   bBottom = m_bottom_btn->GetTagTemp().ToDouble(&bottom_temp);
    bool   bMid    = m_mid_btn->GetTagTemp().ToDouble(&mid_temp);
    if (!m_g3uMachine) {
        if (!bTop || top_temp < 0) {
            m_top_btn->SetTagTemp(m_right_target_temp, true);
            top_temp = m_right_target_temp;
        }
        if (top_temp > 280) {
            top_temp = 280;
            m_top_btn->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        } else if (top_temp < 0) {
            top_temp = 0;
            m_top_btn->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        }
        if (!bBottom || bottom_temp < 0) {
            m_bottom_btn->SetTagTemp(m_plat_target_temp, true);
            bottom_temp = m_plat_target_temp;
        }
        if (bottom_temp > 110) {
            bottom_temp = 110;
            m_bottom_btn->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        } else if (bottom_temp < 0) {
            bottom_temp = 0;
            m_bottom_btn->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        }
        //Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(bottom_temp, top_temp, 0, mid_temp);
        //Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
    } else {
        //right
        if (!bTop || top_temp < 0) {
            m_top_btn->SetTagTemp(m_right_target_temp, true);
            top_temp = m_right_target_temp;
        }
        if (top_temp > 350) {
            top_temp = 350;
            m_top_btn->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        } else if (top_temp < 0) {
            top_temp = 0;
            m_top_btn->SetTagTemp(top_temp, true);
            m_right_target_temp = top_temp;
        }
        //left
        if (!bBottom || bottom_temp < 0) {
            m_bottom_btn->SetTagTemp(m_plat_target_temp, true);
            bottom_temp = m_plat_target_temp;
        }
        if (bottom_temp > 350) {
            bottom_temp = 350;
            m_bottom_btn->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        } else if (bottom_temp < 0) {
            bottom_temp = 0;
            m_bottom_btn->SetTagTemp(bottom_temp, true);
            m_plat_target_temp = bottom_temp;
        }
        //bottom
        if (!bMid || mid_temp < 0) {
            m_mid_btn->SetTagTemp(m_cavity_target_temp, true);
            mid_temp = m_cavity_target_temp;
        }
        if (mid_temp > 110) {
            mid_temp = 110;
            m_mid_btn->SetTagTemp(mid_temp, true);
            m_cavity_target_temp = mid_temp;
        } else if (mid_temp < 0) {
            mid_temp = 0;
            m_mid_btn->SetTagTemp(mid_temp, true);
            m_cavity_target_temp = mid_temp;
        }

        //Slic3r::GUI::ComTempCtrl* tempCtrl = new Slic3r::GUI::ComTempCtrl(mid_temp, top_temp, bottom_temp, 0);
        //Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, tempCtrl);
    }
}

void TempMixDevice::changeMachineType(unsigned short pid)
{
    switch (pid) {
    case 0x0023: //"adventurer_5m"
    case 0x0024: //"adventurer_5m_pro"
        m_g3uMachine = false;
        m_top_btn->SetNormalIcon("device_top_temperature");
        m_top_btn->SetIconNormal();
        m_bottom_btn->SetNormalIcon("device_bottom_temperature");
        m_bottom_btn->SetIconNormal();
        m_mid_btn->SetNormalIcon("device_mid_temperature");
        m_mid_btn->SetIconNormal();
        m_mid_btn->SetReadOnly(true);
        break;
    case 0x001F: //"guider_3_ultra"
        m_g3uMachine = true;
        m_top_btn->SetNormalIcon("device_right_temperature");
        m_top_btn->SetIconNormal();
        m_bottom_btn->SetNormalIcon("device_left_temperature");
        m_bottom_btn->SetIconNormal();
        m_mid_btn->SetNormalIcon("device_bottom_temperature");
        m_mid_btn->SetIconNormal();
        m_mid_btn->SetReadOnly(false);
        break;
    }
}


void TempMixDevice::create_panel(wxWindow* parent,bool idle, wxString nozzleTemp,wxString platformTemp,wxString cavityTemp)
{
    //新建垂直布局
    wxBoxSizer* idleSizer = new wxBoxSizer(wxVERTICAL);
//
//***温度控件

    wxBoxSizer *bSizer_temperature  = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_temperature = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
    m_panel_temperature->SetBackgroundColour(*wxWHITE);

    wxString temperatureString = "100";
    temperatureString.Append(wxString::FromUTF8("\xE2\x84\x83"));
    //m_temp_ctrl_top = new IconText(m_panel_temperature, wxString("device_top_temperature"), 20, temperatureString, 18);
    //m_top_btn = new TempButton(m_panel_temperature, temperatureString, "device_top_temperature", 0, 16);
    wxWindowID top_id = wxWindow::NewControlId();
    m_top_btn = new TempInput(m_panel_temperature, top_id, wxString("--"), wxString("--"), wxString("device_top_temperature"),
                                   wxString("device_top_temperature"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER); 
    m_top_btn->SetMinTemp(20);
    m_top_btn->SetMaxTemp(120);
    m_top_btn->SetMinSize((wxSize(FromDIP(106), FromDIP(20))));
    m_top_btn->SetBorderWidth(0);
    StateColor tempinput_text_colour(std::make_pair(wxColour(51, 51, 51), (int) StateColor::Disabled),
                                     std::make_pair(wxColour(48, 58, 60), (int) StateColor::Normal));
    m_top_btn->SetTextColor(tempinput_text_colour);
    StateColor tempinput_border_colour(std::make_pair(*wxWHITE, (int) StateColor::Disabled),
                                       std::make_pair(wxColour(0, 150, 136), (int) StateColor::Focused),
                                       std::make_pair(wxColour(0, 150, 136), (int) StateColor::Hovered),
                                       std::make_pair(*wxWHITE, (int) StateColor::Normal));
    m_top_btn->SetBorderColor(tempinput_border_colour);
    //m_top_btn->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_top_btn->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_top_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_top_btn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &event) { event.Skip(false); });
    m_top_btn->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) { 
        event.Skip();
        lostFocusmodifyTemp();
    });
    m_top_btn->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    

    bSizer_temperature->Add(m_top_btn, 1, wxALIGN_CENTER | wxEXPAND);

    auto m_panel_temperature_separotor0 = new wxPanel(m_panel_temperature, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1),wxTAB_TRAVERSAL);
    m_panel_temperature_separotor0->SetBackgroundColour(wxColour(240,240,240));

    bSizer_temperature->Add(m_panel_temperature_separotor0, 0, wxEXPAND | wxALL, 0);

    wxString temperatureString_1 = "100";
    temperatureString_1.Append(wxString::FromUTF8("\xE2\x84\x83"));
    //m_temp_ctrl_bottom = new IconText(m_panel_temperature, wxString("device_bottom_temperature"), 20, temperatureString_1, 18);
    //m_bottom_btn = new TempButton(m_panel_temperature, temperatureString_1, "device_bottom_temperature", 0, 16);
    wxWindowID bottom_id = wxWindow::NewControlId();
    m_bottom_btn = new TempInput(m_panel_temperature, bottom_id, wxString("--"), wxString("--"),
                                         wxString("device_bottom_temperature"), wxString("device_bottom_temperature"), wxDefaultPosition,
                                         wxDefaultSize, wxALIGN_CENTER);

    //m_bottom_btn->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_bottom_btn->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_bottom_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_bottom_btn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &event) { event.Skip(false); });
    m_bottom_btn->SetMinTemp(20);
    m_bottom_btn->SetMaxTemp(120);
    m_bottom_btn->SetMinSize((wxSize(FromDIP(106), FromDIP(20))));
    m_bottom_btn->SetBorderWidth(0);
    m_bottom_btn->SetBorderColor(tempinput_border_colour);
    m_bottom_btn->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    m_bottom_btn->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &event) {
        event.Skip();
        lostFocusmodifyTemp();
    });

    bSizer_temperature->Add(m_bottom_btn, wxSizerFlags(1).Expand());

    auto m_panel_temperature_separotor1 = new wxPanel(m_panel_temperature, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(6), -1),wxTAB_TRAVERSAL);
    m_panel_temperature_separotor1->SetBackgroundColour(wxColour(240,240,240));

    bSizer_temperature->Add(m_panel_temperature_separotor1, 0, wxEXPAND | wxALL, 0);

    wxString temperatureString_2 = "100"; 
    temperatureString_2.Append(wxString::FromUTF8("\xE2\x84\x83"));
    //m_temp_ctrl_mid = new IconText(m_panel_temperature, wxString("device_mid_temperature"), 20, temperatureString_2, 18);
    //m_mid_btn = new TempButton(m_panel_temperature, temperatureString_2, "device_mid_temperature", 0, 16);
    wxWindowID bottom_mid = wxWindow::NewControlId();
    m_mid_btn = new TempInput(m_panel_temperature, bottom_mid, wxString("--"), wxString("--"),
                                          wxString("device_mid_temperature"), wxString("device_mid_temperature"), wxDefaultPosition, wxDefaultSize,
                                          wxALIGN_CENTER);
    

    //m_mid_btn->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_mid_btn->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_mid_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) { event.Skip(false); });
    //m_mid_btn->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &event) { event.Skip(false); });
    m_mid_btn->SetMinTemp(20);
    m_mid_btn->SetMaxTemp(120);
    m_mid_btn->SetMinSize((wxSize(FromDIP(106), FromDIP(20))));
    m_mid_btn->SetBorderWidth(0);
    m_mid_btn->SetReadOnly(true);
    //m_mid_btn->SetTextBindInput();
    m_mid_btn->SetBorderColor(tempinput_border_colour);
    m_mid_btn->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
        event.Skip();
        lostFocusmodifyTemp();
    });
    m_mid_btn->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& event) {
        event.Skip();
        lostFocusmodifyTemp();
    });

    bSizer_temperature->Add(m_mid_btn, wxSizerFlags(1).Expand());

    m_panel_temperature->SetSizer(bSizer_temperature);
    m_panel_temperature->Layout();
    bSizer_temperature->Fit(m_panel_temperature);

    idleSizer->Add(m_panel_temperature,0, wxEXPAND | wxALL, 0);

    //添加空白间距
    auto m_panel_separotor5 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor5->SetBackgroundColour(wxColour(240,240,240));
    m_panel_separotor5->SetMinSize(wxSize(-1, FromDIP(10)));

    idleSizer->Add(m_panel_separotor5,0, wxALL | wxEXPAND, 0);

//创建灯控件布局

    m_panel_idle_device_state = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_idle_device_state->SetBackgroundColour(*wxWHITE);
    wxBoxSizer *deviceStateSizer = new wxBoxSizer(wxVERTICAL);
    setupLayoutIdleDeviceState(deviceStateSizer, m_panel_idle_device_state,idle);
    m_panel_idle_device_state->SetSizer(deviceStateSizer);
    m_panel_idle_device_state->Layout();
    deviceStateSizer->Fit(m_panel_idle_device_state);

    idleSizer->Add(m_panel_idle_device_state,0, wxALL | wxEXPAND, 0);
    //*** 设备状态和设备状态信息间距
    // 添加空白间距
    auto m_panel_separotor4 = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_separotor4->SetBackgroundColour(wxColour(240, 240, 240));
    m_panel_separotor4->SetMinSize(wxSize(-1, FromDIP(23)));

    idleSizer->Add(m_panel_separotor4, 0, wxALL | wxEXPAND, 0);
//***添加设备信息布局
    m_panel_idle_device_info = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_panel_idle_device_info->SetBackgroundColour(*wxWHITE);
    wxBoxSizer *deviceInfoSizer = new wxBoxSizer(wxVERTICAL);
    setupLayoutDeviceInfo(deviceInfoSizer, m_panel_idle_device_info);
    m_panel_idle_device_info->SetSizer(deviceInfoSizer);
    m_panel_idle_device_info->Layout();
    deviceInfoSizer->Fit(m_panel_idle_device_info);
//***添加循环过滤信息布局
    m_panel_circula_filter = new StartFiltering(parent);
    //添加空白页面
    auto blank_page = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(300)), wxTAB_TRAVERSAL);
    blank_page->SetBackgroundColour(wxColor("#F0F0F0"));

    idleSizer->Add(m_panel_idle_device_info, 0, wxALL | wxEXPAND , 0);
    idleSizer->Add(m_panel_circula_filter, 0, wxALL | wxEXPAND, 0);
    idleSizer->Add(blank_page, 0, wxALL | wxEXPAND, 0);
    m_panel_idle_device_info->Hide();
    m_panel_circula_filter->Hide();

    parent->SetSizer(idleSizer);
}



void TempMixDevice::setupLayoutIdleDeviceState(wxBoxSizer *deviceStateSizer, wxPanel *parent,bool idle) 
{
//***灯控制布局
    wxBoxSizer *bSizer_control_lamp  = new wxBoxSizer(wxHORIZONTAL);
    auto        m_panel_control_lamp = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(52)), wxTAB_TRAVERSAL);
    m_panel_control_lamp->SetBackgroundColour(wxColour(255, 255, 255));

    // 显示文件信息按钮
    wxString file_pic = idle ? "device_idle_file_info" :"device_file_info";
    m_idle_device_info_button = new Button(m_panel_control_lamp, wxString(""), file_pic, 0, FromDIP(18));
    //m_idle_device_info_button->SetFont(wxFont(wxFontInfo(16)));
    m_idle_device_info_button->SetBorderWidth(0);
    m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
//    m_idle_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
    m_idle_device_info_button->SetBorderColor(wxColour(255, 255, 255));
    // m_idle_device_info_button->SetTextColor(wxColour(51,51,51));
    m_idle_device_info_button->SetMinSize((wxSize(FromDIP(108), FromDIP(20))));
    m_idle_device_info_button->SetCornerRadius(0);
    //bSizer_control_lamp->Add(m_idle_device_info_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM , FromDIP(4));
    bSizer_control_lamp->Add(m_idle_device_info_button, wxSizerFlags(1).Expand());
    bSizer_control_lamp->AddSpacer(FromDIP(6));

    // 显示灯控制按钮
    m_idle_lamp_control_button = new Button(m_panel_control_lamp, wxString(""), "device_lamp_control", 0, FromDIP(18));
    //m_idle_lamp_control_button->SetFont(wxFont(wxFontInfo(16)));
    m_idle_lamp_control_button->SetBorderWidth(0);
    m_idle_lamp_control_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_idle_lamp_control_button->SetBorderColor(wxColour(255, 255, 255));
    // m_idle_lamp_control_button->SetTextColor(wxColour(51,51,51));
    m_idle_lamp_control_button->SetMinSize((wxSize(FromDIP(108), FromDIP(20))));
    m_idle_lamp_control_button->SetCornerRadius(0);

    //bSizer_control_lamp->Add(m_idle_lamp_control_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_idle_lamp_control_button, wxSizerFlags(1).Expand());
    bSizer_control_lamp->AddSpacer(FromDIP(6));

    // 显示过滤按钮
    m_idle_filter_button = new Button(m_panel_control_lamp, wxString(""), "device_filter", 0, FromDIP(18));
    //m_idle_filter_button->SetFont(wxFont(wxFontInfo(16)));
    m_idle_filter_button->SetBorderWidth(0);
    m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    m_idle_filter_button->SetBorderColor(wxColour(255, 255, 255));
    // m_idle_filter_button->SetTextColor(wxColour(51,51,51));
    m_idle_filter_button->SetMinSize((wxSize(FromDIP(108), FromDIP(20))));
    m_idle_filter_button->SetCornerRadius(0);
    //bSizer_control_lamp->Add(m_idle_filter_button, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(4));
    bSizer_control_lamp->Add(m_idle_filter_button, wxSizerFlags(1).Expand());

    //***灯控制布局添加至垂直布局
    m_panel_control_lamp->SetSizer(bSizer_control_lamp);
    m_panel_control_lamp->Layout();
    bSizer_control_lamp->Fit(m_panel_control_lamp);

    deviceStateSizer->Add(m_panel_control_lamp, 0, wxALL | wxEXPAND, 0);
}

void TempMixDevice::setupLayoutDeviceInfo(wxBoxSizer *deviceInfoSizer, wxPanel *parent)
{
//水平布局中添加垂直布局
    //wxBoxSizer *bSizer_device_info  = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer *bSizer_close_row  = new wxBoxSizer(wxHORIZONTAL);
    auto m_panel_close_row = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    auto btn = new FFPushButton(m_panel_close_row, wxID_ANY, "push_button_close_normal", "push_button_close_hover",
                                       "push_button_close_press", "push_button_close_normal");
    btn->SetBackgroundColour(wxColour(255, 255, 255));
    btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        if (m_panel_idle_device_info) {
            m_panel_idle_device_info->Hide();
        }
        if (m_idle_device_info_button) {
            m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        }
        Layout();
    });
    bSizer_close_row->AddStretchSpacer();
    bSizer_close_row->Add(btn, 0, wxEXPAND | wxTOP, FromDIP(8));
    bSizer_close_row->AddSpacer(FromDIP(25));

    m_panel_close_row->SetSizer(bSizer_close_row);
    m_panel_close_row->Layout();
    bSizer_close_row->Fit(m_panel_close_row);

    deviceInfoSizer->Add(m_panel_close_row, 0, wxEXPAND | wxALL, 0);
    //
    wxBoxSizer *deviceStateSizer    = new wxBoxSizer(wxHORIZONTAL);
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
    //machine_type->SetFont(wxFont(wxFontInfo(16)));
    machine_type->SetForegroundColour(wxColour(153, 153, 153));
    machine_type->SetBackgroundColour(wxColour(255, 255, 255));
    machine_type->SetMinSize(wxSize(FromDIP(120), -1));

    auto spray_nozzle = new Label(m_panel_device_info, _L("Spray Nozzle"));
    //spray_nozzle->SetFont(wxFont(wxFontInfo(16)));
    spray_nozzle->SetForegroundColour(wxColour(153, 153, 153));
    spray_nozzle->SetBackgroundColour(wxColour(255, 255, 255));

    auto print_size = new Label(m_panel_device_info, _L("Print Size"));
    //print_size->SetFont(wxFont(wxFontInfo(16)));
    print_size->SetForegroundColour(wxColour(153, 153, 153));
    print_size->SetBackgroundColour(wxColour(255, 255, 255));

    auto firmware_version = new Label(m_panel_device_info, _L("Firmware Version"));
    //firmware_version->SetFont(wxFont(wxFontInfo(16)));
    firmware_version->SetForegroundColour(wxColour(153, 153, 153));
    firmware_version->SetBackgroundColour(wxColour(255, 255, 255));

    auto serial_number = new Label(m_panel_device_info, _L("Serial Number"));
    //serial_number->SetFont(wxFont(wxFontInfo(16)));
    serial_number->SetForegroundColour(wxColour(153, 153, 153));
    serial_number->SetBackgroundColour(wxColour(255, 255, 255));

    auto private_material = new Label(m_panel_device_info, _L("Private Material Statistics"));
    //private_material->SetFont(wxFont(wxFontInfo(16)));
    private_material->SetForegroundColour(wxColour(153, 153, 153));
    private_material->SetBackgroundColour(wxColour(255, 255, 255));

    bSizer_device_name->AddSpacer(FromDIP(2));
    bSizer_device_name->Add(machine_type, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->AddSpacer(FromDIP(18));
    bSizer_device_name->Add(spray_nozzle, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->AddSpacer(FromDIP(18));
    bSizer_device_name->Add(print_size, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->AddSpacer(FromDIP(18));
    bSizer_device_name->Add(firmware_version, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->AddSpacer(FromDIP(18));
    bSizer_device_name->Add(serial_number, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->AddSpacer(FromDIP(18));
    bSizer_device_name->Add(private_material, 0, wxALL | wxEXPAND, 0);
    bSizer_device_name->AddSpacer(FromDIP(15));
    //bSizer_device_name->AddStretchSpacer();

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
    m_machine_type_data = new Label(m_panel_device_data, ("Adventurer 5M"));
    //m_machine_type_data->Wrap(-1);
    //m_machine_type_data->SetFont(wxFont(wxFontInfo(16)));
    m_machine_type_data->SetForegroundColour(wxColour(51, 51, 51));
    m_machine_type_data->SetBackgroundColour(wxColour(255, 255,255));
    //m_machine_type_data->SetMinSize(wxSize(FromDIP(155), -1));

    m_spray_nozzle_data = new Label(m_panel_device_data, ("0.4mm"));
    //m_spray_nozzle_data->SetFont(wxFont(wxFontInfo(16)));
    m_spray_nozzle_data->SetForegroundColour(wxColour(51, 51, 51));
    m_spray_nozzle_data->SetBackgroundColour(wxColour(255, 255, 255));

    m_print_size_data = new Label(m_panel_device_data, ("220*220*220mm"));
    //m_print_size_data->SetFont(wxFont(wxFontInfo(16)));
    m_print_size_data->SetForegroundColour(wxColour(51, 51, 51));
    m_print_size_data->SetBackgroundColour(wxColour(255, 255, 255));

    m_firmware_version_data = new Label(m_panel_device_data, ("2.1.4-2.1.6"));
    //m_firmware_version_data->SetFont(wxFont(wxFontInfo(16)));
    m_firmware_version_data->SetForegroundColour(wxColour(51, 51, 51));
    m_firmware_version_data->SetBackgroundColour(wxColour(255, 255, 255));

    m_serial_number_data = new Label(m_panel_device_data, ("ABCDEFG"));
    //m_serial_number_data->SetFont(wxFont(wxFontInfo(16)));
    m_serial_number_data->SetForegroundColour(wxColour(51, 51, 51));
    m_serial_number_data->SetBackgroundColour(wxColour(255, 255, 255));

    m_private_material_data = new Label(m_panel_device_data, ("1600.9 m"));
    //m_private_material_data->SetFont(wxFont(wxFontInfo(16)));
    m_private_material_data->SetForegroundColour(wxColour(51, 51, 51));
    m_private_material_data->SetBackgroundColour(wxColour(255, 255, 255));

    bSizer_device_data->AddSpacer(FromDIP(2));
    bSizer_device_data->Add(m_machine_type_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->AddSpacer(FromDIP(18));
    bSizer_device_data->Add(m_spray_nozzle_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->AddSpacer(FromDIP(18));
    bSizer_device_data->Add(m_print_size_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->AddSpacer(FromDIP(18));
    bSizer_device_data->Add(m_firmware_version_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->AddSpacer(FromDIP(18));
    bSizer_device_data->Add(m_serial_number_data, 0, wxALL | wxEXPAND, 0);
    bSizer_device_data->AddSpacer(FromDIP(18));
    bSizer_device_data->Add(m_private_material_data, 0, wxALL | wxEXPAND, 0);
#ifdef __WIN32__
    bSizer_device_data->AddSpacer(FromDIP(15));
#else if __APPLE__
    bSizer_device_data->AddSpacer(FromDIP(18));
#endif

    m_panel_device_data->SetSizer(bSizer_device_data);
    m_panel_device_data->Layout();
    bSizer_device_data->Fit(m_panel_device_data);

    deviceStateSizer->Add(m_panel_device_data);

    deviceInfoSizer->Add(deviceStateSizer);
}

void TempMixDevice::connectEvent()
{
    //idle button slot
    m_idle_device_info_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onDevInfoBtnClicked, this);
    m_idle_lamp_control_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onLampBtnClicked, this);
    m_idle_filter_button->Bind(wxEVT_LEFT_DOWN, &TempMixDevice::onFilterBtnClicked, this);
}

void TempMixDevice::onDevInfoBtnClicked(wxMouseEvent &event)
{
    //event.Skip();
    if (m_panel_idle_device_info) {
        bool bShow = !m_panel_idle_device_info->IsShown();
        m_panel_idle_device_info->Show(bShow);
        m_panel_idle_device_info->Layout();
        if (bShow) {
            m_idle_device_info_button->SetBackgroundColor(wxColour(217, 234, 255));
        } else {
            m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
        }
    }
    if (m_panel_circula_filter) {
        m_panel_circula_filter->Hide();
    }
    if (m_idle_filter_button) {
        m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
    Layout();
}

void TempMixDevice::onLampBtnClicked(wxMouseEvent &event) 
{
    //event.Skip();
    if (m_idle_lamp_control_button->GetFlashForgeSelected()) {
        // 关灯
        Slic3r::GUI::ComLightCtrl *lightctrl = new Slic3r::GUI::ComLightCtrl(CLOSE);
        // 测试，临时将id写死
        if (m_cur_id >= 0) {
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
        }
        m_idle_lamp_control_button->SetIcon("device_lamp_control");
        m_idle_lamp_control_button->Refresh();
        m_idle_lamp_control_button->SetFlashForgeSelected(false);
    } else {
        // 开灯
        Slic3r::GUI::ComLightCtrl *lightctrl = new Slic3r::GUI::ComLightCtrl(OPEN);
        // 测试，临时将id写死
        if (m_cur_id >= 0) {
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, lightctrl);
        }
        m_idle_lamp_control_button->SetIcon("device_lamp_control_press");
        m_idle_lamp_control_button->Refresh();
        m_idle_lamp_control_button->SetFlashForgeSelected(true);
    }
}

void TempMixDevice::onFilterBtnClicked(wxMouseEvent &event) 
{
    //event.Skip();
    if (m_g3uMachine) {
        m_clearFanPressed = !m_clearFanPressed;
        if (m_clearFanPressed) {
            m_idle_filter_button->SetIcon("device_filter");
            Slic3r::GUI::ComClearFanCtrl* clearFan = new Slic3r::GUI::ComClearFanCtrl(OPEN);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, clearFan);
        } else {
            m_idle_filter_button->SetIcon("device_filter_offline");
            Slic3r::GUI::ComClearFanCtrl* clearFan = new Slic3r::GUI::ComClearFanCtrl(CLOSE);
            Slic3r::GUI::MultiComMgr::inst()->putCommand(m_cur_id, clearFan);
        }
    } else {
        if (m_panel_circula_filter) {
            bool bShow = !m_panel_circula_filter->IsShown();
            m_panel_circula_filter->Show(bShow);
            m_panel_circula_filter->Layout();
            if (bShow) {
                m_idle_filter_button->SetBackgroundColor(wxColour(217, 234, 255));
            } else {
                m_idle_filter_button->SetBackgroundColor(wxColour(255, 255, 255));
            }
        }
    }

    if (m_panel_idle_device_info) {
        m_panel_idle_device_info->Hide();
    }
    if (m_idle_device_info_button) {
        m_idle_device_info_button->SetBackgroundColor(wxColour(255, 255, 255));
    }
    Layout();
}

void TempMixDevice::setDeviceInfoBtnIcon(const wxString &icon) 
{ 
    m_idle_device_info_button->SetIcon(icon);
}

void TempMixDevice::modifyTemp(
    wxString nozzleTemp, wxString platformTemp, wxString cavityTemp, int topTemp, int bottomTemp, int chamberTemp)
{
    if (nozzleTemp.compare("/") == 0 && platformTemp.compare("/") == 0 && cavityTemp.compare("/") == 0) {
        m_top_btn->Enable(false);
        m_bottom_btn->Enable(false);
        m_mid_btn->Enable(false);
        m_top_btn->SetTagTemp("--");
        m_bottom_btn->SetTagTemp("--");
        m_mid_btn->SetTagTemp("--");
        m_top_btn->SetCurrTemp("--");
        m_bottom_btn->SetCurrTemp("--");
        m_mid_btn->SetCurrTemp("--");
    } else {
        m_top_btn->Enable(true);
        m_bottom_btn->Enable(true);
        m_mid_btn->Enable(true);
        if (!m_g3uMachine) {
            m_top_btn->SetLabel(nozzleTemp);
            m_bottom_btn->SetLabel(platformTemp);
            m_mid_btn->SetLabel(cavityTemp);
            if (m_right_target_temp != topTemp && !m_top_btn->HasFocus()) {
                m_right_target_temp = topTemp;
                m_top_btn->SetTagTemp(topTemp, true);
            }
            if (m_plat_target_temp != bottomTemp && !m_bottom_btn->HasFocus()) {
                m_plat_target_temp = bottomTemp;
                m_bottom_btn->SetTagTemp(bottomTemp, true);
            }
            if (m_cavity_target_temp != chamberTemp && !m_mid_btn->HasFocus()) {
                m_cavity_target_temp = chamberTemp;
                m_mid_btn->SetTagTemp(chamberTemp, true);
            }
        } else {
            m_top_btn->SetLabel(nozzleTemp);
            m_bottom_btn->SetLabel(platformTemp);
            m_mid_btn->SetLabel(cavityTemp);
            if (m_right_target_temp != topTemp && !m_top_btn->HasFocus()) {
                m_right_target_temp = topTemp;
                m_top_btn->SetTagTemp(topTemp, true);
            }
            if (m_plat_target_temp != bottomTemp && !m_bottom_btn->HasFocus()) {
                m_plat_target_temp = bottomTemp;
                m_bottom_btn->SetTagTemp(bottomTemp, true);
            }
            if (m_cavity_target_temp != chamberTemp && !m_mid_btn->HasFocus()) {
                m_cavity_target_temp = chamberTemp;
                m_mid_btn->SetTagTemp(chamberTemp, true);
            }
        }
    }
}

void TempMixDevice::modifyDeviceInfo(wxString machineType, wxString sprayNozzle, wxString printSize, wxString version, wxString number, wxString material)
{
    m_machine_type_data->SetLabel(machineType);
    m_spray_nozzle_data->SetLabel(sprayNozzle);
    m_print_size_data->SetLabel(printSize);
    m_firmware_version_data->SetLabel(version);
    m_serial_number_data->SetLabel(number);
    m_private_material_data->SetLabel(material);
}

void TempMixDevice::modifyDeviceLampState(bool bOpen) 
{
    if (bOpen) {
        m_idle_lamp_control_button->SetIcon("device_lamp_control_press");
        m_idle_lamp_control_button->Refresh();
        m_idle_lamp_control_button->SetFlashForgeSelected(true);
    } else {
        m_idle_lamp_control_button->SetIcon("device_lamp_control");
        m_idle_lamp_control_button->Refresh();
        m_idle_lamp_control_button->SetFlashForgeSelected(false);
    }
}

void TempMixDevice::modifyDeviceFilterState(bool internalOpen, bool externalOpen) 
{
    m_panel_circula_filter->setBtnState(internalOpen, externalOpen);
}

void TempMixDevice::modifyG3UClearFanState(bool bOpen) 
{
    if (bOpen) {
        m_idle_filter_button->SetIcon("device_filter");
        m_clearFanPressed = true;
    } else {
        m_idle_filter_button->SetIcon("device_filter_offline");
        m_clearFanPressed = false;
    }
}
