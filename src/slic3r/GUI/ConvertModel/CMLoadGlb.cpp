#include "CMLoadGlb.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

namespace {

bool accessor_layout(const tinygltf::Model &model, const tinygltf::Accessor &accessor, int components,
                     const uint8_t *&data, int &stride)
{
    if (accessor.sparse.isSparse || tinygltf::GetNumComponentsInType(accessor.type) != components ||
        accessor.bufferView < 0 || accessor.bufferView >= model.bufferViews.size())
        return false;
    const tinygltf::BufferView &view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= model.buffers.size())
        return false;
    const int component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    stride = accessor.ByteStride(view);
    if (component_size <= 0 || stride < component_size * components)
        return false;
    const std::vector<unsigned char> &buffer = model.buffers[view.buffer].data;
    const size_t offset = view.byteOffset + accessor.byteOffset;
    const size_t element_size = static_cast<size_t>(component_size) * components;
    if (offset > buffer.size() || element_size > buffer.size() - offset ||
        (accessor.count > 0 && accessor.count - 1 > (buffer.size() - offset - element_size) / static_cast<size_t>(stride)))
        return false;
    data = buffer.data() + offset;
    return true;
}

float read_texcoord_component(const uint8_t *data, int component_type)
{
    if (component_type == TINYGLTF_COMPONENT_TYPE_FLOAT) {
        float value;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
    if (component_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        return static_cast<float>(*data) / 255.f;
    uint16_t value;
    std::memcpy(&value, data, sizeof(value));
    return static_cast<float>(value) / 65535.f;
}

} // namespace

bool CMLoadGlb::loadGlb(const wxString &glbPath, in_model_data_t &inData, glb_data_t &glbData)
{
    return load(glbPath, true, inData, glbData);
}

bool CMLoadGlb::loadGltf(const wxString &gltfPath, in_model_data_t &inData, glb_data_t &glbData)
{
    return load(gltfPath, false, inData, glbData);
}

bool CMLoadGlb::load(const wxString &path, bool binary, in_model_data_t &inData, glb_data_t &glbData)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    const bool loaded = binary ? loader.LoadBinaryFromFile(&model, &err, &warn, path.utf8_string()) :
                                 loader.LoadASCIIFromFile(&model, &err, &warn, path.utf8_string());
    if (!warn.empty())
        BOOST_LOG_TRIVIAL(warning) << "load glTF warning, " << warn << ", " << path.utf8_string();
    if (!loaded) {
        BOOST_LOG_TRIVIAL(error) << "load glTF error, " << err << ", " << path.utf8_string();
        return false;
    }
    m_baseIndex = 0;
    m_pointIndexMap.clear();
    m_pointMap.clear();
    m_textrueIndexMap.clear();
    m_colorIndexMap.clear();
    m_nodeParentSet.clear();
    const int scene_index = model.defaultScene >= 0 ? model.defaultScene : (model.scenes.empty() ? -1 : 0);
    if (scene_index < 0 || scene_index >= model.scenes.size()) {
        BOOST_LOG_TRIVIAL(warning) << "invalid glTF scene, " << scene_index;
        return false;
    }
    for (int node : model.scenes[scene_index].nodes) {
        if (!addNode(model, node, Eigen::Matrix4f::Identity(), inData, glbData)) {
            return false;
        }
    }
    postProcess(inData, glbData);
    return !glbData.vertexIndices.empty();
}

