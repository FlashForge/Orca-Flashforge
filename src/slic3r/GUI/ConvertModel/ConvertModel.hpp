#ifndef slic3r_GUI_ConvertModel_hpp_
#define slic3r_GUI_ConvertModel_hpp_

#include <memory>
#include <wx/string.h>
#include "CMLoadGlb.hpp"
#include "CMLoadObj.hpp"
#include "ConvertModelDef.hpp"
#include "ConvertModelProc.hpp"

namespace Slic3r { namespace GUI {

struct convert_model_data_t {
    obj_extra_data_t objExtraData;
    glb_data_t glbData;
    std::unique_ptr<ConvertModelProc> convertProc;
};

class ConvertModel
{
public:
    bool initConvertObj(const wxString &inPath, const in_cvt_params_t &params,
        convert_model_data_t &convertModelData);

    bool initConvertGlb(const wxString &inPath, const in_cvt_params_t &params,
        convert_model_data_t &convertModelData);

    cvt_colors_t clusterColors(const convert_model_data_t &convertModelData, int colorNum);

    bool doConvert(convert_model_data_t &convertModelData, const cvt_colors_t &dstColors,
        const wxString &outOBjPath, const wxString &outMtlPath);

private:
    void clearObjExtraData(convert_model_data_t &convertModelData);

    void clearGlbData(convert_model_data_t &convertModelData);
};

}} // namespace Slic3r::GUI

#endif
