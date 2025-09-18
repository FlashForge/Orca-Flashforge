#ifndef slic3r_GUI_ConvertModelDef_hpp_
#define slic3r_GUI_ConvertModelDef_hpp_

#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include <boost/container/deque.hpp>

namespace Slic3r { namespace GUI {

enum texture_wrap_type_t {
    TEX_WRAP_CLAMP_TO_EDGE,
    TEX_WRAP_REPEAT,
    TXT_WRAP_MIRRORED_REPEAT,
};

struct in_cvt_params_t {
    bool transCoordSys{ false };
    float minModelExtent{ 75.0f };
    float maxPrintSize[3]{ 200.0f, 200.0f, 200.0f };
    float subdivideMaxEdgeLenRatio{ 0.2f };
    float colorMinAreaRatio{ 1.0f };
};

struct in_texture_data_t {
    const uint8_t *bits;
    int32_t width;
    int32_t height;
    int32_t channels;
    int32_t bytePerLine;
    texture_wrap_type_t wraps[2];
    bool flipY;
};

struct in_material_data_t {
    int32_t endTriangleIndex;
    const in_texture_data_t *textureData;
};

struct in_model_data_t {
    const float *vertices;
    const float *texCoords;
    const int32_t *vertexIndecis;
    const int32_t *texCoordIndices;
    const in_material_data_t *materialDatas;
    int32_t vertexCnt;
    int32_t texCoordCnt;
    int32_t triangleCnt;
    int32_t materialCnt;
    int32_t vertexStride;
    int32_t texCoordStride;
    int32_t vertexIndexStride;
    int32_t texCoordIndexStride;
    int32_t faceIndexStride;
};

struct out_triangle_data_t {
    int32_t vertexIndices[3];
    int32_t colorIndex;
};

struct out_model_data_t {
    std::vector<std::array<float, 3>> vertices;
    std::vector<std::array<uint8_t, 3>> colors; // RGB
    std::vector<out_triangle_data_t> triangles;
};

// the number of subdivision iterations cannot exceed 29, and the summation calculation does not overflow.
const int32_t CvtEdgePosMax = (1 << 29);

struct cvt_color_data_t {
    std::array<uint8_t, 3> color;
    double weight;
};

struct cvt_root_point_t {
    float coord[3];
    float texCoord[2];
    int32_t texIndex;
    std::array<uint8_t, 3> color; // RGB
};

struct cvt_point_t {
    int32_t rootPoint;
    int32_t pos; // start 0, end CvtEdgePosMax
    bool isolated;
};

struct cvt_root_half_edge_t {
    std::vector<cvt_point_t> points; // order by start-to-end sequence
    int32_t twin; // invalid -1
};

struct cvt_half_edge_t {
    int32_t rootEdge;
    int32_t pointsPos[2];
};

struct cvt_face_t {
    std::array<int32_t, 6> children; // invalid -1
    cvt_half_edge_t edges[3];
};

class TextureSample;
using cvt_color_t = std::array<uint8_t, 3>;
using cvt_colors_t = std::vector<cvt_color_t>;
using cvt_color_datas_t = std::vector<cvt_color_data_t>;
using cvt_faces_t = boost::container::deque<cvt_face_t>;
using cvt_root_half_edges_t = boost::container::deque<cvt_root_half_edge_t>;
using cvt_root_points_t = boost::container::deque<cvt_root_point_t>;
using cvt_points_it_t = std::vector<cvt_point_t>::iterator;
using cvt_points_cit_t = std::vector<cvt_point_t>::const_iterator;
using texture_sample_ptrs_t = std::vector<std::unique_ptr<TextureSample>>;

}} // namespace Slic3r::GUI

#endif