bool CMLoadGlb::addNode(const tinygltf::Model &model, int nodeIdx, const Eigen::Matrix4f &parentMatrix,
    in_model_data_t &inData, glb_data_t &glbData)
{
    if (nodeIdx < 0 || nodeIdx >= model.nodes.size()) {
        BOOST_LOG_TRIVIAL(warning) << "invalid glb node, " << nodeIdx;
        return false;
    }
    auto &node = model.nodes[nodeIdx];
    Eigen::Matrix4f matrix = parentMatrix * getNodeMatrix(node);
    if (node.mesh >= 0) {
        if (node.mesh >= model.meshes.size()) {
            BOOST_LOG_TRIVIAL(warning) << "invalid glb mesh, " << node.mesh;
            return true;
        }
        for (auto &primitive : model.meshes[node.mesh].primitives) {
            int vertexCnt = 0;
            if (!getPrimitiveVertexCount(model, primitive, vertexCnt)) {
                return false;
            }
            if (vertexCnt > std::numeric_limits<int>::max() - m_baseIndex) {
                BOOST_LOG_TRIVIAL(error) << "glTF scene has too many vertices";
                return false;
            }
            indices_info_t info;
            if (!addIndices(model, primitive.mode, primitive.indices, vertexCnt, glbData, info)) {
                return false;
            }
            if (info.cnt <= 0) {
                continue;
            }
            if (!addMaterial(model, primitive.material, inData, glbData)) {
                return false;
            }
            if (!addCoords(model, primitive, info, matrix, glbData)) {
                return false;
            }
        }
    }
    m_nodeParentSet.insert(nodeIdx);
    for (int child : node.children) {
        if (m_nodeParentSet.find(child) != m_nodeParentSet.end()) {
            BOOST_LOG_TRIVIAL(warning) << "glb node hierarchy contain cycles";
            return false;
        }
        if (!addNode(model, child, matrix, inData, glbData)) {
            return false;
        }
    }
    m_nodeParentSet.erase(nodeIdx);
    return true;
}

Eigen::Matrix4f CMLoadGlb::getNodeMatrix(const tinygltf::Node &node)
{
    if (!node.matrix.empty()) {
        Eigen::Matrix4f matrix;
        for (int i = 0; i < 16; ++i) {
            matrix(i % 4, i / 4) = node.matrix[i];
        }
        return matrix;
    }
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();
    if (!node.translation.empty()) {
        transform *= Eigen::Translation3f(node.translation[0], node.translation[1], node.translation[2]);
    }
    if (!node.rotation.empty()) {
        Eigen::Quaternionf q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        transform *= q.normalized();
    }
    if (!node.scale.empty()) {
        transform *= Eigen::AlignedScaling3f(node.scale[0], node.scale[1], node.scale[2]);
    }
    return transform.matrix();
}

bool CMLoadGlb::getPrimitiveVertexCount(const tinygltf::Model &model, const tinygltf::Primitive &primitive,
    int &vertexCnt)
{
    vertexCnt = 0;
    auto vertexIt = primitive.attributes.find("POSITION");
    if (vertexIt == primitive.attributes.end()) {
        BOOST_LOG_TRIVIAL(error) << "can't find glb vertex";
        return false;
    }
    if (vertexIt->second < 0 || vertexIt->second >= model.accessors.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb accessor, " << vertexIt->second;
        return false;
    }
    const size_t count = model.accessors[vertexIt->second].count;
    if (count > static_cast<size_t>(std::numeric_limits<int>::max())) {
        BOOST_LOG_TRIVIAL(error) << "glTF primitive has too many vertices";
        return false;
    }
    vertexCnt = static_cast<int>(count);
    return true;
}

