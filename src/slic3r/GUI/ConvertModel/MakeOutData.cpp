#include "MakeOutData.hpp"
#include <cstdlib>
#include <algorithm>

namespace Slic3r { namespace GUI {

MakeOutData::MakeOutData(const cvt_faces_t &faces, const cvt_root_half_edges_t &rootEdges,
    const cvt_root_points_t &rootPoints)
    : m_faces(faces)
    , m_rootEdges(rootEdges)
    , m_rootPoints(rootPoints)
{
}

void MakeOutData::makeData(int srcFaceCnt, out_model_data_t &outData)
{
    m_colorMap.clear();
    m_pointMap.clear();
    m_pointMap.reserve(m_rootPoints.size());
    for (int i = 0; i < srcFaceCnt; ++i) {
        makeFaceData(m_faces[i], outData);
    }
}

void MakeOutData::makeFaceData(const cvt_face_t &face, out_model_data_t &outData)
{
    auto pushEdgePoints = [this](const cvt_half_edge_t &edge, std::vector<int32_t> &points) {
        auto beginIt = ConvertModelUtils::findPoint(m_rootEdges[edge.rootEdge], edge.pointsPos[0]);
        auto endIt = ConvertModelUtils::findPoint(m_rootEdges[edge.rootEdge], edge.pointsPos[1]);
        for (auto it = beginIt; it != endIt; ++it) {
            points.emplace_back(it->rootPoint);
        }
    };
    if (std::all_of(face.children.begin(), face.children.end(), [](int32_t n) { return n == -1; })) {
        std::vector<int32_t> multiPoints[3];
        pushEdgePoints(face.edges[0], multiPoints[0]);
        pushEdgePoints(face.edges[1], multiPoints[1]);
        pushEdgePoints(face.edges[2], multiPoints[2]);
        makeTriangles(multiPoints, outData);
    } else {
        for (auto &child : face.children) {
            if (child != -1) {
                makeFaceData(m_faces[child], outData);
            }
        }
    }
}

void MakeOutData::makeTriangles(std::vector<int32_t> multiPoints[3], out_model_data_t &outData)
{
    int mid = 0;
    while (mid < 3 && multiPoints[mid].size() < 2) {
        mid++;
    }
    // when converting vertex colors to face colors, the face color is determined by the first vertex's color.
    cvt_color_t color = m_rootPoints[multiPoints[0][0]].color;
    if (mid >= 3) {
        makeTriangle(multiPoints[0][0], multiPoints[1][0], multiPoints[2][0], color, outData);
        return;
    }
    int prev = mid == 0 ? 2 : mid - 1;
    int next = mid == 2 ? 0 : mid + 1;
    for (size_t i = 1; i + 1 < multiPoints[mid].size(); ++i) {
        makeTriangle(multiPoints[prev][0], multiPoints[mid][i], multiPoints[mid][i + 1], color, outData);
    }
    for (size_t i = 0; i + 1 < multiPoints[prev].size(); ++i) {
        makeTriangle(multiPoints[prev][i], multiPoints[prev][i + 1], multiPoints[mid][1], color, outData);
    }
    makeTriangle(multiPoints[prev].back(), multiPoints[mid][0], multiPoints[mid][1], color, outData);
    for (size_t i = 0; i + 1 < multiPoints[next].size(); ++i) {
        makeTriangle(multiPoints[next][i], multiPoints[next][i + 1], multiPoints[mid].back(), color, outData);
    }
    makeTriangle(multiPoints[next].back(), multiPoints[prev][0], multiPoints[mid].back(), color, outData);
}

void MakeOutData::makeTriangle(int32_t rootP0, int32_t rootP1, int32_t rootP2, cvt_color_t color,
    out_model_data_t &outData)
{
    out_triangle_data_t triangle;
    triangle.vertexIndices[0] = pushOutPoint(rootP0, outData);
    triangle.vertexIndices[1] = pushOutPoint(rootP1, outData);
    triangle.vertexIndices[2] = pushOutPoint(rootP2, outData);
    auto it = m_colorMap.find(color);
    if (it != m_colorMap.end()) {
        triangle.colorIndex = it->second;
    } else {
        triangle.colorIndex = outData.colors.size();
        m_colorMap.emplace(color, triangle.colorIndex);
        outData.colors.push_back({ color[0], color[1], color[2] });
    }
    outData.triangles.push_back(triangle);
}

int32_t MakeOutData::pushOutPoint(int32_t rootP, out_model_data_t &outData)
{
    const float *coord = m_rootPoints[rootP].coord;
    std::array<float, 3> point = { coord[0], coord[1], coord[2] };
    auto pair = m_pointMap.emplace(point, outData.vertices.size());
    if (pair.second) {
        outData.vertices.push_back(point);
    }
    return pair.first->second;
}

}} // namespace Slic3r::GUI
