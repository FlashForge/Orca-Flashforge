#include "MeshSubdivide.hpp"
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

MeshSubdivide::MeshSubdivide(cvt_faces_t &faces, cvt_root_half_edges_t &rootEdges,
    cvt_root_points_t &rootPoints, const cvt_colors_t &dstColors,
    const texture_sample_ptrs_t &textureSamples)
    : MeshSubdivideBase(faces, rootEdges, rootPoints, dstColors, textureSamples)
    , m_maxEdgeLenSqr(1.0f)
{
}

void MeshSubdivide::subdivideRootFaces(int srcFaceCnt, float subdivideMaxEdgeLen)
{
    m_maxEdgeLenSqr = subdivideMaxEdgeLen * subdivideMaxEdgeLen;
    for (int i = 0; i < srcFaceCnt; ++i) {
        subdivideFace(m_faces[i],
            m_rootEdges[m_faces[i].edges[0].rootEdge].points[0].rootPoint,
            m_rootEdges[m_faces[i].edges[1].rootEdge].points[0].rootPoint,
            m_rootEdges[m_faces[i].edges[2].rootEdge].points[0].rootPoint);
    }
}

bool MeshSubdivide::subdivideFace(cvt_face_t &face, int32_t rootP0, int32_t rootP1, int32_t rootP2)
{
    float distSqr01 = getRootPointDist2(rootP0, rootP1);
    float distSqr12 = getRootPointDist2(rootP1, rootP2);
    float distSqr20 = getRootPointDist2(rootP2, rootP0);
    bool subdiv01 = distSqr01 > m_maxEdgeLenSqr;
    bool subdiv12 = distSqr12 > m_maxEdgeLenSqr;
    bool subdiv20 = distSqr20 > m_maxEdgeLenSqr;
    bool isSameColor01 = m_rootPoints[rootP0].color == m_rootPoints[rootP1].color;
    bool isSameColor12 = m_rootPoints[rootP1].color == m_rootPoints[rootP2].color;

    bool hasSubdivice = false;
    if (subdiv01 && subdiv12 && subdiv20) {
        hasSubdivice = subdivideFace3(face, rootP0, rootP1, rootP2);
    } else if (subdiv01 && subdiv12) {
        if (distSqr01 > distSqr12) {
            hasSubdivice = subdivideFace2A(face, 0, 1, 2, rootP0, rootP1, rootP2);
        } else {
            hasSubdivice = subdivideFace2B(face, 0, 1, 2, rootP0, rootP1, rootP2);
        }
    } else if (subdiv12 && subdiv20) {
        if (distSqr12 > distSqr20) {
            hasSubdivice = subdivideFace2A(face, 1, 2, 0, rootP1, rootP2, rootP0);
        } else {
            hasSubdivice = subdivideFace2B(face, 1, 2, 0, rootP1, rootP2, rootP0);
        }
    } else if (subdiv20 && subdiv01) {
        if (distSqr20 > distSqr01) {
            hasSubdivice = subdivideFace2A(face, 2, 0, 1, rootP2, rootP0, rootP1);
        } else {
            hasSubdivice = subdivideFace2B(face, 2, 0, 1, rootP2, rootP0, rootP1);
        }
    } else if (subdiv01) {
        hasSubdivice = subdivideFace1(face, 0, 1, 2, rootP0, rootP1, rootP2);
    } else if (subdiv12) {
        hasSubdivice = subdivideFace1(face, 1, 2, 0, rootP1, rootP2, rootP0);
    } else if (subdiv20) {
        hasSubdivice = subdivideFace1(face, 2, 0, 1, rootP2, rootP0, rootP1);
    }
    return hasSubdivice || !isSameColor01 || !isSameColor12;
}

