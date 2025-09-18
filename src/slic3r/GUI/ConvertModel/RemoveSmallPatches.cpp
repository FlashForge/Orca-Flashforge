#include "RemoveSmallPatches.hpp"
#include <algorithm>
#include <queue>
#include <set>
#include <utility>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

void RemoveSmallPatches::mergeSmallPatchesColor(out_model_data_t &modelData, const float modelSize[3],
    float colorMinAreaRatio)
{
    if (colorMinAreaRatio <= 0) {
        return;
    }
    findFaceNeighbors(modelData);
    splitRegions(modelData);
    groupRegions(modelData, modelSize, colorMinAreaRatio);
    mergeSmallRegion(modelData);
}

void RemoveSmallPatches::findFaceNeighbors(const out_model_data_t &modelData)
{
    auto getEdgeLen = [&](const std::array<int32_t, 2> &edge) {
        const float *coord0 = modelData.vertices[edge[0]].data();
        const float *coord1 = modelData.vertices[edge[1]].data();
        float dx = coord0[0] - coord1[0];
        float dy = coord0[1] - coord1[1];
        float dz = coord0[2] - coord1[2];
        return sqrt(dx * dx + dy * dy + dz * dz);
    };
    neighbor_info_t neightborInfo = { -1, 0 };
    m_faceNeighborInfos.resize(modelData.triangles.size(), { neightborInfo, neightborInfo, neightborInfo });
    ConvertModelUtils::edge_multi_map_t map;
    for (size_t i = 0; i < modelData.triangles.size(); ++i) {
        const int32_t *vertexIndices = modelData.triangles[i].vertexIndices;
        for (int j = 0; j < 3; ++j) {
            std::array<int32_t, 2> edgeRev = { vertexIndices[(j + 1) % 3], vertexIndices[j] };
            auto itRev = map.find(edgeRev);
            if (itRev != map.end()) {
                addFaceNeighbor(i, itRev->second, getEdgeLen(edgeRev));
                map.erase(itRev);
                continue;
            }
            std::array<int32_t, 2> edge = { edgeRev[1], edgeRev[0] };
            map.emplace(edge, i);
        }
    }
}

void RemoveSmallPatches::addFaceNeighbor(int32_t face0, int32_t face1, float boundaryLen)
{
    if (m_faceNeighborInfos[face0][0].index == -1) {
        m_faceNeighborInfos[face0][0].index = face1;
        m_faceNeighborInfos[face0][0].boundaryLen = boundaryLen;
    } else if (m_faceNeighborInfos[face0][1].index == -1) {
        m_faceNeighborInfos[face0][1].index = face1;
        m_faceNeighborInfos[face0][1].boundaryLen = boundaryLen;
    } else {
        m_faceNeighborInfos[face0][2].index = face1;
        m_faceNeighborInfos[face0][2].boundaryLen = boundaryLen;
    }
    if (m_faceNeighborInfos[face1][0].index == -1) {
        m_faceNeighborInfos[face1][0].index = face0;
        m_faceNeighborInfos[face1][0].boundaryLen = boundaryLen;
    } else if (m_faceNeighborInfos[face1][1].index == -1) {
        m_faceNeighborInfos[face1][1].index = face0;
        m_faceNeighborInfos[face1][1].boundaryLen = boundaryLen;
    } else {
        m_faceNeighborInfos[face1][2].index = face0;
        m_faceNeighborInfos[face1][2].boundaryLen = boundaryLen;
    }
}

void RemoveSmallPatches::splitRegions(const out_model_data_t &modelData)
{
    m_faceRegions.resize(modelData.triangles.size(), -1);
    std::queue<int32_t> que;
    for (size_t i = 0; i < m_faceRegions.size(); ++i) {
        if (m_faceRegions[i] == -1) {
            m_regions.emplace_back();
            region_t &region = m_regions.back();
            region.colorIndex = modelData.triangles[i].colorIndex;
            region.area = 0;
            que.push(i);
            m_faceRegions[i] = m_regions.size() - 1;
            while (!que.empty()) {
                int32_t face = que.front();
                que.pop();
                region.faces.push_back(face);
                region.area += getFaceArea(modelData, face);
                for (int j = 0; j < 3; ++j) {
                    int neighborFace = m_faceNeighborInfos[face][j].index;
                    if (neighborFace != -1) {
                        if (m_faceRegions[neighborFace] == -1) {
                            if (modelData.triangles[neighborFace].colorIndex == region.colorIndex) {
                                que.push(neighborFace);
                                m_faceRegions[neighborFace] = m_faceRegions[i];
                            }
                        } else if (m_faceRegions[neighborFace] != m_faceRegions[i]) {
                            int32_t region0 = m_faceRegions[i];
                            int32_t region1 = m_faceRegions[neighborFace];
                            auto it0 = m_regions[region0].neighborInfoMap.emplace(region1, 0.0f).first;
                            auto it1 = m_regions[region1].neighborInfoMap.emplace(region0, 0.0f).first;
                            it0->second += m_faceNeighborInfos[face][j].boundaryLen;
                            it1->second += m_faceNeighborInfos[face][j].boundaryLen;
                        }
                    }
                }
            }
        }
    }
}

