#ifndef slic3r_GUI_CMLoadGlb_hpp_
#define slic3r_GUI_CMLoadGlb_hpp_

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <Eigen/Geometry>
#include <wx/string.h>
#include "tinygltf/tiny_gltf.h"
#include "ConvertModelDef.hpp"
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

struct glb_data_t {
    std::vector<float> vertices;
    std::vector<float> texCoords;
    std::vector<int32_t> vertexIndices;
    std::vector<int32_t> texCoordIndices;
    std::vector<in_material_data_t> materialDatas;
    std::deque<in_texture_data_t> textureDatas;
    std::deque<std::vector<uint8_t>> multiTextureBits;
};

class CMLoadGlb
{
public:
    bool loadGlb(const wxString &glbPath, in_model_data_t &inData, glb_data_t &glbData);

private:
    struct indices_info_t {
        int maxIndex;
        int cnt;
    };
    bool addNode(const tinygltf::Model &model, int nodeIdx, const Eigen::Matrix4f &parentMatrix,
        in_model_data_t &inData, glb_data_t &glbData);

    Eigen::Matrix4f getNodeMatrix(const tinygltf::Node &node);

    bool addIndices(const tinygltf::Model &model, int mode, int accessorIdx, glb_data_t &glbData,
        indices_info_t &info);

    template<typename Ty>
    void addIndices(Ty indices, int cnt, int mode, glb_data_t &glbData, indices_info_t &info);

    bool addMaterial(const tinygltf::Model &model, int materialIdx, in_model_data_t &inData,
        glb_data_t &glbData);

    bool getTextureIndex(const tinygltf::Model &model, int materialIdx, int &textureIdx);

    bool addNewTexture(const tinygltf::Model &model, int textureIdx, const tinygltf::Image &image,
        glb_data_t &glbData);

    bool getWrapType(const tinygltf::Model &model, int textureIdx, texture_wrap_type_t wrap[2]);

    bool addCoords(const tinygltf::Model &model, const tinygltf::Primitive &primitive,
        const indices_info_t &info, const Eigen::Matrix4f &matrix, glb_data_t &glbData);

    void postProcess(in_model_data_t &inData, glb_data_t &glbData);

private:
    int m_baseIndex;
    std::set<int> m_nodeParentSet;
    std::vector<int32_t> m_pointIndexMap;
    ConvertModelUtils::point_map_t m_pointMap;
    std::map<int, int> m_textrueIndexMap;
};

}} // namespace Slic3r::GUI

#endif
