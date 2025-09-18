#include "Vertex2FaceColor.hpp"
#include <algorithm>
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

Vertex2FaceColor::Vertex2FaceColor(cvt_faces_t &faces, cvt_root_half_edges_t &rootEdges,
    cvt_root_points_t &rootPoints)
    : MeshSubdivideBase(faces, rootEdges, rootPoints, m_dummyColors, m_dummyTextureSamples)
{
}

void Vertex2FaceColor::subdivideMixedColorFaces(int srcFaceCnt)
{
    for (int i = 0; i < srcFaceCnt; ++i) {
        subdivideMixedColorFace(m_faces[i]);
    }
}

void Vertex2FaceColor::subdivideMixedColorFace(cvt_face_t &face)
{
    if (!std::all_of(face.children.begin(), face.children.end(), [](int32_t n) { return n == -1; })) {
        for (auto &child : face.children) {
            if (child != -1) {
                subdivideMixedColorFace(m_faces[child]);
            }
        }
        return;
    }
    // when converting vertex colors to face colors, the face color is determined by the first vertex's color.
    int32_t rootP0 = getRootPoint(face, 0);
    int32_t rootP1 = getRootPoint(face, 1);
    int32_t rootP2 = getRootPoint(face, 2);
    if (m_rootPoints[rootP0].color == m_rootPoints[rootP1].color
     && m_rootPoints[rootP1].color == m_rootPoints[rootP2].color) {
        return;
    }
    int32_t startPos0 = face.edges[0].pointsPos[0];
    int32_t startPos1 = face.edges[1].pointsPos[0];
    int32_t startPos2 = face.edges[2].pointsPos[0];
    int32_t endPos0 = face.edges[0].pointsPos[1];
    int32_t endPos1 = face.edges[1].pointsPos[1];
    int32_t endPos2 = face.edges[2].pointsPos[1];
    int32_t midPos0 = (startPos0 + endPos0) / 2;
    int32_t midPos1 = (startPos1 + endPos1) / 2;
    int32_t midPos2 = (startPos2 + endPos2) / 2;

    int32_t rootCP = pushCentroidRootPoint(rootP0, rootP1, rootP2);
    auto rootMP0 = insertRootPoint(m_rootEdges[face.edges[0].rootEdge], midPos0);
    auto rootMP1 = insertRootPoint(m_rootEdges[face.edges[1].rootEdge], midPos1);
    auto rootMP2 = insertRootPoint(m_rootEdges[face.edges[2].rootEdge], midPos2);
    auto rootEP0 = pushRootEdgePair(rootCP, rootP0);
    auto rootEP1 = pushRootEdgePair(rootCP, rootMP0.first);
    auto rootEP2 = pushRootEdgePair(rootCP, rootP1);
    auto rootEP3 = pushRootEdgePair(rootCP, rootMP1.first);
    auto rootEP4 = pushRootEdgePair(rootCP, rootP2);
    auto rootEP5 = pushRootEdgePair(rootCP, rootMP2.first);

    // subdivide the triangle into three parts along the medians
    cvt_face_t childFaces[6];
    childFaces[0].edges[0] = { rootEP0.second, 0, CvtEdgePosMax };
    childFaces[0].edges[1] = { rootEP5.first, 0, CvtEdgePosMax };
    childFaces[0].edges[2] = { face.edges[2].rootEdge, midPos2, endPos2 };
    childFaces[1].edges[0] = { face.edges[0].rootEdge, startPos0, midPos0 };
    childFaces[1].edges[1] = { rootEP1.second, 0, CvtEdgePosMax };
    childFaces[1].edges[2] = { rootEP0.first, 0, CvtEdgePosMax };
    childFaces[2].edges[0] = { rootEP2.second, 0, CvtEdgePosMax };
    childFaces[2].edges[1] = { rootEP1.first, 0, CvtEdgePosMax };
    childFaces[2].edges[2] = { face.edges[0].rootEdge, midPos0, endPos0 };
    childFaces[3].edges[0] = { face.edges[1].rootEdge, startPos1, midPos1 };
    childFaces[3].edges[1] = { rootEP3.second, 0, CvtEdgePosMax };
    childFaces[3].edges[2] = { rootEP2.first, 0, CvtEdgePosMax };
    childFaces[4].edges[0] = { rootEP4.second, 0, CvtEdgePosMax };
    childFaces[4].edges[1] = { rootEP3.first, 0, CvtEdgePosMax };
    childFaces[4].edges[2] = { face.edges[1].rootEdge, midPos1, endPos1 };
    childFaces[5].edges[0] = { face.edges[2].rootEdge, startPos2, midPos2 };
    childFaces[5].edges[1] = { rootEP5.second, 0, CvtEdgePosMax };
    childFaces[5].edges[2] = { rootEP4.first, 0, CvtEdgePosMax };

    for (int i = 0; i < 6; ++i) {
        childFaces[i].children.fill(-1);
        face.children[i] = m_faces.size();
        m_faces.push_back(childFaces[i]);
    }
    setIsolatedData(face.edges[0], rootMP0.first, midPos0);
    setIsolatedData(face.edges[1], rootMP1.first, midPos1);
    setIsolatedData(face.edges[2], rootMP2.first, midPos2);
}

int32_t Vertex2FaceColor::getRootPoint(const cvt_face_t &face, int idx)
{
    const cvt_half_edge_t &edge = face.edges[idx];
    const cvt_root_half_edge_t &rootEdge = m_rootEdges[edge.rootEdge];
    return ConvertModelUtils::findPoint(rootEdge, edge.pointsPos[0])->rootPoint;
}

int32_t Vertex2FaceColor::pushCentroidRootPoint(int32_t rootP0, int32_t rootP1, int32_t rootP2)
{
    const cvt_root_point_t &p0 = m_rootPoints[rootP0];
    const cvt_root_point_t &p1 = m_rootPoints[rootP1];
    const cvt_root_point_t &p2 = m_rootPoints[rootP2];

    cvt_root_point_t rootPoint;
    rootPoint.coord[0] = (p0.coord[0] + p1.coord[0] + p2.coord[0]) / 3;
    rootPoint.coord[1] = (p0.coord[1] + p1.coord[1] + p2.coord[1]) / 3;
    rootPoint.coord[2] = (p0.coord[2] + p1.coord[2] + p2.coord[2]) / 3;
    rootPoint.texCoord[0] = (p0.texCoord[0] + p1.texCoord[0] + p2.texCoord[0]) / 3;
    rootPoint.texCoord[1] = (p0.texCoord[1] + p1.texCoord[1] + p2.texCoord[1]) / 3;
    rootPoint.texIndex = p0.texIndex;
    rootPoint.color = getTextureColor(rootPoint.texIndex, rootPoint.texCoord[0], rootPoint.texCoord[1]);

    m_rootPoints.push_back(rootPoint);
    return m_rootPoints.size() - 1;
}

}} // namespace Slic3r::GUI
