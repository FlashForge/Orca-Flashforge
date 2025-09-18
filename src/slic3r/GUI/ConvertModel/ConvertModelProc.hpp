#ifndef slic3r_GUI_ConvertModelProc_hpp_
#define slic3r_GUI_ConvertModelProc_hpp_

#include <cstdint>
#include <memory>
#include <unordered_map>
#include "ConvertModelDef.hpp"
#include "TextureSample.hpp"

namespace Slic3r { namespace GUI {

class ConvertModelProc
{
public:
    ConvertModelProc(const in_model_data_t &inData, const in_cvt_params_t &params);

    cvt_colors_t clusterColors(int colorNum);

    void doConvert(const cvt_colors_t &dstColors, out_model_data_t &outData);

private:
    struct RootPointHash {
        size_t operator()(const cvt_root_point_t &p) const;
    };
    struct RootPointEqual {
        bool operator()(const cvt_root_point_t &p0, const cvt_root_point_t &p1) const;
    };
    using point_map_t = std::unordered_map<cvt_root_point_t, int32_t, RootPointHash, RootPointEqual>;

    void initRootFaces(const in_model_data_t &inData);

    void initPairEdge(const in_model_data_t &inData);

    void updateRootPointColors(const cvt_colors_t &dstColors);

    void transformModel(out_model_data_t &outData);

    int getTexIndex(const in_model_data_t &inData, int triangleIdx);

    void makePointFromSrcVertex(const in_model_data_t &inData, int vertexIdx, int texCoordIdx,
        int texIndex, cvt_point_t &point, point_map_t &pointMap);

    void getModelSize(const in_model_data_t &inData, float modelSize[3]);

private:
    int                   m_srcFaceCnt;
    in_cvt_params_t       m_params;
    float                 m_modelSize[3];
    float                 m_subdivideMaxEdgeLen;
    cvt_color_datas_t     m_colorDatas;
    cvt_faces_t           m_faces;
    cvt_root_half_edges_t m_rootEdges;
    cvt_root_points_t     m_rootPoints;
    texture_sample_ptrs_t m_textureSamples;
};

}} // namespace Slic3r::GUI

#endif
