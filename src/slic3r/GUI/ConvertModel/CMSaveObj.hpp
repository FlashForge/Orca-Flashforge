#ifndef slic3r_GUI_CMSaveObj_hpp_
#define slic3r_GUI_CMSaveObj_hpp_

#include <wx/file.h>
#include <wx/string.h>
#include "ConvertModelDef.hpp"

namespace Slic3r { namespace GUI {

class CMSaveObj
{
public:
    bool saveObj(const out_model_data_t &outData, const wxString &outOBjPath,
        const wxString &outMtlPath);

private:
    bool writeMtllib(wxFile &file, const wxString &outMtlPath);

    bool writeVertex(wxFile &file, const out_model_data_t &outData);

    bool writeFace(wxFile &file, const out_model_data_t &outData);

    bool saveMtl(const out_model_data_t &outData, const wxString &outMtlPath);

    bool write(wxFile &file, const char *buf);

    bool flushWriteBuf(wxFile &file);

private:
    std::string m_tmpWriteBuf;
};

}} // namespace Slic3r::GUI

#endif
