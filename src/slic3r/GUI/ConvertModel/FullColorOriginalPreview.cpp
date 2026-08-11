#include "FullColorOriginalPreview.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Slic3r { namespace GUI {

namespace {

bool set_error(std::string *error_message, const char *message)
{
    if (error_message != nullptr)
        *error_message = message;
    return false;
}

std::array<float, 3> model_size(const FullColorModelSource &source)
{
    std::array<float, 3> minimum = source.vertices.front();
    std::array<float, 3> maximum = source.vertices.front();
    for (const FullColorModelSource::Vertex &vertex : source.vertices) {
        for (size_t axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], vertex[axis]);
            maximum[axis] = std::max(maximum[axis], vertex[axis]);
        }
    }
    return {
        maximum[0] - minimum[0],
        maximum[1] - minimum[1],
        maximum[2] - minimum[2]
    };
}

void transform_vertices(std::vector<FullColorModelSource::Vertex> &vertices,
    const FullColorModelSource &source, const in_cvt_params_t &params)
{
    if (params.transCoordSys) {
        for (FullColorModelSource::Vertex &vertex : vertices) {
            const float y = vertex[1];
            vertex[1] = -vertex[2];
            vertex[2] = y;
        }
    }
    if (!params.scaleModelSize)
        return;

    const std::array<float, 3> size = model_size(source);
    auto scale_vertices = [&vertices](float scale) {
        for (FullColorModelSource::Vertex &vertex : vertices) {
            vertex[0] *= scale;
            vertex[1] *= scale;
            vertex[2] *= scale;
        }
    };

    float scale_x = params.maxPrintSize[0] / size[0];
    float scale_y = params.maxPrintSize[1] / size[1];
    float scale_z = params.maxPrintSize[2] / size[2];
    float scale   = std::min({ scale_x, scale_y, scale_z });
    if (scale < 1.0f) {
        scale_vertices(scale);
    } else if (!std::isinf(scale)) {
        const float extent = std::min({
            params.minModelExtent,
            params.maxPrintSize[0],
            params.maxPrintSize[1],
            params.maxPrintSize[2]
        });
        scale_x = extent / size[0];
        scale_y = extent / size[1];
        scale_z = extent / size[2];
        scale   = std::min({ scale_x, scale_y, scale_z });
        if (scale > 1.0f)
            scale_vertices(scale);
    }
}

} // namespace

bool FullColorOriginalPreview::valid(std::string *error_message) const
{
    if (source == nullptr)
        return set_error(error_message, "Original texture preview contains no model source.");
    if (source->empty())
        return set_error(error_message, "Original texture preview contains no geometry.");
    if (vertices.size() != source->vertices.size())
        return set_error(error_message, "Original texture preview contains incomplete geometry.");
    return true;
}

bool make_full_color_original_preview(std::shared_ptr<const FullColorModelSource> source,
    const in_cvt_params_t &params, FullColorOriginalPreview &preview, std::string *error_message)
{
    FullColorOriginalPreview output;
    if (source == nullptr)
        return set_error(error_message, "Cannot build an original texture preview without a model source.");
    if (!source->valid(error_message))
        return false;

    output.source   = std::move(source);
    output.vertices = output.source->vertices;
    transform_vertices(output.vertices, *output.source, params);
    preview = std::move(output);
    return true;
}

}} // namespace Slic3r::GUI
