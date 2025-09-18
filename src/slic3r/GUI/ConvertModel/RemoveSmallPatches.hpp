#ifndef slic3r_GUI_RemoveSmallPatches_hpp_
#define slic3r_GUI_RemoveSmallPatches_hpp_

#include <cstdint>
#include <array>
#include <map>
#include <vector>
#include "ConvertModelDef.hpp"

namespace Slic3r { namespace GUI {

class RemoveSmallPatches
{
public:
    void mergeSmallPatchesColor(out_model_data_t &modelData, const float modelSize[3],
        float colorMinAreaRatio);

private:
    struct neighbor_info_t {
        int32_t index;
        float boundaryLen;
    };
    struct region_t {
        std::vector<int32_t> faces;
        std::map<int32_t, float> neighborInfoMap;
        int32_t colorIndex;
        float area;
    };
    struct region_group_t {
        std::vector<int32_t> regions;
        float thresholdArea;
    };
    void findFaceNeighbors(const out_model_data_t &modelData);

    void addFaceNeighbor(int32_t face0, int32_t face1, float boundaryLen);

    void splitRegions(const out_model_data_t &modelData);

    double getFaceArea(const out_model_data_t &modelData, int32_t faceIndex);

    void groupRegions(const out_model_data_t &modelData, const float modelSize[3],
        float colorMinAreaRatio);

    void mergeSmallRegion(out_model_data_t &modelData);

    region_t *getMaxNeighborRegion(region_t *region);

    void mergeRegion(out_model_data_t &modelData, const region_t *src, region_t *dst);

private:
    std::vector<std::array<neighbor_info_t, 3>> m_faceNeighborInfos;
    std::vector<int32_t> m_faceRegions;
    std::vector<region_t> m_regions;
    std::map<int32_t, region_group_t> m_regionGroupMap;
};

}} // namespace Slic3r::GUI

#endif
