#ifndef _slic3r_gui_FFUtils_hpp_
#define _slic3r_gui_FFUtils_cpp_
#include <wx/string.h>

namespace Slic3r::GUI
{

class FFUtils
{
public:
	static  wxString getBitmapFileName(unsigned short pid);
};

}

#endif /* _slic3r_gui_FFUtils_hpp_ */