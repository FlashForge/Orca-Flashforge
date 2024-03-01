#include "FFUtils.hpp"

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
	}
	return str;
}

} // end namespace