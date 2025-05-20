#ifndef _Slic3r_GUI_FFToggleButton_hpp_
#define _Slic3r_GUI_FFToggleButton_hpp_
#include <wx/tglbtn.h>


class FFToggleButton : public wxToggleButton
{
public:
	FFToggleButton(wxWindow* parent, const wxString& label = "", wxWindowID id = wxID_ANY);
	~FFToggleButton() {};

	void SetValue(bool state) override;
	void setNormalColor(const wxColour& color);
	void setNormalHoverColor(const wxColour& color);
	void setNormalPressColor(const wxColour& color);
	void setSelectColor(const wxColour& color);
	void setSelectHoverColor(const wxColour& color);
	void setSelectPressColor(const wxColour& color);

private:
	void updateState();

private:
	bool		m_hoverFlag {false};
	bool		m_pressFlag {false};
	wxColour	m_normalColor {"#999999"};
	wxColour	m_normalHoverColor {"#95C5FF"};
	wxColour	m_normalPressColor {"#328DFB"};
	wxColour	m_selectColor {"#328DFB"};
	wxColour	m_selectHoverColor {"#116FDF"};
	wxColour	m_selectPressColor {"#999999"};
};


class FFBitmapToggleButton : public wxBitmapToggleButton
{
public:
	FFBitmapToggleButton(wxWindow* parent, wxWindowID id = wxID_ANY,
		const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	~FFBitmapToggleButton() {};

	void SetValue(bool state) override;
	void setNormalBitmap(const wxBitmap& bmp);
	void setNormalHoverBitmap(const wxBitmap& bmp);
	void setNormalPressBitmap(const wxBitmap& bmp);
	void setSelectBitmap(const wxBitmap& bmp);
	void setSelectHoverBitmap(const wxBitmap& bmp);
	void setSelectPressBitmap(const wxBitmap& bmp);

private:
	void updateState();

private:
	bool		m_hoverFlag {false};
	bool		m_pressFlag {false};
	wxBitmap	m_normalBitmap;
	wxBitmap	m_normalHoverBitmap;
	wxBitmap	m_normalPressBitmap;
	wxBitmap	m_selectBitmap;
	wxBitmap	m_selectHoverBitmap;
	wxBitmap	m_selectPressBitmap;

};

#endif /* _Slic3r_GUI_FFToggleButton_hpp_ */