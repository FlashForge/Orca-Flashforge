#ifndef slic3r_GUI_ConvertModelUtils_hpp_
#define slic3r_GUI_ConvertModelUtils_hpp_

#include <array>
#include <string>
#include <unordered_map>
#include "ConvertModelDef.hpp"

namespace Slic3r { namespace GUI {

class ConvertModelUtils
{
public:
    struct PointHash {
        size_t operator()(const std::array<float, 3> &p) const;
    };
    struct EdgeHash {
        size_t operator()(const std::array<int32_t, 2> &edge) const;
    };
    using point_map_t = std::unordered_map<std::array<float, 3>, int, PointHash>;

    using edge_multi_map_t = std::unordered_multimap<std::array<int32_t, 2>, int, EdgeHash>;

    static cvt_points_it_t findPoint(cvt_root_half_edge_t &rootEdge, int32_t pos);

    static const cvt_points_cit_t findPoint(const cvt_root_half_edge_t &rootEdge, int32_t pos);

    static cvt_points_it_t insertPoint(cvt_root_half_edge_t &rootEdge, const cvt_point_t &point);

    static int getColorDist2(cvt_color_t color0, cvt_color_t color1);

    static int getColorMinDist2(const cvt_colors_t &colors, cvt_color_t color, int &minIdx);

    static double getTriangleArea(const float *coord0, const float *coord1, const float *coord2);
};

}} // namespace Slic3r::GUI

#endif
