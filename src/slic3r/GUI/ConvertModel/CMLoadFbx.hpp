#ifndef slic3r_GUI_CMLoadFbx_hpp_
#define slic3r_GUI_CMLoadFbx_hpp_

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <wx/string.h>

#include "ConvertModelDef.hpp"

namespace Slic3r { namespace GUI {

struct fbx_data_t {
    std::vector<float> vertices;
    std::vector<float> texCoords;
    std::vector<int32_t> vertexIndices;
    std::vector<int32_t> texCoordIndices;
    std::vector<in_material_data_t> materialDatas;
    std::deque<in_texture_data_t> textureDatas;
    std::deque<std::vector<uint8_t>> textureBits;
};

class CMLoadFbx
{
public:
    bool loadFbx(const wxString &path, in_model_data_t &inData, fbx_data_t &fbxData, std::string *error_message = nullptr);
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_CMLoadFbx_hpp_
