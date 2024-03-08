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
	}
	return str;
}

unsigned short FFUtils::getPrinterPID(const std::string& type)
{
	unsigned short pid = 0;
	if (type == "Adventurer 5M") {
		pid = 0x0023;
	} else if (type == "Adventurer 5M Pro") {
		pid == 0x0024;
	}
	return pid;
}

} // end namespace