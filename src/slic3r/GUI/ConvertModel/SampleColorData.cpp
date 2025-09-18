#include "SampleColorData.hpp"
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

SampleColorData::SampleColorData(const cvt_faces_t &faces, const cvt_root_half_edges_t &rootEdges,
    const cvt_root_points_t &rootPoints, const texture_sample_ptrs_t &textureSamples)
    : m_faces(faces)
    , m_rootEdges(rootEdges)
    , m_rootPoints(rootPoints)
    , m_textureSamples(textureSamples)
    , m_maxEdgeLenSqr(1.0)
{
}

cvt_color_datas_t SampleColorData::getColorDatas(int srcFaceCnt, float subdivideMaxEdgeLen)
{
    m_maxEdgeLenSqr = subdivideMaxEdgeLen * subdivideMaxEdgeLen;
    for (int i = 0; i < srcFaceCnt; ++i) {
        const cvt_root_half_edge_t &rootEdge0 = m_rootEdges[m_faces[i].edges[0].rootEdge];
        const cvt_root_half_edge_t &rootEdge1 = m_rootEdges[m_faces[i].edges[1].rootEdge];
        const cvt_root_half_edge_t &rootEdge2 = m_rootEdges[m_faces[i].edges[2].rootEdge];
        const cvt_root_point_t &p0 = m_rootPoints[rootEdge0.points[0].rootPoint];
        const cvt_root_point_t &p1 = m_rootPoints[rootEdge1.points[0].rootPoint];
        const cvt_root_point_t &p2 = m_rootPoints[rootEdge2.points[0].rootPoint];
        face_data_t faceData;
        faceData.coord[0] = p0.coord;
        faceData.coord[1] = p1.coord;
        faceData.coord[2] = p2.coord;
        faceData.texCoord[0] = p0.texCoord;
        faceData.texCoord[1] = p1.texCoord;
        faceData.texCoord[2] = p2.texCoord;
        faceData.color[0] = &p0.color;
        faceData.color[1] = &p1.color;
        faceData.color[2] = &p2.color;
        faceData.texIndex = p0.texIndex;
        subdivideFace(faceData, true);
    }
    cvt_color_datas_t colorDatas;
    colorDatas.reserve(m_colorMap.size());
    for (auto &item : m_colorMap) {
        cvt_color_data_t data = { item.first, item.second };
        colorDatas.push_back(data);
    }
    return std::move(colorDatas);
}

bool SampleColorData::subdivideFace(const face_data_t &faceData, bool isSrcFace)
{
    float distSqr01 = getPointDist2(faceData.coord[0], faceData.coord[1]);
    float distSqr12 = getPointDist2(faceData.coord[1], faceData.coord[2]);
    float distSqr20 = getPointDist2(faceData.coord[2], faceData.coord[0]);
    bool subdiv01 = distSqr01 > m_maxEdgeLenSqr;
    bool subdiv12 = distSqr12 > m_maxEdgeLenSqr;
    bool subdiv20 = distSqr20 > m_maxEdgeLenSqr;

    face_data_t tmpFaceData;
    bool hasSubdivice = false;
    if (subdiv01 && subdiv12 && subdiv20) {
        hasSubdivice = subdivideFace3(faceData);
    } else if (subdiv01 && subdiv12) {
        if (distSqr01 > distSqr12) {
            hasSubdivice = subdivideFace2(faceData);
        } else {
            getNewFaceData(faceData, tmpFaceData, 2, 1, 0);
            hasSubdivice = subdivideFace2(tmpFaceData);
        }
    } else if (subdiv12 && subdiv20) {
        if (distSqr12 > distSqr20) {
            getNewFaceData(faceData, tmpFaceData, 1, 2, 0);
        } else {
            getNewFaceData(faceData, tmpFaceData, 0, 2, 1);
        }
        hasSubdivice = subdivideFace2(tmpFaceData);
    } else if (subdiv20 && subdiv01) {
        if (distSqr20 > distSqr01) {
            getNewFaceData(faceData, tmpFaceData, 2, 0, 1);
        } else {
            getNewFaceData(faceData, tmpFaceData, 1, 0, 2);
        }
        hasSubdivice = subdivideFace2(tmpFaceData);
    } else if (subdiv01) {
        hasSubdivice = subdivideFace1(faceData);
    } else if (subdiv12) {
        getNewFaceData(faceData, tmpFaceData, 1, 2, 0);
        hasSubdivice = subdivideFace1(tmpFaceData);
    } else if (subdiv20) {
        getNewFaceData(faceData, tmpFaceData, 2, 0, 1);
        hasSubdivice = subdivideFace1(tmpFaceData);
    }
    bool isSameColor01 = *faceData.color[0] == *faceData.color[1];
    bool isSameColor12 = *faceData.color[1] == *faceData.color[2];
    bool isSameColor = isSameColor01 && isSameColor12;
    if (!hasSubdivice && (!isSameColor || isSrcFace)) {
        getFaceColorData(faceData);
    }
    return hasSubdivice || !isSameColor;
}

