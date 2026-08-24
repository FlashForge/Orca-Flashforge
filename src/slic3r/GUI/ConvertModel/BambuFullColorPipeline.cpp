#include "BambuFullColorPipeline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include <boost/log/trivial.hpp>

#include "libslic3r/BambuTexturePainting.hpp"

namespace Slic3r { namespace GUI {

namespace {

bool set_error(std::string *error_message, const std::string &message)
{
    if (error_message != nullptr)
        *error_message = message;
    return false;
}

class BambuFullColorArtifact final : public FullColorQuantizationArtifact
{
public:
    FullColorQuantizationBackend backend() const noexcept override
    {
        return FullColorQuantizationBackend::Bambu;
    }

    PaintedMesh painted_mesh;
};

bool copy_texture(const FullColorTexture &source, TextureImage &target, std::string *error_message)
{
    if (!source.valid())
        return set_error(error_message, "Bambu full-color pipeline received an invalid texture.");

    const int target_channels = source.channels >= 4 ? 4 : 3;
    const size_t width = static_cast<size_t>(source.width);
    const size_t height = static_cast<size_t>(source.height);
    if (width > std::numeric_limits<size_t>::max() / height
        || width * height > std::numeric_limits<size_t>::max() / static_cast<size_t>(target_channels)) {
        return set_error(error_message, "Bambu full-color pipeline texture is too large.");
    }

    target.width = source.width;
    target.height = source.height;
    target.channels = target_channels;
    target.data.resize(width * height * static_cast<size_t>(target_channels));

    for (size_t row = 0; row < height; ++row) {
        const uint8_t *source_row = source.pixels.data() + row * static_cast<size_t>(source.bytes_per_line);
        uint8_t *target_row = target.data.data() + row * width * static_cast<size_t>(target_channels);
        for (size_t column = 0; column < width; ++column) {
            const uint8_t *source_pixel = source_row + column * static_cast<size_t>(source.channels);
            uint8_t *target_pixel = target_row + column * static_cast<size_t>(target_channels);
            for (int channel = 0; channel < target_channels; ++channel)
                target_pixel[channel] = source_pixel[channel];
        }
    }
    return true;
}

bool build_textured_mesh(const FullColorModelSource &source, TexturedMesh &textured, std::string *error_message)
{
    if (!source.valid(error_message))
        return false;
    if (source.vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max())
        || source.triangles.size() > static_cast<size_t>(std::numeric_limits<int>::max()) / 3) {
        return set_error(error_message, "Bambu full-color pipeline model is too large.");
    }

    textured.vertices = source.vertices;
    textured.indices.resize(source.triangles.size());
    for (size_t face_idx = 0; face_idx < source.triangles.size(); ++face_idx) {
        textured.indices[face_idx] = {
            source.triangles[face_idx][0],
            source.triangles[face_idx][1],
            source.triangles[face_idx][2]
        };
    }

    textured.textures.resize(source.textures.size());
    for (size_t texture_idx = 0; texture_idx < source.textures.size(); ++texture_idx) {
        if (!copy_texture(source.textures[texture_idx], textured.textures[texture_idx], error_message))
            return false;
    }

    textured.material_texture_map.resize(source.materials.size(), -1);
    textured.material_colors.resize(source.materials.size(), { 1.0f, 1.0f, 1.0f, 1.0f });
    for (size_t material_idx = 0; material_idx < source.materials.size(); ++material_idx) {
        const FullColorMaterial &material = source.materials[material_idx];
        textured.material_texture_map[material_idx] = material.texture_index;
        if (material.has_base_color) {
            for (size_t channel = 0; channel < 4; ++channel)
                textured.material_colors[material_idx][channel] = material.base_color[channel] / 255.0f;
        }
    }

    textured.material_ids.reserve(source.face_material_ids.size());
    for (int32_t material_idx : source.face_material_ids)
        textured.material_ids.emplace_back(material_idx);

    // Expand the UV pool per face. This preserves seams and lets each material
    // apply its own image-origin convention without mutating shared UV entries.
    textured.uv_coords.resize(source.triangles.size() * 3);
    textured.uv_indices.resize(source.triangles.size());
    for (size_t face_idx = 0; face_idx < source.triangles.size(); ++face_idx) {
        const int32_t material_idx = source.face_material_ids[face_idx];
        const FullColorMaterial &material = source.materials[material_idx];
        bool flip_y = false;
        if (material.texture_index >= 0)
            flip_y = source.textures[material.texture_index].flip_y;

        const size_t uv_base = face_idx * 3;
        for (size_t corner = 0; corner < 3; ++corner) {
            const int32_t source_uv_idx = source.tex_coord_indices[face_idx][corner];
            std::array<float, 2> uv = source.texture_coords[source_uv_idx];
            if (flip_y)
                uv[1] = 1.0f - uv[1];
            textured.uv_coords[uv_base + corner] = uv;
            textured.uv_indices[face_idx][corner] = static_cast<int>(uv_base + corner);
        }
    }

