#ifndef slic3r_GUI_Vertex2FaceColor_hpp_
#define slic3r_GUI_Vertex2FaceColor_hpp_

#include <array>
#include <utility>
#include "ConvertModelDef.hpp"
#include "MeshSubdivideBase.hpp"
#include "TextureSample.hpp"

namespace Slic3r { namespace GUI {

class Vertex2FaceColor : protected MeshSubdivideBase
{
public:
    Vertex2FaceColor(cvt_faces_t &faces, cvt_root_half_edges_t &rootEdges,
        cvt_root_points_t &rootPoints);

    void subdivideMixedColorFaces(int srcFaceCnt);

private:
    void subdivideMixedColorFace(cvt_face_t &face);

    int32_t getRootPoint(const cvt_face_t &face, int idx);

    int32_t pushCentroidRootPoint(int32_t rootP0, int32_t rootP1, int32_t rootP2);

private:
    cvt_colors_t m_dummyColors;
    texture_sample_ptrs_t m_dummyTextureSamples;
};

}} // namespace Slic3r::GUI

#endif
