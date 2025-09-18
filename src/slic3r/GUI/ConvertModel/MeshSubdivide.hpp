#ifndef slic3r_GUI_MeshSubdivide_hpp_
#define slic3r_GUI_MeshSubdivide_hpp_

#include <memory>
#include <utility>
#include "ConvertModelDef.hpp"
#include "MeshSubdivideBase.hpp"
#include "TextureSample.hpp"

namespace Slic3r { namespace GUI {

class MeshSubdivide : protected MeshSubdivideBase
{
public:
    MeshSubdivide(cvt_faces_t &faces, cvt_root_half_edges_t &rootEdges,
        cvt_root_points_t &rootPoints, const cvt_colors_t &dstColors,
        const texture_sample_ptrs_t &textureSamples);

    void subdivideRootFaces(int srcFaceCnt, float subdivideMaxEdgeLen);

private:
    bool subdivideFace(cvt_face_t &face, int32_t rootP0, int32_t rootP1, int32_t rootP2);

    bool subdivideFace3(cvt_face_t &face, int32_t rootP0, int32_t rootP1, int32_t rootP2);

    bool subdivideFace2A(cvt_face_t &face, int edge0, int edge1, int edge2, int32_t rootP0,
        int32_t rootP1, int32_t rootP2);

    bool subdivideFace2B(cvt_face_t &face, int edge0, int edge1, int edge2, int32_t rootP0,
        int32_t rootP1, int32_t rootP2);

    bool subdivideFace1(cvt_face_t &face, int edge0, int edge1, int edge2, int32_t rootP0,
        int32_t rootP1, int32_t rootP2);

    float getRootPointDist2(int32_t rootP0, int32_t rootP1);

private:
    float m_maxEdgeLenSqr;
};

}} // namespace Slic3r::GUI

#endif
