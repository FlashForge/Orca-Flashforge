#include "ConvertModelProc.hpp"
#include <algorithm>
#include <array>
#include <boost/functional/hash.hpp>
#include "ConvertModelUtils.hpp"
#include "KMeansCluster.hpp"
#include "MakeOutData.hpp"
#include "MeshSubdivide.hpp"
#include "RemoveSmallPatches.hpp"
#include "SampleColorData.hpp"
#include "Vertex2FaceColor.hpp"

namespace Slic3r { namespace GUI {

size_t ConvertModelProc::RootPointHash::operator()(const cvt_root_point_t &p) const
{
    size_t seed = p.texIndex;
    if (p.coord[0] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p.coord[0]);
    }
    if (p.coord[1] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p.coord[1]);
    }
    if (p.coord[2] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p.coord[2]);
    }
    if (p.texCoord[0] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p.texCoord[0]);
    }
    if (p.texCoord[1] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p.texCoord[1]);
    }
    return seed;
}

bool ConvertModelProc::RootPointEqual::operator()(const cvt_root_point_t &p0, const cvt_root_point_t &p1) const
{
    return p0.coord[0] == p1.coord[0]
        && p0.coord[1] == p1.coord[1]
        && p0.coord[2] == p1.coord[2]
        && p0.texCoord[0] == p1.texCoord[0]
        && p0.texCoord[1] == p1.texCoord[1]
        && p0.texIndex == p1.texIndex;
}

ConvertModelProc::ConvertModelProc(const in_model_data_t &inData, const in_cvt_params_t &params)
    : m_srcFaceCnt(inData.triangleCnt)
    , m_params(params)
{
    for (int i = 0; i < inData.materialCnt; ++i) {
        m_textureSamples.emplace_back(new TextureSample(*inData.materialDatas[i].textureData));
    }
    initRootFaces(inData);
    getModelSize(inData, m_modelSize);

    float modelExtent = std::max({ m_modelSize[0], m_modelSize[1], m_modelSize[2] });
    float dstExtent = std::max(params.minModelExtent, 150.0f);
    m_subdivideMaxEdgeLen = params.subdivideMaxEdgeLenRatio * modelExtent / dstExtent;

    SampleColorData sampleColorData(m_faces, m_rootEdges, m_rootPoints, m_textureSamples);
    m_colorDatas = sampleColorData.getColorDatas(m_srcFaceCnt, m_subdivideMaxEdgeLen);
}

cvt_colors_t ConvertModelProc::clusterColors(int colorNum)
{
    KMeansCluster kmeansCluster(m_colorDatas);
    return kmeansCluster.clusterColors(colorNum);
}

void ConvertModelProc::doConvert(const cvt_colors_t &dstColors, out_model_data_t &outData)
{
    if (m_faces.empty()) {
        return;
    }
    cvt_color_datas_t().swap(m_colorDatas);
    updateRootPointColors(dstColors);

    MeshSubdivide meshSubdivide(m_faces, m_rootEdges, m_rootPoints, dstColors, m_textureSamples);
    meshSubdivide.subdivideRootFaces(m_srcFaceCnt, m_subdivideMaxEdgeLen);

    Vertex2FaceColor vertex2FaceColor(m_faces, m_rootEdges, m_rootPoints);
    vertex2FaceColor.subdivideMixedColorFaces(m_srcFaceCnt);

    MakeOutData makeOutData(m_faces, m_rootEdges, m_rootPoints);
    makeOutData.makeData(m_srcFaceCnt, outData);
    cvt_faces_t().swap(m_faces);
    cvt_root_half_edges_t().swap(m_rootEdges);
    cvt_root_points_t().swap(m_rootPoints);

    RemoveSmallPatches removeSmallPatches;
    removeSmallPatches.mergeSmallPatchesColor(outData, m_modelSize, m_params.colorMinAreaRatio);
    transformModel(outData);
}

void ConvertModelProc::initRootFaces(const in_model_data_t &inData)
{
    point_map_t pointMap;
    pointMap.reserve(inData.vertexCnt);
    m_faces.resize(inData.triangleCnt);
    m_rootEdges.resize(inData.triangleCnt * 3);
    for (int i = 0; i < inData.triangleCnt; ++i) {
        int texIndex = getTexIndex(inData, i);
        int faceIdxOfs = inData.faceIndexStride * i;
        cvt_point_t points[3];
        for (int j = 0; j < 3; ++j) {
            int vertexIdxOfs = faceIdxOfs + inData.vertexIndexStride * j;
            int texCoordIdxOfs = faceIdxOfs + inData.texCoordIndexStride * j;
            int vertexIdx = *(const int32_t *)((const uint8_t *)inData.vertexIndecis + vertexIdxOfs);
            int texCoordIdx = *(const int32_t *)((const uint8_t *)inData.texCoordIndices + texCoordIdxOfs);
            makePointFromSrcVertex(inData, vertexIdx, texCoordIdx, texIndex, points[j], pointMap);
        }
        for (int j = 0; j < 3; ++j) {
            int edgeIdx = i * 3 + j;
            m_faces[i].edges[j].rootEdge = edgeIdx;
            m_faces[i].edges[j].pointsPos[0] = 0;
            m_faces[i].edges[j].pointsPos[1] = CvtEdgePosMax;
            m_rootEdges[edgeIdx].points.reserve(2);
            m_rootEdges[edgeIdx].points.push_back(points[j]);
            m_rootEdges[edgeIdx].points.push_back(points[(j + 1) % 3]);
            m_rootEdges[edgeIdx].points[0].pos = 0;
            m_rootEdges[edgeIdx].points[1].pos = CvtEdgePosMax;
            m_rootEdges[edgeIdx].twin = -1;
        }
        m_faces[i].children.fill(-1);
    }
    m_rootPoints.resize(pointMap.size());
    for (auto &item : pointMap) {
        m_rootPoints[item.second] = item.first;
    }
    initPairEdge(inData);
}