bool CMLoadGlb::addIndices(const tinygltf::Model &model, int mode, int accessorIdx, int vertexCnt, glb_data_t &glbData,
    indices_info_t &info)
{
    info.maxIndex = 0;
    info.cnt = 0;
    if (mode == -1) {
        mode = TINYGLTF_MODE_TRIANGLES;
    }
    if (mode != TINYGLTF_MODE_TRIANGLES && mode != TINYGLTF_MODE_TRIANGLE_FAN
     && mode != TINYGLTF_MODE_TRIANGLE_STRIP) {
        return true;
    }
    if (accessorIdx < 0 || accessorIdx >= model.accessors.size()) {
        addSequentialIndices(vertexCnt, mode, glbData, info);
        return true;
    }
    auto &indexAccessor = model.accessors[accessorIdx];
    if (indexAccessor.sparse.isSparse || indexAccessor.type != TINYGLTF_TYPE_SCALAR) {
        BOOST_LOG_TRIVIAL(error) << "unsupported sparse or non-scalar glTF index accessor";
        return false;
    }
    if (indexAccessor.bufferView < 0 || indexAccessor.bufferView >= model.bufferViews.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb bufferView, " << indexAccessor.bufferView;
        return false;
    }
    if (indexAccessor.count < 3) {
        return true;
    }
    if (indexAccessor.count > static_cast<size_t>(std::numeric_limits<int>::max())) {
        BOOST_LOG_TRIVIAL(error) << "glTF primitive has too many indices";
        return false;
    }
    auto &indexView = model.bufferViews[indexAccessor.bufferView];
    if (indexView.buffer < 0 || indexView.buffer >= model.buffers.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb buffer, " << indexView.buffer;
        return false;
    }
    const int component_size = tinygltf::GetComponentSizeInBytes(indexAccessor.componentType);
    const int stride = indexAccessor.ByteStride(indexView);
    const size_t offset = indexAccessor.byteOffset + indexView.byteOffset;
    const auto &buffer = model.buffers[indexView.buffer].data;
    if (component_size <= 0 || stride < component_size || offset > buffer.size() ||
        static_cast<size_t>(component_size) > buffer.size() - offset ||
        (indexAccessor.count > 0 && indexAccessor.count - 1 >
            (buffer.size() - offset - component_size) / static_cast<size_t>(stride))) {
        BOOST_LOG_TRIVIAL(error) << "invalid glTF index accessor layout";
        return false;
    }
    std::vector<uint32_t> indices(indexAccessor.count);
    for (size_t i = 0; i < indexAccessor.count; ++i) {
        const uint8_t *source = buffer.data() + offset + i * stride;
        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            std::memcpy(&indices[i], source, sizeof(uint32_t));
        else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            uint16_t value;
            std::memcpy(&value, source, sizeof(value));
            indices[i] = value;
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            indices[i] = *source;
        else {
            BOOST_LOG_TRIVIAL(error) << "unsupported glTF index component type, " << indexAccessor.componentType;
            return false;
        }
    }
    addIndices(indices.data(), static_cast<int>(indices.size()), mode, glbData, info);
    return true;
}

void CMLoadGlb::addSequentialIndices(int cnt, int mode, glb_data_t &glbData, indices_info_t &info)
{
    if (cnt < 3) {
        return;
    }
    std::vector<int32_t> indices(cnt);
    for (int i = 0; i < cnt; ++i) {
        indices[i] = i;
    }
    addIndices(indices.data(), cnt, mode, glbData, info);
}

template<typename Ty>
void CMLoadGlb::addIndices(Ty indices, int cnt, int mode, glb_data_t &glbData, indices_info_t &info)
{
    auto addIndex = [&](int index) {
        if (index > info.maxIndex) {
            info.maxIndex = index;
        }
        glbData.texCoordIndices.push_back(m_baseIndex + index);
    };
    if (mode == TINYGLTF_MODE_TRIANGLES) {
        for (int i = 2; i < cnt; i += 3) {
            addIndex(indices[i - 2]);
            addIndex(indices[i - 1]);
            addIndex(indices[i]);
        }
        info.cnt = cnt / 3;
    } else if (mode == TINYGLTF_MODE_TRIANGLE_FAN) {
        for (int i = 2; i < cnt; ++i) {
            addIndex(indices[0]);
            addIndex(indices[i - 1]);
            addIndex(indices[i]);
        }
        info.cnt = cnt - 2;
    } else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
        int i = 3;
        for (; i < cnt; i += 4) {
            addIndex(indices[i - 3]);
            addIndex(indices[i - 2]);
            addIndex(indices[i - 1]);
            addIndex(indices[i - 1]);
            addIndex(indices[i - 2]);
            addIndex(indices[i]);
        }
        if (i - 1 < cnt) {
            addIndex(indices[i - 3]);
            addIndex(indices[i - 2]);
            addIndex(indices[i - 1]);
        }
        info.cnt = cnt - 2;
    }
}