bool SampleColorData::subdivideFace3(const face_data_t &faceData)
{
    point_data_t points[3];
    getMidPointData(faceData, 0, 1, points[0]);
    getMidPointData(faceData, 1, 2, points[1]);
    getMidPointData(faceData, 2, 0, points[2]);

    face_data_t childFaces[4];
    childFaces[0].coord[0] = faceData.coord[0];
    childFaces[0].coord[1] = points[0].coord;
    childFaces[0].coord[2] = points[2].coord;
    childFaces[0].texCoord[0] = faceData.texCoord[0];
    childFaces[0].texCoord[1] = points[0].texCoord;
    childFaces[0].texCoord[2] = points[2].texCoord;
    childFaces[0].color[0] = faceData.color[0];
    childFaces[0].color[1] = &points[0].color;
    childFaces[0].color[2] = &points[2].color;
    childFaces[0].texIndex = faceData.texIndex;

    childFaces[1].coord[0] = points[0].coord;
    childFaces[1].coord[1] = faceData.coord[1];
    childFaces[1].coord[2] = points[1].coord;
    childFaces[1].texCoord[0] = points[0].texCoord;
    childFaces[1].texCoord[1] = faceData.texCoord[1];
    childFaces[1].texCoord[2] = points[1].texCoord;
    childFaces[1].color[0] = &points[0].color;
    childFaces[1].color[1] = faceData.color[1];
    childFaces[1].color[2] = &points[1].color;
    childFaces[1].texIndex = faceData.texIndex;

    childFaces[2].coord[0] = points[1].coord;
    childFaces[2].coord[1] = faceData.coord[2];
    childFaces[2].coord[2] = points[2].coord;
    childFaces[2].texCoord[0] = points[1].texCoord;
    childFaces[2].texCoord[1] = faceData.texCoord[2];
    childFaces[2].texCoord[2] = points[2].texCoord;
    childFaces[2].color[0] = &points[1].color;
    childFaces[2].color[1] = faceData.color[2];
    childFaces[2].color[2] = &points[2].color;
    childFaces[2].texIndex = faceData.texIndex;

    childFaces[3].coord[0] = points[0].coord;
    childFaces[3].coord[1] = points[1].coord;
    childFaces[3].coord[2] = points[2].coord;
    childFaces[3].texCoord[0] = points[0].texCoord;
    childFaces[3].texCoord[1] = points[1].texCoord;
    childFaces[3].texCoord[2] = points[2].texCoord;
    childFaces[3].color[0] = &points[0].color;
    childFaces[3].color[1] = &points[1].color;
    childFaces[3].color[2] = &points[2].color;
    childFaces[3].texIndex = faceData.texIndex;

    bool needExist[4];
    needExist[0] = subdivideFace(childFaces[0], false);
    needExist[1] = subdivideFace(childFaces[1], false);
    needExist[2] = subdivideFace(childFaces[2], false);
    needExist[3] = subdivideFace(childFaces[3], false);
    if (!needExist[0] && !needExist[1] && !needExist[2] && !needExist[3]) {
        return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (!needExist[i]) {
            getFaceColorData(childFaces[i]);
        }
    }
    return true;
}

bool SampleColorData::subdivideFace2(const face_data_t &faceData)
{
    point_data_t points[2];
    getMidPointData(faceData, 0, 1, points[0]);
    getMidPointData(faceData, 1, 2, points[1]);

    face_data_t childFaces[3];
    childFaces[0].coord[0] = faceData.coord[0];
    childFaces[0].coord[1] = points[0].coord;
    childFaces[0].coord[2] = faceData.coord[2];
    childFaces[0].texCoord[0] = faceData.texCoord[0];
    childFaces[0].texCoord[1] = points[0].texCoord;
    childFaces[0].texCoord[2] = faceData.texCoord[2];
    childFaces[0].color[0] = faceData.color[0];
    childFaces[0].color[1] = &points[0].color;
    childFaces[0].color[2] = faceData.color[2];
    childFaces[0].texIndex = faceData.texIndex;

    childFaces[1].coord[0] = points[0].coord;
    childFaces[1].coord[1] = faceData.coord[1];
    childFaces[1].coord[2] = points[1].coord;
    childFaces[1].texCoord[0] = points[0].texCoord;
    childFaces[1].texCoord[1] = faceData.texCoord[1];
    childFaces[1].texCoord[2] = points[1].texCoord;
    childFaces[1].color[0] = &points[0].color;
    childFaces[1].color[1] = faceData.color[1];
    childFaces[1].color[2] = &points[1].color;
    childFaces[1].texIndex = faceData.texIndex;

    childFaces[2].coord[0] = points[1].coord;
    childFaces[2].coord[1] = faceData.coord[2];
    childFaces[2].coord[2] = points[0].coord;
    childFaces[2].texCoord[0] = points[1].texCoord;
    childFaces[2].texCoord[1] = faceData.texCoord[2];
    childFaces[2].texCoord[2] = points[0].texCoord;
    childFaces[2].color[0] = &points[1].color;
    childFaces[2].color[1] = faceData.color[2];
    childFaces[2].color[2] = &points[0].color;
    childFaces[2].texIndex = faceData.texIndex;

    bool needExist[3];
    needExist[0] = subdivideFace(childFaces[0], false);
    needExist[1] = subdivideFace(childFaces[1], false);
    needExist[2] = subdivideFace(childFaces[2], false);
    if (!needExist[0] && !needExist[1] && !needExist[2]) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        if (!needExist[i]) {
            getFaceColorData(childFaces[i]);
        }
    }
    return true;
}

