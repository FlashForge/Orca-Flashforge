#ifndef slic3r_GUI_CMLoadObj_hpp_
#define slic3r_GUI_CMLoadObj_hpp_

#include <deque>
#include <map>
#include <string>
#include <wx/image.h>
#include <wx/string.h>
#include "ConvertModelDef.hpp"
#include "libslic3r/Format/objparser.hpp"

namespace Slic3r { namespace GUI {

struct obj_extra_data_t {
    std::vector<int32_t> vertexIndices;
    std::vector<int32_t> texCoordIndices;
    std::vector<in_material_data_t> materialDatas;
    std::deque<in_texture_data_t> textureDatas;
    std::deque<wxImage> images;
};

class CMLoadObj
{
public:
    bool loadObj(const wxString &objPath, in_model_data_t &inData, ObjParser::ObjData &objData,
        obj_extra_data_t &objExtraData);

private:
    using mtl_map_t = std::map<std::string, ObjParser::ObjNewMtl>;

    bool loadMtlLibs(const wxString &dirPath, const ObjParser::ObjData &objData,
        mtl_map_t &mtlMap);

    bool checkObjData(const ObjParser::ObjData &objData, int &triangleCnt, int &maxPolySize);

    bool checkObjIndex(const ObjParser::ObjData &objData, int &triangleCnt, int &maxPolySize);

    bool checkObjMtl(const ObjParser::ObjData &objData, int triangleCnt);

    bool makeExtraData(const wxString &dirPath, const ObjParser::ObjData &objData,
        int triangleCnt, const mtl_map_t &mtlMap, obj_extra_data_t &objExtraData);

    void makeExtraIncides(const ObjParser::ObjData &objData, obj_extra_data_t &objExtraData);

    void makeInData(in_model_data_t &inData, const ObjParser::ObjData &objData, int triangleCnt,
        const obj_extra_data_t &objExtraData);
};

}} // namespace Slic3r::GUI

#endif
