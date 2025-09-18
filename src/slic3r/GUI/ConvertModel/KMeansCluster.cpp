#include "KMeansCluster.hpp"
#include <algorithm>
#include "ConvertModelUtils.hpp"

namespace Slic3r { namespace GUI {

KMeansCluster::KMeansCluster(const cvt_color_datas_t &colorDatas)
    : m_colorDatas(colorDatas)
{
}

cvt_colors_t KMeansCluster::clusterColors(int colorNum, int maxIterations /* = 100 */)
{
    if (colorNum == 1) {
        return processColor1();
    }
    cvt_colors_t colors;
    if (m_colorDatas.size() <= colorNum) {
        for (auto &colorData : m_colorDatas) {
            colors.push_back(colorData.color);
        }
        return std::move(colors);
    }
    colors = getStartColors(colorNum);
    std::vector<int> groupIndices(m_colorDatas.size());
    for (int i = 0; i < maxIterations; ++i) {
        for (size_t i = 0; i < m_colorDatas.size(); ++i) {
            ConvertModelUtils::getColorMinDist2(colors, m_colorDatas[i].color, groupIndices[i]);
        }
        cvt_colors_t newColors = getGroupColors(groupIndices, colorNum);
        if (newColors == colors) {
            break;
        }
        colors = std::move(newColors);
    }
    return std::move(colors);
}

cvt_colors_t KMeansCluster::processColor1()
{
    double colorSum[3] = { 0.0, 0.0, 0.0 };
    double weightSum = 0.0;
    for (size_t i = 0; i < m_colorDatas.size(); ++i) {
        colorSum[0] += m_colorDatas[i].color[0] * m_colorDatas[i].weight;
        colorSum[1] += m_colorDatas[i].color[1] * m_colorDatas[i].weight;
        colorSum[2] += m_colorDatas[i].color[2] * m_colorDatas[i].weight;
        weightSum += m_colorDatas[i].weight;
    }
    cvt_color_t color;
    color[0] = colorSum[0] / weightSum + 0.5;
    color[1] = colorSum[1] / weightSum + 0.5;
    color[2] = colorSum[2] / weightSum + 0.5;
    return cvt_colors_t(1, color);
}

cvt_colors_t KMeansCluster::getStartColors(int colorNum)
{
    int maxIdx = 0;
    getMaxDist2(m_colorDatas, m_colorDatas[0].color, maxIdx);
    cvt_colors_t colors;
    colors.push_back(m_colorDatas[0].color);
    colors.push_back(m_colorDatas[maxIdx].color);
    for (int i = 2; i < colorNum; ++i) {
        int maxDist2 = INT_MIN;
        for (size_t j = 1; j < m_colorDatas.size(); ++j) {
            int minIdx;
            int dist2 = ConvertModelUtils::getColorMinDist2(colors, m_colorDatas[j].color, minIdx);
            if (dist2 > maxDist2) {
                maxIdx = j;
                maxDist2 = dist2;
            }
        }
        colors.push_back(m_colorDatas[maxIdx].color);
    }
    std::sort(colors.begin(), colors.end());
    return std::move(colors);
}

cvt_colors_t KMeansCluster::getGroupColors(const std::vector<int> &groupIndices, int colorNum)
{
    std::vector<std::array<double, 3>> colorSums(colorNum, { 0.0, 0.0, 0.0 });
    std::vector<double> colorWeightSums(colorNum, 0);
    for (size_t i = 0; i < m_colorDatas.size(); ++i) {
        colorSums[groupIndices[i]][0] += m_colorDatas[i].color[0] * m_colorDatas[i].weight;
        colorSums[groupIndices[i]][1] += m_colorDatas[i].color[1] * m_colorDatas[i].weight;
        colorSums[groupIndices[i]][2] += m_colorDatas[i].color[2] * m_colorDatas[i].weight;
        colorWeightSums[groupIndices[i]] += m_colorDatas[i].weight;
    }
    cvt_colors_t colors(colorNum);
    for (size_t i = 0; i < colors.size(); ++i) {
        colors[i][0] = colorSums[i][0] / colorWeightSums[i] + 0.5;
        colors[i][1] = colorSums[i][1] / colorWeightSums[i] + 0.5;
        colors[i][2] = colorSums[i][2] / colorWeightSums[i] + 0.5;
    }
    std::sort(colors.begin(), colors.end());
    return colors;
}

int KMeansCluster::getMaxDist2(const cvt_color_datas_t &colorDatas, cvt_color_t color, int &maxIdx)
{
    maxIdx = 0;
    int maxDist2 = ConvertModelUtils::getColorDist2(color, colorDatas[0].color);
    for (size_t i = 1; i < colorDatas.size(); ++i) {
        int dist2 = ConvertModelUtils::getColorDist2(color, colorDatas[i].color);
        if (dist2 > maxDist2) {
            maxIdx = i;
            maxDist2 = dist2;
        }
    }
    return maxDist2;
}

}} // namespace Slic3r::GUI
