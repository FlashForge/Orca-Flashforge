#ifndef slic3r_GUI_SampleColorData_hpp_
#define slic3r_GUI_SampleColorData_hpp_

#include <array>
#include <memory>
#include <unordered_map>
#include "ConvertModelDef.hpp"
#include "TextureSample.hpp"

namespace Slic3r { namespace GUI {

class SampleColorData
{
public:
    SampleColorData(const cvt_faces_t &faces, const cvt_root_half_edges_t &rootEdges,
        const cvt_root_points_t &rootPoints, const texture_sample_ptrs_t &textureSamples);

    cvt_color_datas_t getColorDatas(int srcFaceCnt, float subdivideMaxEdgeLen);

private:
    struct point_data_t {
        float coord[3];
        float texCoord[2];
        int32_t texIndex;
        cvt_color_t color;
    };
    struct face_data_t {
        const float *coord[3];
        const float *texCoord[3];
        const cvt_color_t *color[3];
        int32_t texIndex;
    };
    struct CvtColorHash {
        size_t operator()(const cvt_color_t &color) const {
            return color[0] + (color[1] << 8) + (color[2] << 16);
        }
    };
    bool subdivideFace(const face_data_t &faceData, bool isSrcFace);

    bool subdivideFace3(const face_data_t &faceData);

    bool subdivideFace2(const face_data_t &faceData);

    bool subdivideFace1(const face_data_t &faceData);

    void getFaceColorData(const face_data_t &faceData);

    void getNewFaceData(const face_data_t &src, face_data_t &dst, int p0, int p1, int p2);

    void getMidPointData(const face_data_t &faceData, int p0, int p1, point_data_t &pointData);

    float getPointDist2(const float *coord0, const float *coord1);

private:
    const cvt_faces_t &m_faces;
    const cvt_root_half_edges_t &m_rootEdges;
    const cvt_root_points_t &m_rootPoints;
    const texture_sample_ptrs_t &m_textureSamples;
    float m_maxEdgeLenSqr;
    std::unordered_map<cvt_color_t, double, CvtColorHash> m_colorMap;
};

}} // namespace Slic3r::GUI

#endif
