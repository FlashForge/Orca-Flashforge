#include "ClassicFullColorPipeline.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <iterator>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <boost/log/trivial.hpp>

#include "ConvertModelProc.hpp"

namespace Slic3r { namespace GUI {

namespace {

bool set_error(std::string *error_message, const std::string &message)
{
    if (error_message != nullptr)
        *error_message = message;
    return false;
}

bool canonicalize_preview_palette(out_model_data_t &preview, const cvt_colors_t &palette,
    std::string *error_message)
{
    std::vector<int32_t> color_index_remap(preview.colors.size(), -1);
    for (size_t preview_color_idx = 0; preview_color_idx < preview.colors.size(); ++preview_color_idx) {
        const auto palette_it = std::find(palette.begin(), palette.end(), preview.colors[preview_color_idx]);
        if (palette_it == palette.end())
            return set_error(error_message, "Classic preview contains a color outside its quantization palette.");
        color_index_remap[preview_color_idx] = static_cast<int32_t>(std::distance(palette.begin(), palette_it));
    }

    for (out_triangle_data_t &triangle : preview.triangles) {
        if (triangle.colorIndex < 0 || static_cast<size_t>(triangle.colorIndex) >= color_index_remap.size())
            return set_error(error_message, "Classic preview contains an invalid face color index.");
        triangle.colorIndex = color_index_remap[triangle.colorIndex];
    }
    preview.colors = palette;
    return true;
}

class ClassicFullColorArtifact final : public FullColorQuantizationArtifact
{
public:
    FullColorQuantizationBackend backend() const noexcept override
    {
        return FullColorQuantizationBackend::Classic;
    }

    bool initialize(std::shared_ptr<const FullColorModelSource> source, std::string *error_message)
    {
        if (source == nullptr)
            return set_error(error_message, "Classic full-color pipeline received no model source.");
        if (!source->valid(error_message))
            return false;
        if (source->vertices.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())
            || source->texture_coords.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())
            || source->triangles.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            return set_error(error_message, "Classic full-color pipeline model is too large.");
        }

        m_source = std::move(source);
        if (!make_legacy_textures(error_message) || !make_legacy_material_ranges(error_message))
            return false;

        return true;
    }

    std::unique_ptr<ConvertModelProc> make_processor(const in_cvt_params_t &params, std::string *error_message) const
    {
        if (m_source == nullptr || m_material_ranges.empty()) {
            set_error(error_message, "Classic full-color artifact contains no conversion source.");
            return {};
        }

        in_model_data_t input{};
        input.vertices            = m_source->vertices.front().data();
        input.texCoords           = m_source->texture_coords.front().data();
        input.vertexIndecis       = m_source->triangles.front().data();
        input.texCoordIndices     = m_source->tex_coord_indices.front().data();
        input.materialDatas       = m_material_ranges.data();
        input.vertexCnt           = static_cast<int32_t>(m_source->vertices.size());
        input.texCoordCnt         = static_cast<int32_t>(m_source->texture_coords.size());
        input.triangleCnt         = static_cast<int32_t>(m_source->triangles.size());
        input.materialCnt         = static_cast<int32_t>(m_material_ranges.size());
        input.vertexStride        = static_cast<int32_t>(sizeof(FullColorModelSource::Vertex));
        input.texCoordStride      = static_cast<int32_t>(sizeof(FullColorModelSource::TextureCoord));
        input.vertexIndexStride   = static_cast<int32_t>(sizeof(int32_t));
        input.texCoordIndexStride = static_cast<int32_t>(sizeof(int32_t));
        input.faceIndexStride     = static_cast<int32_t>(sizeof(FullColorModelSource::Triangle));
        return std::make_unique<ConvertModelProc>(input, params);
    }

private:
    bool make_legacy_textures(std::string *error_message)
    {
        m_texture_views.clear();
        m_solid_color_pixels.clear();

        for (const FullColorMaterial &material : m_source->materials) {
            in_texture_data_t texture_view;
            if (material.texture_index >= 0) {
                if (static_cast<size_t>(material.texture_index) >= m_source->textures.size())
                    return set_error(error_message, "Classic full-color pipeline material has an invalid texture.");

                const FullColorTexture &texture = m_source->textures[material.texture_index];
                if (!texture.valid())
                    return set_error(error_message, "Classic full-color pipeline material has an invalid texture.");

                texture_view.bits          = texture.pixels.data();
                texture_view.width         = texture.width;
                texture_view.height        = texture.height;
                texture_view.channels      = texture.channels;
                texture_view.bytePerLine   = texture.bytes_per_line;
                texture_view.wraps[0]      = texture.wrap_u;
                texture_view.wraps[1]      = texture.wrap_v;
                texture_view.flipY         = texture.flip_y;
            } else if (material.has_base_color) {
                m_solid_color_pixels.push_back({ material.base_color[0], material.base_color[1], material.base_color[2] });
                texture_view.bits          = m_solid_color_pixels.back().data();
                texture_view.width         = 1;
                texture_view.height        = 1;
                texture_view.channels      = 3;
                texture_view.bytePerLine   = 3;
                texture_view.wraps[0]      = TEX_WRAP_REPEAT;
                texture_view.wraps[1]      = TEX_WRAP_REPEAT;
                texture_view.flipY         = false;
                texture_view.isSolidColor  = true;
                texture_view.solidColor    = material.base_color;
            } else {
                return set_error(error_message, "Classic full-color pipeline material has no texture or base color.");
            }
            m_texture_views.emplace_back(texture_view);
        }
        return true;
    }