    return true;
}

bool painted_palette(const PaintedMesh &painted, cvt_colors_t &palette, std::string *error_message)
{
    palette.clear();
    palette.reserve(painted.cluster_colors.size());
    for (const auto &color : painted.cluster_colors) {
        if (color[0] > 255 || color[1] > 255 || color[2] > 255)
            return set_error(error_message, "Bambu full-color pipeline produced an invalid cluster color.");
        palette.push_back({
            static_cast<uint8_t>(color[0]),
            static_cast<uint8_t>(color[1]),
            static_cast<uint8_t>(color[2])
        });
    }
    return !palette.empty() || set_error(error_message, "Bambu full-color pipeline produced no cluster colors.");
}

void transform_output(out_model_data_t &output, const FullColorModelSource &source, const in_cvt_params_t &params)
{
    if (params.transCoordSys) {
        for (auto &vertex : output.vertices) {
            const float y = vertex[1];
            vertex[1] = -vertex[2];
            vertex[2] = y;
        }
    }
    if (!params.scaleModelSize || source.vertices.empty())
        return;

    std::array<float, 3> minimum = source.vertices.front();
    std::array<float, 3> maximum = source.vertices.front();
    for (const auto &vertex : source.vertices) {
        for (size_t axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], vertex[axis]);
            maximum[axis] = std::max(maximum[axis], vertex[axis]);
        }
    }
    const std::array<float, 3> model_size = {
        maximum[0] - minimum[0],
        maximum[1] - minimum[1],
        maximum[2] - minimum[2]
    };

    auto scale_model = [&output](float scale) {
        for (auto &vertex : output.vertices) {
            vertex[0] *= scale;
            vertex[1] *= scale;
            vertex[2] *= scale;
        }
    };

    float scale_x = params.maxPrintSize[0] / model_size[0];
    float scale_y = params.maxPrintSize[1] / model_size[1];
    float scale_z = params.maxPrintSize[2] / model_size[2];
    float scale = std::min({ scale_x, scale_y, scale_z });
    if (scale < 1.0f) {
        scale_model(scale);
    } else if (!std::isinf(scale)) {
        const float extent = std::min({
            params.minModelExtent,
            params.maxPrintSize[0],
            params.maxPrintSize[1],
            params.maxPrintSize[2]
        });
        scale_x = extent / model_size[0];
        scale_y = extent / model_size[1];
        scale_z = extent / model_size[2];
        scale = std::min({ scale_x, scale_y, scale_z });
        if (scale > 1.0f)
            scale_model(scale);
    }
}

bool painted_to_output(const PaintedMesh &painted, const cvt_colors_t &source_palette,
    const cvt_colors_t &output_palette, const FullColorModelSource &source, const in_cvt_params_t &params,
    out_model_data_t &output, std::string *error_message)
{
    output.vertices = painted.vertices;
    output.colors = output_palette;
    output.triangles.clear();

    if (painted.indices.size() != painted.face_colors.size())
        return set_error(error_message, "Bambu PaintedMesh contains incomplete face colors.");
    const bool collapse_to_single_color = output_palette.size() == 1;
    if (!collapse_to_single_color && source_palette.size() != output_palette.size())
        return set_error(error_message, "Bambu color mapping size does not match the source palette.");

    std::map<cvt_color_t, int32_t> color_indices;
    if (!collapse_to_single_color) {
        for (size_t color_idx = 0; color_idx < source_palette.size(); ++color_idx)
            color_indices.emplace(source_palette[color_idx], static_cast<int32_t>(color_idx));
    }

    output.triangles.resize(painted.indices.size());
    for (size_t face_idx = 0; face_idx < painted.indices.size(); ++face_idx) {
        const auto &painted_color = painted.face_colors[face_idx];
        if (painted_color[0] > 255 || painted_color[1] > 255 || painted_color[2] > 255)
            return set_error(error_message, "Bambu PaintedMesh contains an invalid face color.");
        const cvt_color_t face_color = {
            static_cast<uint8_t>(painted_color[0]),
            static_cast<uint8_t>(painted_color[1]),
            static_cast<uint8_t>(painted_color[2])
        };
        int32_t output_color_idx = 0;
        if (!collapse_to_single_color) {
            const auto color_it = color_indices.find(face_color);
            if (color_it == color_indices.end())
                return set_error(error_message, "Bambu PaintedMesh face color is missing from its palette.");
            output_color_idx = color_it->second;
        }

        const auto &face = painted.indices[face_idx];
        out_triangle_data_t &output_face = output.triangles[face_idx];
        output_face.vertexIndices[0] = face[0];
        output_face.vertexIndices[1] = face[1];
        output_face.vertexIndices[2] = face[2];
        output_face.colorIndex = output_color_idx;
    }

    transform_output(output, source, params);
    return !output.vertices.empty() && !output.triangles.empty();
}

} // namespace