bool CMLoadGlb::addMaterial(const tinygltf::Model &model, int materialIdx, in_model_data_t &inData,
    glb_data_t &glbData)
{
    int textureIdx = -1;
    if (!getTextureIndex(model, materialIdx, textureIdx)) {
        return addSolidColorMaterial(model, materialIdx, glbData);
    }
    int imageIdx = model.textures[textureIdx].source;
    if (imageIdx < 0 || imageIdx >= model.images.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb image, " << imageIdx;
        return false;
    }
    auto &image = model.images[imageIdx];
    if (image.bits > 8) {
        BOOST_LOG_TRIVIAL(error) << "unsupported glb image bits, " << image.bits;
        return false;
    }
    auto pair = m_textrueIndexMap.emplace(imageIdx, glbData.textureDatas.size());
    if (pair.second) {
        return addNewTexture(model, textureIdx, image, glbData);
    }
    if (&glbData.textureDatas[pair.first->second] == glbData.materialDatas.back().textureData) {
        glbData.materialDatas.back().endTriangleIndex = glbData.texCoordIndices.size() / 3;
    } else {
        int32_t endTriangleIndex = glbData.texCoordIndices.size() / 3;
        glbData.materialDatas.push_back({ endTriangleIndex, &glbData.textureDatas[pair.first->second] });
    }
    return true;
}

bool CMLoadGlb::addSolidColorMaterial(const tinygltf::Model &model, int materialIdx, glb_data_t &glbData)
{
    cvt_color_t color = getMaterialColor(model, materialIdx);
    auto pair = m_colorIndexMap.emplace(color, glbData.textureDatas.size());
    if (pair.second) {
        glbData.multiTextureBits.emplace_back(std::vector<uint8_t>{ color[0], color[1], color[2] });
        glbData.textureDatas.emplace_back();
        glbData.textureDatas.back().bits = glbData.multiTextureBits.back().data();
        glbData.textureDatas.back().width = 1;
        glbData.textureDatas.back().height = 1;
        glbData.textureDatas.back().channels = 3;
        glbData.textureDatas.back().bytePerLine = 3;
        glbData.textureDatas.back().wraps[0] = TEX_WRAP_REPEAT;
        glbData.textureDatas.back().wraps[1] = TEX_WRAP_REPEAT;
        glbData.textureDatas.back().flipY = false;
        glbData.textureDatas.back().isSolidColor = true;
        glbData.textureDatas.back().solidColor = { color[0], color[1], color[2], 255 };
    }
    int textureDataIdx = pair.first->second;
    if (!glbData.materialDatas.empty()
     && &glbData.textureDatas[textureDataIdx] == glbData.materialDatas.back().textureData) {
        glbData.materialDatas.back().endTriangleIndex = glbData.texCoordIndices.size() / 3;
    } else {
        int32_t endTriangleIndex = glbData.texCoordIndices.size() / 3;
        glbData.materialDatas.push_back({ endTriangleIndex, &glbData.textureDatas[textureDataIdx] });
    }
    return true;
}

cvt_color_t CMLoadGlb::getMaterialColor(const tinygltf::Model &model, int materialIdx)
{
    cvt_color_t color = { 255, 255, 255 };
    auto setFromFactor = [&color](const std::vector<double> &factor) {
        if (factor.size() < 3) {
            return;
        }
        for (int i = 0; i < 3; ++i) {
            color[i] = static_cast<uint8_t>(std::max(0.0, std::min(1.0, factor[i])) * 255.0);
        }
    };
    if (materialIdx < 0 || materialIdx >= model.materials.size()) {
        return color;
    }
    const tinygltf::Material &material = model.materials[materialIdx];
    setFromFactor(material.pbrMetallicRoughness.baseColorFactor);
    auto it = material.extensions.find("KHR_materials_pbrSpecularGlossiness");
    if (it != material.extensions.end() && it->second.Has("diffuseFactor")
     && it->second.Get("diffuseFactor").IsArray()) {
        std::vector<double> diffuseFactor;
        const tinygltf::Value::Array &array = it->second.Get("diffuseFactor").Get<tinygltf::Value::Array>();
        for (const tinygltf::Value &value : array) {
            diffuseFactor.push_back(value.GetNumberAsDouble());
        }
        setFromFactor(diffuseFactor);
    }
    return color;
}

