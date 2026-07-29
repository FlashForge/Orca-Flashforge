#include "MeshRepair.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Surface_mesh.h>

#include <boost/log/trivial.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Slic3r {
namespace {

namespace PMP = CGAL::Polygon_mesh_processing;

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using CGALMesh = CGAL::Surface_mesh<Kernel::Point_3>;
using HalfedgeDescriptor = boost::graph_traits<CGALMesh>::halfedge_descriptor;
using FaceDescriptor = boost::graph_traits<CGALMesh>::face_descriptor;

struct BoundaryEdgeStats
{
    std::size_t total_boundary_edges = 0;
    std::size_t max_cycle_edges      = 0;
    std::size_t cycle_count          = 0;
};

void report_progress(const MeshRepairProgressFn &progress_callback, const char *message, unsigned progress)
{
    if (progress_callback)
        progress_callback(message, progress);
}

void throw_if_canceled(const MeshRepairCancelFn &cancel_callback)
{
    if (cancel_callback && cancel_callback())
        throw std::runtime_error("Model repair has been canceled.");
}

BoundaryEdgeStats compute_boundary_edge_stats(const CGALMesh &cgal_mesh)
{
    std::vector<HalfedgeDescriptor> border_cycles;
    PMP::extract_boundary_cycles(cgal_mesh, std::back_inserter(border_cycles));

    BoundaryEdgeStats stats;
    stats.cycle_count = border_cycles.size();
    for (const HalfedgeDescriptor h0 : border_cycles) {
        std::size_t len = 0;
        HalfedgeDescriptor h = h0;
        do {
            ++len;
            h = next(h, cgal_mesh);
        } while (h != h0);
        stats.max_cycle_edges = std::max(stats.max_cycle_edges, len);
        stats.total_boundary_edges += len;
    }
    return stats;
}

void close_boundaries_and_repair_manifoldness(CGALMesh &cgal_mesh)
{
    PMP::stitch_borders(cgal_mesh);
    PMP::duplicate_non_manifold_vertices(cgal_mesh);

    std::vector<HalfedgeDescriptor> border_cycles;
    PMP::extract_boundary_cycles(cgal_mesh, std::back_inserter(border_cycles));

    for (const HalfedgeDescriptor h : border_cycles) {
        std::vector<FaceDescriptor> patch_faces;
        PMP::triangulate_hole(cgal_mesh, h, std::back_inserter(patch_faces));
    }

    PMP::remove_degenerate_faces(cgal_mesh);
    PMP::duplicate_non_manifold_vertices(cgal_mesh);
    cgal_mesh.collect_garbage();
}

indexed_triangle_set cgal_to_indexed_triangle_set(const CGALMesh &cgal_mesh)
{
    indexed_triangle_set out;
    out.vertices.reserve(cgal_mesh.number_of_vertices());
    out.indices.reserve(cgal_mesh.number_of_faces());

    std::map<CGALMesh::Vertex_index, int> vertex_map;
    int vertex_idx = 0;
    for (const CGALMesh::Vertex_index v : cgal_mesh.vertices()) {
        if (!cgal_mesh.is_valid(v) || cgal_mesh.is_removed(v))
            continue;
        const Kernel::Point_3 &p = cgal_mesh.point(v);
        out.vertices.emplace_back(static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()));
        vertex_map.emplace(v, vertex_idx++);
    }

    for (const CGALMesh::Face_index f : cgal_mesh.faces()) {
        if (!cgal_mesh.is_valid(f) || cgal_mesh.is_removed(f))
            continue;

        const HalfedgeDescriptor h0 = cgal_mesh.halfedge(f);
        const CGALMesh::Vertex_index v0 = cgal_mesh.target(h0);
        const CGALMesh::Vertex_index v1 = cgal_mesh.target(cgal_mesh.next(h0));
        const CGALMesh::Vertex_index v2 = cgal_mesh.target(cgal_mesh.next(cgal_mesh.next(h0)));
        auto it0 = vertex_map.find(v0);
        auto it1 = vertex_map.find(v1);
        auto it2 = vertex_map.find(v2);
        if (it0 == vertex_map.end() || it1 == vertex_map.end() || it2 == vertex_map.end())
            continue;
        if (it0->second == it1->second || it1->second == it2->second || it2->second == it0->second)
            continue;
        out.indices.emplace_back(it0->second, it1->second, it2->second);
    }

    out.properties.resize(out.indices.size());
    return out;
}

} // namespace

