#ifndef _Slic3r_GUI_FFButton_hpp_
#define _Slic3r_GUI_FFButton_hpp_
#include <wx/window.h>
#include <wx/button.h>


class FFButton : public wxWindow
{
public:
	FFButton(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& label = "", int borderRadius = 4, bool borderFlag = true);
	~FFButton() {};

	bool Enable(bool enable = true) override;
    void SetEnable(bool enable = true);
	void SetLabel(const wxString & label) override;
	void SetFontColor(const wxColour& color);
	void SetFontHoverColor(const wxColour& color);
	void SetFontPressColor(const wxColour& color);
	void SetFontDisableColor(const wxColour& color);
	void SetFontUniformColor(const wxColour& color);
	void SetBorderColor(const wxColour& color);
	void SetBorderHoverColor(const wxColour& color);
	void SetBorderPressColor(const wxColour& color);
	void SetBorderDisableColor(const wxColour& color);
	void SetBGColor(const wxColour& color);
	void SetBGHoverColor(const wxColour& color);
	void SetBGPressColor(const wxColour& color);
	void SetBGDisableColor(const wxColour& color);
	void SetBGUniformColor(const wxColour& color);

protected:
	void OnPaint(wxPaintEvent& event);	
    void render(wxDC &dc);

private:
	void updateState();
	void sendEvent();

private:
	bool		m_hoverFlag;
	bool		m_pressFlag;
	bool		m_borderFlag;
    bool        m_enable;
	int			m_borderRadius;
	wxColour	m_fontColor;
	wxColour	m_fontHoverColor;
	wxColour	m_fontPressColor;
	wxColour	m_fontDisableColor;
	wxColour	m_borderColor;
	wxColour	m_borderHoverColor;
	wxColour	m_borderPressColor;
	wxColour	m_borderDisableColor;
	wxColour	m_bgColor;
	wxColour	m_bgHoverColor;
	wxColour	m_bgPressColor;
	wxColour	m_bgDisableColor;
	wxString	m_text;
};

class FFPushButton : public wxButton
{
public:
    FFPushButton(wxWindow *parent, wxWindowID id = wxID_ANY, const wxString &normalIcon = "",const wxString &hoverIcon = "",const wxString &pressIcon = "",const wxString &disableIcon = "",const int iconSize = 16);
    ~FFPushButton(){};

	void OnMousePress(wxMouseEvent &event)
    {
        m_isPressed = true;
        Refresh();
    }

    void OnMouseRelease(wxMouseEvent &event)
    {
        m_isPressed = false;
        Refresh();
    }

    void OnMouseEnter(wxMouseEvent &event)
    {
        m_isHover = true;
        Refresh();
    }

    void OnMouseLeave(wxMouseEvent &event)
    {
        m_isHover = false;
        Refresh();
    }
    void OnPaint(wxPaintEvent &event);

private:
    wxString m_normalIcon;
    wxString m_pressIcon;
    wxString m_hoverIcon;
    wxString m_disableIcon;
    wxBitmap m_normalBitmap;
    wxBitmap m_pressBitmap;
    wxBitmap m_hoverBitmap;
    wxBitmap m_disableBitmap;
    bool     m_isPressed = false;
    bool     m_isHover   = false;
};
#endif /* _Slic3r_GUI_FFButton_hpp_ */