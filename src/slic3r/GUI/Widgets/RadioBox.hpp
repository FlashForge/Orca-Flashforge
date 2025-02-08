#ifndef slic3r_GUI_RADIOBOX_hpp_
#define slic3r_GUI_RADIOBOX_hpp_

#include "../wxExtensions.hpp"

#include <wx/tglbtn.h>

namespace Slic3r { 
namespace GUI {

class RadioBox : public wxBitmapToggleButton
{
public:
    RadioBox(wxWindow *parent);

public:
    void SetValue(bool value) override;
	bool GetValue();
    void Rescale();
    bool Disable() { 
        return wxBitmapToggleButton::Disable(); 
    }
    bool Enable() { 
        return wxBitmapToggleButton::Enable(); 
    }

private:
    void update();

private:
    ScalableBitmap m_on;
    ScalableBitmap m_off;
};

class RadioButton : public wxBitmapToggleButton
{
public:
    RadioButton(wxWindow* parent);
    ~RadioButton();
    void SetValue(bool value) override;
    bool GetValue();
    bool Disable() { return wxBitmapToggleButton::Disable(); }
    bool Enable() { return wxBitmapToggleButton::Enable(); }
    enum PaintMode { Normal = 0, Hover = 1};

protected:
    void paintEvent(wxPaintEvent& event);
    void OnMouseEnter(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);

private:
    void connectEvent();

private:
    PaintMode      m_mode;
    ScalableBitmap m_on_normal;
    ScalableBitmap m_on_hover;
    ScalableBitmap m_off_normal;
    ScalableBitmap m_off_hover;
};





}}//namespace



#endif // !slic3r_GUI_CheckBox_hpp_