bool CMLoadGlb::getTextureIndex(const tinygltf::Model &model, int materialIdx, int &textureIdx)
{
    if (materialIdx < 0 || materialIdx >= model.materials.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb material, " << materialIdx;
        return false;
    }
    textureIdx = model.materials[materialIdx].pbrMetallicRoughness.baseColorTexture.index;
    if (textureIdx >= 0 && textureIdx < model.textures.size()) {
        return true;
    }
    auto it = model.materials[materialIdx].extensions.find("KHR_materials_pbrSpecularGlossiness");
    if (it == model.materials[materialIdx].extensions.end()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb texture, " << textureIdx;
        return false;
    }
    if (!it->second.Has("diffuseTexture") || !it->second.Get("diffuseTexture").Has("index")) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb texture, " << textureIdx;
        return false;
    }
    auto &value = it->second.Get("diffuseTexture").Get("index");
    if (value.Type() != tinygltf::INT_TYPE) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb texture, " << textureIdx;
        return false;
    }
    textureIdx = value.GetNumberAsInt();
    return true;
}

bool CMLoadGlb::addNewTexture(const tinygltf::Model &model, int textureIdx, const tinygltf::Image &image,
    glb_data_t &glbData)
{
    std::vector<uint8_t> textureBits(image.image.data(), image.image.data() + image.image.size());
    glbData.multiTextureBits.emplace_back(std::move(textureBits));
    glbData.textureDatas.emplace_back();
    glbData.textureDatas.back().bits = glbData.multiTextureBits.back().data();
    glbData.textureDatas.back().width = image.width;
    glbData.textureDatas.back().height = image.height;
    glbData.textureDatas.back().channels = image.component;
    glbData.textureDatas.back().bytePerLine = image.width * image.component;
    glbData.textureDatas.back().flipY = false;
    if (!getWrapType(model, textureIdx, glbData.textureDatas.back().wraps)) {
        return false;
    }
    glbData.materialDatas.emplace_back();
    glbData.materialDatas.back().endTriangleIndex = glbData.texCoordIndices.size() / 3;
    glbData.materialDatas.back().textureData = &glbData.textureDatas.back();
    return true;
}

bool CMLoadGlb::getWrapType(const tinygltf::Model &model, int textureIdx, texture_wrap_type_t wrap[2])
{
    auto setWrap = [](int src, texture_wrap_type_t &dst) {
        if (src == TINYGLTF_TEXTURE_WRAP_REPEAT) {
            dst = TEX_WRAP_REPEAT;
        } else if (src == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE) {
            dst = TEX_WRAP_CLAMP_TO_EDGE;
        } else if (src == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) {
            dst = TXT_WRAP_MIRRORED_REPEAT;
        } else {
            BOOST_LOG_TRIVIAL(error) << "invalid glb texture wrap, " << src;
            return false;
        }
        return true;
    };
    int samperIdx = model.textures[textureIdx].sampler;
    if (samperIdx < 0 || samperIdx >= model.samplers.size()) {
        wrap[0] = TEX_WRAP_REPEAT;
        wrap[1] = TEX_WRAP_REPEAT;
        return true;
    }
    return setWrap(model.samplers[samperIdx].wrapS, wrap[0])
        && setWrap(model.samplers[samperIdx].wrapT, wrap[1]);
}

