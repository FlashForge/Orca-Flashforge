#ifndef slic3r_GUI_MeshSubdivideBase_hpp_
#define slic3r_GUI_MeshSubdivideBase_hpp_

#include <memory>
#include <utility>
#include "ConvertModelDef.hpp"
#include "TextureSample.hpp"

namespace Slic3r { namespace GUI {

class MeshSubdivideBase
{
protected:
    MeshSubdivideBase(cvt_faces_t &faces, cvt_root_half_edges_t &rootEdges,
        cvt_root_points_t &rootPoints, const cvt_colors_t &dstColors,
        const texture_sample_ptrs_t &textureSamples);

    std::pair<int32_t, bool> insertRootPoint(cvt_root_half_edge_t &rootEdge, int32_t pos);

    std::pair<int32_t, int32_t> pushRootEdgePair(int32_t rootP0, int32_t rootP1);

    void setIsolatedData(const cvt_half_edge_t &edge, int32_t rootPoint, int32_t pos);

    int32_t insertTwinRootPoint(const cvt_half_edge_t &edge, int32_t rootPointIdx, int32_t pos);

    cvt_color_t getTextureColor(int32_t texIndex, float x, float y);

protected:
    cvt_faces_t &m_faces;
    cvt_root_half_edges_t &m_rootEdges;
    cvt_root_points_t &m_rootPoints;
    const cvt_colors_t &m_dstColors;
    const texture_sample_ptrs_t &m_textureSamples;
};

}} // namespace Slic3r::GUI

#endif