bool is_mesh_halfedge_compatible(const indexed_triangle_set &mesh)
{
    if (mesh.vertices.empty())
        return mesh.indices.empty();

    std::vector<std::unordered_set<std::size_t>> vtx_to_adj_faces(mesh.vertices.size());
    std::size_t edge_id = 0;
    std::vector<std::unordered_set<std::size_t>> edge_to_faces;
    std::vector<std::unordered_set<std::size_t>> vtx_to_prev_vtxs(mesh.vertices.size());
    std::vector<std::unordered_set<std::size_t>> vtx_to_next_vtxs(mesh.vertices.size());
    std::vector<std::unordered_map<std::size_t, std::size_t>> vtx_vtx_to_edge(mesh.vertices.size());

    for (std::size_t fid = 0; fid < mesh.indices.size(); ++fid) {
        const stl_triangle_vertex_indices &face = mesh.indices[fid];
        std::array<std::size_t, 3> face_vertices;
        for (std::size_t i = 0; i < 3; ++i) {
            if (face[i] < 0 || static_cast<std::size_t>(face[i]) >= mesh.vertices.size())
                return false;
            face_vertices[i] = static_cast<std::size_t>(face[i]);
        }

        if (face_vertices[0] == face_vertices[1] || face_vertices[1] == face_vertices[2] || face_vertices[2] == face_vertices[0])
            return false;

        for (std::size_t i = 0; i < 3; ++i) {
            const std::size_t vtx      = face_vertices[i];
            const std::size_t prev_vtx = face_vertices[(i + 2) % 3];
            const std::size_t next_vtx = face_vertices[(i + 1) % 3];

            vtx_to_adj_faces[vtx].insert(fid);

            if (vtx_to_prev_vtxs[vtx].count(prev_vtx) != 0)
                return false;
            vtx_to_prev_vtxs[vtx].insert(prev_vtx);

            if (vtx_to_next_vtxs[vtx].count(next_vtx) != 0)
                return false;
            vtx_to_next_vtxs[vtx].insert(next_vtx);
        }

        for (std::size_t i = 0; i < 3; ++i) {
            const std::size_t va = face_vertices[i];
            const std::size_t vb = face_vertices[(i + 1) % 3];
            if (vtx_vtx_to_edge[va].count(vb) == 0) {
                vtx_vtx_to_edge[va][vb] = edge_id;
                vtx_vtx_to_edge[vb][va] = edge_id;
                ++edge_id;
                edge_to_faces.emplace_back();
            }
            edge_to_faces[vtx_vtx_to_edge[va][vb]].insert(fid);
        }
    }

    for (std::size_t vid = 0; vid < mesh.vertices.size(); ++vid) {
        if (vtx_to_adj_faces[vid].empty())
            continue;

        std::unordered_set<std::size_t> visited_faces;
        std::queue<std::size_t> face_queue;
        const std::size_t first_face = *vtx_to_adj_faces[vid].begin();
        face_queue.push(first_face);
        visited_faces.insert(first_face);

        while (!face_queue.empty()) {
            const std::size_t fid = face_queue.front();
            face_queue.pop();
            const stl_triangle_vertex_indices &face = mesh.indices[fid];

            std::array<std::size_t, 3> face_vertices;
            for (std::size_t i = 0; i < 3; ++i)
                face_vertices[i] = static_cast<std::size_t>(face[i]);

            for (std::size_t i = 0; i < 3; ++i) {
                if (face_vertices[i] != vid)
                    continue;

                const std::size_t v_next = face_vertices[(i + 1) % 3];
                const std::size_t v_prev = face_vertices[(i + 2) % 3];
                for (const std::size_t nbr : { v_next, v_prev }) {
                    const auto edge_it = vtx_vtx_to_edge[vid].find(nbr);
                    if (edge_it == vtx_vtx_to_edge[vid].end())
                        return false;
                    for (const std::size_t adj_fid : edge_to_faces[edge_it->second]) {
                        if (visited_faces.count(adj_fid) == 0 && vtx_to_adj_faces[vid].count(adj_fid) != 0) {
                            visited_faces.insert(adj_fid);
                            face_queue.push(adj_fid);
                        }
                    }
                }
                break;
            }
        }

        for (const std::size_t fid : vtx_to_adj_faces[vid])
            if (visited_faces.count(fid) == 0)
                return false;
    }

    return true;
}

