#ifndef slic3r_GUI_KMeansCluster_hpp_
#define slic3r_GUI_KMeansCluster_hpp_

#include "ConvertModelDef.hpp"
#include <vector>

namespace Slic3r { namespace GUI {

class KMeansCluster
{
public:
    KMeansCluster(const cvt_color_datas_t &colorDatas);

    cvt_colors_t clusterColors(int colorNum, int maxIterations = 100);

private:
    cvt_colors_t processColor1();

    cvt_colors_t getStartColors(int colorNum);

    cvt_colors_t getGroupColors(const std::vector<int> &groupIndices, int colorNum);

    int getMaxDist2(const cvt_color_datas_t &colorDatas, cvt_color_t color, int &maxIdx);

private:
    const cvt_color_datas_t &m_colorDatas;
};

}} // namespace Slic3r::GUI

#endif