void ConvertModelProc::initPairEdge(const in_model_data_t &inData)
{
    ConvertModelUtils::edge_multi_map_t map;
    for (int i = 0; i < inData.triangleCnt; ++i) {
        int faceIdxOfs = inData.faceIndexStride * i;
        int32_t vertexIndices[3];
        for (int j = 0; j < 3; ++j) {
            int vertexIdxOfs = faceIdxOfs + inData.vertexIndexStride * j;
            vertexIndices[j] = *(const int32_t *)((const uint8_t *)inData.vertexIndecis + vertexIdxOfs);
        }
        for (int j = 0; j < 3; ++j) {
            int edgeIdx = i * 3 + j;
            std::array<int32_t, 2> edgeRev = { vertexIndices[(j + 1) % 3], vertexIndices[j]};
            auto itRev = map.find(edgeRev);
            if (itRev != map.end()) {
                m_rootEdges[edgeIdx].twin = itRev->second;
                m_rootEdges[itRev->second].twin = edgeIdx;
                map.erase(itRev);
                continue;
            }
            std::array<int32_t, 2> edge = { edgeRev[1], edgeRev[0] };
            map.emplace(edge, edgeIdx);
        }
    }
}

void ConvertModelProc::updateRootPointColors(const cvt_colors_t &dstColors)
{
    for (auto &rootPoint : m_rootPoints) {
        int minIdx = 0;
        ConvertModelUtils::getColorMinDist2(dstColors, rootPoint.color, minIdx);
        rootPoint.color = dstColors[minIdx];
    }
}

void ConvertModelProc::transformModel(out_model_data_t &outData)
{
    if (m_params.transCoordSys) {
        for (auto &vertex : outData.vertices) {
            float y = vertex[1];
            vertex[1] = -vertex[2];
            vertex[2] = y;
        }
    }
    auto scaleModel = [&](float scale) {
        for (auto &vertex : outData.vertices) {
            vertex[0] *= scale;
            vertex[1] *= scale;
            vertex[2] *= scale;
        }
    };
    float scaleX = m_params.maxPrintSize[0] / m_modelSize[0];
    float scaleY = m_params.maxPrintSize[1] / m_modelSize[1];
    float scaleZ = m_params.maxPrintSize[2] / m_modelSize[2];
    float scale = std::min({ scaleX, scaleY, scaleZ });
    if (scale < 1.0f) {
        scaleModel(scale);
    } else if (!isinf(scale)) {
        float extent = std::min({
            m_params.minModelExtent, m_params.maxPrintSize[0],
            m_params.maxPrintSize[1], m_params.maxPrintSize[2]
        });
        scaleX = extent / m_modelSize[0];
        scaleY = extent / m_modelSize[1];
        scaleZ = extent / m_modelSize[2];
        scale = std::min({ scaleX, scaleY, scaleZ });
        if (scale > 1.0f) {
            scaleModel(scale);
        }
    }
}

int ConvertModelProc::getTexIndex(const in_model_data_t &inData, int triangleIdx)
{
    auto pred = [](int idx, const in_material_data_t &materialData) {
        return idx < materialData.endTriangleIndex;
    };
    const in_material_data_t *endMaterialData = inData.materialDatas + inData.materialCnt;
    auto it = std::upper_bound(inData.materialDatas, endMaterialData, triangleIdx, pred);
    if (it == endMaterialData) {
        return inData.materialCnt - 1;
    }
    return it - inData.materialDatas;
}

void ConvertModelProc::makePointFromSrcVertex(const in_model_data_t &inData, int vertexIdx, int texCoordIdx,
    int texIndex, cvt_point_t &point, point_map_t &pointMap)
{
    int vertexOfs = inData.vertexStride * vertexIdx;
    int texCoordOfs = inData.texCoordStride * texCoordIdx;
    const float *vertex = (const float *)((const uint8_t *)inData.vertices + vertexOfs);
    const float *texCoord = (const float *)((const uint8_t *)inData.texCoords + texCoordOfs); 

    cvt_root_point_t rootPoint;
    rootPoint.coord[0] = vertex[0];
    rootPoint.coord[1] = vertex[1];
    rootPoint.coord[2] = vertex[2];
    rootPoint.texCoord[0] = texCoord[0];
    rootPoint.texCoord[1] = texCoord[1];
    rootPoint.texIndex = texIndex;
    auto it = pointMap.find(rootPoint);
    if (it == pointMap.end()) {
        rootPoint.color = m_textureSamples[texIndex]->getTextureColor(texCoord[0], texCoord[1]);
        it = pointMap.emplace(rootPoint, pointMap.size()).first;
    }
    point.rootPoint = it->second;
    point.pos = 0;
    point.isolated = false;
}

void ConvertModelProc::getModelSize(const in_model_data_t &inData, float modelSize[3])
{
    float min[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float max[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (int i = 0; i < inData.vertexCnt; ++i) {
        const float *vertex = (const float *)((const uint8_t *)inData.vertices + inData.vertexStride * i);
        for (int j = 0; j < 3; ++j) {
            if (vertex[j] < min[j]) {
                min[j] = vertex[j];
            }
            if (vertex[j] > max[j]) {
                max[j] = vertex[j];
            }
        }
    }
    modelSize[0] = max[0] - min[0];
    modelSize[1] = max[1] - min[1];
    modelSize[2] = max[2] - min[2];
}

}} // namespace Slic3r::GUI