bool CMLoadGlb::addCoords(const tinygltf::Model &model, const tinygltf::Primitive &primitive,
    const indices_info_t &info, const Eigen::Matrix4f &matrix, glb_data_t &glbData)
{
    auto vertexIt = primitive.attributes.find("POSITION");
    if (vertexIt == primitive.attributes.end()) {
        BOOST_LOG_TRIVIAL(error) << "can't find glb vertex";
        return false;
    }
    if (vertexIt->second < 0 || vertexIt->second >= model.accessors.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb accessor, " << vertexIt->second;
        return false;
    }
    auto &vertexAccessor = model.accessors[vertexIt->second];
    const uint8_t *vertexData = nullptr;
    int vertexStride = 0;
    if (vertexAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        !accessor_layout(model, vertexAccessor, 3, vertexData, vertexStride)) {
        BOOST_LOG_TRIVIAL(error) << "unsupported glTF POSITION accessor layout";
        return false;
    }
    if (info.maxIndex >= vertexAccessor.count) {
        BOOST_LOG_TRIVIAL(error) << "glb index out of range, " << info.maxIndex;
        return false;
    }
    for (int i = 0; i < vertexAccessor.count; ++i) {
        float coords[3];
        std::memcpy(coords, vertexData + static_cast<size_t>(i) * vertexStride, sizeof(coords));
        Eigen::Vector4f pos(coords[0], coords[1], coords[2], 1.0f);
        pos = matrix * pos;
        std::array<float, 3> vertex = { pos[0], pos[1], pos[2] };
        auto pointIt = m_pointMap.emplace(vertex, m_pointMap.size()).first;
        m_pointIndexMap.push_back(pointIt->second);
    }
    auto texCoordIt = primitive.attributes.find("TEXCOORD_0");
    if (texCoordIt == primitive.attributes.end()) {
        for (int i = 0; i < vertexAccessor.count; ++i) {
            glbData.texCoords.push_back(0.0f);
            glbData.texCoords.push_back(0.0f);
        }
    } else {
        if (texCoordIt->second < 0 || texCoordIt->second >= model.accessors.size()) {
            BOOST_LOG_TRIVIAL(error) << "invalid glb accessor, " << texCoordIt->second;
            return false;
        }
        auto &texCoordAccessor = model.accessors[texCoordIt->second];
        if (vertexAccessor.count != texCoordAccessor.count) {
            BOOST_LOG_TRIVIAL(error) << "invalid vertex/texture coord count, " << vertexAccessor.count
                << "/" << texCoordAccessor.count;
            return false;
        }
        const bool valid_component = texCoordAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ||
                                     ((texCoordAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
                                       texCoordAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) &&
                                      texCoordAccessor.normalized);
        const uint8_t *texCoordData = nullptr;
        int texCoordStride = 0;
        if (!valid_component || !accessor_layout(model, texCoordAccessor, 2, texCoordData, texCoordStride)) {
            BOOST_LOG_TRIVIAL(error) << "unsupported glTF TEXCOORD_0 accessor layout";
            return false;
        }
        const int component_size = tinygltf::GetComponentSizeInBytes(texCoordAccessor.componentType);
        for (int i = 0; i < texCoordAccessor.count; ++i) {
            const uint8_t *element = texCoordData + static_cast<size_t>(i) * texCoordStride;
            glbData.texCoords.push_back(read_texcoord_component(element, texCoordAccessor.componentType));
            glbData.texCoords.push_back(read_texcoord_component(element + component_size, texCoordAccessor.componentType));
        }
    }
    m_baseIndex += vertexAccessor.count;
    return true;
}

void CMLoadGlb::postProcess(in_model_data_t &inData, glb_data_t &glbData)
{
    glbData.vertices.resize(m_pointMap.size() * 3);
    for (auto &item : m_pointMap) {
        glbData.vertices[item.second * 3] = item.first[0];
        glbData.vertices[item.second * 3 + 1] = item.first[1];
        glbData.vertices[item.second * 3 + 2] = item.first[2];
    }
    glbData.vertexIndices.resize(glbData.texCoordIndices.size());
    for (size_t i = 0; i < glbData.vertexIndices.size(); ++i) {
        glbData.vertexIndices[i] = m_pointIndexMap[glbData.texCoordIndices[i]];
    }
    inData.vertices = glbData.vertices.data();
    inData.texCoords = glbData.texCoords.data();
    inData.vertexIndecis = glbData.vertexIndices.data();
    inData.texCoordIndices = glbData.texCoordIndices.data();
    inData.materialDatas = glbData.materialDatas.data();
    inData.vertexCnt = glbData.vertices.size() / 3;
    inData.texCoordCnt = glbData.texCoords.size() / 2;
    inData.triangleCnt = glbData.vertexIndices.size() / 3;
    inData.materialCnt = glbData.materialDatas.size();
    inData.vertexStride = sizeof(glbData.vertices[0]) * 3;
    inData.texCoordStride = sizeof(glbData.texCoords[0]) * 2;
    inData.vertexIndexStride = sizeof(glbData.vertexIndices[0]);
    inData.texCoordIndexStride = sizeof(glbData.texCoordIndices[0]);
    inData.faceIndexStride = sizeof(glbData.vertexIndices[0]) * 3;
}

}} // namespace Slic3r::GUI
