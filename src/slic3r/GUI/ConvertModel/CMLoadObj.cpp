#include "CMLoadObj.hpp"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

namespace {

static std::string trim_copy(const std::string &s)
{
    const size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

static std::string map_kd_texture_name(const std::string &mapKd)
{
    std::string trimmed = trim_copy(mapKd);
    if (trimmed.empty())
        return {};

    const size_t last_quote = trimmed.find_last_of("\"'");
    if (last_quote != std::string::npos) {
        const size_t first_quote = trimmed.find_last_of(trimmed[last_quote], last_quote == 0 ? 0 : last_quote - 1);
        if (first_quote != std::string::npos && first_quote < last_quote)
            return trimmed.substr(first_quote + 1, last_quote - first_quote - 1);
    }

    std::vector<std::string> tokens;
    boost::split(tokens, trimmed, boost::is_any_of(" \t"), boost::token_compress_on);
    return tokens.empty() ? std::string() : tokens.back();
}

static boost::filesystem::path normalized_resource_path(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return boost::filesystem::path(path);
}

static bool resolve_existing_resource_path(const std::string &resource, const std::vector<boost::filesystem::path> &base_dirs,
    boost::filesystem::path &resolved_path)
{
    if (resource.empty())
        return false;

    boost::system::error_code ec;
    const boost::filesystem::path resource_path = normalized_resource_path(resource);
    if (resource_path.is_absolute() && boost::filesystem::exists(resource_path, ec)) {
        resolved_path = resource_path;
        return true;
    }

    for (const boost::filesystem::path &base_dir : base_dirs) {
        if (base_dir.empty())
            continue;
        const boost::filesystem::path candidate = base_dir / resource_path;
        ec.clear();
        if (boost::filesystem::exists(candidate, ec)) {
            resolved_path = candidate;
            return true;
        }
    }

    ec.clear();
    if (boost::filesystem::exists(resource_path, ec)) {
        resolved_path = resource_path;
        return true;
    }

    return false;
}

} // namespace

bool CMLoadObj::loadObj(const wxString &objPath, in_model_data_t &inData, ObjParser::ObjData &objData,
    obj_extra_data_t &objExtraData)
{
    boost::filesystem::path u8ObjPath(objPath.utf8_string());
    if (!ObjParser::objparse(u8ObjPath.string().c_str(), objData)) {
        BOOST_LOG_TRIVIAL(error) << "parse obj error, " << u8ObjPath.c_str();
        return false;
    }
    int triangleCnt, maxPolySize;
    if (!checkObjData(objData, triangleCnt, maxPolySize)) {
        return false;
    }
    mtl_map_t mtlMap;
    wxString dirPath = wxString::FromUTF8(u8ObjPath.parent_path().string());
    if (!loadMtlLibs(u8ObjPath, objData, mtlMap)) {
        return false;
    }
    if (!makeExtraData(dirPath, objData, triangleCnt, mtlMap, objExtraData)) {
        return false;
    }
    if (maxPolySize > 3) {
        BOOST_LOG_TRIVIAL(warning) << "obj faces have more than 3 vertices, " << u8ObjPath.string();
        makeExtraIncides(objData, objExtraData);
    }
    makeInData(inData, objData, triangleCnt, objExtraData);
    return true;
}

bool CMLoadObj::loadMtlLibs(const boost::filesystem::path &objPath, const ObjParser::ObjData &objData, mtl_map_t &mtlMap)
{
    std::vector<std::string> mtllibs = objData.mtllibs;
    boost::system::error_code ec;
    const boost::filesystem::path objDir = objPath.parent_path();
    boost::filesystem::path sameNameMtlPath = objPath;
    sameNameMtlPath.replace_extension(".mtl");
    if (mtllibs.empty() && boost::filesystem::exists(sameNameMtlPath, ec))
        mtllibs.emplace_back(sameNameMtlPath.string());

    for (const std::string &mtllib : mtllibs) {
        boost::filesystem::path mtlPath;
        if (!resolve_existing_resource_path(mtllib, { objDir }, mtlPath)) {
            BOOST_LOG_TRIVIAL(error) << "find obj mtl failed, " << mtllib;
            return false;
        }

        ObjParser::MtlData mtlData;
        if (!ObjParser::mtlparse(mtlPath.string().c_str(), mtlData)) {
            BOOST_LOG_TRIVIAL(error) << "parse obj mtl error, " << mtlPath;
            return false;
        }
        for (auto &item : mtlData.new_mtl_unmap) {
            mtlMap.emplace(item.first, mtl_entry_t{ *item.second, mtlPath.parent_path().string() });
        }
    }
    return true;
}

bool CMLoadObj::checkObjData(const ObjParser::ObjData &objData, int &triangleCnt, int &maxPolySize)
{
    if (objData.coordinates.empty()) {
        BOOST_LOG_TRIVIAL(error) << "obj vertex size is 0";
        return false;
    }
    if (objData.textureCoordinates.empty()) {
        BOOST_LOG_TRIVIAL(error) << "obj texture coordinate size is 0";
        return false;
    }
    if (objData.vertices.empty()) {
        BOOST_LOG_TRIVIAL(error) << "obj triangle size is 0";
        return false;
    }
    if (!checkObjIndex(objData, triangleCnt, maxPolySize)) {
        return false;
    }
    if (!checkObjMtl(objData, triangleCnt)) {
        return false;
    }
    return true;
}

bool CMLoadObj::checkObjIndex(const ObjParser::ObjData &objData, int &triangleCnt, int &maxPolySize)
{
    triangleCnt = 0;
    maxPolySize = 3;
    int vertexCnt = 0;
    const int coordCnt = int(objData.coordinates.size() / 7);
    const int texCoordCnt = int(objData.textureCoordinates.size() / 2);
    for (auto &index : objData.vertices) {
        if (index.coordIdx == -1) {
            if (vertexCnt < 3) {
                BOOST_LOG_TRIVIAL(error) << "invalid obj polygon vertice size, " << vertexCnt;
                return false;
            } else if (vertexCnt > maxPolySize) {
                maxPolySize = vertexCnt;
            }
            triangleCnt += vertexCnt - 2;
            vertexCnt = 0;
            continue;
        }
        if (index.coordIdx < 0 || index.coordIdx >= coordCnt) {
            BOOST_LOG_TRIVIAL(error) << "obj vertex index out of range, " << index.coordIdx;
            return false;
        }
        if (index.textureCoordIdx < 0 || index.textureCoordIdx >= texCoordCnt) {
            BOOST_LOG_TRIVIAL(error) << "obj textrue coordinate index out of range, " << index.textureCoordIdx;
            return false;
        }
        if (vertexCnt >= 4) {
            BOOST_LOG_TRIVIAL(error) << "obj contains polygon with more than 3 vertices, " << vertexCnt;
            return false;
        }
        vertexCnt++;
    }
    if (vertexCnt > 0) {
        if (vertexCnt < 3) {
            BOOST_LOG_TRIVIAL(error) << "invalid obj polygon vertice size, " << vertexCnt;
            return false;
        } else if (vertexCnt > maxPolySize) {
            maxPolySize = vertexCnt;
        }
        triangleCnt += vertexCnt - 2;
    }
    return true;
}

bool CMLoadObj::checkObjMtl(const ObjParser::ObjData &objData, int triangleCnt)
{
    if (objData.usemtls.empty()) {
        BOOST_LOG_TRIVIAL(error) << "obj usemtl size is 0";
        return false;
    }
    if (objData.usemtls.front().face_start != 0) {
        BOOST_LOG_TRIVIAL(error) << "obj usemtl face-start is not 1";
        return false;
    }
    if (objData.usemtls.back().face_end >= 0 && objData.usemtls.back().face_end != triangleCnt - 1) {
        BOOST_LOG_TRIVIAL(error) << "invalid usemtl face-end" << objData.usemtls.back().face_end;
        return false;
    }
    for (size_t i = 1; i < objData.usemtls.size(); ++i) {
        if (objData.usemtls[i].face_start != objData.usemtls[i - 1].face_end + 1) {
            BOOST_LOG_TRIVIAL(error) << "usemtl has non-consecutive face indices";
            return false;
        }
    }
    return true;
}

bool CMLoadObj::makeExtraData(const wxString &dirPath, const ObjParser::ObjData &objData,
    int triangleCnt, const mtl_map_t &mtlMap, obj_extra_data_t &objExtraData)
{
    std::map<std::string, int> textureMap;
    for (auto &usemtl : objData.usemtls) {
        auto it = mtlMap.find(usemtl.name);
        if (it == mtlMap.end()) {
            BOOST_LOG_TRIVIAL(error) << "find mtl failed: " << usemtl.name;
            return false;
        }
        int32_t endTriangleIndex = usemtl.face_end < 0 ? triangleCnt : usemtl.face_end + 1;
        const std::string textureName = map_kd_texture_name(it->second.material.map_Kd);
        if (textureName.empty()) {
            BOOST_LOG_TRIVIAL(error) << "obj material texture is empty, " << usemtl.name;
            return false;
        }
        auto pair = textureMap.emplace(textureName, objExtraData.textureDatas.size());
        if (!pair.second) {
            const in_texture_data_t *textureData = &objExtraData.textureDatas[pair.first->second];
            objExtraData.materialDatas.push_back({ endTriangleIndex, textureData });
            continue;
        }
        boost::filesystem::path imageFsPath;
        if (!resolve_existing_resource_path(textureName,
                { boost::filesystem::path(it->second.base_dir), boost::filesystem::path(dirPath.ToUTF8().data()) }, imageFsPath)) {
            BOOST_LOG_TRIVIAL(error) << "find obj texture failed, " << textureName;
            return false;
        }
        wxString imagePath = wxString::FromUTF8(imageFsPath.string());
        objExtraData.images.emplace_back();
        if (!objExtraData.images.back().LoadFile(imagePath)) {
            BOOST_LOG_TRIVIAL(error) << "load obj texture error, " << imagePath.To8BitData().data();
            return false;
        }
        wxImage &image = objExtraData.images.back();
        objExtraData.textureDatas.emplace_back();
        objExtraData.textureDatas.back().bits = image.GetData();
        objExtraData.textureDatas.back().width = image.GetWidth();
        objExtraData.textureDatas.back().height = image.GetHeight();
        objExtraData.textureDatas.back().channels = 3;
        objExtraData.textureDatas.back().bytePerLine = image.GetWidth() * 3;
        objExtraData.textureDatas.back().wraps[0] = TEX_WRAP_REPEAT;
        objExtraData.textureDatas.back().wraps[1] = TEX_WRAP_REPEAT;
        objExtraData.textureDatas.back().flipY = true;
        objExtraData.materialDatas.emplace_back();
        objExtraData.materialDatas.back().endTriangleIndex = endTriangleIndex;
        objExtraData.materialDatas.back().textureData = &objExtraData.textureDatas.back();
    }
    return true;
}

void CMLoadObj::makeExtraIncides(const ObjParser::ObjData &objData, obj_extra_data_t &objExtraData)
{
    const std::vector<ObjParser::ObjVertex> &vertices = objData.vertices;
    objExtraData.vertexIndices.reserve(vertices.size());
    objExtraData.texCoordIndices.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size();) {
        if (i + 3 >= vertices.size() || vertices[i + 3].coordIdx == -1) {
            objExtraData.vertexIndices.push_back(vertices[i].coordIdx);
            objExtraData.vertexIndices.push_back(vertices[i + 1].coordIdx);
            objExtraData.vertexIndices.push_back(vertices[i + 2].coordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i].textureCoordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i + 1].textureCoordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i + 2].textureCoordIdx);
            i += 4;
        } else {
            objExtraData.vertexIndices.push_back(vertices[i].coordIdx);
            objExtraData.vertexIndices.push_back(vertices[i + 1].coordIdx);
            objExtraData.vertexIndices.push_back(vertices[i + 2].coordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i].textureCoordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i + 1].textureCoordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i + 2].textureCoordIdx);
            objExtraData.vertexIndices.push_back(vertices[i].coordIdx);
            objExtraData.vertexIndices.push_back(vertices[i + 2].coordIdx);
            objExtraData.vertexIndices.push_back(vertices[i + 3].coordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i].textureCoordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i + 2].textureCoordIdx);
            objExtraData.texCoordIndices.push_back(vertices[i + 3].textureCoordIdx);
            i += 5;
        }
    }
}

