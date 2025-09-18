#ifndef slic3r_GUI_PromoShareDlg_hpp_
#define slic3r_GUI_PromoShareDlg_hpp_

#include <string>
#include <wx/colour.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include "FFTitleLessDialog.hpp"
#include "slic3r/GUI/Widgets/FFButton.hpp"
#include "slic3r/GUI/wxExtensions.hpp"

namespace Slic3r { namespace GUI {

class PromoShareUrlInput : public wxPanel
{
public:
    PromoShareUrlInput(wxWindow *parent);

    const wxString getText() { return m_staticTxt->GetLabelText(); }

    void setText(const wxString &text) { m_staticTxt->SetLabelText(text); }

private:
    void onPaint(wxPaintEvent &event);

    void onSize(wxSizeEvent &event);

private:
    int           m_radius;
    wxColour      m_backgroundColor;
    wxStaticText *m_staticTxt;
};

class PromoShareDlg : public FFTitleLessDialog
{
public:
    PromoShareDlg(wxWindow *parent, const std::string &data);

    int ShowModal();

private:
    void drawBackground(wxBufferedPaintDC &dc, wxGraphicsContext *gc);

    void initData(const std::string &data);

    void initSize();

    void onCopyPromoShareUrl(wxCommandEvent &event);

private:
    PromoShareUrlInput *m_urlInput;
    FFButton           *m_copyBtn;
    wxString            m_title;
    wxString            m_message1;
    wxString            m_message2;
    ScalableBitmap      m_iconBmp;
    ScalableBitmap      m_bg1Bmp;
    ScalableBitmap      m_bg2Bmp;
    const int           m_totalWidth;
    const int           m_contentWidth;
    const int           m_topSpace;
    const int           m_iconHeight;
    const int           m_iconSpace;
    const int           m_titleSpace;
    const int           m_message1Space;
    const int           m_urlInputSpace;
    const int           m_buttonSpace;
    const int           m_message2Space;
    wxSize              m_titleSize;
    wxSize              m_message1Size;
    wxSize              m_message2Size;
};

}} // namespace Slic3r::GUI

#endif
