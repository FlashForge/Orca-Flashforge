#include "ConvertModelUtils.hpp"
#include <algorithm>
#include <stdexcept>
#include <boost/functional/hash.hpp>

namespace Slic3r { namespace GUI {

size_t ConvertModelUtils::PointHash::operator()(const std::array<float, 3> &p) const
{
    size_t seed = 0;
    if (p[0] != 0.0f) {
        seed = *(uint32_t *)&p[0];
    }
    if (p[1] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p[1]);
    }
    if (p[2] != 0.0f) {
        boost::hash_combine(seed, *(uint32_t *)&p[2]);
    }
    return seed;
}

size_t ConvertModelUtils::EdgeHash::operator()(const std::array<int32_t, 2> &edge) const
{
    return edge[0] + edge[1];
}

cvt_points_it_t ConvertModelUtils::findPoint(cvt_root_half_edge_t &rootEdge, int32_t pos)
{
    auto pred = [](const cvt_point_t &a, int32_t pos) {
        return a.pos < pos;
    };
    auto it = std::lower_bound(rootEdge.points.begin(), rootEdge.points.end(), pos, pred);
    if (it->pos != pos) {
        return rootEdge.points.end();
    }
    return it;
}

const cvt_points_cit_t ConvertModelUtils::findPoint(const cvt_root_half_edge_t &rootEdge, int32_t pos)
{
    auto pred = [](const cvt_point_t &a, int32_t pos) {
        return a.pos < pos;
    };
    auto it = std::lower_bound(rootEdge.points.cbegin(), rootEdge.points.cend(), pos, pred);
    if (it->pos != pos) {
        return rootEdge.points.cend();
    }
    return it;
}

cvt_points_it_t ConvertModelUtils::insertPoint(cvt_root_half_edge_t &rootEdge, const cvt_point_t &point)
{
    auto pred = [](int32_t pos, const cvt_point_t &a) {
        return pos < a.pos;
    };
    auto it = std::upper_bound(rootEdge.points.begin(), rootEdge.points.end(), point.pos, pred);
    return rootEdge.points.insert(it, point);
}

int ConvertModelUtils::getColorDist2(cvt_color_t color0, cvt_color_t color1)
{
    auto rgb2yuv = [](const cvt_color_t &rgb, float yuv[3]) {
        yuv[0] = 0.299f * rgb[0] + 0.587f * rgb[1] + 0.114f * rgb[2];
        yuv[1] = -0.169f * rgb[0] - 0.331f * rgb[1] + 0.5f * rgb[2];
        yuv[2] = 0.5f * rgb[0] - 0.419f * rgb[1] - 0.081f * rgb[2];
    };
    float yuv0[3];
    float yuv1[3];
    rgb2yuv(color0, yuv0);
    rgb2yuv(color1, yuv1);
    float dy = (yuv0[0] - yuv1[0]) * 0.5f;
    float du = yuv0[1] - yuv1[1];
    float dv = yuv0[2] - yuv1[2];
    return dy * dy + du * du + dv * dv + 0.5f;
}

int ConvertModelUtils::getColorMinDist2(const cvt_colors_t &colors, cvt_color_t color, int &minIdx)
{
    minIdx = 0;
    int minDist2 = getColorDist2(color, colors[0]);
    for (size_t i = 1; i < colors.size(); ++i) {
        int dist2 = getColorDist2(color, colors[i]);
        if (dist2 < minDist2) {
            minIdx = i;
            minDist2 = dist2;
        }
    }
    return minDist2;
}

double ConvertModelUtils::getTriangleArea(const float *coord0, const float *coord1, const float *coord2)
{
    double dx01 = coord1[0] - coord0[0];
    double dy01 = coord1[1] - coord0[1];
    double dz01 = coord1[2] - coord0[2];
    double dx02 = coord2[0] - coord0[0];
    double dy02 = coord2[1] - coord0[1];
    double dz02 = coord2[2] - coord0[2];
    double cx = dy01 * dz02 - dz01 * dy02;
    double cy = dz01 * dx02 - dx01 * dz02;
    double cz = dx01 * dy02 - dy01 * dx02;
    return 0.5 * sqrt(cx * cx + cy * cy + cz * cz);
}

}} // namespace Slic3r::GUI