double RemoveSmallPatches::getFaceArea(const out_model_data_t &modelData, int32_t faceIndex)
{
    const int32_t *vertexIndices = modelData.triangles[faceIndex].vertexIndices;
    const float *coord0 = modelData.vertices[vertexIndices[0]].data();
    const float *coord1 = modelData.vertices[vertexIndices[1]].data();
    const float *coord2 = modelData.vertices[vertexIndices[2]].data();
    return ConvertModelUtils::getTriangleArea(coord0, coord1, coord2);
}

void RemoveSmallPatches::groupRegions(const out_model_data_t &modelData, const float modelSize[3],
    float colorMinAreaRatio)
{
    std::vector<float> totalAreas(modelData.colors.size(), 0.0f);
    for (size_t i = 0; i < m_regions.size(); ++i) {
        auto it = m_regionGroupMap.emplace(m_regions[i].colorIndex, region_group_t()).first;
        it->second.regions.push_back(i);
        totalAreas[m_regions[i].colorIndex] += m_regions[i].area;
    }
    float modelExtent = std::max({ modelSize[0], modelSize[1], modelSize[2] });
    float globalThresholdArea = colorMinAreaRatio * modelExtent * modelExtent / 10000.0f;
    for (auto &item : m_regionGroupMap) {
        float thresholdArea = 0.01f * totalAreas[item.first] / item.second.regions.size();
        item.second.thresholdArea = std::clamp(
            thresholdArea, globalThresholdArea * 0.5f, globalThresholdArea * 2.0f);
    }
}

void RemoveSmallPatches::mergeSmallRegion(out_model_data_t &modelData)
{
    struct ReigonAreaCmp {
        bool operator()(const region_t *l, const region_t *r) const {
            return l->area < r->area;
        }
    };
    using identity_t = boost::multi_index::identity<region_t *>;
    using ptr_index_t = boost::multi_index::ordered_unique<identity_t>;
    using area_index_t = boost::multi_index::ordered_non_unique<identity_t, ReigonAreaCmp>;
    using index_by_t = boost::multi_index::indexed_by<ptr_index_t, area_index_t>;
    boost::multi_index::multi_index_container<region_t *, index_by_t> smallReigons;
    for (auto &groupItem : m_regionGroupMap) {
        for (auto region : groupItem.second.regions) {
            if (m_regions[region].area < groupItem.second.thresholdArea) {
                smallReigons.emplace(&m_regions[region]);
            }
        }
    }
    bool merged;
    std::set<region_t *> mergedRegionSet;
    do {
        merged = false;
        auto &ptrIndex = smallReigons.get<0>();
        auto &areaIndex = smallReigons.get<1>();
        for (auto it = areaIndex.begin(); it != areaIndex.end();) {
            region_t *current = *it;
            region_t *neighbor = getMaxNeighborRegion(current);
            if (neighbor != nullptr && mergedRegionSet.find(neighbor) == mergedRegionSet.end()) {
                ptrIndex.erase(neighbor);
                mergeRegion(modelData, current, neighbor);
                if (neighbor->area < m_regionGroupMap.at(neighbor->colorIndex).thresholdArea) {
                    smallReigons.emplace(neighbor);
                }
                it = areaIndex.erase(it);
                mergedRegionSet.insert(current);
                merged = true;
            } else {
                ++it;
            }
        }
    } while (merged);
}

RemoveSmallPatches::region_t *RemoveSmallPatches::getMaxNeighborRegion(region_t *region)
{
    int maxNeighborIdx = -1;
    float maxBoundaryLen = -FLT_MAX;
    for (auto &neighborInfoItem : region->neighborInfoMap) {
        if (neighborInfoItem.second > maxBoundaryLen) {
            maxNeighborIdx = neighborInfoItem.first;
            maxBoundaryLen = neighborInfoItem.second;
        }
    }
    if (maxNeighborIdx == -1 || m_regions[maxNeighborIdx].area == 0.0) {
        return nullptr;
    }
    return &m_regions[maxNeighborIdx];
}

void RemoveSmallPatches::mergeRegion(out_model_data_t &modelData, const region_t *src, region_t *dst)
{
    for (auto face : src->faces) {
        modelData.triangles[face].colorIndex = dst->colorIndex;
    }
    dst->faces.insert(dst->faces.end(), src->faces.begin(), src->faces.end());
    dst->area += src->area;
    for (auto &item : src->neighborInfoMap) {
        if (&m_regions[item.first] != dst) {
            auto pair = dst->neighborInfoMap.emplace(item);
            if (!pair.second) {
                pair.first->second += item.second;
            }
        } else {
            dst->neighborInfoMap.erase(item.first);
        }
    }
}

}} // namespace Slic3r::GUI
