#ifndef slic3r_BambuTexturePainting_hpp_
#define slic3r_BambuTexturePainting_hpp_

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

struct indexed_triangle_set;

namespace Slic3r {

struct TextureImage
{
    int                        width{ 0 };
    int                        height{ 0 };
    int                        channels{ 4 };
    std::vector<unsigned char> data;
};

struct TexturedMesh
{
    std::vector<std::array<float, 3>> vertices;
    std::vector<std::array<int, 3>>   indices;
    std::vector<std::array<float, 2>> uvs;
    std::vector<TextureImage>         textures;
    std::vector<int>                  material_ids;

    // Material index -> textures index. A negative value selects material_colors.
    std::vector<int>                 material_texture_map;
    std::vector<std::array<float, 4>> material_colors;

    // Independent per-face UV indices retain OBJ-style UV seams.
    std::vector<std::array<float, 2>> uv_coords;
    std::vector<std::array<int, 3>>   uv_indices;

    bool has_face_uvs() const { return !uv_indices.empty() && !uv_coords.empty(); }
};

struct PaintedMesh
{
    std::vector<std::array<float, 3>>       vertices;
    std::vector<std::array<int, 3>>         indices;
    std::vector<std::array<std::size_t, 3>> face_colors;
    std::vector<std::array<std::size_t, 3>> cluster_colors;
};

using PaintProgressCallback = std::function<void(int percent, const char *message)>;
using PaintCancelCallback   = std::function<bool()>;
using PaintMeshRepairCallback = std::function<bool(const indexed_triangle_set &mesh, indexed_triangle_set &repaired_mesh,
    std::function<void(const char *message, unsigned progress)> progress_callback, std::function<bool()> cancel_callback,
    std::string *error_message)>;

struct TexturePaintingSettings
{
    enum class MeshRepairDecision
    {
        Ask,
        ImportWithoutRepair,
        RepairAndImport
    };

    std::size_t target_colors_num{ 4 };
    double      smooth_weight{ 0.5 };
    std::size_t oversampling_iters{ 0 };
    std::size_t oversampling_min_face_count{ 10000 };
    std::size_t oversampling_max_face_count{ 1000000 };
    double      max_color_distance{ 25.0 };
    std::size_t max_cluster_k{ 32 };

    MeshRepairDecision mesh_repair_decision{ MeshRepairDecision::ImportWithoutRepair };
    bool              *mesh_repair_decision_required{ nullptr };
    PaintMeshRepairCallback mesh_repair_callback;
};

bool texture_to_painting(const TexturedMesh &textured, PaintedMesh &painted,
    const TexturePaintingSettings &settings = {}, PaintProgressCallback progress = nullptr,
    PaintCancelCallback cancel = nullptr);

} // namespace Slic3r

#endif
