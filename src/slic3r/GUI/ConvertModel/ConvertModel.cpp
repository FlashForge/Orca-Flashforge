#include "ConvertModel.hpp"
#include "CMLoadGlb.hpp"
#include "CMLoadObj.hpp"
#include "CMSaveObj.hpp"
#include "ConvertModelProc.hpp"

namespace Slic3r { namespace GUI {

bool ConvertModel::initConvertObj(const wxString &inPath, const in_cvt_params_t &params,
    convert_model_data_t &convertModelData)
{
    clearObjExtraData(convertModelData);
    in_model_data_t inData;
    ObjParser::ObjData objData;
    if (!CMLoadObj().loadObj(inPath, inData, objData, convertModelData.objExtraData)) {
        return false;
    }
    convertModelData.convertProc = std::make_unique<ConvertModelProc>(inData, params);
    std::vector<int32_t>().swap(convertModelData.objExtraData.vertexIndices);
    std::vector<int32_t>().swap(convertModelData.objExtraData.texCoordIndices);
    return true;
}

bool ConvertModel::initConvertGlb(const wxString &inPath, const in_cvt_params_t &params,
    convert_model_data_t &convertModelData)
{
    clearGlbData(convertModelData);
    in_model_data_t inData;
    if (!CMLoadGlb().loadGlb(inPath, inData, convertModelData.glbData)) {
        return false;
    }
    convertModelData.convertProc = std::make_unique<ConvertModelProc>(inData, params);
    std::vector<float>().swap(convertModelData.glbData.vertices);
    std::vector<float>().swap(convertModelData.glbData.texCoords);
    std::vector<int32_t>().swap(convertModelData.glbData.vertexIndices);
    std::vector<int32_t>().swap(convertModelData.glbData.texCoordIndices);
    return true;
}

cvt_colors_t ConvertModel::clusterColors(const convert_model_data_t &convertModelData, int colorNum)
{
    if (convertModelData.convertProc.get() == nullptr) {
        return cvt_colors_t();
    }
    return convertModelData.convertProc->clusterColors(colorNum);
}

bool ConvertModel::doConvert(convert_model_data_t &convertModelData, const cvt_colors_t &dstColors,
    const wxString &outOBjPath, const wxString &outMtlPath)
{
    if (convertModelData.convertProc.get() == nullptr) {
        return false;
    }
    out_model_data_t outData;
    convertModelData.convertProc->doConvert(dstColors, outData);
    convertModelData.convertProc.reset();
    if (!CMSaveObj().saveObj(outData, outOBjPath, outMtlPath)) {
        return false;
    }
    return true;
}

void ConvertModel::clearObjExtraData(convert_model_data_t &convertModelData)
{
    convertModelData.objExtraData.vertexIndices.clear();
    convertModelData.objExtraData.texCoordIndices.clear();
    convertModelData.objExtraData.materialDatas.clear();
    convertModelData.objExtraData.textureDatas.clear();
    convertModelData.objExtraData.images.clear();
}

void ConvertModel::clearGlbData(convert_model_data_t &convertModelData)
{
    convertModelData.glbData.vertices.clear();
    convertModelData.glbData.texCoords.clear();
    convertModelData.glbData.vertexIndices.clear();
    convertModelData.glbData.texCoordIndices.clear();
    convertModelData.glbData.materialDatas.clear();
    convertModelData.glbData.textureDatas.clear();
    convertModelData.glbData.multiTextureBits.clear();
}

}} // namespace Slic3r::GUI
