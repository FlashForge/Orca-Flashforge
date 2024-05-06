#include "Button.hpp"
#include "Label.hpp"

#include <wx/dcgraph.h>

BEGIN_EVENT_TABLE(Button, StaticBox)

EVT_LEFT_DOWN(Button::mouseDown)
EVT_LEFT_UP(Button::mouseReleased)
EVT_ENTER_WINDOW(Button::mouseEnter)
EVT_LEAVE_WINDOW(Button::mouseLeave)
EVT_MOUSE_CAPTURE_LOST(Button::mouseCaptureLost)
EVT_KEY_DOWN(Button::keyDownUp)
EVT_KEY_UP(Button::keyDownUp)

// catch paint events
EVT_PAINT(Button::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

Button::Button()
    : paddingSize(10, 8)
{
    background_color = StateColor(
        std::make_pair(0xF0F0F1, (int) StateColor::Disabled),
        std::make_pair(0x52c7b8, (int) StateColor::Hovered | StateColor::Checked),
        std::make_pair(0x009688, (int) StateColor::Checked),
        std::make_pair(*wxLIGHT_GREY, (int) StateColor::Hovered), 
        std::make_pair(*wxWHITE, (int) StateColor::Normal));
    text_color       = StateColor(
        std::make_pair(*wxLIGHT_GREY, (int) StateColor::Disabled), 
        std::make_pair(*wxBLACK, (int) StateColor::Normal));
}

Button::Button(wxWindow* parent, wxString text, wxString icon, long style, int iconSize, wxWindowID btn_id)
    : Button()
{
    Create(parent, text, icon, style, iconSize, btn_id);
}

bool Button::Create(wxWindow* parent, wxString text, wxString icon, long style, int iconSize, wxWindowID btn_id)
{
    StaticBox::Create(parent, btn_id, wxDefaultPosition, wxDefaultSize, style);
    state_handler.attach({&text_color});
    state_handler.update_binds();
    //BBS set default font
    SetFont(Label::Body_14);
    wxWindow::SetLabel(text);
    if (!icon.IsEmpty()) {
        //BBS set button icon default size to 20
        this->active_icon = ScalableBitmap(this, icon.ToStdString(), iconSize > 0 ? iconSize : 20);
    }
    messureSize();
    return true;
}

void Button::SetLabel(const wxString& label)
{
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
}

bool Button::SetFont(const wxFont& font)
{
    wxWindow::SetFont(font);
    messureSize();
    Refresh();
    return true;
}

void Button::SetIcon(const wxString& icon)
{
    if (!icon.IsEmpty()) {
        //BBS set button icon default size to 20
        this->active_icon = ScalableBitmap(this, icon.ToStdString(), this->active_icon.px_cnt());
    }
    else
    {
        this->active_icon = ScalableBitmap();
    }
    Refresh();
}

void Button::SetInactiveIcon(const wxString &icon)
{
    if (!icon.IsEmpty()) {
        // BBS set button icon default size to 20
        this->inactive_icon = ScalableBitmap(this, icon.ToStdString(), this->active_icon.px_cnt());
    } else {
        this->inactive_icon = ScalableBitmap();
    }
    Refresh();
}

void Button::SetMinSize(const wxSize& size)
{
    minSize = size;
    messureSize();
}

void Button::SetPaddingSize(const wxSize& size)
{
    paddingSize = size;
    messureSize();
}

void Button::SetTextColor(StateColor const& color)
{
    text_color = color;
    state_handler.update_binds();
    Refresh();
}

void Button::SetTextColorNormal(wxColor const &color)
{
    text_color.setColorForStates(color, 0);
    Refresh();
}

bool Button::Enable(bool enable)
{
    bool result = wxWindow::Enable(enable);
    if (result) {
        wxCommandEvent e(EVT_ENABLE_CHANGED);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(e);
    }
    return result;
}

void Button::SetCanFocus(bool canFocus) { this->canFocus = canFocus; }

void Button::SetValue(bool state)
{
    if (GetValue() == state) return;
    state_handler.set_state(state ? StateHandler::Checked : 0, StateHandler::Checked);
}

bool Button::GetValue() const { return state_handler.states() & StateHandler::Checked; }

void Button::Rescale()
{
    if (this->active_icon.bmp().IsOk())
        this->active_icon.msw_rescale();

    if (this->inactive_icon.bmp().IsOk())
        this->inactive_icon.msw_rescale();

    messureSize();
}

void Button::SetFlashForge(bool bFlashForge) 
{
    m_flashforge = bFlashForge; 
}

void Button::SetPureText(bool bPureText) 
{
    m_pure_text = bPureText;
}

void Button::paintEvent(wxPaintEvent& evt)
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
void Button::render(wxDC& dc)
{
    StaticBox::render(dc);
    int states = state_handler.states();
    wxSize size = GetSize();
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    // calc content size
    wxSize szIcon;
    wxSize szContent = textSize.GetSize();

    ScalableBitmap icon;
    if (m_selected || ((states & (int)StateColor::State::Hovered) != 0))
        icon = active_icon;
    else
        icon = inactive_icon;
    int padding = 5;
    if (icon.bmp().IsOk()) {
        if (szContent.y > 0) {
            //BBS norrow size between text and icon
            szContent.x += padding;
        }
        szIcon = icon.GetBmpSize();
        szContent.x += szIcon.x;
        if (szIcon.y > szContent.y) {
            szContent.y = szIcon.y;
            //szContent.y += 10; 
        }
        if (szContent.x > size.x) {
            int d = std::min(padding, szContent.x - size.x);
            padding -= d;
            szContent.x -= d;
        }
    }
    // move to center
    wxRect rcContent = { {0, 0}, size };
    wxSize offset = (size - szContent) / 2;
    if (offset.x < 0) offset.x = 0;
    rcContent.Deflate(offset.x, offset.y);
    // start draw
    wxPoint pt = rcContent.GetLeftTop();
    if (icon.bmp().IsOk()) {
        pt.y += (rcContent.height - szIcon.y) / 2;
        dc.DrawBitmap(icon.bmp(), pt);
        //BBS norrow size between text and icon
        pt.x += szIcon.x + padding;
        pt.y = rcContent.y;
    }
    auto text = GetLabel();
    if (!text.IsEmpty()) {
        if (pt.x + textSize.width > size.x)
            text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, size.x - pt.x);
        pt.y += (rcContent.height - textSize.height) / 2;
        dc.SetTextForeground(text_color.colorForStates(states));
#if 0
        dc.SetBrush(*wxLIGHT_GREY);
        dc.SetPen(wxPen(*wxLIGHT_GREY));
        dc.DrawRectangle(pt, textSize.GetSize());
#endif
#ifdef __WXOSX__
        pt.y -= textSize.x / 2;
#endif
        dc.DrawText(text, pt);
    }
}

