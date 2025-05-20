#ifndef _slic3r_gui_FFUtils_hpp_
#define _slic3r_gui_FFUtils_hpp_
#include <string.h>
#include <wx/string.h>

namespace Slic3r::GUI
{

class FFUtils
{
public:
	static wxString getBitmapFileName(unsigned short pid);

	static std::string getPrinterName(unsigned short pid);

	static std::string getPrinterModelId(unsigned short pid);

	static bool isPrinterSupportAms(const std::string &modelId);
    static bool isPrinterSupportCoolingFan(const std::string& modelId);
    static bool isPrinterSupportDeviceFilter(const std::string& modelId);
   
	static wxString convertStatus(const std::string& status);
	static wxString convertStatus(const std::string& status, wxColour& color);

	static wxString converDeviceError(const std::string &error);

	static std::string utf8Substr(const std::string& str, int start, int length);

	static std::string truncateString(const std::string &s, size_t length);
    static std::string wxString2StdString(const wxString& str);

	static wxString trimString(wxDC &dc, const wxString &str, int width);
    static wxString elideString(wxWindow* wnd, const wxString& str, int width);
	static wxString elideString(wxWindow* wnd, const wxString& str, int width, int lines);
	static wxString wrapString(wxWindow* wnd, const wxString& str, int width);
	static wxString wrapString(wxDC &dc, const wxString &str, int width);
	static int getStringLines(const wxString& str);

	static std::string flashforgeWebsite();
	static wxString privacyPolicy();
	static wxString userAgreement();
	static wxString userRegister();
	static wxString passwordForget();
};

}

#endif /* _slic3r_gui_FFUtils_hpp_ */