bool repair_mesh_by_cgal(const indexed_triangle_set &mesh,
                         indexed_triangle_set       &repaired_mesh,
                         MeshRepairProgressFn        progress_callback,
                         MeshRepairCancelFn          cancel_callback,
                         std::string                *error_message,
                         const CgalMeshRepairSettings &settings)
{
    using Clock = std::chrono::steady_clock;
    const Clock::time_point t_total = Clock::now();
    auto elapsed_ms = [](Clock::time_point t0) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    };

    try {
        if (mesh.vertices.empty() || mesh.indices.empty())
            throw std::runtime_error("Input mesh is empty.");

        std::vector<Kernel::Point_3> soup_points;
        soup_points.reserve(mesh.vertices.size());
        for (const stl_vertex &vertex : mesh.vertices)
            soup_points.emplace_back(vertex.x(), vertex.y(), vertex.z());

        std::vector<std::vector<std::size_t>> soup_triangles;
        soup_triangles.reserve(mesh.indices.size());
        for (const stl_triangle_vertex_indices &face : mesh.indices) {
            if (face[0] < 0 || face[1] < 0 || face[2] < 0 ||
                static_cast<std::size_t>(face[0]) >= mesh.vertices.size() ||
                static_cast<std::size_t>(face[1]) >= mesh.vertices.size() ||
                static_cast<std::size_t>(face[2]) >= mesh.vertices.size() ||
                face[0] == face[1] || face[1] == face[2] || face[2] == face[0])
                continue;
            soup_triangles.push_back({static_cast<std::size_t>(face[0]),
                                      static_cast<std::size_t>(face[1]),
                                      static_cast<std::size_t>(face[2])});
        }
        if (soup_triangles.empty())
            throw std::runtime_error("Input mesh has no valid triangles.");

        report_progress(progress_callback, "Repairing polygon soup", 20);
        throw_if_canceled(cancel_callback);
        {
            const auto t0 = Clock::now();
            PMP::repair_polygon_soup(soup_points, soup_triangles);
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair stage=repair_polygon_soup took=" << elapsed_ms(t0) << " ms";
        }

        report_progress(progress_callback, "Orienting polygon soup", 40);
        throw_if_canceled(cancel_callback);
        {
            const auto t0 = Clock::now();
            PMP::orient_polygon_soup(soup_points, soup_triangles);
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair stage=orient_polygon_soup took=" << elapsed_ms(t0) << " ms";
        }

        report_progress(progress_callback, "Converting to mesh", 60);
        throw_if_canceled(cancel_callback);
        CGALMesh cgal_mesh;
        {
            const auto t0 = Clock::now();
            PMP::polygon_soup_to_polygon_mesh(soup_points, soup_triangles, cgal_mesh);
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair stage=polygon_soup_to_polygon_mesh took=" << elapsed_ms(t0) << " ms";
        }
        if (cgal_mesh.number_of_faces() == 0)
            throw std::runtime_error("CGAL mesh repair produced an empty mesh.");

        {
            const auto t0 = Clock::now();
            PMP::remove_degenerate_faces(cgal_mesh);
            cgal_mesh.collect_garbage();
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair stage=remove_degenerate_faces took=" << elapsed_ms(t0) << " ms";
        }

        report_progress(progress_callback, "Repairing mesh boundaries", 75);
        throw_if_canceled(cancel_callback);
        BoundaryEdgeStats stats;
        {
            const auto t0 = Clock::now();
            PMP::stitch_borders(cgal_mesh);
            PMP::duplicate_non_manifold_vertices(cgal_mesh);
            cgal_mesh.collect_garbage();
            stats = compute_boundary_edge_stats(cgal_mesh);
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair stage=boundary_stats took=" << elapsed_ms(t0) << " ms"
                                    << ", total_boundary_edges=" << stats.total_boundary_edges
                                    << ", max_cycle_edges=" << stats.max_cycle_edges
                                    << ", cycle_count=" << stats.cycle_count;
        }

        const bool can_repair_holes =
            settings.close_holes &&
            stats.total_boundary_edges <= settings.max_boundary_edges &&
            stats.max_cycle_edges <= settings.max_hole_edges;
        if (can_repair_holes) {
            const auto t0 = Clock::now();
            close_boundaries_and_repair_manifoldness(cgal_mesh);
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair stage=close_boundaries took=" << elapsed_ms(t0) << " ms";
        } else {
            BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair skipped hole closing"
                                    << ", close_holes=" << settings.close_holes
                                    << ", total_boundary_edges=" << stats.total_boundary_edges
                                    << " (limit=" << settings.max_boundary_edges << ")"
                                    << ", max_cycle_edges=" << stats.max_cycle_edges
                                    << " (limit=" << settings.max_hole_edges << ")"
                                    << ", cycle_count=" << stats.cycle_count;
        }

        report_progress(progress_callback, "Finalizing repaired mesh", 95);
        throw_if_canceled(cancel_callback);
        repaired_mesh = cgal_to_indexed_triangle_set(cgal_mesh);
        if (repaired_mesh.indices.empty())
            throw std::runtime_error("CGAL mesh repair produced no valid triangles.");

        report_progress(progress_callback, "Done", 100);
        BOOST_LOG_TRIVIAL(info) << "CGAL mesh repair total=" << elapsed_ms(t_total) << " ms";
        return true;
    } catch (const std::exception &ex) {
        if (error_message)
            *error_message = ex.what();
        BOOST_LOG_TRIVIAL(warning) << "CGAL mesh repair failed: " << ex.what();
        return false;
    }
}

} // namespace Slic3r