void CMLoadObj::makeInData(in_model_data_t &inData, const ObjParser::ObjData &objData, int triangleCnt,
    const obj_extra_data_t &objExtraData)
{
    if (objExtraData.texCoordIndices.empty()) {
        inData.vertexIndecis = &objData.vertices[0].coordIdx;
        inData.texCoordIndices = &objData.vertices[0].textureCoordIdx;
        inData.vertexIndexStride = sizeof(objData.vertices[0]);
        inData.texCoordIndexStride = sizeof(objData.vertices[0]);
        inData.faceIndexStride = sizeof(objData.vertices[0]) * 4;
    } else {
        inData.vertexIndecis = objExtraData.vertexIndices.data();
        inData.texCoordIndices = objExtraData.texCoordIndices.data();
        inData.vertexIndexStride = sizeof(objExtraData.vertexIndices[0]);
        inData.texCoordIndexStride = sizeof(objExtraData.texCoordIndices[0]);
        inData.faceIndexStride = sizeof(objExtraData.vertexIndices[0]) * 3;
    }
    inData.vertices = objData.coordinates.data();
    inData.texCoords = objData.textureCoordinates.data();
    inData.materialDatas = objExtraData.materialDatas.data();
    inData.vertexCnt = objData.coordinates.size() / 7;
    inData.texCoordCnt = objData.textureCoordinates.size() / 2;
    inData.triangleCnt = triangleCnt;
    inData.materialCnt = objExtraData.materialDatas.size();
    inData.vertexStride = sizeof(objData.coordinates[0]) * 7;
    inData.texCoordStride = sizeof(objData.textureCoordinates[0]) * 2;
}

}} // namespace Slic3r::GUI
