#ifndef slic3r_GUI_FullColorModelSource_hpp_
#define slic3r_GUI_FullColorModelSource_hpp_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ConvertModelDef.hpp"

namespace Slic3r { namespace GUI {

struct FullColorTexture
{
    std::vector<uint8_t> pixels;
    int32_t              width{ 0 };
    int32_t              height{ 0 };
    int32_t              channels{ 0 };
    int32_t              bytes_per_line{ 0 };
    texture_wrap_type_t   wrap_u{ TEX_WRAP_REPEAT };
    texture_wrap_type_t   wrap_v{ TEX_WRAP_REPEAT };
    bool                  flip_y{ false };

    bool valid() const;
};

struct FullColorMaterial
{
    // A negative index means that this material is represented by base_color.
    int32_t                texture_index{ -1 };
    std::array<uint8_t, 4> base_color{ 255, 255, 255, 255 };
    bool                   has_base_color{ false };
};

class FullColorModelSource
{
public:
    using Vertex       = std::array<float, 3>;
    using Triangle     = std::array<int32_t, 3>;
    using TextureCoord = std::array<float, 2>;

    // Creates an owning representation from the legacy pointer-based import data.
    // All geometry, UVs and texture pixels are deep-copied.
    static bool create_from_legacy_input(const in_model_data_t &input, FullColorModelSource &output,
        std::string *error_message = nullptr);

    bool valid(std::string *error_message = nullptr) const;
    bool empty() const;
    void clear();

public:
    // Unique for each successfully imported immutable source in this process.
    uint64_t source_id{ 0 };
    std::string source_path;

    std::vector<Vertex>       vertices;
    std::vector<Triangle>     triangles;
    std::vector<TextureCoord> texture_coords;

    // UV indices are stored independently from vertex indices so UV seams are
    // retained. The Nth entry describes the three UVs of the Nth triangle.
    std::vector<Triangle> tex_coord_indices;

    std::vector<FullColorTexture>  textures;
    std::vector<FullColorMaterial> materials;

    // One material index per triangle. Each material maps either to a texture
    // or to a solid base color.
    std::vector<int32_t> face_material_ids;
};

}} // namespace Slic3r::GUI

#endif
