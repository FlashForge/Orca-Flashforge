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

    std::array<float, 3> modelSize(const convert_model_data_t &convertModelData) const;

    bool makePreviewModel(const convert_model_data_t &convertModelData, out_model_data_t &outData,
        const cvt_colors_t &dstColors = {}) const;

    bool makeMappedPreviewModel(const convert_model_data_t &convertModelData, out_model_data_t &outData,
        const cvt_colors_t &sourceColors, const cvt_colors_t &targetColors) const;

    void setScaleModelSize(convert_model_data_t &convertModelData, bool scaleModelSize);

    bool doConvert(convert_model_data_t &convertModelData, const cvt_colors_t &dstColors,
        const wxString &outOBjPath, const wxString &outMtlPath);

    bool doConvertMapped(convert_model_data_t &convertModelData, const cvt_colors_t &sourceColors, const cvt_colors_t &targetColors,
        const wxString &outOBjPath, const wxString &outMtlPath);

    bool doConvertMapped(convert_model_data_t &convertModelData, const cvt_colors_t &sourceColors, const cvt_colors_t &targetColors,
        out_model_data_t &outData);

    bool saveObj(const out_model_data_t &outData, const wxString &outOBjPath, const wxString &outMtlPath) const;

private:
    void clearObjExtraData(convert_model_data_t &convertModelData);

    void clearGlbData(convert_model_data_t &convertModelData);
};

}} // namespace Slic3r::GUI

#endif