bool MeshSubdivide::subdivideFace3(cvt_face_t &face, int32_t rootP0, int32_t rootP1, int32_t rootP2)
{
    int32_t startPos0 = face.edges[0].pointsPos[0];
    int32_t startPos1 = face.edges[1].pointsPos[0];
    int32_t startPos2 = face.edges[2].pointsPos[0];
    int32_t endPos0 = face.edges[0].pointsPos[1];
    int32_t endPos1 = face.edges[1].pointsPos[1];
    int32_t endPos2 = face.edges[2].pointsPos[1];
    int32_t midPos0 = (startPos0 + endPos0) / 2;
    int32_t midPos1 = (startPos1 + endPos1) / 2;
    int32_t midPos2 = (startPos2 + endPos2) / 2;
    
    auto rootMP0 = insertRootPoint(m_rootEdges[face.edges[0].rootEdge], midPos0);
    auto rootMP1 = insertRootPoint(m_rootEdges[face.edges[1].rootEdge], midPos1);
    auto rootMP2 = insertRootPoint(m_rootEdges[face.edges[2].rootEdge], midPos2);
    auto rootEP0 = pushRootEdgePair(rootMP0.first, rootMP2.first);
    auto rootEP1 = pushRootEdgePair(rootMP1.first, rootMP0.first);
    auto rootEP2 = pushRootEdgePair(rootMP2.first, rootMP1.first);

    cvt_face_t childFaces[4];
    childFaces[0].children.fill(-1);
    childFaces[1].children.fill(-1);
    childFaces[2].children.fill(-1);
    childFaces[3].children.fill(-1);
    childFaces[0].edges[0] = { face.edges[2].rootEdge, midPos2, endPos2 };
    childFaces[0].edges[1] = { face.edges[0].rootEdge, startPos0, midPos0 };
    childFaces[0].edges[2] = { rootEP0.first, 0, CvtEdgePosMax };
    childFaces[1].edges[0] = { face.edges[0].rootEdge, midPos0, endPos0 };
    childFaces[1].edges[1] = { face.edges[1].rootEdge, startPos1, midPos1 };
    childFaces[1].edges[2] = { rootEP1.first, 0, CvtEdgePosMax };
    childFaces[2].edges[0] = { face.edges[1].rootEdge, midPos1, endPos1 };
    childFaces[2].edges[1] = { face.edges[2].rootEdge, startPos2, midPos2 };
    childFaces[2].edges[2] = { rootEP2.first, 0, CvtEdgePosMax };
    childFaces[3].edges[0] = { rootEP0.second, 0, CvtEdgePosMax };
    childFaces[3].edges[1] = { rootEP1.second, 0, CvtEdgePosMax };
    childFaces[3].edges[2] = { rootEP2.second, 0, CvtEdgePosMax };
    
    bool needExist0 = subdivideFace(childFaces[0], rootMP2.first, rootP0, rootMP0.first);
    bool needExist1 = subdivideFace(childFaces[1], rootMP0.first, rootP1, rootMP1.first);
    bool needExist2 = subdivideFace(childFaces[2], rootMP1.first, rootP2, rootMP2.first);
    bool needExist3 = subdivideFace(childFaces[3], rootMP2.first, rootMP0.first, rootMP1.first);
    if (!needExist0 && !needExist1 && !needExist2 && !needExist3) {
        int insertedPointCnt = (int)rootMP0.second + (int)rootMP1.second + (int)rootMP2.second;
        m_rootPoints.resize(m_rootPoints.size() - insertedPointCnt);
        m_rootEdges.resize(m_rootEdges.size() - 6);
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        face.children[i] = m_faces.size();
        m_faces.push_back(childFaces[i]);
    }
    
    setIsolatedData(face.edges[0], rootMP0.first, midPos0);
    setIsolatedData(face.edges[1], rootMP1.first, midPos1);
    setIsolatedData(face.edges[2], rootMP2.first, midPos2);
    return true;
}

