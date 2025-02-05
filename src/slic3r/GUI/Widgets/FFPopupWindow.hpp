#ifndef slic3r_GUI_FFPopupWindow_hpp_
#define slic3r_GUI_FFPopupWindow_hpp_
#include <wx/nonownedwnd.h>
#include <wx/popupwin.h>


class FFPopupWindow : public wxPopupWindow
{
public:
    FFPopupWindow(wxWindow* parent);
    virtual ~FFPopupWindow();

    virtual void Popup(wxWindow* focus = nullptr);
    virtual void Dismiss();
    virtual void OnDismiss();
    
protected:
    bool Show(bool show = true) override;
    virtual void ProcessLeftDown(const wxPoint& pnt) {}
    virtual void ProcessLeftUp(const wxPoint& pnt) {}
    virtual void ProcessMotion(const wxPoint& pnt) {}

private:
    void onPaint(wxPaintEvent& event);
    void onShow(wxShowEvent& event);
    void onActivateApp(wxActivateEvent& event);
    void onCaptureMouseLost(wxMouseCaptureLostEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onMotion(wxMouseEvent& event);

private:
    bool    m_leftPressed {false};
};

#endif
