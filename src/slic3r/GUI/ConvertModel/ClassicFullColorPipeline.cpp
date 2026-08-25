#include "ClassicFullColorPipeline.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <boost/log/trivial.hpp>

#include "BambuFullColorPipeline.hpp"
#include "ConvertModelProc.hpp"

namespace Slic3r { namespace GUI {

namespace {

bool set_error(std::string *error_message, const std::string &message)
{
    if (error_message != nullptr)
        *error_message = message;
    return false;
}

int64_t color_distance_squared(const cvt_color_t &left, const cvt_color_t &right)
{
    int64_t distance = 0;
    for (size_t channel = 0; channel < left.size(); ++channel) {
        const int64_t difference = static_cast<int64_t>(left[channel]) - right[channel];
        distance += difference * difference;
    }
    return distance;
}

bool match_palettes(const cvt_colors_t &bambu_palette, const cvt_colors_t &classic_palette,
    std::vector<int32_t> &bambu_to_classic, std::string *error_message)
{
    if (bambu_palette.empty() || bambu_palette.size() != classic_palette.size())
        return set_error(error_message, "Classic and Bambu quantization produced incompatible palette sizes.");

    const size_t color_count = bambu_palette.size();
    if (color_count > 20)
        return set_error(error_message, "Classic palette is too large to match safely.");

    const size_t state_count = size_t(1) << color_count;
    const int64_t infinity = std::numeric_limits<int64_t>::max();
    std::vector<int64_t> minimum_cost(state_count, infinity);
    std::vector<int32_t> selected_classic_index(state_count, -1);
    minimum_cost[0] = 0;

    for (size_t mask = 0; mask + 1 < state_count; ++mask) {
        if (minimum_cost[mask] == infinity)
            continue;
        size_t bambu_index = 0;
        for (size_t bits = mask; bits != 0; bits &= bits - 1)
            ++bambu_index;
        for (size_t classic_index = 0; classic_index < color_count; ++classic_index) {
            const size_t classic_bit = size_t(1) << classic_index;
            if ((mask & classic_bit) != 0)
                continue;
            const size_t next_mask = mask | classic_bit;
            const int64_t candidate = minimum_cost[mask] +
                color_distance_squared(bambu_palette[bambu_index], classic_palette[classic_index]);
            if (candidate < minimum_cost[next_mask]) {
                minimum_cost[next_mask] = candidate;
                selected_classic_index[next_mask] = static_cast<int32_t>(classic_index);
            }
        }
    }

    bambu_to_classic.resize(color_count);
    size_t mask = state_count - 1;
    for (size_t bambu_count = color_count; bambu_count > 0; --bambu_count) {
        const int32_t classic_index = selected_classic_index[mask];
        if (classic_index < 0)
            return set_error(error_message, "Classic and Bambu palettes could not be matched.");
        bambu_to_classic[bambu_count - 1] = classic_index;
        mask ^= size_t(1) << classic_index;
    }
    return true;
}

cvt_colors_t reorder_classic_palette_for_bambu(const cvt_colors_t &classic_palette,
    const std::vector<int32_t> &bambu_to_classic)
{
    cvt_colors_t reordered;
    reordered.reserve(bambu_to_classic.size());
    for (int32_t classic_index : bambu_to_classic)
        reordered.push_back(classic_palette[classic_index]);
    return reordered;
}

bool restore_classic_palette_order(out_model_data_t &model, const std::vector<int32_t> &bambu_to_classic,
    const cvt_colors_t &classic_palette, std::string *error_message)
{
    if (classic_palette.size() == 1) {
        for (out_triangle_data_t &triangle : model.triangles)
            triangle.colorIndex = 0;
        model.colors = classic_palette;
        return true;
    }
    if (bambu_to_classic.size() != classic_palette.size())
        return set_error(error_message, "Classic palette mapping has an invalid size.");

    for (out_triangle_data_t &triangle : model.triangles) {
        if (triangle.colorIndex < 0 || static_cast<size_t>(triangle.colorIndex) >= bambu_to_classic.size())
            return set_error(error_message, "Bambu geometry contains an invalid face color index.");
        triangle.colorIndex = bambu_to_classic[triangle.colorIndex];
    }
    model.colors = classic_palette;
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

    void set_bambu_geometry(QuantizationResult result, std::vector<int32_t> bambu_to_classic)
    {
        m_bambu_result = std::move(result);
        m_bambu_to_classic = std::move(bambu_to_classic);
    }

    const QuantizationResult &bambu_result() const { return m_bambu_result; }
    const std::vector<int32_t> &bambu_to_classic() const { return m_bambu_to_classic; }

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
    QuantizationResult                          m_bambu_result;
    std::vector<int32_t>                        m_bambu_to_classic;
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

    QuantizationSettings bambu_settings = settings;
    bambu_settings.color_count = static_cast<int32_t>(result.source_palette.size());
    bambu_settings.repair_mesh = false;

    // Keep Classic color clustering, but use Bambu's PaintedMesh for import geometry.
    // The former Classic output path recursively subdivided textured faces.
    BambuFullColorPipeline bambu_pipeline;
    QuantizationResult bambu_result = bambu_pipeline.quantize(result.source, bambu_settings);
    if (!bambu_result.valid()) {
        result.error_message = bambu_result.error_message.empty() ?
            "Classic full-color pipeline could not generate Bambu import geometry." : bambu_result.error_message;
        return result;
    }

    std::vector<int32_t> bambu_to_classic;
    if (!match_palettes(bambu_result.source_palette, result.source_palette, bambu_to_classic, &result.error_message))
        return result;

    // Bambu finalization does not consume its preview. Move it to avoid retaining
    // a second copy of the generated mesh in the nested result.
    result.quantized_preview = std::move(bambu_result.quantized_preview);
    if (!restore_classic_palette_order(
            result.quantized_preview, bambu_to_classic, result.source_palette, &result.error_message)) {
        return result;
    }

    artifact->set_bambu_geometry(std::move(bambu_result), std::move(bambu_to_classic));

    BOOST_LOG_TRIVIAL(info) << "FullColorQuantization: backend=Classic stage=quantize source_faces="
                            << result.source->triangles.size() << " quantized_faces="
                            << result.quantized_preview.triangles.size() << " colors="
                            << result.source_palette.size() << " geometry_backend=Bambu";

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

    const QuantizationResult &bambu_result = artifact->bambu_result();
    const std::vector<int32_t> &bambu_to_classic = artifact->bambu_to_classic();
    if (bambu_result.backend != FullColorQuantizationBackend::Bambu || bambu_result.source == nullptr ||
        bambu_result.source_palette.empty() || bambu_result.artifact == nullptr ||
        bambu_result.artifact->backend() != FullColorQuantizationBackend::Bambu) {
        return set_error(error_message, "Classic full-color pipeline contains no Bambu import geometry.");
    }

    const cvt_colors_t &classic_output_palette = target_colors.empty() ? result.source_palette : target_colors;
    if (classic_output_palette.size() != 1 && classic_output_palette.size() != result.source_palette.size())
        return set_error(error_message, "Classic target palette has an invalid size.");

    const cvt_colors_t bambu_output_palette = classic_output_palette.size() == 1 ?
        classic_output_palette : reorder_classic_palette_for_bambu(classic_output_palette, bambu_to_classic);
    BambuFullColorPipeline bambu_pipeline;
    if (!bambu_pipeline.finalize(bambu_result, bambu_output_palette, output, error_message))
        return false;
    if (!restore_classic_palette_order(output, bambu_to_classic, classic_output_palette, error_message))
        return false;
    if (output.triangles.size() != result.quantized_preview.triangles.size())
        return set_error(error_message, "Classic finalization changed the Bambu geometry face count.");

    BOOST_LOG_TRIVIAL(info) << "FullColorQuantization: backend=Classic stage=finalize source_faces="
                            << result.source->triangles.size() << " finalized_faces="
                            << output.triangles.size() << " colors=" << output.colors.size()
                            << " geometry_backend=Bambu";
    return true;
}

}} // namespace Slic3r::GUI
