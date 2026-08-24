#include "ConvertModel.hpp"
#include "CMLoadGlb.hpp"
#include "CMLoadObj.hpp"
#include "CMSaveObj.hpp"
#include "ConvertModelProc.hpp"
#include "FullColorModelSource.hpp"

#include <boost/log/trivial.hpp>
#include <utility>

namespace Slic3r { namespace GUI {

bool ConvertModel::initConvertObj(const wxString &inPath, const in_cvt_params_t &params,
    convert_model_data_t &convertModelData)
{
    clearObjExtraData(convertModelData);
    convertModelData.modelSource.reset();
    in_model_data_t inData;
    ObjParser::ObjData objData;
    if (!CMLoadObj().loadObj(inPath, inData, objData, convertModelData.objExtraData)) {
        return false;
    }
    auto modelSource = std::make_shared<FullColorModelSource>();
    std::string sourceError;
    if (!FullColorModelSource::create_from_legacy_input(inData, *modelSource, &sourceError)) {
        BOOST_LOG_TRIVIAL(error) << "create full-color OBJ source failed, " << sourceError;
        return false;
    }
    modelSource->source_path = inPath.utf8_string();
    convertModelData.modelSource = std::move(modelSource);
    convertModelData.conversionParams = params;
    convertModelData.convertProc = std::make_unique<ConvertModelProc>(inData, params);
    std::vector<int32_t>().swap(convertModelData.objExtraData.vertexIndices);
    std::vector<int32_t>().swap(convertModelData.objExtraData.texCoordIndices);
    return true;
}

bool ConvertModel::initConvertGlb(const wxString &inPath, const in_cvt_params_t &params,
    convert_model_data_t &convertModelData)
{
    clearGlbData(convertModelData);
    convertModelData.modelSource.reset();
    in_model_data_t inData;
    if (!CMLoadGlb().loadGlb(inPath, inData, convertModelData.glbData)) {
        return false;
    }
    auto modelSource = std::make_shared<FullColorModelSource>();
    std::string sourceError;
    if (!FullColorModelSource::create_from_legacy_input(inData, *modelSource, &sourceError)) {
        BOOST_LOG_TRIVIAL(error) << "create full-color GLB source failed, " << sourceError;
        return false;
    }
    modelSource->source_path = inPath.utf8_string();
    convertModelData.modelSource = std::move(modelSource);
    convertModelData.conversionParams = params;
    convertModelData.convertProc = std::make_unique<ConvertModelProc>(inData, params);
    std::vector<float>().swap(convertModelData.glbData.vertices);
    std::vector<float>().swap(convertModelData.glbData.texCoords);
    std::vector<int32_t>().swap(convertModelData.glbData.vertexIndices);
    std::vector<int32_t>().swap(convertModelData.glbData.texCoordIndices);
    return true;
}

bool ConvertModel::initConvertGltf(const wxString &inPath, const in_cvt_params_t &params,
    convert_model_data_t &convertModelData)
{
    clearGlbData(convertModelData);
    convertModelData.modelSource.reset();
    in_model_data_t inData{};
    if (!CMLoadGlb().loadGltf(inPath, inData, convertModelData.glbData))
        return false;
    auto modelSource = std::make_shared<FullColorModelSource>();
    std::string sourceError;
    if (!FullColorModelSource::create_from_legacy_input(inData, *modelSource, &sourceError)) {
        BOOST_LOG_TRIVIAL(error) << "create full-color GLTF source failed, " << sourceError;
        return false;
    }
    modelSource->source_path = inPath.utf8_string();
    convertModelData.modelSource = std::move(modelSource);
    convertModelData.conversionParams = params;
    convertModelData.convertProc = std::make_unique<ConvertModelProc>(inData, params);
    std::vector<float>().swap(convertModelData.glbData.vertices);
    std::vector<float>().swap(convertModelData.glbData.texCoords);
    std::vector<int32_t>().swap(convertModelData.glbData.vertexIndices);
    std::vector<int32_t>().swap(convertModelData.glbData.texCoordIndices);
    return true;
}

bool ConvertModel::initConvertFbx(const wxString &inPath, const in_cvt_params_t &params,
    convert_model_data_t &convertModelData)
{
    clearFbxData(convertModelData);
    convertModelData.modelSource.reset();
    in_model_data_t inData{};
    std::string loadError;
    if (!CMLoadFbx().loadFbx(inPath, inData, convertModelData.fbxData, &loadError)) {
        BOOST_LOG_TRIVIAL(error) << "load full-color FBX failed, " << loadError;
        return false;
    }
    auto modelSource = std::make_shared<FullColorModelSource>();
    std::string sourceError;
    if (!FullColorModelSource::create_from_legacy_input(inData, *modelSource, &sourceError)) {
        BOOST_LOG_TRIVIAL(error) << "create full-color FBX source failed, " << sourceError;
        return false;
    }
    modelSource->source_path = inPath.utf8_string();
    convertModelData.modelSource = std::move(modelSource);
    convertModelData.conversionParams = params;
    convertModelData.convertProc = std::make_unique<ConvertModelProc>(inData, params);
    std::vector<float>().swap(convertModelData.fbxData.vertices);
    std::vector<float>().swap(convertModelData.fbxData.texCoords);
    std::vector<int32_t>().swap(convertModelData.fbxData.vertexIndices);
    std::vector<int32_t>().swap(convertModelData.fbxData.texCoordIndices);
    return true;
}

cvt_colors_t ConvertModel::clusterColors(const convert_model_data_t &convertModelData, int colorNum)
{
    if (convertModelData.convertProc.get() == nullptr) {
        return cvt_colors_t();
    }
    return convertModelData.convertProc->clusterColors(colorNum);
}

std::array<float, 3> ConvertModel::modelSize(const convert_model_data_t &convertModelData) const
{
    if (convertModelData.convertProc.get() == nullptr) {
        return { 0.0f, 0.0f, 0.0f };
    }
    return convertModelData.convertProc->modelSize();
}

bool ConvertModel::makePreviewModel(const convert_model_data_t &convertModelData, out_model_data_t &outData,
    const cvt_colors_t &dstColors) const
{
    if (convertModelData.convertProc.get() == nullptr) {
        return false;
    }
    convertModelData.convertProc->makePreviewModel(outData, dstColors);
    return !outData.vertices.empty() && !outData.triangles.empty();
}

bool ConvertModel::makeMappedPreviewModel(const convert_model_data_t &convertModelData, out_model_data_t &outData,
    const cvt_colors_t &sourceColors, const cvt_colors_t &targetColors) const
{
    if (convertModelData.convertProc.get() == nullptr) {
        return false;
    }
    convertModelData.convertProc->makeMappedPreviewModel(outData, sourceColors, targetColors);
    return !outData.vertices.empty() && !outData.triangles.empty();
}

bool ConvertModel::makeGeometryOnlyModel(const convert_model_data_t &convertModelData, const cvt_color_t &color,
    out_model_data_t &outData) const
{
    if (convertModelData.convertProc == nullptr || convertModelData.modelSource == nullptr)
        return false;
    convertModelData.convertProc->makeGeometryOnlyModel(*convertModelData.modelSource, color, outData);
    return !outData.vertices.empty() && !outData.triangles.empty();
}

bool ConvertModel::makeOriginalTexturePreview(const convert_model_data_t &convertModelData,
    FullColorOriginalPreview &preview, std::string *error_message) const
{
    return make_full_color_original_preview(
        convertModelData.modelSource, convertModelData.conversionParams, preview, error_message);
}

void ConvertModel::setScaleModelSize(convert_model_data_t &convertModelData, bool scaleModelSize)
{
    convertModelData.conversionParams.scaleModelSize = scaleModelSize;
    if (convertModelData.convertProc.get() != nullptr) {
        convertModelData.convertProc->setScaleModelSize(scaleModelSize);
    }
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

bool ConvertModel::doConvertMapped(convert_model_data_t &convertModelData, const cvt_colors_t &sourceColors, const cvt_colors_t &targetColors,
    const wxString &outOBjPath, const wxString &outMtlPath)
{
    if (convertModelData.convertProc.get() == nullptr) {
        return false;
    }
    out_model_data_t outData;
    convertModelData.convertProc->doConvertMapped(sourceColors, targetColors, outData);
    convertModelData.convertProc.reset();
    if (!CMSaveObj().saveObj(outData, outOBjPath, outMtlPath)) {
        return false;
    }
    return true;
}

bool ConvertModel::doConvertMapped(convert_model_data_t &convertModelData, const cvt_colors_t &sourceColors, const cvt_colors_t &targetColors,
    out_model_data_t &outData)
{
    if (convertModelData.convertProc.get() == nullptr) {
        return false;
    }
    convertModelData.convertProc->doConvertMapped(sourceColors, targetColors, outData);
    convertModelData.convertProc.reset();
    return !outData.vertices.empty() && !outData.triangles.empty();
}

bool ConvertModel::saveObj(const out_model_data_t &outData, const wxString &outOBjPath, const wxString &outMtlPath) const
{
    return CMSaveObj().saveObj(outData, outOBjPath, outMtlPath);
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

void ConvertModel::clearFbxData(convert_model_data_t &convertModelData)
{
    convertModelData.fbxData.vertices.clear();
    convertModelData.fbxData.texCoords.clear();
    convertModelData.fbxData.vertexIndices.clear();
    convertModelData.fbxData.texCoordIndices.clear();
    convertModelData.fbxData.materialDatas.clear();
    convertModelData.fbxData.textureDatas.clear();
    convertModelData.fbxData.textureBits.clear();
}

}} // namespace Slic3r::GUI
