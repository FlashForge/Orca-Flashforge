#include "CMLoadGlb.hpp"
#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

bool CMLoadGlb::loadGlb(const wxString &glbPath, in_model_data_t &inData, glb_data_t &glbData)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, glbPath.utf8_string())) {
        BOOST_LOG_TRIVIAL(error) << "load glb error, " << err << ", " << glbPath.utf8_string();
        return false;
    }
    m_baseIndex = 0;
    m_pointIndexMap.clear();
    m_pointMap.clear();
    m_textrueIndexMap.clear();
    if (model.defaultScene < 0 || model.defaultScene >= model.scenes.size()) {
        BOOST_LOG_TRIVIAL(warning) << "invalid glb scene, " << model.defaultScene;
        return false;
    }
    for (auto node : model.scenes[model.defaultScene].nodes) {
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
            indices_info_t info;
            if (!addIndices(model, primitive.mode, primitive.indices, glbData, info)) {
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

bool CMLoadGlb::addIndices(const tinygltf::Model &model, int mode, int accessorIdx, glb_data_t &glbData,
    indices_info_t &info)
{
    info.maxIndex = 0;
    info.cnt = 0;
    if (mode != TINYGLTF_MODE_TRIANGLES && mode != TINYGLTF_MODE_TRIANGLE_FAN
     && mode != TINYGLTF_MODE_TRIANGLE_STRIP) {
        return true;
    }
    if (accessorIdx < 0 || accessorIdx >= model.accessors.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb accessor, " << accessorIdx;
        return false;
    }
    auto &indexAccessor = model.accessors[accessorIdx];
    if (indexAccessor.bufferView < 0 || indexAccessor.bufferView >= model.bufferViews.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb bufferView, " << indexAccessor.bufferView;
        return false;
    }
    if (indexAccessor.count < 3) {
        return true;
    }
    auto &indexView = model.bufferViews[indexAccessor.bufferView];
    if (indexView.buffer < 0 || indexView.buffer >= model.buffers.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb buffer, " << indexView.buffer;
        return false;
    }
    auto indexData = &model.buffers[indexView.buffer].data[indexAccessor.byteOffset + indexView.byteOffset];
    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        addIndices((const uint32_t *)indexData, indexAccessor.count, mode, glbData, info);
    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        addIndices((const uint16_t *)indexData, indexAccessor.count, mode, glbData, info);
    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        addIndices((const uint8_t *)indexData, indexAccessor.count, mode, glbData, info);
    }
    return true;
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
        return false;
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
    if (samperIdx < 0 || samperIdx < model.samplers.size()) {
        return TEX_WRAP_REPEAT;
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
    if (vertexAccessor.bufferView < 0 || vertexAccessor.bufferView >= model.bufferViews.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb bufferView, " << vertexAccessor.bufferView;
        return false;
    }
    auto &vertexView = model.bufferViews[vertexAccessor.bufferView];
    if (vertexView.buffer < 0 || vertexView.buffer >= model.buffers.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb buffer, " << vertexView.buffer;
        return false;
    }
    auto texCoordIt = primitive.attributes.find("TEXCOORD_0");
    if (texCoordIt == primitive.attributes.end()) {
        BOOST_LOG_TRIVIAL(error) << "can't find glb texture coord";
        return false;
    }
    if (texCoordIt->second < 0 || texCoordIt->second >= model.accessors.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb accessor, " << texCoordIt->second;
        return false;
    }
    auto &texCoordAccessor = model.accessors[texCoordIt->second];
    if (texCoordAccessor.bufferView < 0 || texCoordAccessor.bufferView >= model.bufferViews.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb bufferView, " << texCoordAccessor.bufferView;
        return false;
    }
    auto &texCoordView = model.bufferViews[texCoordAccessor.bufferView];
    if (texCoordView.buffer < 0 || texCoordView.buffer >= model.buffers.size()) {
        BOOST_LOG_TRIVIAL(error) << "invalid glb buffer, " << texCoordView.buffer;
        return false;
    }
    if (info.maxIndex >= vertexAccessor.count) {
        BOOST_LOG_TRIVIAL(error) << "glb index out of range, " << info.maxIndex;
        return false;
    }
    if (vertexAccessor.count != texCoordAccessor.count) {
        BOOST_LOG_TRIVIAL(error) << "invalid vertex/texture coord count, " << vertexAccessor.count
            << "/" << texCoordAccessor.count;
        return false;
    }
    auto vertexData = (const float*)&model.buffers[vertexView.buffer].data[
        vertexAccessor.byteOffset + vertexView.byteOffset];
    for (int i = 0; i < vertexAccessor.count; ++i) {
        Eigen::Vector4f pos(vertexData[i * 3], vertexData[i * 3 + 1], vertexData[i * 3 + 2], 1.0f);
        pos = matrix * pos;
        std::array<float, 3> vertex = { pos[0], pos[1], pos[2] };
        auto pointIt = m_pointMap.emplace(vertex, m_pointMap.size()).first;
        m_pointIndexMap.push_back(pointIt->second);
    }
    auto texCoordData = (const float*)&model.buffers[texCoordView.buffer].data[
        texCoordAccessor.byteOffset + texCoordView.byteOffset];
    for (int i = 0; i < texCoordAccessor.count; ++i) {
        glbData.texCoords.push_back(texCoordData[i * 2]);
        glbData.texCoords.push_back(texCoordData[i * 2 + 1]);
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