bool MeshSubdivide::subdivideFace2A(cvt_face_t &face, int edge0, int edge1, int edge2, int32_t rootP0,
    int32_t rootP1, int32_t rootP2)
{
    int32_t startPos0 = face.edges[edge0].pointsPos[0];
    int32_t startPos1 = face.edges[edge1].pointsPos[0];
    int32_t startPos2 = face.edges[edge2].pointsPos[0];
    int32_t endPos0 = face.edges[edge0].pointsPos[1];
    int32_t endPos1 = face.edges[edge1].pointsPos[1];
    int32_t endPos2 = face.edges[edge2].pointsPos[1];
    int32_t midPos0 = (startPos0 + endPos0) / 2;
    int32_t midPos1 = (startPos1 + endPos1) / 2;

    auto rootMP0 = insertRootPoint(m_rootEdges[face.edges[edge0].rootEdge], midPos0);
    auto rootMP1 = insertRootPoint(m_rootEdges[face.edges[edge1].rootEdge], midPos1);
    auto rootEP0 = pushRootEdgePair(rootMP0.first, rootP2);
    auto rootEP1 = pushRootEdgePair(rootMP1.first, rootMP0.first);

    cvt_face_t childFaces[3];
    childFaces[0].children.fill(-1);
    childFaces[1].children.fill(-1);
    childFaces[2].children.fill(-1);
    childFaces[0].edges[0] = { face.edges[edge0].rootEdge, startPos0, midPos0 };
    childFaces[0].edges[1] = { rootEP0.first, 0, CvtEdgePosMax };
    childFaces[0].edges[2] = { face.edges[edge2].rootEdge, startPos2, endPos2 };
    childFaces[1].edges[0] = { face.edges[edge0].rootEdge, midPos0, endPos0 };
    childFaces[1].edges[1] = { face.edges[edge1].rootEdge, startPos1, midPos1 };
    childFaces[1].edges[2] = { rootEP1.first, 0, CvtEdgePosMax };
    childFaces[2].edges[0] = { face.edges[edge1].rootEdge, midPos1, endPos1 };
    childFaces[2].edges[1] = { rootEP0.second, 0, CvtEdgePosMax };
    childFaces[2].edges[2] = { rootEP1.second, 0, CvtEdgePosMax };

    bool needExist0 = subdivideFace(childFaces[0], rootP0, rootMP0.first, rootP2 );
    bool needExist1 = subdivideFace(childFaces[1], rootMP0.first, rootP1, rootMP1.first );
    bool needExist2 = subdivideFace(childFaces[2], rootMP1.first, rootP2, rootMP0.first );
    if (!needExist0 && !needExist1 && !needExist2) {
        int insertedPointCnt = (int)rootMP0.second + (int)rootMP1.second;
        m_rootPoints.resize(m_rootPoints.size() - insertedPointCnt);
        m_rootEdges.resize(m_rootEdges.size() - 4);
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        face.children[i] = m_faces.size();
        m_faces.push_back(childFaces[i]);
    }

    setIsolatedData(face.edges[edge0], rootMP0.first, midPos0);
    setIsolatedData(face.edges[edge1], rootMP1.first, midPos1);
    return true;
}

bool MeshSubdivide::subdivideFace2B(cvt_face_t &face, int edge0, int edge1, int edge2, int32_t rootP0,
    int32_t rootP1, int32_t rootP2)
{
    int32_t startPos0 = face.edges[edge0].pointsPos[0];
    int32_t startPos1 = face.edges[edge1].pointsPos[0];
    int32_t startPos2 = face.edges[edge2].pointsPos[0];
    int32_t endPos0 = face.edges[edge0].pointsPos[1];
    int32_t endPos1 = face.edges[edge1].pointsPos[1];
    int32_t endPos2 = face.edges[edge2].pointsPos[1];
    int32_t midPos0 = (startPos0 + endPos0) / 2;
    int32_t midPos1 = (startPos1 + endPos1) / 2;

    auto rootMP0 = insertRootPoint(m_rootEdges[face.edges[edge0].rootEdge], midPos0);
    auto rootMP1 = insertRootPoint(m_rootEdges[face.edges[edge1].rootEdge], midPos1);
    auto rootEP0 = pushRootEdgePair(rootMP0.first, rootMP1.first);
    auto rootEP1 = pushRootEdgePair(rootMP1.first, rootP0);

    cvt_face_t childFaces[3];
    childFaces[0].children.fill(-1);
    childFaces[1].children.fill(-1);
    childFaces[2].children.fill(-1);
    childFaces[0].edges[0] = { face.edges[edge0].rootEdge, startPos0, midPos0 };
    childFaces[0].edges[1] = { rootEP0.first, 0, CvtEdgePosMax };
    childFaces[0].edges[2] = { rootEP1.first, 0, CvtEdgePosMax };
    childFaces[1].edges[0] = { face.edges[edge0].rootEdge, midPos0, endPos0 };
    childFaces[1].edges[1] = { face.edges[edge1].rootEdge, startPos1, midPos1 };
    childFaces[1].edges[2] = { rootEP0.second, 0, CvtEdgePosMax };    
    childFaces[2].edges[0] = { face.edges[edge1].rootEdge, midPos1, endPos1 };
    childFaces[2].edges[1] = { face.edges[edge2].rootEdge, startPos2, endPos2 };
    childFaces[2].edges[2] = { rootEP1.second, 0, CvtEdgePosMax };

    bool needExist0 = subdivideFace(childFaces[0], rootP0, rootMP0.first, rootMP1.first);
    bool needExist1 = subdivideFace(childFaces[1], rootMP0.first, rootP1, rootMP1.first);
    bool needExist2 = subdivideFace(childFaces[2], rootMP1.first, rootP2, rootP0);
    if (!needExist0 && !needExist1 && !needExist2) {
        int insertedPointCnt = (int)rootMP0.second + (int)rootMP1.second;
        m_rootPoints.resize(m_rootPoints.size() - insertedPointCnt);
        m_rootEdges.resize(m_rootEdges.size() - 4);
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        face.children[i] = m_faces.size();
        m_faces.push_back(childFaces[i]);
    }

    setIsolatedData(face.edges[edge0], rootMP0.first, midPos0);
    setIsolatedData(face.edges[edge1], rootMP1.first, midPos1);
    return true;
}

