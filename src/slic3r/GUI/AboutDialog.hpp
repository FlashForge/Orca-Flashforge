#ifndef slic3r_GUI_AboutDialog_hpp_
#define slic3r_GUI_AboutDialog_hpp_

#include <wx/wx.h>
#include <wx/intl.h>
#include <wx/html/htmlwin.h>

#include "GUI_Utils.hpp"
#include "wxExtensions.hpp"

namespace Slic3r { 
namespace GUI {

class AboutDialogLogo : public wxPanel
{
public:
    AboutDialogLogo(wxWindow* parent);
    
private:
    ScalableBitmap logo;
    void onRepaint(wxEvent &event);
};



class CopyrightsDialog : public DPIDialog
{
public:
    CopyrightsDialog();
    ~CopyrightsDialog() {}

    struct Entry {
        Entry(const std::string &lib_name, const std::string &copyright, const std::string &link) : 
            lib_name(lib_name), copyright(copyright), link(link) {}

        std::string     lib_name;
        std::string     copyright;
        std::string   	link;
    };

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    
private:
    wxHtmlWindow*   m_html;
    std::vector<Entry> m_entries;

    void onLinkClicked(wxHtmlLinkEvent &event);
    void onCloseDialog(wxEvent &);

    void fill_entries();
    wxString get_html_text();
};



class AboutDialog : public wxDialog
{
public:
    AboutDialog();

protected:
    void onPaint(wxPaintEvent &evt);

private:
    ScalableBitmap m_backgorundBmp;
};

} // namespace GUI
} // namespace Slic3r

#endif
