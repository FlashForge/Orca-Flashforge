#ifndef _Slic3r_GUI_FFButton_hpp_
#define _Slic3r_GUI_FFButton_hpp_
#include <wx/tglbtn.h>


class FFButton : public wxButton
{
public:
	FFButton(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& label = "", int borderRadius = 4, bool borderFlag = true);
	~FFButton() {};

	void SetFontColor(const wxColour& color);
	void SetFontHoverColor(const wxColour& color);
	void SetFontPressColor(const wxColour& color);
	void SetFontDisableColor(const wxColour& color);
	void SetBorderColor(const wxColour& color);
	void SetBorderHoverColor(const wxColour& color);
	void SetBorderPressColor(const wxColour& color);
	void SetBorderDisableColor(const wxColour& color);
	void SetBGColor(const wxColour& color);
	void SetBGHoverColor(const wxColour& color);
	void SetBGPressColor(const wxColour& color);
	void SetBGDisableColor(const wxColour& color);

protected:
	void OnPaint(wxPaintEvent& event);	
    void render(wxDC &dc);

private:
	void updateState();

private:
	bool		m_hoverFlag;
	bool		m_pressFlag;
	bool		m_borderFlag;
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
};

#endif /* _Slic3r_GUI_FFButton_hpp_ */