bool MeshSubdivide::subdivideFace1(cvt_face_t &face, int edge0, int edge1, int edge2, int32_t rootP0,
    int32_t rootP1, int32_t rootP2)
{
    int32_t startPos0 = face.edges[edge0].pointsPos[0];
    int32_t startPos1 = face.edges[edge1].pointsPos[0];
    int32_t startPos2 = face.edges[edge2].pointsPos[0];
    int32_t endPos0 = face.edges[edge0].pointsPos[1];
    int32_t endPos1 = face.edges[edge1].pointsPos[1];
    int32_t endPos2 = face.edges[edge2].pointsPos[1];
    int32_t midPos0 = (startPos0 + endPos0) / 2;
    int32_t midPos1 = (startPos1 + endPos1) / 2;

    auto rootMP0 = insertRootPoint(m_rootEdges[face.edges[edge0].rootEdge], midPos0);
    auto rootEP0 = pushRootEdgePair(rootMP0.first, rootP2);

    cvt_face_t childFaces[2];
    childFaces[0].children.fill(-1);
    childFaces[1].children.fill(-1);
    childFaces[0].edges[0] = { face.edges[edge0].rootEdge, startPos0, midPos0 };
    childFaces[0].edges[1] = { rootEP0.first, 0, CvtEdgePosMax };
    childFaces[0].edges[2] = { face.edges[edge2].rootEdge, startPos2, endPos2 };
    childFaces[1].edges[0] = { face.edges[edge0].rootEdge, midPos0, endPos0 };
    childFaces[1].edges[1] = { face.edges[edge1].rootEdge, startPos1, endPos1};
    childFaces[1].edges[2] = { rootEP0.second, 0, CvtEdgePosMax };

    bool needExist0 = subdivideFace(childFaces[0], rootP0, rootMP0.first, rootP2);
    bool needExist1 = subdivideFace(childFaces[1], rootMP0.first, rootP1, rootP2);
    if (!needExist0 && !needExist1) {
        m_rootPoints.resize(m_rootPoints.size() - (int)rootMP0.second);
        m_rootEdges.resize(m_rootEdges.size() - 2);
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        face.children[i] = m_faces.size();
        m_faces.push_back(childFaces[i]);
    }

    setIsolatedData(face.edges[edge0], rootMP0.first, midPos0);
    return true;
}

float MeshSubdivide::getRootPointDist2(int32_t rootP0, int32_t rootP1)
{
    float dx = m_rootPoints[rootP0].coord[0] - m_rootPoints[rootP1].coord[0];
    float dy = m_rootPoints[rootP0].coord[1] - m_rootPoints[rootP1].coord[1];
    float dz = m_rootPoints[rootP0].coord[2] - m_rootPoints[rootP1].coord[2];
    return dx * dx + dy * dy + dz * dz;
}

}} // namespace Slic3r::GUI
