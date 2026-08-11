#ifndef slic3r_GUI_FullColorQuantization_hpp_
#define slic3r_GUI_FullColorQuantization_hpp_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "ConvertModelDef.hpp"
#include "FullColorModelSource.hpp"

namespace Slic3r { namespace GUI {

enum class FullColorQuantizationBackend : uint8_t
{
    Classic,
    Bambu
};

struct QuantizationSettings
{
    // Zero allows a backend to choose the color count automatically.
    int32_t color_count{ 0 };

    // Primarily used by the Bambu backend. Kept here so cache keys and UI
    // settings can be shared by both pipelines.
    float  smooth_weight{ 0.5f };
    size_t min_oversampled_face_count{ 10000 };
    size_t max_oversampled_face_count{ 1000000 };
    bool   repair_mesh{ false };

    // The classic backend consumes these values without changing its existing
    // conversion behavior. Common output transforms may also reuse them.
    in_cvt_params_t conversion;
};

class BackendArtifact
{
public:
    virtual ~BackendArtifact() = default;

    virtual FullColorQuantizationBackend backend() const noexcept = 0;
};

using FullColorQuantizationArtifact = BackendArtifact;

struct QuantizationResult
{
    FullColorQuantizationBackend backend{ FullColorQuantizationBackend::Classic };
    QuantizationSettings         settings;

    // Keep the source alive while this result is cached or used for finalization.
    std::shared_ptr<const FullColorModelSource> source;

    // Palette produced by the selected backend before mapping to real filaments.
    // quantized_preview must use this exact color-table order; every face colorIndex
    // therefore remains stable when source_palette is mapped to filament colors.
    cvt_colors_t     source_palette;
    out_model_data_t quantized_preview;

    // Backend-specific state. The classic backend may retain conversion state;
    // the Bambu backend will retain its PaintedMesh here.
    std::shared_ptr<BackendArtifact> artifact;

    std::string error_message;

    bool valid() const
    {
        return source != nullptr && error_message.empty() && !source_palette.empty()
            && quantized_preview.colors == source_palette
            && !quantized_preview.vertices.empty() && !quantized_preview.triangles.empty()
            && artifact != nullptr && artifact->backend() == backend;
    }

    const cvt_colors_t &palette() const { return source_palette; }

    template<class ArtifactType>
    std::shared_ptr<ArtifactType> artifact_as() const
    {
        if (artifact == nullptr || artifact->backend() != backend)
            return {};
        return std::dynamic_pointer_cast<ArtifactType>(artifact);
    }
};

class IFullColorQuantizationPipeline
{
public:
    virtual ~IFullColorQuantizationPipeline() = default;

    virtual FullColorQuantizationBackend backend() const noexcept = 0;

    virtual QuantizationResult quantize(std::shared_ptr<const FullColorModelSource> source,
        const QuantizationSettings &settings) = 0;

    // target_colors contains the filament colors selected by the user. A Bambu
    // implementation should reuse the PaintedMesh stored in result.artifact.
    virtual bool finalize(const QuantizationResult &result, const cvt_colors_t &target_colors,
        out_model_data_t &output, std::string *error_message = nullptr) = 0;
};

}} // namespace Slic3r::GUI

#endif
