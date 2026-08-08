#include "BambuTexturePainting.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "TextureToColor/Repair.hpp"
#include "TextureToColor/TextureToColor.hpp"

namespace Slic3r {

namespace {

cv::Mat decode_texture_image(const TextureImage &image)
{
    if (image.data.empty())
        return {};

    if (image.width <= 0 || image.height <= 0) {
        cv::Mat raw(1, static_cast<int>(image.data.size()), CV_8UC1, const_cast<unsigned char *>(image.data.data()));
        return cv::imdecode(raw, cv::IMREAD_COLOR);
    }

    const int cv_type = image.channels == 4 ? CV_8UC4 : CV_8UC3;
    cv::Mat source(image.height, image.width, cv_type, const_cast<unsigned char *>(image.data.data()));

    cv::Mat bgr;
    if (image.channels == 4)
        cv::cvtColor(source, bgr, cv::COLOR_RGBA2BGR);
    else if (image.channels == 3)
        cv::cvtColor(source, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

bool repair_with_texture_to_color_core(const indexed_triangle_set &mesh, indexed_triangle_set &repaired_mesh,
    std::function<void(const char *message, unsigned progress)> progress_callback,
    std::function<bool()> cancel_callback, std::string *error_message)
{
    tex2color::TriMesh input(mesh);
    std::shared_ptr<tex2color::TriMesh> repaired;

    tex2color::AlgoProgressCallback repair_progress;
    if (progress_callback) {
        repair_progress = [&progress_callback](tex2color::AlgoProgress state) {
            progress_callback(state.message, static_cast<unsigned>(std::clamp(state.percent, 0, 100)));
        };
    }

    if (!tex2color::RepairMesh(input, repaired, repair_progress, std::move(cancel_callback)) || repaired == nullptr) {
        if (error_message != nullptr)
            *error_message = "Bambu texture-to-color mesh repair failed.";
        return false;
    }

    repaired_mesh.vertices = std::move(repaired->vertices);
    repaired_mesh.indices = std::move(repaired->indices);
    return true;
}

void build_tex2color_mesh(const TexturedMesh &textured, tex2color::TriMesh &mesh,
    std::vector<std::vector<Vec2f>> &uv_coords)
{
    mesh.vertices.resize(textured.vertices.size());
    for (size_t vertex_idx = 0; vertex_idx < textured.vertices.size(); ++vertex_idx) {
        const auto &vertex = textured.vertices[vertex_idx];
        mesh.vertices[vertex_idx] = Vec3f(vertex[0], vertex[1], vertex[2]);
    }

    mesh.indices.resize(textured.indices.size());
    for (size_t face_idx = 0; face_idx < textured.indices.size(); ++face_idx) {
        const auto &face = textured.indices[face_idx];
        mesh.indices[face_idx] = tex2color::TriFace(face[0], face[1], face[2]);
    }

    uv_coords.resize(textured.indices.size());
    for (size_t face_idx = 0; face_idx < textured.indices.size(); ++face_idx) {
        uv_coords[face_idx].resize(3);
        for (size_t corner = 0; corner < 3; ++corner) {
            if (textured.has_face_uvs()) {
                const int uv_idx = textured.uv_indices[face_idx][corner];
                if (uv_idx >= 0 && static_cast<size_t>(uv_idx) < textured.uv_coords.size()) {
                    uv_coords[face_idx][corner] = Vec2f(
                        textured.uv_coords[uv_idx][0], textured.uv_coords[uv_idx][1]);
                } else {
                    uv_coords[face_idx][corner] = Vec2f(0.0f, 0.0f);
                }
            } else {
                const int vertex_idx = textured.indices[face_idx][corner];
                if (vertex_idx >= 0 && static_cast<size_t>(vertex_idx) < textured.uvs.size()) {
                    uv_coords[face_idx][corner] = Vec2f(
                        textured.uvs[vertex_idx][0], textured.uvs[vertex_idx][1]);
                } else {
                    uv_coords[face_idx][corner] = Vec2f(0.0f, 0.0f);
                }
            }
        }
    }
}

void extract_painted_mesh(const tex2color::TriMesh &color_mesh,
    const std::vector<std::array<std::size_t, 3>> &face_colors, PaintedMesh &painted)
{
    painted.vertices.resize(color_mesh.vertices.size());
    for (size_t vertex_idx = 0; vertex_idx < color_mesh.vertices.size(); ++vertex_idx) {
        const auto &vertex = color_mesh.vertices[vertex_idx];
        painted.vertices[vertex_idx] = { vertex.x(), vertex.y(), vertex.z() };
    }

    painted.indices.resize(color_mesh.indices.size());
    for (size_t face_idx = 0; face_idx < color_mesh.indices.size(); ++face_idx) {
        const auto &face = color_mesh.indices[face_idx];
        painted.indices[face_idx] = { face[0], face[1], face[2] };
    }

    painted.face_colors = face_colors;
    const std::set<std::array<std::size_t, 3>> unique_colors(face_colors.begin(), face_colors.end());
    painted.cluster_colors.assign(unique_colors.begin(), unique_colors.end());
}

bool build_multi_texture_atlas(const TexturedMesh &textured, cv::Mat &atlas,
    std::vector<std::vector<Vec2f>> &atlas_uv_coords)
{
    std::vector<cv::Mat> decoded;
    decoded.reserve(textured.textures.size());
    for (const TextureImage &texture : textured.textures)
        decoded.emplace_back(decode_texture_image(texture));

    const bool   has_mapping = !textured.material_texture_map.empty();
    const size_t face_count  = textured.indices.size();

    auto resolve_texture = [&](int material_idx) {
        if (!has_mapping || material_idx < 0
            || static_cast<size_t>(material_idx) >= textured.material_texture_map.size()) {
            return -1;
        }
        const int texture_idx = textured.material_texture_map[material_idx];
        if (texture_idx < 0 || static_cast<size_t>(texture_idx) >= decoded.size() || decoded[texture_idx].empty())
            return -1;
        return texture_idx;
    };

    int atlas_width = 0;
    int atlas_height = 0;
    int first_usable_texture = -1;
    std::vector<int> y_offsets(decoded.size(), 0);
    for (size_t texture_idx = 0; texture_idx < decoded.size(); ++texture_idx) {
        if (decoded[texture_idx].empty())
            continue;
        if (first_usable_texture < 0)
            first_usable_texture = static_cast<int>(texture_idx);
        y_offsets[texture_idx] = atlas_height;
        atlas_width = std::max(atlas_width, decoded[texture_idx].cols);
        atlas_height += decoded[texture_idx].rows;
    }

    std::map<int, int>                  solid_material_rows;
    std::map<int, std::array<float, 4>> solid_material_colors;
    for (size_t face_idx = 0; face_idx < face_count; ++face_idx) {
        const int material_idx = face_idx < textured.material_ids.size() ? textured.material_ids[face_idx] : -1;
        if (material_idx < 0 || resolve_texture(material_idx) >= 0
            || static_cast<size_t>(material_idx) >= textured.material_colors.size()
            || solid_material_rows.find(material_idx) != solid_material_rows.end()) {
            continue;
        }
        solid_material_rows[material_idx] = atlas_height++;
        solid_material_colors[material_idx] = textured.material_colors[material_idx];
    }

    if (atlas_width == 0 && solid_material_rows.empty())
        return false;
    atlas_width = std::max(atlas_width, 1);

    atlas = cv::Mat::zeros(atlas_height, atlas_width, CV_8UC3);
    for (size_t texture_idx = 0; texture_idx < decoded.size(); ++texture_idx) {
        if (decoded[texture_idx].empty())
            continue;
        cv::Mat destination = atlas(cv::Rect(0, y_offsets[texture_idx], decoded[texture_idx].cols, decoded[texture_idx].rows));
        decoded[texture_idx].copyTo(destination);
    }
    for (const auto &item : solid_material_colors) {
        const auto &color = item.second;
        atlas.at<cv::Vec3b>(solid_material_rows[item.first], 0) = cv::Vec3b(
            static_cast<uchar>(std::clamp(color[2] * 255.0f, 0.0f, 255.0f)),
            static_cast<uchar>(std::clamp(color[1] * 255.0f, 0.0f, 255.0f)),
            static_cast<uchar>(std::clamp(color[0] * 255.0f, 0.0f, 255.0f)));
    }

    atlas_uv_coords.resize(face_count);
    for (size_t face_idx = 0; face_idx < face_count; ++face_idx) {
        const int material_idx = face_idx < textured.material_ids.size() ? textured.material_ids[face_idx] : -1;
        const int texture_idx = resolve_texture(material_idx);

        int  y_offset = 0;
        int  texture_height = atlas_height;
        int  texture_width = atlas_width;
        bool use_solid_color = false;
        if (texture_idx >= 0) {
            y_offset = y_offsets[texture_idx];
            texture_height = decoded[texture_idx].rows;
            texture_width = decoded[texture_idx].cols;
        } else if (material_idx >= 0 && solid_material_rows.count(material_idx) > 0) {
            y_offset = solid_material_rows[material_idx];
            texture_height = 1;
            texture_width = 1;
            use_solid_color = true;
        } else if (first_usable_texture >= 0) {
            y_offset = y_offsets[first_usable_texture];
            texture_height = decoded[first_usable_texture].rows;
            texture_width = decoded[first_usable_texture].cols;
        }

        atlas_uv_coords[face_idx].resize(3);
        for (size_t corner = 0; corner < 3; ++corner) {
            float u = 0.0f;
            float v = 0.0f;
            if (textured.has_face_uvs()) {
                const int uv_idx = textured.uv_indices[face_idx][corner];
                if (uv_idx >= 0 && static_cast<size_t>(uv_idx) < textured.uv_coords.size()) {
                    u = textured.uv_coords[uv_idx][0];
                    v = textured.uv_coords[uv_idx][1];
                }
            } else {
                const int vertex_idx = textured.indices[face_idx][corner];
                if (vertex_idx >= 0 && static_cast<size_t>(vertex_idx) < textured.uvs.size()) {
                    u = textured.uvs[vertex_idx][0];
                    v = textured.uvs[vertex_idx][1];
                }
            }

            if (use_solid_color) {
                atlas_uv_coords[face_idx][corner] = Vec2f(
                    0.5f / static_cast<float>(atlas_width),
                    (y_offset + 0.5f) / static_cast<float>(atlas_height));
            } else {
                u -= std::floor(u);
                v -= std::floor(v);
                atlas_uv_coords[face_idx][corner] = Vec2f(
                    u * texture_width / static_cast<float>(atlas_width),
                    (y_offset + v * texture_height) / static_cast<float>(atlas_height));
            }
        }
    }
    return true;
}

bool needs_texture_atlas(const TexturedMesh &textured)
{
    if (textured.material_texture_map.empty())
        return false;
    if (textured.textures.size() > 1)
        return true;
    return std::any_of(textured.material_texture_map.begin(), textured.material_texture_map.end(),
        [](int texture_idx) { return texture_idx < 0; });
}

} // namespace

bool texture_to_painting(const TexturedMesh &textured, PaintedMesh &painted,
    const TexturePaintingSettings &settings, PaintProgressCallback progress, PaintCancelCallback cancel)
{
    if (textured.vertices.empty() || textured.indices.empty())
        return false;

    cv::Mat texture;
    tex2color::TriMesh input_mesh;
    std::vector<std::vector<Vec2f>> uv_coords;

    if (needs_texture_atlas(textured)) {
        if (!build_multi_texture_atlas(textured, texture, uv_coords))
            return false;

        input_mesh.vertices.resize(textured.vertices.size());
        for (size_t vertex_idx = 0; vertex_idx < textured.vertices.size(); ++vertex_idx) {
            const auto &vertex = textured.vertices[vertex_idx];
            input_mesh.vertices[vertex_idx] = Vec3f(vertex[0], vertex[1], vertex[2]);
        }
        input_mesh.indices.resize(textured.indices.size());
        for (size_t face_idx = 0; face_idx < textured.indices.size(); ++face_idx) {
            const auto &face = textured.indices[face_idx];
            input_mesh.indices[face_idx] = tex2color::TriFace(face[0], face[1], face[2]);
        }
    } else {
        if (textured.textures.empty())
            return false;
        texture = decode_texture_image(textured.textures.front());
        if (texture.empty())
            return false;
        build_tex2color_mesh(textured, input_mesh, uv_coords);
    }

    tex2color::TextureToColorSettings algorithm_settings;
    algorithm_settings.target_colors_num = settings.target_colors_num;
    algorithm_settings.smooth_weight = settings.smooth_weight;
    algorithm_settings.oversampling_iters = settings.oversampling_iters;
    algorithm_settings.oversampling_min_face_count = settings.oversampling_min_face_count;
    algorithm_settings.oversampling_max_face_count = settings.oversampling_max_face_count;
    algorithm_settings.max_color_distance = settings.max_color_distance;
    algorithm_settings.max_cluster_k = settings.max_cluster_k;
    algorithm_settings.mesh_repair_decision_required = settings.mesh_repair_decision_required;
    algorithm_settings.mesh_repair_callback = settings.mesh_repair_callback;

    switch (settings.mesh_repair_decision) {
    case TexturePaintingSettings::MeshRepairDecision::Ask:
        algorithm_settings.mesh_repair_decision = tex2color::MeshRepairDecision::Ask;
        break;
    case TexturePaintingSettings::MeshRepairDecision::RepairAndImport:
        algorithm_settings.mesh_repair_decision = tex2color::MeshRepairDecision::RepairAndImport;
        if (!algorithm_settings.mesh_repair_callback)
            algorithm_settings.mesh_repair_callback = repair_with_texture_to_color_core;
        break;
    case TexturePaintingSettings::MeshRepairDecision::ImportWithoutRepair:
    default:
        algorithm_settings.mesh_repair_decision = tex2color::MeshRepairDecision::ImportWithoutRepair;
        break;
    }

    tex2color::AlgoProgressCallback algorithm_progress;
    if (progress) {
        algorithm_progress = [&progress](tex2color::AlgoProgress state) {
            progress(state.percent, state.message);
        };
    }

    tex2color::AlgoCancelCallback algorithm_cancel;
    if (cancel)
        algorithm_cancel = [&cancel]() { return cancel(); };

    tex2color::TriMesh color_mesh;
    std::vector<std::array<std::size_t, 3>> face_colors;
    if (!tex2color::TextureToColor(input_mesh, uv_coords, texture, color_mesh, face_colors,
            algorithm_settings, algorithm_progress, algorithm_cancel)) {
        return false;
    }

    extract_painted_mesh(color_mesh, face_colors, painted);
    return true;
}

} // namespace Slic3r
