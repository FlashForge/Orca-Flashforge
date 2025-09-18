#include "MeshSubdivideBase.hpp"
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

MeshSubdivideBase::MeshSubdivideBase(cvt_faces_t &faces, cvt_root_half_edges_t &rootEdges,
    cvt_root_points_t &rootPoints, const cvt_colors_t &dstColors,
    const texture_sample_ptrs_t &textureSamples)
    : m_faces(faces)
    , m_rootEdges(rootEdges)
    , m_rootPoints(rootPoints)
    , m_dstColors(dstColors)
    , m_textureSamples(textureSamples)
{
}

std::pair<int32_t, bool> MeshSubdivideBase::insertRootPoint(cvt_root_half_edge_t &rootEdge, int32_t pos)
{
    auto it = ConvertModelUtils::findPoint(rootEdge, pos);
    if (it != rootEdge.points.end()) {
        return std::make_pair(it->rootPoint, false);
    }
    const cvt_root_point_t &p0 = m_rootPoints[rootEdge.points.front().rootPoint];
    const cvt_root_point_t &p1 = m_rootPoints[rootEdge.points.back().rootPoint];
    double posRatio = (double)pos / CvtEdgePosMax;

    cvt_root_point_t rootPoint;
    rootPoint.coord[0] = p0.coord[0] * (1.0 - posRatio) + p1.coord[0] * posRatio;
    rootPoint.coord[1] = p0.coord[1] * (1.0 - posRatio) + p1.coord[1] * posRatio;
    rootPoint.coord[2] = p0.coord[2] * (1.0 - posRatio) + p1.coord[2] * posRatio;
    rootPoint.texCoord[0] = p0.texCoord[0] * (1.0 - posRatio) + p1.texCoord[0] * posRatio;
    rootPoint.texCoord[1] = p0.texCoord[1] * (1.0 - posRatio) + p1.texCoord[1] * posRatio;
    rootPoint.texIndex = p0.texIndex;
    rootPoint.color = getTextureColor(rootPoint.texIndex, rootPoint.texCoord[0], rootPoint.texCoord[1]);

    m_rootPoints.push_back(rootPoint);
    return std::make_pair(m_rootPoints.size() - 1, true);
}

std::pair<int32_t, int32_t> MeshSubdivideBase::pushRootEdgePair(int32_t rootP0, int32_t rootP1)
{
    int32_t prevSize = m_rootEdges.size();
    cvt_root_half_edge_t rootEdge;
    rootEdge.points.push_back({ rootP0, 0, false });
    rootEdge.points.push_back({ rootP1, CvtEdgePosMax, false });
    rootEdge.twin = prevSize + 1;

    cvt_root_half_edge_t twinRootEdge;
    twinRootEdge.points.push_back({ rootP1, 0, false });
    twinRootEdge.points.push_back({ rootP0, CvtEdgePosMax, false });
    twinRootEdge.twin = prevSize;

    m_rootEdges.push_back(rootEdge);
    m_rootEdges.push_back(twinRootEdge);
    return std::make_pair(prevSize, prevSize + 1);
}

void MeshSubdivideBase::setIsolatedData(const cvt_half_edge_t &edge, int32_t rootPoint, int32_t pos)
{
    auto it = ConvertModelUtils::findPoint(m_rootEdges[edge.rootEdge], pos);
    if (it != m_rootEdges[edge.rootEdge].points.end() && it->isolated) {
        it->isolated = false;
        return;
    }
    cvt_point_t point;
    point.rootPoint = rootPoint;
    point.pos = pos;
    point.isolated = false;
    ConvertModelUtils::insertPoint(m_rootEdges[edge.rootEdge], point);

    if (m_rootEdges[edge.rootEdge].twin != -1) {
        cvt_point_t twinPoint;
        twinPoint.rootPoint = insertTwinRootPoint(edge, rootPoint, pos);
        twinPoint.pos = CvtEdgePosMax - pos;
        twinPoint.isolated = true;
        ConvertModelUtils::insertPoint(m_rootEdges[m_rootEdges[edge.rootEdge].twin], twinPoint);
    }
}

int32_t MeshSubdivideBase::insertTwinRootPoint(const cvt_half_edge_t &edge, int32_t rootPointIdx, int32_t pos)
{
    const cvt_root_half_edge_t &rootEdge = m_rootEdges[edge.rootEdge];
    const cvt_root_half_edge_t &twinRootEdge = m_rootEdges[rootEdge.twin];
    const cvt_point_t &p0 = rootEdge.points.front();
    const cvt_point_t &p1 = rootEdge.points.back();
    const cvt_point_t &twinP0 = twinRootEdge.points.front();
    const cvt_point_t &twinP1 = twinRootEdge.points.back();
    if (p0.rootPoint == twinP1.rootPoint && p1.rootPoint == twinP0.rootPoint) {
        return rootPointIdx;
    }
    const cvt_root_point_t &twinRootP0 = m_rootPoints[twinRootEdge.points.front().rootPoint];
    const cvt_root_point_t &twinRootP1 = m_rootPoints[twinRootEdge.points.back().rootPoint];
    double posRatio = (double)(CvtEdgePosMax - pos) / CvtEdgePosMax;

    cvt_root_point_t twinRootPoint;
    twinRootPoint.coord[0] = m_rootPoints[rootPointIdx].coord[0];
    twinRootPoint.coord[1] = m_rootPoints[rootPointIdx].coord[1];
    twinRootPoint.coord[2] = m_rootPoints[rootPointIdx].coord[2];
    twinRootPoint.texCoord[0] = twinRootP0.texCoord[0] * (1.0 - posRatio) + twinRootP1.texCoord[0] * posRatio;
    twinRootPoint.texCoord[1] = twinRootP0.texCoord[1] * (1.0 - posRatio) + twinRootP1.texCoord[1] * posRatio;
    twinRootPoint.texIndex = twinRootP0.texIndex;
    twinRootPoint.color = getTextureColor(twinRootPoint.texIndex, twinRootPoint.texCoord[0], twinRootPoint.texCoord[1]);
    m_rootPoints.push_back(twinRootPoint);
    return m_rootPoints.size() - 1;
}

cvt_color_t MeshSubdivideBase::getTextureColor(int32_t texIndex, float x, float y)
{
    if (m_textureSamples.empty()) {
        return { 0, 0, 0 };
    }
    int minIdx = 0;
    cvt_color_t color = m_textureSamples[texIndex]->getTextureColor(x, y);
    ConvertModelUtils::getColorMinDist2(m_dstColors, color, minIdx);
    return m_dstColors[minIdx];
}

}} // namespace Slic3r::GUI
