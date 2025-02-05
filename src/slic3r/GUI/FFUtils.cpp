#include "FFUtils.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "GUI_App.hpp"

namespace Slic3r::GUI
{

wxString FFUtils::getBitmapFileName(unsigned short pid)
{
	wxString str;
	switch (pid) {
	case 0x0023:
		str = "adventurer_5m";
		break;
	case 0x0024:
		str = "adventurer_5m_pro";
		break;
    case 0x001F:
        str = "guider_3_ultra";
        break;
	}
	return str;
}

std::string FFUtils::getPrinterName(unsigned short pid)
{
	std::string str;
	switch (pid) {
	case 0x0023:
		str = "Adventurer 5M";
		break;
	case 0x0024:
		str = "Adventurer 5M Pro";
		break;
    case 0x001F:
        str = "Guider 3 Ultra";
        break;
	}
	return str;
}

std::string FFUtils::getPrinterModelId(unsigned short pid)
{
    std::string str;
	switch (pid) {
	case 0x0023:
		str = "Flashforge-Adventurer-5M";
		break;
	case 0x0024:
		str = "Flashforge-Adventurer-5M-Pro";
		break;
    case 0x001F:
        str = "Flashforge-Guider-3-Ultra";
        break;
	}
	return str;
}

wxString FFUtils::convertStatus(const std::string& status, wxColour& color)
{
	wxString st = _L("Idle");
    color = wxColour("#00CD6D");
    if ("offline" == status) {
        st = _L("Offline");
        color = wxColour("#999999");
    } else {
        if ("printing" == status || "canceling" == status) {
            st = _L("Printing");
            color = wxColour("#4D54FF");
        } else if ("pause" == status || "pausing" == status) {
            st = _L("Paused");
            color = wxColour("#982187");
        } else if ("error" == status) {
            st = _L("Error");
            color = wxColour("#FD4A29");
        } else if ("busy" == status || "calibrate_doing" == status || "heating" == status) {
            st = _L("Busy");
            color = wxColour("#F9B61C");
        } else if ("completed" == status || "cancel" == status) {
            st = _L("Completed");
            color = wxColour("#328DFB");
        }
        //} else if ("cancel" == status || "canceling" == status) {
        //    st = _L("Cancel");
        //    color = wxColour("#328DFB");
        //}
        //} else if ("heating" == rawstatus) {
        //    status = _L("Heating");
        //    //color = wxColour("");
        //}
    }
    return st;
}

wxString FFUtils::converDeviceError(const std::string &error) 
{
    wxString st = _L("The printer move out of range.Please go home again!");
    if ("E0001" == error) {
        st = _L("The printer move out of range.Please go home again!");
    } else if ("E0002" == error) {
        st = _L("Lost communication with MCU eboard");
    } else if ("E0003" == error) {
        st = _L("TMC reports error:GSTAT");
    } else if ("E0004" == error) {
        st = _L("Unable to read tmc");
    } else if ("E0005" == error) {
        st = _L("MCU eboard: Unable to connect");
    } else if ("E0006" == error) {
        st = _L("Nozzle temperature error.");
    } else if ("E0007" == error) {
        st = _L("Extruder not heating at expected.Please check extruder.");
    } else if ("E0008" == error) {
        st = _L("Extruder not heating at expected.Please check extruder.");
    } else if ("E0009" == error) {
        st = _L("Platform not heating at expected.Please check platform.");
    } else if ("E0010" == error) {
        st = _L("Chamber not heating at expected.Please check chamber.");
    } else if ("E0011" == error) {
        st = _L("Host error, please restart !");
    } else if ("E0012" == error) {
        st = _L("Z-Axis go home error.");
    } else if ("E0013" == error) {
        st = _L("Y-Axis go home error.");
    } else if ("E0014" == error) {
        st = _L("X-Axis go home error.");
    } else if ("E0015" == error) {
        st = _L("Extruder temperature error !");
    } else if ("E0016" == error) {
        st = _L("Platform temperature error !");
    } else if ("E0017" == error) {
        st = _L("Move queue overflow");
    } else if ("E0018" == error) {
        st = _L("No filament");
    }
    return st;
}

std::string FFUtils::utf8Substr(const std::string &str, int start, int length) 
{
    int bytes = 0;
    int i = 0;
    for (i = start, bytes = 0; i < str.length() && bytes < length; ++i) {
        if ((str[i] & 0xC0) != 0x80) {
            ++bytes;
        }
    }
    return str.substr(start, i - start);
}

std::string FFUtils::truncateString(const std::string &s, size_t length) 
{
    if (s.length() > length) {
        std::string transName = wxString::FromUTF8(s).ToStdString();
        std::string trunkName = utf8Substr(transName, 0, length);
        return trunkName + "...";
    } else {
        return wxString::FromUTF8(s).ToStdString();
    }
}

std::string FFUtils::wxString2StdString(const wxString& str)
{
    return std::string(str.utf8_str().data(), str.utf8_str().length()); 
}

wxString FFUtils::trimString(wxDC &dc, const wxString &str, int width)
{
    wxString clipText = str;
    int      clipw    = 0;
    if (dc.GetTextExtent(str).x > width) {
        for (int i = 0; i < str.length(); ++i) {
            clipText = str.Left(i) + wxT("...");
            clipw    = dc.GetTextExtent(clipText).x;
            if (clipw + dc.GetTextExtent(wxT("...")).x > width) {
                break;
            }
        }
    }
    return clipText;
}

wxString FFUtils::elideString(wxWindow* wnd, const wxString& str, int width)
{
    if (!wnd) return str;
    wxString elide_str = str;
    int      elide_width    = 0;
    if (wnd->GetTextExtent(str).x > width) {
        for (int i = 0; i < str.length(); ++i) {
            elide_str = str.Left(i) + wxT("...");
            elide_width    = wnd->GetTextExtent(elide_str).x;
            if (elide_width + wnd->GetTextExtent(wxT("...")).x > width) {
                break;
            }
        }
    }
    return elide_str;
}

wxString FFUtils::elideString(wxWindow* wnd, const wxString& str, int width, int lines)
{
    if (!wnd || wnd->GetTextExtent(str).x <= width) return str;
    if (lines <= 0) {
        lines = 1;
    }
    wxString elide_str;
    wxString _str = str;
    while (lines > 0 && !_str.empty()) {
        if (wnd->GetTextExtent(_str).x <= width) {
            elide_str += _str;
            break;
        }
        wxString append_str;
        int elide_width = 0;
        if (lines == 1) {
            append_str = "...";
            elide_width = wnd->GetTextExtent(append_str).x;
        }
        wxString tmp_str;
        for (size_t i = 0; i < _str.length(); ++i) {
            elide_width += wnd->GetTextExtent(_str[i]).x;
            if (elide_width > width) {
                if (lines == 1) {
                    elide_str += _str.Left(i) + append_str;
                    _str = "";
                } else {
                    elide_str += _str.Left(i) + "\n";
                    _str = _str.substr(i);
                }
                break;
            }
        }
        --lines;
    }
    return elide_str;
}

wxString FFUtils::wrapString(wxWindow* wnd, const wxString& str, int width)
{
    if (!wnd || wnd->GetTextExtent(str).x <= width) return str;

    wxString wrap_str;
    wxString _str = str;
    while (!_str.empty()) {
        if (wnd->GetTextExtent(_str).x <= width) {
            wrap_str += _str;
            break;
        }
        int wrap_width = 0;
        wxString tmp_str;
        for (size_t i = 0; i < _str.length(); ++i) {
            wrap_width += wnd->GetTextExtent(_str[i]).x;
            if (wrap_width > width) {
                wrap_str += _str.Left(i) + "\n";
                _str = _str.substr(i);
                break;
            }
        }
    }
    return wrap_str;
}

int FFUtils::getStringLines(const wxString& str)
{
    int lines = 1;
    size_t pos   = 0;
    while ((pos = str.find('\n', pos)) != wxString::npos) {
        ++lines;
        ++pos;
    }
    return lines;
}

std::string FFUtils::flashforgeWebsite()
{
    std::string code = Slic3r::GUI::wxGetApp().app_config->get("language");
    if (code == "zh_CN") {
        return "https://www.sz3dp.com";
    }
    return "https://www.flashforge.com";
}

wxString FFUtils::privacyPolicy()
{
    std::string code = Slic3r::GUI::wxGetApp().app_config->get("language");
    if (code == "zh_CN") {
        return "https://auth.flashforge.com/privacyPolicy";
    }
    return "https://auth.flashforge.com/en/privacyPolicy";
}

wxString FFUtils::userAgreement()
{
    std::string code = Slic3r::GUI::wxGetApp().app_config->get("language");
    if (code == "zh_CN") {
        return "https://auth.flashforge.com/userAgreement";
    }
    return "https://auth.flashforge.com/en/userAgreement";
}

wxString FFUtils::userRegister()
{
    std::string code = Slic3r::GUI::wxGetApp().app_config->get("language");
    if (code == "zh_CN") {
        return "https://auth.flashforge.com/zh/signUp/?channel=Orca";
    }else if(code.compare("fr_FR") == 0){
		return "https://auth.flashforge.com/fr/signUp/?channel=Orca";
	}else if(code.compare("es_ES") == 0){
		return "https://auth.flashforge.com/es/signUp/?channel=Orca";
	}else if(code.compare("de_DE") == 0){
		return "https://auth.flashforge.com/de/signUp/?channel=Orca";
	}else if(code.compare("ja_JP") == 0){
		return "https://auth.flashforge.com/ja/signUp/?channel=Orca";
	}else if(code.compare("ko_KR") == 0){
		return "https://auth.flashforge.com/ko/signUp/?channel=Orca";
	}else if(code.compare("lt_LT") == 0){
		return "https://auth.flashforge.com/lt/signUp/?channel=Orca";
	}
    return "https://auth.flashforge.com/en/signUp/?channel=Orca";
}

wxString FFUtils::passwordForget()
{
    std::string code = Slic3r::GUI::wxGetApp().app_config->get("language");
    if (code == "zh_CN") {
        return "https://auth.flashforge.com/zh/resetPassword/?channel=Orca";
    }else if(code.compare("fr_FR") == 0){
		return "https://auth.flashforge.com/fr/resetPassword/?channel=Orca";
	}else if(code.compare("es_ES") == 0){
		return "https://auth.flashforge.com/es/resetPassword/?channel=Orca";
	}else if(code.compare("de_DE") == 0){
		return "https://auth.flashforge.com/de/resetPassword/?channel=Orca";
	}else if(code.compare("ja_JP") == 0){
		return "https://auth.flashforge.com/ja/resetPassword/?channel=Orca";
	}else if(code.compare("ko_KR") == 0){
		return "https://auth.flashforge.com/ko/resetPassword/?channel=Orca";
	}else if(code.compare("lt_LT") == 0){
		return "https://auth.flashforge.com/lt/resetPassword/?channel=Orca";
	}
    return "https://auth.flashforge.com/en/resetPassword/?channel=Orca";
}

} // end namespace