void Button::messureSize()
{
    wxClientDC dc(this);
    dc.GetTextExtent(GetLabel(), &textSize.width, &textSize.height, &textSize.x, &textSize.y);
    wxSize szContent = textSize.GetSize();
    if (this->active_icon.bmp().IsOk()) {
        if (szContent.y > 0) {
            //BBS norrow size between text and icon
            szContent.x += 5;
        }
        wxSize szIcon = this->active_icon.GetBmpSize();
        szContent.x += szIcon.x;
        if (szIcon.y > szContent.y)
            szContent.y = szIcon.y;
    }
    wxSize size = szContent + paddingSize * 2;
    //size.SetHeight(size.GetHeight() + 10);
    //minSize.SetHeight(minSize.GetHeight() + 10);
    if (minSize.GetHeight() > 0)
        size.SetHeight(minSize.GetHeight());

    if (minSize.GetWidth() > size.GetWidth())
        wxWindow::SetMinSize(minSize);
    else
        wxWindow::SetMinSize(size);
}

void Button::mouseDown(wxMouseEvent& event)
{
    event.Skip();
    pressedDown = true;
    if (m_flashforge) {
        SetBorderWidth(0);
        SetBorderColor(wxColour(255, 255, 255));
        SetBackgroundColor(wxColour(217, 234, 255));
    }
    if (m_pure_text) {
        SetBorderColor(wxColour(17, 111, 223));
        SetTextColor(wxColour(17, 111, 223));
    }
    if (canFocus)
        SetFocus();
    if (!HasCapture())
        CaptureMouse();
}

void Button::mouseReleased(wxMouseEvent& event)
{
    event.Skip();
    if (pressedDown) {
        pressedDown = false;
        if (m_flashforge) {
            SetBorderWidth(0);
            SetBorderColor(wxColour(255, 255, 255));
            SetBackgroundColor(wxColour(255, 255, 255));
        }
        if (HasCapture())
            ReleaseMouse();
        if (wxRect({0, 0}, GetSize()).Contains(event.GetPosition()))
            sendButtonEvent();
    }
}

