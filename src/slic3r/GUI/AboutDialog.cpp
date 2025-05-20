#include "AboutDialog.hpp"
#include <wx/clipbrd.h>
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/Color.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "format.hpp"
#include "Widgets/Button.hpp"
#include "FFUtils.hpp"

namespace Slic3r {
namespace GUI {

AboutDialogLogo::AboutDialogLogo(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
{
    this->SetBackgroundColour(*wxWHITE);
    this->logo = ScalableBitmap(this, Slic3r::var("Orca-Flashforge_192px.png"), wxBITMAP_TYPE_PNG);
    this->SetMinSize(this->logo.GetBmpSize());

    this->Bind(wxEVT_PAINT, &AboutDialogLogo::onRepaint, this);
}

void AboutDialogLogo::onRepaint(wxEvent &event)
{
    wxPaintDC dc(this);
    dc.SetBackgroundMode(wxTRANSPARENT);

    wxSize size = this->GetSize();
    int logo_w = this->logo.GetBmpWidth();
    int logo_h = this->logo.GetBmpHeight();
    dc.DrawBitmap(this->logo.bmp(), (size.GetWidth() - logo_w)/2, (size.GetHeight() - logo_h)/2, true);

    event.Skip();
}


// -----------------------------------------
// CopyrightsDialog
// -----------------------------------------
CopyrightsDialog::CopyrightsDialog()
    : DPIDialog(static_cast<wxWindow*>(wxGetApp().mainframe), wxID_ANY, from_u8((boost::format("%1% - %2%")
        % (wxGetApp().is_editor() ? SLIC3R_APP_FULL_NAME : GCODEVIEWER_APP_NAME)
        % _utf8(L("Portions copyright"))).str()),
        wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    this->SetFont(wxGetApp().normal_font());
	this->SetBackgroundColour(*wxWHITE);

    std::string icon_path = (boost::format("%1%/images/Orca-FlashforgeTitle.ico") % resources_dir()).str();
    SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

    wxStaticLine *staticline1 = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );

	auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add( staticline1, 0, wxEXPAND | wxALL, 5 );

    fill_entries();

    m_html = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition,
                              wxSize(40 * em_unit(), 20 * em_unit()), wxHW_SCROLLBAR_AUTO);
    m_html->SetMinSize(wxSize(FromDIP(870),FromDIP(520)));
    m_html->SetBackgroundColour(*wxWHITE);
    wxFont font = get_default_font(this);
    const int fs = font.GetPointSize();
    const int fs2 = static_cast<int>(1.2f*fs);
    int size[] = { fs, fs, fs, fs, fs2, fs2, fs2 };

    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), size);
    m_html->SetBorders(2);
    m_html->SetPage(get_html_text());

    sizer->Add(m_html, 1, wxEXPAND | wxALL, 15);
    m_html->Bind(wxEVT_HTML_LINK_CLICKED, &CopyrightsDialog::onLinkClicked, this);

    SetSizer(sizer);
    sizer->SetSizeHints(this);
    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
}

void CopyrightsDialog::fill_entries()
{
    m_entries = {
        { "Admesh",                                         "",      "https://admesh.readthedocs.io/" },
        { "Anti-Grain Geometry",                            "",      "http://antigrain.com" },
        { "ArcWelderLib",                                   "",      "https://plugins.octoprint.org/plugins/arc_welder" },
        { "Boost",                                          "",      "http://www.boost.org" },
        { "Cereal",                                         "",      "http://uscilab.github.io/cereal" },
        { "CGAL",                                           "",      "https://www.cgal.org" },
        { "Clipper",                                        "",      "http://www.angusj.co" },
        { "libcurl",                                        "",      "https://curl.se/libcurl" },
        { "Eigen3",                                         "",      "http://eigen.tuxfamily.org" },
        { "Expat",                                          "",      "http://www.libexpat.org" },
        { "fast_float",                                     "",      "https://github.com/fastfloat/fast_float" },
        { "GLEW (The OpenGL Extension Wrangler Library)",   "",      "http://glew.sourceforge.net" },
        { "GLFW",                                           "",      "https://www.glfw.org" },
        { "GNU gettext",                                    "",      "https://www.gnu.org/software/gettext" },
        { "ImGUI",                                          "",      "https://github.com/ocornut/imgui" },
        { "ImGuizmo",                                       "",      "https://github.com/CedricGuillemet/ImGuizmo" },
        { "Libigl",                                         "",      "https://libigl.github.io" },
        { "libnest2d",                                      "",      "https://github.com/tamasmeszaros/libnest2d" },
        { "lib_fts",                                        "",      "https://www.forrestthewoods.com" },
        { "Mesa 3D",                                        "",      "https://mesa3d.org" },
        { "Miniz",                                          "",      "https://github.com/richgel999/miniz" },
        { "Nanosvg",                                        "",      "https://github.com/memononen/nanosvg" },
        { "nlohmann/json",                                  "",      "https://json.nlohmann.me" },
        { "Qhull",                                          "",      "http://qhull.org" },
        { "Open Cascade",                                   "",      "https://www.opencascade.com" },
        { "OpenGL",                                         "",      "https://www.opengl.org" },
        { "PoEdit",                                         "",      "https://poedit.net" },
        { "PrusaSlicer",                                    "",      "https://www.prusa3d.com" },
        { "Real-Time DXT1/DXT5 C compression library",      "",      "https://github.com/Cyan4973/RygsDXTc" },
        { "SemVer",                                         "",      "https://semver.org" },
        { "Shinyprofiler",                                  "",      "https://code.google.com/p/shinyprofiler" },
        { "SuperSlicer",                                    "",      "https://github.com/supermerill/SuperSlicer" },
        { "TBB",                                            "",      "https://www.intel.cn/content/www/cn/zh/developer/tools/oneapi/onetbb.html" },
        { "wxWidgets",                                      "",      "https://www.wxwidgets.org" },
        { "zlib",                                           "",      "http://zlib.net" },

    };
}

