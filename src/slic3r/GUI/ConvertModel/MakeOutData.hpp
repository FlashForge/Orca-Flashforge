#ifndef slic3r_GUI_MakeOutData_hpp_
#define slic3r_GUI_MakeOutData_hpp_

#include <cstdint>
#include <map>
#include <unordered_map>
#include "ConvertModelDef.hpp"
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

class MakeOutData
{
public:
    MakeOutData(const cvt_faces_t &faces, const cvt_root_half_edges_t &rootEdges,
        const cvt_root_points_t &rootPoints);

    void makeData(int srcFaceCnt, out_model_data_t &outData);

private:
    void makeFaceData(const cvt_face_t &face, out_model_data_t &outData);

    void makeTriangles(std::vector<int32_t> multiPoints[3], out_model_data_t &outData);

    void makeTriangle(int32_t rootP0, int32_t rootP1, int32_t rootP2, cvt_color_t color,
        out_model_data_t &outData);

    int32_t pushOutPoint(int32_t rootP, out_model_data_t &outData);

private:
    const cvt_faces_t &m_faces;
    const cvt_root_half_edges_t &m_rootEdges;
    const cvt_root_points_t &m_rootPoints;
    std::map<cvt_color_t, int> m_colorMap;
    ConvertModelUtils::point_map_t m_pointMap;
};

}} // namespace Slic3r::GUI

#endif
