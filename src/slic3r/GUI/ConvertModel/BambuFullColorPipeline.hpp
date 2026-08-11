#ifndef slic3r_GUI_BambuFullColorPipeline_hpp_
#define slic3r_GUI_BambuFullColorPipeline_hpp_

#include "FullColorQuantization.hpp"

namespace Slic3r { namespace GUI {

class BambuFullColorPipeline final : public IFullColorQuantizationPipeline
{
public:
    FullColorQuantizationBackend backend() const noexcept override;

    QuantizationResult quantize(std::shared_ptr<const FullColorModelSource> source,
        const QuantizationSettings &settings) override;

    bool finalize(const QuantizationResult &result, const cvt_colors_t &target_colors,
        out_model_data_t &output, std::string *error_message = nullptr) override;
};

}} // namespace Slic3r::GUI

#endif