wxString CopyrightsDialog::get_html_text()
{
    wxColour bgr_clr = wxGetApp().get_window_default_clr();//wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);

    const auto text_clr = wxGetApp().get_label_clr_default();// wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const auto text_clr_str = encode_color(ColorRGB(text_clr.Red(), text_clr.Green(), text_clr.Blue()));
    const auto bgr_clr_str = encode_color(ColorRGB(bgr_clr.Red(), bgr_clr.Green(), bgr_clr.Blue()));

    const wxString copyright_str = _L("Copyright") + "&copy; ";

    wxString text = wxString::Format(
        "<html>"
            "<body bgcolor= %s link= %s>"
            "<font color=%s>"
                "<font size=\"5\">%s</font><br/>"
                "<font size=\"5\">%s</font>"
                "<a href=\"%s\">%s.</a><br/>"
                "<font size=\"5\">%s.</font><br/>"
                "<br /><br />"
                "<font size=\"5\">%s</font><br/>"
                "<font size=\"5\">%s:</font><br/>"
                "<br />"
                "<font size=\"3\">",
         bgr_clr_str, text_clr_str, text_clr_str,
        _L("License"),
        _L("Orca-Flashforge is licensed under "),
        "https://www.gnu.org/licenses/agpl-3.0.html",_L("GNU Affero General Public License, version 3"),
        _L("Orca Slicer is based on PrusaSlicer and BambuStudio"),
        _L("Libraries"),
        _L("This software uses open source components whose copyright and other proprietary rights belong to their respective owners"));

    for (auto& entry : m_entries) {
        text += format_wxstr(
                    "%s<br/>"
                    , entry.lib_name);

         text += wxString::Format(
                    "<a href=\"%s\">%s</a><br/><br/>"
                    , entry.link, entry.link);
    }

    text += wxString(
                "</font>"
            "</font>"
            "</body>"
        "</html>");

    return text;
}

void CopyrightsDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const wxFont& font = GetFont();
    const int fs = font.GetPointSize();
    const int fs2 = static_cast<int>(1.2f*fs);
    int font_size[] = { fs, fs, fs, fs, fs2, fs2, fs2 };

    m_html->SetFonts(font.GetFaceName(), font.GetFaceName(), font_size);

    const int& em = em_unit();

    msw_buttons_rescale(this, em, { wxID_CLOSE });

    const wxSize& size = wxSize(40 * em, 20 * em);

    m_html->SetMinSize(size);
    m_html->Refresh();

    SetMinSize(size);
    Fit();

    Refresh();
}

void CopyrightsDialog::onLinkClicked(wxHtmlLinkEvent &event)
{
    wxGetApp().open_browser_with_warning_dialog(event.GetLinkInfo().GetHref());
    event.Skip(false);
}

void CopyrightsDialog::onCloseDialog(wxEvent &)
{
     this->EndModal(wxID_CLOSE);
}

AboutDialog::AboutDialog()
    : wxDialog(static_cast<wxWindow *>(wxGetApp().mainframe), wxID_ANY,
        from_u8((boost::format(_utf8(L("About %s"))) % SLIC3R_APP_FULL_NAME).str()))
    , m_backgorundBmp(this, "Orca-Flashforge_about", 400)
{
    SetClientSize(m_backgorundBmp.GetBmpSize());
    SetMinClientSize(m_backgorundBmp.GetBmpSize());
    SetMaxClientSize(m_backgorundBmp.GetBmpSize());
    CenterOnParent();
    Bind(wxEVT_PAINT, &AboutDialog::onPaint, this);
}

void AboutDialog::onPaint(wxPaintEvent &evt)
{
    wxPaintDC dc(this);
    dc.DrawBitmap(m_backgorundBmp.bmp(), 0, 0);
    
    wxString str1 = _L("Tailor-Make Everyone's World");
    dc.SetFont(::Label::Body_12);
    dc.SetTextForeground("#7a7a7a");
    dc.DrawText(str1, FromDIP(600) - dc.GetTextExtent(str1).x / 2, FromDIP(145));

    wxString verStr = wxString("V") + Orca_Flashforge_VERSION;
    dc.SetFont(::Label::Body_14);
    dc.SetTextForeground("#7a7a7a");
    dc.DrawText(verStr, FromDIP(600) - dc.GetTextExtent(verStr).x / 2, FromDIP(168));

    wxString str2 = _L("Based On OrcaSlicer");
    dc.SetFont(::Label::Body_12);
    dc.SetTextForeground("#bababa");
    dc.DrawText(str2, FromDIP(600) - dc.GetTextExtent(str2).x / 2, FromDIP(358));
}

} // namespace GUI
} // namespace Slic3r
