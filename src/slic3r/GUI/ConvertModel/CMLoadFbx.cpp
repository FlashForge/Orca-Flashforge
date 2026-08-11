#include "CMLoadFbx.hpp"

#include "libslic3r/BambuTexturePainting.hpp"
#include "libslic3r/Format/AssimpImport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI { namespace {

uint8_t color_byte(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.f, 1.f) * 255.f));
}

bool decode_texture(const TextureImage &source, std::vector<uint8_t> &bits, int &width, int &height, int &channels)
{
    cv::Mat image;
    if (source.width <= 0 || source.height <= 0) {
        if (source.data.empty() || source.data.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
            return false;
        cv::Mat encoded(1, static_cast<int>(source.data.size()), CV_8UC1, const_cast<uint8_t *>(source.data.data()));
        image = cv::imdecode(encoded, cv::IMREAD_UNCHANGED);
        if (image.empty() || image.depth() != CV_8U)
            return false;
        if (image.channels() == 4)
            cv::cvtColor(image, image, cv::COLOR_BGRA2RGBA);
        else if (image.channels() == 3)
            cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    } else {
        if (source.channels != 3 && source.channels != 4)
            return false;
        image = cv::Mat(source.height, source.width, source.channels == 4 ? CV_8UC4 : CV_8UC3,
                        const_cast<uint8_t *>(source.data.data())).clone();
    }
    if (!image.isContinuous())
        image = image.clone();
    width = image.cols;
    height = image.rows;
    channels = image.channels();
    bits.assign(image.data, image.data + image.total() * image.elemSize());
    return width > 0 && height > 0 && (channels == 3 || channels == 4);
}

const in_texture_data_t *add_solid_texture(const std::array<float, 4> &color, fbx_data_t &data)
{
    const std::array<uint8_t, 4> rgba = {color_byte(color[0]), color_byte(color[1]), color_byte(color[2]), color_byte(color[3])};
    data.textureBits.emplace_back(std::vector<uint8_t>{rgba[0], rgba[1], rgba[2]});
    data.textureDatas.emplace_back();
    in_texture_data_t &texture = data.textureDatas.back();
    texture.bits = data.textureBits.back().data();
    texture.width = 1;
    texture.height = 1;
    texture.channels = 3;
    texture.bytePerLine = 3;
    texture.isSolidColor = true;
    texture.solidColor = rgba;
    return &texture;
}

const in_texture_data_t *add_image_texture(const TextureImage &source, fbx_data_t &data)
{
    data.textureBits.emplace_back();
    int width = 0, height = 0, channels = 0;
    if (!decode_texture(source, data.textureBits.back(), width, height, channels)) {
        data.textureBits.pop_back();
        return nullptr;
    }
    data.textureDatas.emplace_back();
    in_texture_data_t &texture = data.textureDatas.back();
    texture.bits = data.textureBits.back().data();
    texture.width = width;
    texture.height = height;
    texture.channels = channels;
    texture.bytePerLine = width * channels;
    texture.wraps[0] = TEX_WRAP_REPEAT;
    texture.wraps[1] = TEX_WRAP_REPEAT;
    return &texture;
}

} // namespace

bool CMLoadFbx::loadFbx(const wxString &path, in_model_data_t &inData, fbx_data_t &data, std::string *error_message)
{
    TexturedMesh mesh;
    if (!load_assimp_textured_model(path.utf8_string(), mesh, error_message))
        return false;
    if (mesh.vertices.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()) ||
        mesh.indices.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
        if (error_message)
            *error_message = "FBX model is too large for the full-color conversion pipeline";
        return false;
    }

    data.vertices.reserve(mesh.vertices.size() * 3);
    data.texCoords.reserve(mesh.uvs.size() * 2);
    for (const auto &vertex : mesh.vertices)
        data.vertices.insert(data.vertices.end(), vertex.begin(), vertex.end());
    for (const auto &uv : mesh.uvs)
        data.texCoords.insert(data.texCoords.end(), uv.begin(), uv.end());
    data.vertexIndices.reserve(mesh.indices.size() * 3);
    data.texCoordIndices.reserve(mesh.indices.size() * 3);
    for (const auto &face : mesh.indices) {
        data.vertexIndices.insert(data.vertexIndices.end(), face.begin(), face.end());
        data.texCoordIndices.insert(data.texCoordIndices.end(), face.begin(), face.end());
    }

    const size_t material_count = std::max<size_t>(1, std::max(mesh.material_colors.size(), mesh.material_texture_map.size()));
    std::vector<const in_texture_data_t *> material_textures;
    material_textures.reserve(material_count);
    for (size_t index = 0; index < material_count; ++index) {
        const in_texture_data_t *texture = nullptr;
        if (index < mesh.material_texture_map.size()) {
            const int texture_index = mesh.material_texture_map[index];
            if (texture_index >= 0 && static_cast<size_t>(texture_index) < mesh.textures.size())
                texture = add_image_texture(mesh.textures[texture_index], data);
        }
        const std::array<float, 4> color = index < mesh.material_colors.size() ? mesh.material_colors[index] :
                                                                                 std::array<float, 4>{1.f, 1.f, 1.f, 1.f};
        material_textures.push_back(texture ? texture : add_solid_texture(color, data));
    }

    for (size_t face_index = 0; face_index < mesh.indices.size(); ++face_index) {
        int material_id = face_index < mesh.material_ids.size() ? mesh.material_ids[face_index] : 0;
        if (material_id < 0 || static_cast<size_t>(material_id) >= material_textures.size())
            material_id = 0;
        const in_texture_data_t *texture = material_textures[material_id];
        if (data.materialDatas.empty() || data.materialDatas.back().textureData != texture)
            data.materialDatas.push_back({static_cast<int32_t>(face_index + 1), texture});
        else
            data.materialDatas.back().endTriangleIndex = static_cast<int32_t>(face_index + 1);
    }

    inData = {};
    inData.vertices = data.vertices.data();
    inData.texCoords = data.texCoords.data();
    inData.vertexIndecis = data.vertexIndices.data();
    inData.texCoordIndices = data.texCoordIndices.data();
    inData.materialDatas = data.materialDatas.data();
    inData.vertexCnt = static_cast<int32_t>(mesh.vertices.size());
    inData.texCoordCnt = static_cast<int32_t>(mesh.uvs.size());
    inData.triangleCnt = static_cast<int32_t>(mesh.indices.size());
    inData.materialCnt = static_cast<int32_t>(data.materialDatas.size());
    inData.vertexStride = sizeof(float) * 3;
    inData.texCoordStride = sizeof(float) * 2;
    inData.vertexIndexStride = sizeof(int32_t);
    inData.texCoordIndexStride = sizeof(int32_t);
    inData.faceIndexStride = sizeof(int32_t) * 3;
    BOOST_LOG_TRIVIAL(info) << "CMLoadFbx: prepared full-color input for " << path.utf8_string();
    return true;
}

}} // namespace Slic3r::GUI