    bool make_legacy_material_ranges(std::string *error_message)
    {
        m_material_ranges.clear();
        if (m_source->face_material_ids.empty())
            return set_error(error_message, "Classic full-color pipeline contains no face materials.");

        int32_t current_material = m_source->face_material_ids.front();
        if (!valid_material_index(current_material))
            return set_error(error_message, "Classic full-color pipeline contains an invalid face material.");

        for (size_t face_idx = 1; face_idx < m_source->face_material_ids.size(); ++face_idx) {
            const int32_t material_idx = m_source->face_material_ids[face_idx];
            if (!valid_material_index(material_idx))
                return set_error(error_message, "Classic full-color pipeline contains an invalid face material.");
            if (material_idx == current_material)
                continue;

            m_material_ranges.push_back({
                static_cast<int32_t>(face_idx),
                &m_texture_views[current_material]
            });
            current_material = material_idx;
        }
        m_material_ranges.push_back({
            static_cast<int32_t>(m_source->face_material_ids.size()),
            &m_texture_views[current_material]
        });
        return true;
    }

    bool valid_material_index(int32_t material_idx) const
    {
        return material_idx >= 0 && static_cast<size_t>(material_idx) < m_texture_views.size();
    }

private:
    std::shared_ptr<const FullColorModelSource> m_source;
    std::deque<std::array<uint8_t, 3>>          m_solid_color_pixels;
    std::deque<in_texture_data_t>               m_texture_views;
    std::vector<in_material_data_t>             m_material_ranges;
};

} // namespace

FullColorQuantizationBackend ClassicFullColorPipeline::backend() const noexcept
{
    return FullColorQuantizationBackend::Classic;
}

QuantizationResult ClassicFullColorPipeline::quantize(std::shared_ptr<const FullColorModelSource> source,
    const QuantizationSettings &settings)
{
    QuantizationResult result;
    result.backend  = backend();
    result.settings = settings;
    result.source   = std::move(source);

    if (result.source == nullptr) {
        result.error_message = "Classic full-color pipeline received no model source.";
        return result;
    }
    if (settings.color_count <= 0) {
        result.error_message = "Classic full-color pipeline requires a positive color count.";
        return result;
    }

    auto artifact = std::make_shared<ClassicFullColorArtifact>();
    if (!artifact->initialize(result.source, &result.error_message))
        return result;

    std::unique_ptr<ConvertModelProc> processor = artifact->make_processor(settings.conversion, &result.error_message);
    if (processor == nullptr)
        return result;

    result.source_palette = processor->clusterColors(settings.color_count);
    if (result.source_palette.empty()) {
        result.error_message = "Classic full-color pipeline produced no palette.";
        return result;
    }

    processor->makePreviewModel(result.quantized_preview, result.source_palette);
    if (result.quantized_preview.vertices.empty() || result.quantized_preview.triangles.empty()) {
        result.error_message = "Classic full-color pipeline produced no quantized preview.";
        return result;
    }
    if (!canonicalize_preview_palette(result.quantized_preview, result.source_palette, &result.error_message))
        return result;

    BOOST_LOG_TRIVIAL(info) << "FullColorQuantization: backend=Classic stage=quantize source_faces="
                            << result.source->triangles.size() << " quantized_faces="
                            << result.quantized_preview.triangles.size() << " colors="
                            << result.source_palette.size();

    result.artifact = std::move(artifact);
    return result;
}

bool ClassicFullColorPipeline::finalize(const QuantizationResult &result, const cvt_colors_t &target_colors,
    out_model_data_t &output, std::string *error_message)
{
    output.vertices.clear();
    output.colors.clear();
    output.triangles.clear();

    if (result.backend != backend())
        return set_error(error_message, "Classic full-color pipeline received a result from another backend.");
    if (result.source_palette.empty())
        return set_error(error_message, "Classic full-color pipeline result contains no source palette.");

    const auto artifact = result.artifact_as<ClassicFullColorArtifact>();
    if (artifact == nullptr)
        return set_error(error_message, "Classic full-color pipeline result contains no conversion state.");

    std::unique_ptr<ConvertModelProc> processor = artifact->make_processor(result.settings.conversion, error_message);
    if (processor == nullptr)
        return false;
    processor->doConvertMapped(result.source_palette, target_colors, output);
    if (output.vertices.empty() || output.triangles.empty())
        return set_error(error_message, "Classic full-color pipeline final conversion produced no model.");

    BOOST_LOG_TRIVIAL(info) << "FullColorQuantization: backend=Classic stage=finalize source_faces="
                            << result.source->triangles.size() << " finalized_faces="
                            << output.triangles.size() << " colors=" << output.colors.size();
    return true;
}

}} // namespace Slic3r::GUI
