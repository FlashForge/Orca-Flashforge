#ifndef slic3r_MeshRepair_hpp_
#define slic3r_MeshRepair_hpp_

#include <admesh/stl.h>

#include <cstddef>
#include <functional>
#include <string>

namespace Slic3r {

using MeshRepairProgressFn = std::function<void(const char *message, unsigned progress)>;
using MeshRepairCancelFn   = std::function<bool()>;

inline constexpr std::size_t BAMBULAB_MAX_REPAIRABLE_MESH_HOLE_EDGES     = 500;
inline constexpr std::size_t BAMBULAB_MAX_REPAIRABLE_MESH_BOUNDARY_EDGES = 5000;
inline constexpr std::size_t SAFE_MAX_REPAIRABLE_MESH_HOLE_EDGES         = 100;
inline constexpr std::size_t SAFE_MAX_REPAIRABLE_MESH_BOUNDARY_EDGES     = 1000;

struct CgalMeshRepairSettings
{
    bool        close_holes        = false;
    std::size_t max_hole_edges     = SAFE_MAX_REPAIRABLE_MESH_HOLE_EDGES;
    std::size_t max_boundary_edges = SAFE_MAX_REPAIRABLE_MESH_BOUNDARY_EDGES;
};

bool is_mesh_halfedge_compatible(const indexed_triangle_set &mesh);

bool repair_mesh_by_cgal(const indexed_triangle_set &mesh,
                         indexed_triangle_set       &repaired_mesh,
                         MeshRepairProgressFn        progress_callback = {},
                         MeshRepairCancelFn          cancel_callback = {},
                         std::string                *error_message = nullptr,
                         const CgalMeshRepairSettings &settings = CgalMeshRepairSettings{});

} // namespace Slic3r

#endif // slic3r_MeshRepair_hpp_