void Button::mouseEnter(wxMouseEvent &event)
{ 
    event.Skip(); 
    if (m_flashforge) {
        SetBorderWidth(1);
        SetBorderColor(wxColour(50, 141, 251));
    }
    if (m_pure_text) {
        SetBorderColor(wxColour(149, 197, 255));
        SetTextColor(wxColour(149, 197, 255));
    }
}

void Button::mouseLeave(wxMouseEvent &event) 
{
    event.Skip();
    if (m_flashforge) {
        SetBorderWidth(0);
        SetBorderColor(wxColour(255, 255, 255));
        SetBackgroundColor(wxColour(255, 255, 255));
    }
    if (m_pure_text) {
        SetBorderColor(wxColour(50, 141, 251));
        SetTextColor(wxColour(50, 141, 251));
    }
}

void Button::mouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    wxMouseEvent evt;
    mouseReleased(evt);
}

void Button::keyDownUp(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_SPACE || event.GetKeyCode() == WXK_RETURN) {
        wxMouseEvent evt(event.GetEventType() == wxEVT_KEY_UP ? wxEVT_LEFT_UP : wxEVT_LEFT_DOWN);
        event.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        return;
    }
    if (event.GetEventType() == wxEVT_KEY_DOWN &&
        (event.GetKeyCode() == WXK_TAB || event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT 
        || event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_DOWN))
        HandleAsNavigationKey(event);
    else
        event.Skip();
}

void Button::sendButtonEvent()
{
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}

TempButton::TempButton(wxWindow *parent, wxString text, wxString icon, long style, int iconSize, wxWindowID btn_id) 
    : Button(parent, text, icon, style, iconSize, btn_id)
{ 
    
}

void TempButton::doRender(wxDC &dc)
{ 
    wxSize size   = GetSize();
    int    states = StateColor::Enabled;
    if (background_color2.count() == 0) {
        if ((border_width && border_color.count() > 0) || background_color.count() > 0) {
            wxRect rc(0, 0, size.x, size.y);
            if (border_width && border_color.count() > 0) {
                if (dc.GetContentScaleFactor() == 1.0) {
                    int d  = floor(border_width / 2.0);
                    int d2 = floor(border_width - 1);
                    rc.x += d;
                    rc.width -= d2;
                    rc.y += d;
                    rc.height -= d2;
                } else {
                    int d = 1;
                    rc.x += d;
                    rc.width -= d;
                    rc.y += d;
                    rc.height -= d;
                }
                dc.SetPen(wxPen(border_color.colorForStates(states), border_width));
            } else {
                dc.SetPen(wxPen(background_color.colorForStates(states)));
            }
            if (background_color.count() > 0)
                dc.SetBrush(wxBrush(background_color.colorForStates(states)));
            else
                dc.SetBrush(wxBrush(GetBackgroundColour()));
            if (radius == 0) {
                dc.DrawRectangle(rc);
            } else {
                dc.DrawRoundedRectangle(rc, radius - border_width);
            }
        }
    } else {
        wxColor start = background_color.colorForStates(states);
        wxColor stop  = background_color2.colorForStates(states);
        int     r = start.Red(), g = start.Green(), b = start.Blue();
        int     dr = (int) stop.Red() - r, dg = (int) stop.Green() - g, db = (int) stop.Blue() - b;
        int     lr = 0, lg = 0, lb = 0;
        for (int y = 0; y < size.y; ++y) {
            dc.SetPen(wxPen(wxColor(r, g, b)));
            dc.DrawLine(0, y, size.x, y);
            lr += dr;
            while (lr >= size.y) {
                ++r, lr -= size.y;
            }
            while (lr <= -size.y) {
                --r, lr += size.y;
            }
            lg += dg;
            while (lg >= size.y) {
                ++g, lg -= size.y;
            }
            while (lg <= -size.y) {
                --g, lg += size.y;
            }
            lb += db;
            while (lb >= size.y) {
                ++b, lb -= size.y;
            }
            while (lb <= -size.y) {
                --b, lb += size.y;
            }
        }
    }
}

#ifdef __WIN32__

WXLRESULT Button::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
    if (nMsg == WM_GETDLGCODE) { return DLGC_WANTMESSAGE; }
    if (nMsg == WM_KEYDOWN) {
        wxKeyEvent event(CreateKeyEvent(wxEVT_KEY_DOWN, wParam, lParam));
        switch (wParam) {
        case WXK_RETURN: { // WXK_RETURN key is handled by default button
            GetEventHandler()->ProcessEvent(event);
            return 0;
        }
        }
    }
    return wxWindow::MSWWindowProc(nMsg, wParam, lParam);
}

#endif

bool Button::AcceptsFocus() const { return canFocus; }
