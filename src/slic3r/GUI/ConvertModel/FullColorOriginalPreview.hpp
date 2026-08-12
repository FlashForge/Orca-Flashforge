#ifndef slic3r_GUI_FullColorOriginalPreview_hpp_
#define slic3r_GUI_FullColorOriginalPreview_hpp_

#include <memory>
#include <string>
#include <vector>

#include "FullColorModelSource.hpp"

namespace Slic3r { namespace GUI {

// Backend-independent view of the imported textured model. Geometry transformed
// for display is stored separately while UVs, materials and texture pixels stay
// in the immutable FullColorModelSource shared by both quantization backends.
struct FullColorOriginalPreview
{
    std::shared_ptr<const FullColorModelSource> source;
    std::vector<FullColorModelSource::Vertex>   vertices;

    bool valid(std::string *error_message = nullptr) const;
};

bool make_full_color_original_preview(std::shared_ptr<const FullColorModelSource> source,
    const in_cvt_params_t &params, FullColorOriginalPreview &preview, std::string *error_message = nullptr);

}} // namespace Slic3r::GUI

#endif