FullColorQuantizationBackend BambuFullColorPipeline::backend() const noexcept
{
    return FullColorQuantizationBackend::Bambu;
}

QuantizationResult BambuFullColorPipeline::quantize(std::shared_ptr<const FullColorModelSource> source,
    const QuantizationSettings &settings)
{
    QuantizationResult result;
    result.backend = backend();
    result.settings = settings;
    result.source = std::move(source);

    if (result.source == nullptr) {
        result.error_message = "Bambu full-color pipeline received no model source.";
        return result;
    }
    if (settings.color_count < 0) {
        result.error_message = "Bambu full-color pipeline received an invalid color count.";
        return result;
    }
    if (settings.smooth_weight < 0.0f || settings.smooth_weight > 1.0f) {
        result.error_message = "Bambu full-color pipeline smooth weight must be between zero and one.";
        return result;
    }
    if (settings.min_oversampled_face_count == 0 || settings.max_oversampled_face_count == 0
        || settings.min_oversampled_face_count > settings.max_oversampled_face_count) {
        result.error_message = "Bambu full-color pipeline received invalid oversampling limits.";
        return result;
    }

    TexturedMesh textured;
    if (!build_textured_mesh(*result.source, textured, &result.error_message))
        return result;

    TexturePaintingSettings painting_settings;
    painting_settings.target_colors_num = static_cast<size_t>(settings.color_count);
    painting_settings.smooth_weight = settings.smooth_weight;
    painting_settings.oversampling_min_face_count = settings.min_oversampled_face_count;
    painting_settings.oversampling_max_face_count = settings.max_oversampled_face_count;
    if (settings.repair_mesh) {
        painting_settings.mesh_repair_decision = TexturePaintingSettings::MeshRepairDecision::RepairAndImport;
    }

    auto artifact = std::make_shared<BambuFullColorArtifact>();
    if (!texture_to_painting(textured, artifact->painted_mesh, painting_settings)) {
        result.error_message = "Bambu texture-to-color conversion failed.";
        return result;
    }
    if (!painted_palette(artifact->painted_mesh, result.source_palette, &result.error_message))
        return result;
    if (!painted_to_output(artifact->painted_mesh, result.source_palette, result.source_palette,
            *result.source, settings.conversion, result.quantized_preview, &result.error_message)) {
        return result;
    }

    BOOST_LOG_TRIVIAL(info) << "FullColorQuantization: backend=Bambu stage=quantize source_faces="
                            << result.source->triangles.size() << " quantized_faces="
                            << result.quantized_preview.triangles.size() << " colors="
                            << result.source_palette.size();

    result.artifact = std::move(artifact);
    return result;
}

bool BambuFullColorPipeline::finalize(const QuantizationResult &result, const cvt_colors_t &target_colors,
    out_model_data_t &output, std::string *error_message)
{
    output.vertices.clear();
    output.colors.clear();
    output.triangles.clear();

    if (result.backend != backend())
        return set_error(error_message, "Bambu full-color pipeline received a result from another backend.");
    if (result.source == nullptr)
        return set_error(error_message, "Bambu full-color pipeline result contains no model source.");

    const auto artifact = result.artifact_as<BambuFullColorArtifact>();
    if (artifact == nullptr)
        return set_error(error_message, "Bambu full-color pipeline result contains no PaintedMesh.");

    const cvt_colors_t &mapped_palette = target_colors.empty() ? result.source_palette : target_colors;
    if (!painted_to_output(artifact->painted_mesh, result.source_palette, mapped_palette,
            *result.source, result.settings.conversion, output, error_message)) {
        return false;
    }

    BOOST_LOG_TRIVIAL(info) << "FullColorQuantization: backend=Bambu stage=finalize source_faces="
                            << result.source->triangles.size() << " finalized_faces="
                            << output.triangles.size() << " colors=" << output.colors.size();
    return true;
}

}} // namespace Slic3r::GUI