bool SampleColorData::subdivideFace1(const face_data_t &faceData)
{
    point_data_t points[1];
    getMidPointData(faceData, 0, 1, points[0]);

    face_data_t childFaces[2];
    childFaces[0].coord[0] = faceData.coord[0];
    childFaces[0].coord[1] = points[0].coord;
    childFaces[0].coord[2] = faceData.coord[2];
    childFaces[0].texCoord[0] = faceData.texCoord[0];
    childFaces[0].texCoord[1] = points[0].texCoord;
    childFaces[0].texCoord[2] = faceData.texCoord[2];
    childFaces[0].color[0] = faceData.color[0];
    childFaces[0].color[1] = &points[0].color;
    childFaces[0].color[2] = faceData.color[2];
    childFaces[0].texIndex = faceData.texIndex;

    childFaces[1].coord[0] = points[0].coord;
    childFaces[1].coord[1] = faceData.coord[1];
    childFaces[1].coord[2] = faceData.coord[2];
    childFaces[1].texCoord[0] = points[0].texCoord;
    childFaces[1].texCoord[1] = faceData.texCoord[1];
    childFaces[1].texCoord[2] = faceData.texCoord[2];
    childFaces[1].color[0] = &points[0].color;
    childFaces[1].color[1] = faceData.color[1];
    childFaces[1].color[2] = faceData.color[2];
    childFaces[1].texIndex = faceData.texIndex;

    bool needExist[2];
    needExist[0] = subdivideFace(childFaces[0], false);
    needExist[1] = subdivideFace(childFaces[1], false);
    if (!needExist[0] && !needExist[1]) {
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        if (!needExist[i]) {
            getFaceColorData(childFaces[i]);
        }
    }
    return true;
}

void SampleColorData::getFaceColorData(const face_data_t &faceData)
{
    double area = ConvertModelUtils::getTriangleArea(faceData.coord[0], faceData.coord[1], faceData.coord[2]);
    for (int i = 0; i < 3; ++i) {
        auto colorIt = m_colorMap.find(*faceData.color[i]);
        if (colorIt != m_colorMap.end()) {
            colorIt->second += area;
        } else {
            m_colorMap.emplace(*faceData.color[i], area);
        }
    }
}

void SampleColorData::getNewFaceData(const face_data_t &src, face_data_t &dst, int p0, int p1, int p2)
{
    dst.coord[0] = src.coord[p0];
    dst.coord[1] = src.coord[p1];
    dst.coord[2] = src.coord[p2];

    dst.texCoord[0] = src.texCoord[p0];
    dst.texCoord[1] = src.texCoord[p1];
    dst.texCoord[2] = src.texCoord[p2];

    dst.color[0] = src.color[p0];
    dst.color[1] = src.color[p1];
    dst.color[2] = src.color[p2];

    dst.texIndex = src.texIndex;
}

void SampleColorData::getMidPointData(const face_data_t &faceData, int p0, int p1, point_data_t &pointData)
{
    pointData.coord[0] = (faceData.coord[p0][0] + faceData.coord[p1][0]) * 0.5;
    pointData.coord[1] = (faceData.coord[p0][1] + faceData.coord[p1][1]) * 0.5;
    pointData.coord[2] = (faceData.coord[p0][2] + faceData.coord[p1][2]) * 0.5;

    pointData.texCoord[0] = (faceData.texCoord[p0][0] + faceData.texCoord[p1][0]) * 0.5;
    pointData.texCoord[1] = (faceData.texCoord[p0][1] + faceData.texCoord[p1][1]) * 0.5;
    pointData.texIndex = faceData.texIndex;

    pointData.color = m_textureSamples[faceData.texIndex]->getTextureColor(
        pointData.texCoord[0], pointData.texCoord[1]);
}

float SampleColorData::getPointDist2(const float *coord0, const float *coord1)
{
    float dx = coord0[0] - coord1[0];
    float dy = coord0[1] - coord1[1];
    float dz = coord0[2] - coord1[2];
    return dx * dx + dy * dy + dz * dz;
}

}} // namespace Slic3r::GUI
