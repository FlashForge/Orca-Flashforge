// MixedFilamentConfigPanel.cpp
// Extracted from FullSpectrum Plater.cpp:4848-6857.
// Contains:
//   4848-4942  (anonymous-namespace helpers used inside class statics)
//   4943-6042  (MixedFilamentConfigPanel static class methods)
//   6044-6857  (MixedFilamentConfigPanel constructor + instance methods)
//
// Functions not used by this class (build_mixed_filament_display_context,
// build_display_weighted_multi_sequence, blend_display_color_from_sequence,
// compute_color_match_recipe_display_color) live in MixedFilamentColorMatchDialog.cpp.

#include "MixedFilamentConfigPanel.hpp"
#include "MixedMixPreview.hpp"
#include "MixedGradientSelector.hpp"
#include "MixedGradientWeightsDialog.hpp"
#include "GUI_App.hpp"            // wxGetApp()
#include "I18N.hpp"               // _L()
#include "NotificationManager.hpp"
#include "Plater.hpp"
#include "format.hpp"             // from_u8 / into_u8

#include "libslic3r/MixedFilament.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/libslic3r.h"   // EPSILON

#include <boost/algorithm/string.hpp>

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/menu.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Slic3r { namespace GUI {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers
// (verbatim from FullSpectrum Plater.cpp:4848-4942 and 5126-5228)
// ---------------------------------------------------------------------------
namespace {

std::string normalized_editor_filament_type(std::string filament_type)
{
    boost::algorithm::trim(filament_type);
    boost::algorithm::to_lower(filament_type);
    return filament_type;
}

std::string normalized_editor_filament_identity(std::string filament_identity)
{
    boost::algorithm::trim(filament_identity);
    const size_t printer_suffix = filament_identity.find('@');
    if (printer_suffix != std::string::npos) {
        filament_identity = filament_identity.substr(0, printer_suffix);
        boost::algorithm::trim(filament_identity);
    }
    boost::algorithm::to_lower(filament_identity);
    return filament_identity;
}

std::vector<std::string> current_editor_physical_filament_identities(size_t num_physical)
{
    std::vector<std::string> identities(num_physical);
    PresetBundle *preset_bundle = wxGetApp().preset_bundle;
    if (preset_bundle == nullptr || num_physical == 0)
        return identities;

    const auto *project_vendor_opt = preset_bundle->project_config.option<ConfigOptionStrings>("filament_vendor");
    const auto *project_settings_opt = preset_bundle->project_config.option<ConfigOptionStrings>("filament_settings_id");

    for (size_t i = 0; i < num_physical; ++i) {
        std::string preset_name = i < preset_bundle->filament_presets.size() ? preset_bundle->filament_presets[i] : std::string();
        const Preset *preset = preset_name.empty() ? nullptr : preset_bundle->filaments.find_preset(preset_name);

        if (preset_name.empty() && preset != nullptr)
            preset_name = preset->name;
        if (preset_name.empty() && project_settings_opt != nullptr && i < project_settings_opt->values.size())
            preset_name = project_settings_opt->values[i];
        if (preset_name.empty() && preset != nullptr) {
            if (const auto *settings_opt = preset->config.option<ConfigOptionStrings>("filament_settings_id")) {
                if (!settings_opt->values.empty())
                    preset_name = settings_opt->values.front();
            }
        }

        const std::string name_key = normalized_editor_filament_identity(preset_name);
        if (name_key.empty())
            continue;

        std::string vendor_key;
        if (preset != nullptr) {
            if (const auto *vendor_opt = preset->config.option<ConfigOptionStrings>("filament_vendor")) {
                if (!vendor_opt->values.empty())
                    vendor_key = normalized_editor_filament_type(vendor_opt->values.front());
            }
        }
        if (vendor_key.empty() && preset != nullptr && preset->vendor != nullptr)
            vendor_key = normalized_editor_filament_type(preset->vendor->id.empty() ? preset->vendor->name : preset->vendor->id);
        if (vendor_key.empty() && project_vendor_opt != nullptr && i < project_vendor_opt->values.size())
            vendor_key = normalized_editor_filament_type(project_vendor_opt->values[i]);
        if (vendor_key == "(undefined)")
            vendor_key.clear();

        identities[i] = vendor_key.empty() ? name_key : vendor_key + "::" + name_key;
    }

    return identities;
}

void show_mixed_filament_type_toast(const wxString &message)
{
    Plater *plater = wxGetApp().plater();
    if (plater == nullptr)
        return;
    NotificationManager *notification_manager = plater->get_notification_manager();
    if (notification_manager == nullptr)
        return;
    notification_manager->push_notification(into_u8(message));
}

static std::vector<unsigned int> build_manual_pattern_preview_sequence(const MixedFilament &mf,
                                                                       size_t               num_physical,
                                                                       size_t               wall_loops)
{
    return MixedFilamentManager::manual_pattern_preview_sequence(mf, num_physical, wall_loops);
}

enum class ManualPatternValidationError
{
    None,
    Invalid,
    MissingSameType,
    TypeMismatch
};

struct ManualPatternValidationResult
{
    unsigned int                 component_a = 0;
    unsigned int                 component_b = 0;
    ManualPatternValidationError error       = ManualPatternValidationError::None;
};

static ManualPatternValidationResult validate_manual_pattern_filament_types(const MixedFilament &mf,
                                                                             size_t               num_physical)
{
    ManualPatternValidationResult result;
    const std::vector<std::string> identities = current_editor_physical_filament_identities(num_physical);
    if (!MixedFilamentManager::manual_pattern_is_valid_for_physical(mf, num_physical)) {
        result.error = ManualPatternValidationError::Invalid;
        return result;
    }

    std::string expected_identity;
    std::vector<unsigned int> pattern_ids;
    for (const unsigned int filament_id : MixedFilamentManager::decode_manual_pattern_physical_ids(mf, num_physical)) {
        if (filament_id == 0 || filament_id > identities.size()) {
            result.error = ManualPatternValidationError::Invalid;
            return result;
        }
        if (identities[size_t(filament_id - 1)].empty()) {
            result.error = ManualPatternValidationError::MissingSameType;
            return result;
        }

        const std::string &identity = identities[size_t(filament_id - 1)];
        if (expected_identity.empty())
            expected_identity = identity;
        else if (identity != expected_identity) {
            result.error = ManualPatternValidationError::TypeMismatch;
            return result;
        }
        if (std::find(pattern_ids.begin(), pattern_ids.end(), filament_id) == pattern_ids.end())
            pattern_ids.emplace_back(filament_id);
    }

    if (pattern_ids.empty() || expected_identity.empty()) {
        result.error = ManualPatternValidationError::Invalid;
        return result;
    }

    result.component_a = pattern_ids.front();
    if (pattern_ids.size() > 1)
        result.component_b = pattern_ids[1];
    for (size_t i = 0; i < identities.size(); ++i) {
        const unsigned int filament_id = unsigned(i + 1);
        if (result.component_b != 0 || filament_id == result.component_a || identities[i] != expected_identity)
            continue;
        result.component_b = filament_id;
    }
    if (result.component_b == 0)
        result.error = ManualPatternValidationError::MissingSameType;

    return result;
}

// -- Plater.cpp:5127 --------------------------------------------------------
std::pair<int, int> effective_pair_preview_ratios(int percent_b)
{
    const int mix_b = std::clamp(percent_b, 0, 100);
    int       ratio_a = 1;
    int       ratio_b = 0;

    if (mix_b >= 100) {
        ratio_a = 0;
        ratio_b = 1;
    } else if (mix_b > 0) {
        const int pct_b      = mix_b;
        const int pct_a      = 100 - pct_b;
        const bool b_is_major = pct_b >= pct_a;
        const int major_pct  = b_is_major ? pct_b : pct_a;
        const int minor_pct  = b_is_major ? pct_a : pct_b;
        const int major_layers =
            std::max(1, int(std::lround(double(major_pct) / double(std::max(1, minor_pct)))));
        ratio_a = b_is_major ? 1 : major_layers;
        ratio_b = b_is_major ? major_layers : 1;
    }

    if (ratio_a > 0 && ratio_b > 0) {
        const int g = std::gcd(ratio_a, ratio_b);
        if (g > 1) {
            ratio_a /= g;
            ratio_b /= g;
        }
    }

    return { std::max(0, ratio_a), std::max(0, ratio_b) };
}

std::vector<unsigned int> build_effective_pair_preview_sequence(unsigned int component_a,
                                                                unsigned int component_b,
                                                                int          percent_b,
                                                                bool         limit_cycle)
{
    std::vector<unsigned int> sequence;
    if (component_a == 0 || component_b == 0 || component_a == component_b)
        return sequence;

    auto [ratio_a, ratio_b] = effective_pair_preview_ratios(percent_b);
    constexpr int k_max_cycle = 24;
    if (limit_cycle && ratio_a > 0 && ratio_b > 0 && ratio_a + ratio_b > k_max_cycle) {
        const double scale = double(k_max_cycle) / double(ratio_a + ratio_b);
        ratio_a = std::max(1, int(std::round(double(ratio_a) * scale)));
        ratio_b = std::max(1, int(std::round(double(ratio_b) * scale)));
    }
    if (ratio_a == 0 && ratio_b == 0)
        ratio_a = 1;

    const int cycle = std::max(1, ratio_a + ratio_b);
    sequence.reserve(size_t(cycle));
    for (int pos = 0; pos < cycle; ++pos) {
        const int b_before = (pos * ratio_b) / cycle;
        const int b_after  = ((pos + 1) * ratio_b) / cycle;
        sequence.emplace_back((b_after > b_before) ? component_b : component_a);
    }
    return sequence;
}

std::string format_preview_sequence_percent(int count, int total)
{
    if (count <= 0 || total <= 0)
        return "";

    const double percent         = 100.0 * double(count) / double(total);
    const double rounded_tenths  = std::round(percent * 10.0) / 10.0;
    const double nearest_integer = std::round(rounded_tenths);
    if (std::abs(rounded_tenths - nearest_integer) < 1e-6)
        return wxString::Format("%d%%", int(nearest_integer)).ToStdString();
    return wxString::Format("%.1f%%", rounded_tenths).ToStdString();
}

// -- Plater.cpp:5134 --------------------------------------------------------
void reduce_weight_counts_to_cycle_limit(std::vector<int> &counts, size_t cycle_limit)
{
    if (counts.empty() || cycle_limit == 0)
        return;

    int total = std::accumulate(counts.begin(), counts.end(), 0);
    if (total <= 0 || size_t(total) <= cycle_limit)
        return;

    std::vector<size_t> positive_indices;
    positive_indices.reserve(counts.size());
    for (size_t i = 0; i < counts.size(); ++i)
        if (counts[i] > 0)
            positive_indices.emplace_back(i);

    if (positive_indices.empty()) {
        counts.assign(counts.size(), 0);
        return;
    }

    std::vector<int> reduced(counts.size(), 0);
    if (cycle_limit < positive_indices.size()) {
        std::sort(positive_indices.begin(), positive_indices.end(), [&counts](size_t lhs, size_t rhs) {
            if (counts[lhs] != counts[rhs])
                return counts[lhs] > counts[rhs];
            return lhs < rhs;
        });
        for (size_t i = 0; i < cycle_limit; ++i)
            reduced[positive_indices[i]] = 1;
        counts = std::move(reduced);
        return;
    }

    size_t remaining_slots = cycle_limit;
    for (const size_t idx : positive_indices) {
        reduced[idx] = 1;
        --remaining_slots;
    }

    int total_extras = 0;
    std::vector<int> extra_counts(counts.size(), 0);
    for (const size_t idx : positive_indices) {
        extra_counts[idx] = std::max(0, counts[idx] - 1);
        total_extras += extra_counts[idx];
    }
    if (remaining_slots == 0 || total_extras <= 0) {
        counts = std::move(reduced);
        return;
    }

    std::vector<double> remainders(counts.size(), -1.0);
    size_t assigned_slots = 0;
    for (const size_t idx : positive_indices) {
        if (extra_counts[idx] == 0)
            continue;
        const double exact = double(remaining_slots) * double(extra_counts[idx]) / double(total_extras);
        const int assigned = int(std::floor(exact));
        reduced[idx] += assigned;
        assigned_slots += size_t(assigned);
        remainders[idx] = exact - double(assigned);
    }

    size_t missing_slots = remaining_slots > assigned_slots ? (remaining_slots - assigned_slots) : size_t(0);
    while (missing_slots > 0) {
        size_t best_idx = size_t(-1);
        double best_remainder = -1.0;
        int    best_extra = -1;
        for (const size_t idx : positive_indices) {
            if (extra_counts[idx] == 0)
                continue;
            if (remainders[idx] > best_remainder ||
                (std::abs(remainders[idx] - best_remainder) <= 1e-9 && extra_counts[idx] > best_extra) ||
                (std::abs(remainders[idx] - best_remainder) <= 1e-9 && extra_counts[idx] == best_extra && idx < best_idx)) {
                best_idx = idx;
                best_remainder = remainders[idx];
                best_extra = extra_counts[idx];
            }
        }
        if (best_idx == size_t(-1))
            break;
        ++reduced[best_idx];
        remainders[best_idx] = -1.0;
        --missing_slots;
    }

    counts = std::move(reduced);
}

// -- Plater.cpp:5553 --------------------------------------------------------
double mixed_filament_reference_nozzle_mm(unsigned int               component_a,
                                          unsigned int               component_b,
                                          const std::vector<double> &nozzle_diameters)
{
    std::vector<double> samples;
    samples.reserve(2);

    auto append_if_valid = [&samples, &nozzle_diameters](unsigned int component_id) {
        if (component_id >= 1 && component_id <= nozzle_diameters.size())
            samples.emplace_back(std::max(0.05, nozzle_diameters[size_t(component_id - 1)]));
    };

    append_if_valid(component_a);
    append_if_valid(component_b);

    if (samples.empty())
        return 0.4;
    return std::accumulate(samples.begin(), samples.end(), 0.0) / double(samples.size());
}

double mixed_filament_bias_limit_mm(const MixedFilament &mf, const std::vector<double> &nozzle_diameters)
{
    const double reference_nozzle_mm = mixed_filament_reference_nozzle_mm(mf.component_a, mf.component_b, nozzle_diameters);
    return MixedFilamentManager::max_pair_bias_mm(float(reference_nozzle_mm));
}

float mixed_filament_single_surface_offset_value(const MixedFilament       &mf,
                                                  const std::vector<double> &nozzle_diameters)
{
    const double reference_nozzle_mm = mixed_filament_reference_nozzle_mm(mf.component_a, mf.component_b, nozzle_diameters);
    return MixedFilamentManager::bias_ui_value_from_surface_offsets(
        mf.component_a_surface_offset,
        mf.component_b_surface_offset,
        float(reference_nozzle_mm));
}

std::pair<float, float> mixed_filament_single_surface_offset_pair(const MixedFilament       &mf,
                                                                   float                      value,
                                                                   const std::vector<double> &nozzle_diameters)
{
    const double reference_nozzle_mm = mixed_filament_reference_nozzle_mm(mf.component_a, mf.component_b, nozzle_diameters);
    return MixedFilamentManager::surface_offset_pair_from_signed_bias(value, float(reference_nozzle_mm));
}

std::string mixed_filament_apparent_pair_summary(const MixedFilament               &mf,
                                                  const MixedFilamentPreviewSettings &preview_settings,
                                                  const std::vector<double>          &nozzle_diameters,
                                                  bool                                bias_mode_enabled)
{
    if (!Slic3r::mixed_filament_supports_bias_apparent_color(mf, preview_settings, bias_mode_enabled))
        return {};

    const int base_b = MixedFilamentConfigPanel::effective_local_z_preview_mix_b_percent(mf, preview_settings);
    const int base_a = 100 - base_b;
    const auto [apparent_a, apparent_b] =
        Slic3r::mixed_filament_apparent_pair_percentages(mf, preview_settings, nozzle_diameters, bias_mode_enabled);

    if (std::abs(mf.component_a_surface_offset - mf.component_b_surface_offset) > 1e-4f &&
        (apparent_a != base_a || apparent_b != base_b)) {
        std::ostringstream ss;
        ss << '~' << apparent_a << '/' << apparent_b;
        return ss.str();
    }

    std::ostringstream ss;
    ss << apparent_a << "%/" << apparent_b << '%';
    return ss.str();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MixedFilamentConfigPanel — static class methods
// (verbatim from FullSpectrum Plater.cpp:4943-6042)
// ---------------------------------------------------------------------------

std::vector<unsigned int> MixedFilamentConfigPanel::decode_gradient_ids(const std::string &s)
{
    return MixedFilamentManager::decode_gradient_component_ids(s, 0);
}

std::string MixedFilamentConfigPanel::encode_gradient_ids(const std::vector<unsigned int> &ids)
{
    return MixedFilamentManager::encode_gradient_component_ids(ids);
}

std::vector<unsigned int> MixedFilamentConfigPanel::decode_manual_pattern_ids(const MixedFilament &mf,
                                                                              size_t               num_physical,
                                                                              size_t               wall_loops)
{
    return build_manual_pattern_preview_sequence(mf, num_physical, wall_loops);
}

std::vector<int> MixedFilamentConfigPanel::decode_gradient_weights(const std::string &s, size_t n)
{
    std::vector<int> w;
    if (s.empty() || n == 0)
        return w;

    std::string token;
    for (const char c : s) {
        if (c >= '0' && c <= '9') {
            token.push_back(c);
            continue;
        }
        if (!token.empty()) {
            w.emplace_back(std::max(0, std::atoi(token.c_str())));
            token.clear();
        }
    }
    if (!token.empty())
        w.emplace_back(std::max(0, std::atoi(token.c_str())));
    if (w.size() != n)
        w.clear();
    return w;
}

std::vector<int> MixedFilamentConfigPanel::normalize_gradient_weights(const std::vector<int> &w, size_t n)
{
    std::vector<int> out = w;
    if (out.size() != n) out.assign(n, n > 0 ? int(100 / n) : 0);
    int sum = 0;
    for (int &v : out) { v = std::max(0, v); sum += v; }
    if (sum <= 0 && n > 0) { out.assign(n, 0); out[0] = 100; return out; }
    std::vector<double> rem(n, 0.);
    int assigned = 0;
    for (size_t i = 0; i < n; ++i) {
        const double exact = 100.0 * double(out[i]) / double(sum);
        out[i] = int(std::floor(exact));
        rem[i] = exact - double(out[i]);
        assigned += out[i];
    }
    int missing = std::max(0, 100 - assigned);
    while (missing > 0) {
        size_t best = 0;
        double best_rem = -1.0;
        for (size_t i = 0; i < rem.size(); ++i) {
            if (rem[i] > best_rem) { best_rem = rem[i]; best = i; }
        }
        ++out[best];
        rem[best] = 0.0;
        --missing;
    }
    return out;
}

std::string MixedFilamentConfigPanel::encode_gradient_weights(const std::vector<int> &w)
{
    std::ostringstream out;
    for (size_t i = 0; i < w.size(); ++i) {
        if (i > 0)
            out << '/';
        out << std::max(0, w[i]);
    }
    return out.str();
}

std::vector<unsigned int> MixedFilamentConfigPanel::build_weighted_pair_sequence(unsigned int a,
                                                                                 unsigned int b,
                                                                                 int          percent_b,
                                                                                 bool         limit_cycle)
{
    return build_effective_pair_preview_sequence(a, b, percent_b, limit_cycle);
}

std::vector<unsigned int> MixedFilamentConfigPanel::build_weighted_multi_sequence(const std::vector<unsigned int> &ids,
                                                                                  const std::vector<int> &weights,
                                                                                  size_t max_cycle_limit)
{
    std::vector<unsigned int> seq;
    if (ids.empty())
        return seq;

    std::vector<unsigned int> filtered_ids;
    std::vector<int> counts;
    filtered_ids.reserve(ids.size());
    counts.reserve(ids.size());

    std::vector<int> normalized = normalize_gradient_weights(weights, ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        const int weight = (i < normalized.size()) ? std::max(0, normalized[i]) : 0;
        if (weight <= 0)
            continue;
        filtered_ids.emplace_back(ids[i]);
        counts.emplace_back(weight);
    }
    if (filtered_ids.empty()) {
        filtered_ids = ids;
        counts.assign(ids.size(), 1);
    }

    int g = 0;
    for (const int c : counts)
        g = std::gcd(g, std::max(1, c));
    if (g > 1) {
        for (int &c : counts)
            c = std::max(1, c / g);
    }

    constexpr size_t k_max_cycle = 48;
    const size_t effective_cycle_limit =
        max_cycle_limit > 0 ? std::min(k_max_cycle, std::max<size_t>(1, max_cycle_limit)) : k_max_cycle;
    reduce_weight_counts_to_cycle_limit(counts, effective_cycle_limit);

    std::vector<unsigned int> reduced_ids;
    std::vector<int> reduced_counts;
    reduced_ids.reserve(filtered_ids.size());
    reduced_counts.reserve(counts.size());
    for (size_t i = 0; i < counts.size(); ++i) {
        if (counts[i] <= 0)
            continue;
        reduced_ids.emplace_back(filtered_ids[i]);
        reduced_counts.emplace_back(counts[i]);
    }
    if (reduced_ids.empty())
        return seq;
    filtered_ids = std::move(reduced_ids);
    counts = std::move(reduced_counts);

    const int total = std::accumulate(counts.begin(), counts.end(), 0);
    if (total <= 0)
        return seq;

    const size_t cycle = size_t(total);

    seq.reserve(cycle);
    std::vector<int> emitted(counts.size(), 0);
    for (size_t pos = 0; pos < cycle; ++pos) {
        size_t best_idx = 0;
        double best_score = -1e9;
        for (size_t i = 0; i < counts.size(); ++i) {
            const double target = double(pos + 1) * double(counts[i]) / double(total);
            const double score = target - double(emitted[i]);
            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
        ++emitted[best_idx];
        seq.emplace_back(filtered_ids[best_idx]);
    }
    if (seq.empty())
        seq = filtered_ids;
    return seq;
}

std::vector<double> MixedFilamentConfigPanel::build_local_z_preview_pass_heights(double nominal_layer_height,
                                                                                 double lower_bound,
                                                                                 double upper_bound,
                                                                                 double preferred_a_height,
                                                                                 double preferred_b_height,
                                                                                 int mix_b_percent,
                                                                                 int max_sublayers_limit)
{
    if (nominal_layer_height <= EPSILON)
        return {};

    const double base_height = nominal_layer_height;
    const double lo = std::max<double>(0.01, lower_bound);
    const double hi = std::max<double>(lo, upper_bound);
    const size_t max_passes_limit = max_sublayers_limit >= 2 ? size_t(max_sublayers_limit) : size_t(0);

    auto fit_pass_heights_to_interval = [](std::vector<double> &passes, double total_height, double local_lo, double local_hi) {
        if (passes.empty() || total_height <= EPSILON)
            return false;

        const auto within = [local_lo, local_hi](double value) {
            return value >= local_lo - 1e-6 && value <= local_hi + 1e-6;
        };

        double sum = 0.0;
        for (const double h : passes)
            sum += h;

        double delta = total_height - sum;
        if (std::abs(delta) > 1e-6) {
            if (delta > 0.0) {
                for (double &h : passes) {
                    if (delta <= 1e-6)
                        break;
                    const double room = local_hi - h;
                    if (room <= 1e-6)
                        continue;
                    const double take = std::min(room, delta);
                    h += take;
                    delta -= take;
                }
            } else {
                for (auto it = passes.rbegin(); it != passes.rend() && delta < -1e-6; ++it) {
                    const double room = *it - local_lo;
                    if (room <= 1e-6)
                        continue;
                    const double take = std::min(room, -delta);
                    *it -= take;
                    delta += take;
                }
            }
        }

        if (std::abs(delta) > 1e-6)
            return false;
        return std::all_of(passes.begin(), passes.end(), within);
    };

    auto build_uniform = [&fit_pass_heights_to_interval, base_height, lo, hi, max_passes_limit]() {
        std::vector<double> out;
        size_t min_passes = size_t(std::max<double>(1.0, std::ceil((base_height - EPSILON) / hi)));
        size_t max_passes = size_t(std::max<double>(1.0, std::floor((base_height + EPSILON) / lo)));
        size_t pass_count = min_passes;

        if (max_passes >= min_passes) {
            const double target_step = 0.5 * (lo + hi);
            const size_t target_passes =
                size_t(std::max<double>(1.0, std::llround(base_height / std::max<double>(target_step, EPSILON))));
            pass_count = std::clamp(target_passes, min_passes, max_passes);
        }

        if (max_passes_limit > 0 && pass_count > max_passes_limit)
            pass_count = max_passes_limit;

        if (pass_count == 1 && base_height >= 2.0 * lo - EPSILON && max_passes >= 2)
            pass_count = 2;

        if (pass_count <= 1) {
            out.emplace_back(base_height);
            return out;
        }

        out.assign(pass_count, base_height / double(pass_count));
        double accumulated = 0.0;
        for (size_t i = 0; i + 1 < out.size(); ++i)
            accumulated += out[i];
        out.back() = std::max<double>(EPSILON, base_height - accumulated);
        if (!fit_pass_heights_to_interval(out, base_height, lo, hi) && max_passes_limit == 0) {
            out.assign(pass_count, base_height / double(pass_count));
            accumulated = 0.0;
            for (size_t i = 0; i + 1 < out.size(); ++i)
                accumulated += out[i];
            out.back() = std::max<double>(EPSILON, base_height - accumulated);
        }
        return out;
    };

    auto build_alternating = [&build_uniform, &fit_pass_heights_to_interval, base_height, lo, hi, max_passes_limit](double gradient_h_a, double gradient_h_b) {
        if (base_height < 2.0 * lo - EPSILON)
            return std::vector<double>{ base_height };

        const double cycle_h = std::max<double>(EPSILON, gradient_h_a + gradient_h_b);
        const double ratio_a = std::clamp(gradient_h_a / cycle_h, 0.0, 1.0);

        size_t min_passes = size_t(std::max<double>(2.0, std::ceil((base_height - EPSILON) / hi)));
        if ((min_passes % 2) != 0)
            ++min_passes;

        size_t max_passes = size_t(std::max<double>(2.0, std::floor((base_height + EPSILON) / lo)));
        if ((max_passes % 2) != 0)
            --max_passes;
        if (max_passes_limit > 0) {
            size_t capped_limit = std::max<size_t>(2, max_passes_limit);
            if ((capped_limit % 2) != 0)
                --capped_limit;
            if (capped_limit >= 2)
                max_passes = std::min(max_passes, capped_limit);
        }
        if (max_passes < 2)
            return build_uniform();
        if (min_passes > max_passes)
            min_passes = max_passes;
        if (min_passes < 2)
            min_passes = 2;
        if ((min_passes % 2) != 0)
            ++min_passes;
        if (min_passes > max_passes)
            return build_uniform();

        const double target_step = 0.5 * (lo + hi);
        size_t target_passes =
            size_t(std::max<double>(2.0, std::llround(base_height / std::max<double>(target_step, EPSILON))));
        if ((target_passes % 2) != 0) {
            const size_t round_up = (target_passes < max_passes) ? (target_passes + 1) : max_passes;
            const size_t round_down = (target_passes > min_passes) ? (target_passes - 1) : min_passes;
            if (round_up > max_passes)
                target_passes = round_down;
            else if (round_down < min_passes)
                target_passes = round_up;
            else
                target_passes = ((round_up - target_passes) <= (target_passes - round_down)) ? round_up : round_down;
        }
        target_passes = std::clamp(target_passes, min_passes, max_passes);

        bool                has_best           = false;
        std::vector<double> best_passes;
        double              best_ratio_error   = 0.0;
        size_t              best_pass_distance = 0;
        double              best_max_height    = 0.0;
        size_t              best_pass_count    = 0;

        for (size_t pass_count = min_passes; pass_count <= max_passes; pass_count += 2) {
            const size_t pair_count = pass_count / 2;
            if (pair_count == 0)
                continue;
            const double pair_h = base_height / double(pair_count);

            const double h_a_min = std::max(lo, pair_h - hi);
            const double h_a_max = std::min(hi, pair_h - lo);
            if (h_a_min > h_a_max + EPSILON)
                continue;

            const double h_a = std::clamp(pair_h * ratio_a, h_a_min, h_a_max);
            const double h_b = pair_h - h_a;

            std::vector<double> out;
            out.reserve(pass_count);
            for (size_t pair_idx = 0; pair_idx < pair_count; ++pair_idx) {
                out.emplace_back(h_a);
                out.emplace_back(h_b);
            }
            if (!fit_pass_heights_to_interval(out, base_height, lo, hi))
                continue;

            const double ratio_actual = (h_a + h_b > EPSILON) ? (h_a / (h_a + h_b)) : 0.5;
            const double ratio_error  = std::abs(ratio_actual - ratio_a);
            const size_t pass_distance =
                (pass_count > target_passes) ? (pass_count - target_passes) : (target_passes - pass_count);
            const double max_height = std::max(h_a, h_b);

            const bool better_ratio         = !has_best || (ratio_error + 1e-6 < best_ratio_error);
            const bool similar_ratio        = has_best && std::abs(ratio_error - best_ratio_error) <= 1e-6;
            const bool better_distance      = similar_ratio && (pass_distance < best_pass_distance);
            const bool similar_distance     = similar_ratio && (pass_distance == best_pass_distance);
            const bool better_max_height    = similar_distance && (max_height + 1e-6 < best_max_height);
            const bool similar_max_height   = similar_distance && std::abs(max_height - best_max_height) <= 1e-6;
            const bool better_pass_count    = similar_max_height && (pass_count > best_pass_count);

            if (better_ratio || better_distance || better_max_height || better_pass_count) {
                has_best = true;
                best_passes = std::move(out);
                best_ratio_error = ratio_error;
                best_pass_distance = pass_distance;
                best_max_height = max_height;
                best_pass_count = pass_count;
            }
        }

        return has_best ? best_passes : build_uniform();
    };

    if (preferred_a_height > EPSILON || preferred_b_height > EPSILON) {
        std::vector<double> cadence_unit;
        if (preferred_a_height > EPSILON)
            cadence_unit.push_back(std::clamp(preferred_a_height, lo, hi));
        if (preferred_b_height > EPSILON)
            cadence_unit.push_back(std::clamp(preferred_b_height, lo, hi));

        if (!cadence_unit.empty()) {
            std::vector<double> out;
            out.reserve(size_t(std::ceil(base_height / lo)) + 2);

            double z_used = 0.0;
            size_t idx = 0;
            size_t guard = 0;
            while (z_used + cadence_unit[idx] < base_height - EPSILON && guard++ < 100000) {
                out.push_back(cadence_unit[idx]);
                z_used += cadence_unit[idx];
                idx = (idx + 1) % cadence_unit.size();
            }

            const double remainder = base_height - z_used;
            if (remainder > EPSILON)
                out.push_back(remainder);

            if (fit_pass_heights_to_interval(out, base_height, lo, hi) &&
                (max_passes_limit == 0 || out.size() <= max_passes_limit))
                return out;
        }

        if (preferred_a_height > EPSILON && preferred_b_height > EPSILON)
            return build_alternating(preferred_a_height, preferred_b_height);
        return build_uniform();
    }

    const int mix_b = std::clamp(mix_b_percent, 0, 100);
    const double pct_b = double(mix_b) / 100.0;
    const double pct_a = 1.0 - pct_b;
    const double gradient_h_a = lo + pct_a * (hi - lo);
    const double gradient_h_b = lo + pct_b * (hi - lo);
    return build_alternating(gradient_h_a, gradient_h_b);
}

int MixedFilamentConfigPanel::effective_local_z_preview_mix_b_percent(const MixedFilament &mf,
                                                                      const MixedFilamentPreviewSettings &preview_settings)
{
    return Slic3r::mixed_filament_effective_local_z_preview_mix_b_percent(mf, preview_settings);
}

std::string MixedFilamentConfigPanel::summarize_sequence(const std::vector<unsigned int> &seq)
{
    if (seq.empty()) return "";
    std::unordered_map<unsigned int, int> counts;
    for (unsigned int id : seq) counts[id]++;
    std::vector<std::pair<int, unsigned int>> sorted;
    for (auto &kv : counts) sorted.emplace_back(kv.second, kv.first);
    std::sort(sorted.begin(), sorted.end(), std::greater<>());
    std::string out;
    for (auto &p : sorted) {
        if (!out.empty()) out += "/";
        out += format_preview_sequence_percent(p.first, int(seq.size()));
    }
    return out;
}

wxString MixedFilamentConfigPanel::summarize_local_z_breakdown(const MixedFilament &mf,
                                                              const std::vector<int> &weights,
                                                              const MixedFilamentPreviewSettings &preview_settings)
{
    const std::string normalized_pattern = MixedFilamentManager::normalize_manual_pattern(mf);
    if (!normalized_pattern.empty())
        return _L("Local-Z breakdown: manual pattern rows do not use pair decomposition.");

    if (mf.distribution_mode == int(MixedFilament::SameLayerPointillisme))
        return _L("Local-Z breakdown: same-layer mode does not use local-Z pair decomposition.");

    auto pair_name = [](unsigned int a, unsigned int b) {
        std::ostringstream ss;
        ss << 'F' << a << "+F" << b;
        return ss.str();
    };
    auto pair_split = [](unsigned int a, unsigned int b, int weight_a, int weight_b) {
        const int safe_a = std::max(0, weight_a);
        const int safe_b = std::max(0, weight_b);
        const int total  = std::max(1, safe_a + safe_b);
        const int pct_a  = int(std::lround(100.0 * double(safe_a) / double(total)));
        const int pct_b  = std::max(0, 100 - pct_a);

        std::ostringstream ss;
        ss << 'F' << a << "/F" << b << " " << safe_a << ':' << safe_b << " (" << pct_a << '/' << pct_b << ')';
        return ss.str();
    };
    auto cadence_entry = [&pair_name](unsigned int a, unsigned int b, int weight, int total) {
        const int pct = int(std::lround(100.0 * double(std::max(0, weight)) / double(std::max(1, total))));
        std::ostringstream ss;
        ss << pair_name(a, b) << ' ' << pct << '%';
        return ss.str();
    };

    const std::vector<unsigned int> ids = decode_gradient_ids(mf.gradient_component_ids);
    if (preview_settings.local_z_mode && preview_settings.local_z_direct_multicolor && ids.size() >= 3) {
        const std::vector<int> normalized = normalize_gradient_weights(weights, ids.size());
        const size_t effective_sublayers =
            mf.local_z_max_sublayers >= 2 ? size_t(std::max(2, mf.local_z_max_sublayers)) : ids.size();

        std::ostringstream components;
        for (size_t idx = 0; idx < ids.size(); ++idx) {
            if (idx > 0)
                components << ", ";
            const int pct = idx < normalized.size() ? normalized[idx] : 0;
            components << 'F' << ids[idx] << ' ' << pct << '%';
        }
        wxString text = wxString::Format(_L("Local-Z direct multicolor solver: %s"), from_u8(components.str()).c_str());
        text += "\n";
        text += wxString::Format(_L("Carry-over error is distributed directly across all %d components instead of collapsing them into pair cadence."),
                                 int(ids.size()));
        if (mf.local_z_max_sublayers >= 2)
            text += "\n" + wxString::Format(_L("Effective Local-Z cap: up to %d sublayers per nominal layer."),
                                            int(effective_sublayers));
        return text;
    }

    if (ids.size() >= 4) {
        const std::vector<int> normalized = normalize_gradient_weights(weights, ids.size());
        const std::vector<unsigned int> pair_tokens = { 1, 2 };
        const std::vector<int> pair_weights = {
            std::max(1, normalized[0] + normalized[1]),
            std::max(1, normalized[2] + normalized[3])
        };
        const size_t max_pair_layers =
            (preview_settings.local_z_mode && mf.local_z_max_sublayers >= 2) ?
                std::max<size_t>(1, size_t(mf.local_z_max_sublayers) / 2) :
                size_t(0);
        const std::vector<unsigned int> uncapped_pair_sequence = build_weighted_multi_sequence(pair_tokens, pair_weights);
        const std::vector<unsigned int> effective_pair_sequence =
            max_pair_layers > 0 ? build_weighted_multi_sequence(pair_tokens, pair_weights, max_pair_layers) : uncapped_pair_sequence;
        const std::vector<unsigned int> &pair_sequence = effective_pair_sequence.empty() ? uncapped_pair_sequence : effective_pair_sequence;
        const int pair_ab_weight = int(std::count(pair_sequence.begin(), pair_sequence.end(), 1u));
        const int pair_cd_weight = int(std::count(pair_sequence.begin(), pair_sequence.end(), 2u));
        const int pair_total = std::max(1, int(pair_sequence.size()));

        const std::string cadence_summary =
            cadence_entry(ids[0], ids[1], pair_ab_weight, pair_total) + ", " +
            cadence_entry(ids[2], ids[3], pair_cd_weight, pair_total);
        const std::string split_summary =
            pair_split(ids[0], ids[1], normalized[0], normalized[1]) + ", " +
            pair_split(ids[2], ids[3], normalized[2], normalized[3]);

        wxString text = wxString::Format(_L("Local-Z layer cadence: %s."), from_u8(cadence_summary).c_str());
        text += "\n" + wxString::Format(_L("Pair splits: %s."), from_u8(split_summary).c_str());
        if (!preview_settings.local_z_mode && mf.local_z_max_sublayers >= 2)
            text += "\n" + _L("Saved row limit will apply when Local-Z dithering mode is enabled in print settings.");
        if (preview_settings.local_z_mode && mf.local_z_max_sublayers >= 2) {
            if (uncapped_pair_sequence.size() > pair_sequence.size())
                text += "\n" + wxString::Format(_L("Effective Local-Z stack: %d sublayers over %d pair layers (uncapped %d)."),
                                                pair_total * 2, pair_total, int(uncapped_pair_sequence.size() * 2));
            else
                text += "\n" + wxString::Format(_L("Effective Local-Z stack: %d sublayers over %d pair layers."),
                                                pair_total * 2, pair_total);
        }
        return text;
    }

    if (ids.size() == 3) {
        const std::vector<int> normalized = normalize_gradient_weights(weights, ids.size());
        const std::vector<unsigned int> pair_tokens = { 1, 2, 3 };
        const std::vector<int> pair_weights = {
            std::max(1, normalized[0] + normalized[1]),
            std::max(1, normalized[0] + normalized[2]),
            std::max(1, normalized[1] + normalized[2])
        };
        const size_t max_pair_layers =
            (preview_settings.local_z_mode && mf.local_z_max_sublayers >= 2) ?
                std::max<size_t>(1, size_t(mf.local_z_max_sublayers) / 2) :
                size_t(0);
        const std::vector<unsigned int> uncapped_pair_sequence = build_weighted_multi_sequence(pair_tokens, pair_weights);
        const std::vector<unsigned int> effective_pair_sequence =
            max_pair_layers > 0 ? build_weighted_multi_sequence(pair_tokens, pair_weights, max_pair_layers) : uncapped_pair_sequence;
        const std::vector<unsigned int> &pair_sequence = effective_pair_sequence.empty() ? uncapped_pair_sequence : effective_pair_sequence;
        const int pair_ab_weight = int(std::count(pair_sequence.begin(), pair_sequence.end(), 1u));
        const int pair_ac_weight = int(std::count(pair_sequence.begin(), pair_sequence.end(), 2u));
        const int pair_bc_weight = int(std::count(pair_sequence.begin(), pair_sequence.end(), 3u));
        const int pair_total     = std::max(1, int(pair_sequence.size()));

        const std::string cadence_summary =
            cadence_entry(ids[0], ids[1], pair_ab_weight, pair_total) + ", " +
            cadence_entry(ids[0], ids[2], pair_ac_weight, pair_total) + ", " +
            cadence_entry(ids[1], ids[2], pair_bc_weight, pair_total);
        const std::string split_summary =
            pair_split(ids[0], ids[1], normalized[0], normalized[1]) + ", " +
            pair_split(ids[0], ids[2], normalized[0], normalized[2]) + ", " +
            pair_split(ids[1], ids[2], normalized[1], normalized[2]);

        wxString text = wxString::Format(_L("Local-Z layer cadence: %s."), from_u8(cadence_summary).c_str());
        text += "\n" + wxString::Format(_L("Pair splits: %s."), from_u8(split_summary).c_str());
        if (!preview_settings.local_z_mode && mf.local_z_max_sublayers >= 2)
            text += "\n" + _L("Saved row limit will apply when Local-Z dithering mode is enabled in print settings.");
        if (preview_settings.local_z_mode && mf.local_z_max_sublayers >= 2) {
            if (uncapped_pair_sequence.size() > pair_sequence.size())
                text += "\n" + wxString::Format(_L("Effective Local-Z stack: %d sublayers over %d pair layers (uncapped %d)."),
                                                pair_total * 2, pair_total, int(uncapped_pair_sequence.size() * 2));
            else
                text += "\n" + wxString::Format(_L("Effective Local-Z stack: %d sublayers over %d pair layers."),
                                                pair_total * 2, pair_total);
        }
        return text;
    }

    if (mf.component_a >= 1 && mf.component_b >= 1 && mf.component_a != mf.component_b) {
        const int pct_b = std::clamp(mf.mix_b_percent, 0, 100);
        const int pct_a = 100 - pct_b;
        if (preview_settings.local_z_mode) {
            const std::vector<double> effective_passes = build_local_z_preview_pass_heights(preview_settings.nominal_layer_height,
                                                                                            preview_settings.mixed_lower_bound,
                                                                                            preview_settings.mixed_upper_bound,
                                                                                            preview_settings.preferred_a_height,
                                                                                            preview_settings.preferred_b_height,
                                                                                            mf.mix_b_percent,
                                                                                            0);
            if (!effective_passes.empty()) {
                const int effective_pct_b = effective_local_z_preview_mix_b_percent(mf, preview_settings);
                return wxString::Format(
                    _L("Local-Z pair split: requested F%u/F%u %d/%d, effective %d/%d over %d sublayers."),
                    mf.component_a, mf.component_b, pct_a, pct_b,
                    100 - effective_pct_b, effective_pct_b, int(effective_passes.size()));
            }
        }
        return wxString::Format(
            _L("Local-Z pair split: requested F%u/F%u %d/%d."),
            mf.component_a, mf.component_b, pct_a, pct_b);
    }

    return _L("Local-Z breakdown: unavailable.");
}

std::string MixedFilamentConfigPanel::blend_from_sequence(const std::vector<std::string> &colors, const std::vector<unsigned int> &seq, const std::string &fallback)
{
    if (colors.empty() || seq.empty())
        return fallback;

    std::vector<size_t> counts(colors.size() + 1, size_t(0));
    size_t total = 0;
    for (const unsigned int id : seq) {
        if (id == 0 || id > colors.size())
            continue;
        ++counts[id];
        ++total;
    }
    if (total == 0)
        return fallback;

    unsigned int first_id = 0;
    for (size_t id = 1; id <= colors.size(); ++id) {
        if (counts[id] > 0) {
            first_id = unsigned(id);
            break;
        }
    }
    if (first_id == 0 || first_id > colors.size())
        return fallback;

    std::string blended = colors[first_id - 1];
    int acc = int(counts[first_id]);
    for (size_t id = size_t(first_id + 1); id <= colors.size(); ++id) {
        if (counts[id] == 0)
            continue;
        blended = MixedFilamentManager::blend_color(blended, colors[id - 1], acc, int(counts[id]));
        acc += int(counts[id]);
    }

    return blended;
}

// ---------------------------------------------------------------------------
// MixedFilamentConfigPanel — constructor and instance methods
// (verbatim from FullSpectrum Plater.cpp:6044-6857)
// ---------------------------------------------------------------------------

MixedFilamentConfigPanel::MixedFilamentConfigPanel(wxWindow *parent,
                                                   size_t mixed_id,
                                                   const MixedFilament &mf,
                                                   size_t num_physical,
                                                   const std::vector<std::string> &physical_colors,
                                                   const std::vector<double> &nozzle_diameters,
                                                   const std::vector<wxColour> &palette,
                                                   const MixedFilamentPreviewSettings &preview_settings,
                                                   bool bias_mode_enabled,
                                                   OnChangeFn on_change)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE)
    , m_mixed_id(mixed_id)
    , m_mf(mf)
    , m_num_physical(num_physical)
    , m_physical_colors(physical_colors)
    , m_nozzle_diameters(nozzle_diameters)
    , m_palette(palette)
    , m_preview_settings(preview_settings)
    , m_bias_mode_enabled(bias_mode_enabled)
    , m_selected_weight_state(std::make_shared<std::vector<int>>())
    , m_on_change(on_change)
{
    if (parent)
        SetBackgroundColour(parent->GetBackgroundColour());
    else
        SetBackgroundColour(wxGetApp().dark_mode() ? wxColour(52, 52, 56) : wxColour(255, 255, 255));
    build_ui();
}

void MixedFilamentConfigPanel::build_ui()
{
    const int gap = FromDIP(6);
    const int compact_gap = std::max(FromDIP(2), gap / 3);
    const bool is_dark = wxGetApp().dark_mode();
    const wxColour panel_bg = GetBackgroundColour().IsOk() ? GetBackgroundColour() :
        (is_dark ? wxColour(52, 52, 56) : wxColour(255, 255, 255));
    SetBackgroundColour(panel_bg);
    auto *root = new wxBoxSizer(wxVERTICAL);

    // Filament choices
    wxArrayString filament_choices;
    for (size_t i = 0; i < m_num_physical; ++i)
        filament_choices.Add(wxString::Format("F%d", int(i + 1)));
    wxArrayString optional_filament_choices;
    optional_filament_choices.Add(_L("None"));
    for (size_t i = 0; i < m_num_physical; ++i)
        optional_filament_choices.Add(wxString::Format("F%d", int(i + 1)));

    const int component_a = std::clamp(int(m_mf.component_a), 1, int(m_num_physical));
    const int component_b = std::clamp(int(m_mf.component_b), 1, int(m_num_physical));

    const std::vector<unsigned int> initial_gradient_ids = decode_gradient_ids(m_mf.gradient_component_ids);
    if (m_mf.distribution_mode == int(MixedFilament::SameLayerPointillisme)) {
        m_mf.distribution_mode = initial_gradient_ids.size() >= 3 ? int(MixedFilament::LayerCycle) : int(MixedFilament::Simple);
        m_mf.pointillism_all_filaments = false;
    }
    const int stored_distribution_mode = std::clamp(m_mf.distribution_mode,
                                                    int(MixedFilament::LayerCycle),
                                                    int(MixedFilament::Simple));
    const int row_distribution_mode = initial_gradient_ids.size() >= 3 ?
        (stored_distribution_mode == int(MixedFilament::Simple) ? int(MixedFilament::LayerCycle) : stored_distribution_mode) :
        int(MixedFilament::Simple);
    m_mf.distribution_mode = row_distribution_mode;
    const bool multi_gradient_row = row_distribution_mode != int(MixedFilament::Simple) && initial_gradient_ids.size() >= 3;
    const int selection_c = initial_gradient_ids.size() >= 3 ? int(initial_gradient_ids[2]) : 0;
    const int selection_d = initial_gradient_ids.size() >= 4 ? int(initial_gradient_ids[3]) : 0;

    // Hidden data controls used as backing state for swatch pickers.
    m_choice_a = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, filament_choices);
    m_choice_b = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, filament_choices);
    m_choice_a->SetSelection(component_a - 1);
    m_choice_b->SetSelection(component_b - 1);
    m_choice_a->Hide();
    m_choice_b->Hide();
    if (multi_gradient_row) {
        m_choice_c = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, optional_filament_choices);
        m_choice_c->SetSelection(std::clamp(selection_c, 0, int(m_num_physical)));
        m_choice_c->Hide();
        if (initial_gradient_ids.size() >= 4) {
            m_choice_d = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, optional_filament_choices);
            m_choice_d->SetSelection(std::clamp(selection_d, 0, int(m_num_physical)));
            m_choice_d->Hide();
        }
    }

    auto create_component_picker = [this, gap](wxPanel *&container_out, wxPanel *&swatch_out, wxStaticText *&label_out, const wxString &tooltip) {
        const int inner_gap = std::max(FromDIP(1), gap / 4);
        const bool local_is_dark = wxGetApp().dark_mode();
        const wxColour local_picker_bg = local_is_dark ? wxColour(64, 64, 70) : wxColour(255, 255, 255);
        const wxColour local_picker_text = local_is_dark ? wxColour(230, 230, 230) : wxColour(32, 32, 32);
        container_out = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
        container_out->SetBackgroundColour(local_picker_bg);
        const wxSize picker_size(FromDIP(38), FromDIP(22));
        container_out->SetMinSize(picker_size);
        container_out->SetMaxSize(picker_size);

        auto *container_sizer = new wxBoxSizer(wxHORIZONTAL);
        swatch_out = new wxPanel(container_out, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(12), FromDIP(12)), wxBORDER_SIMPLE);
        swatch_out->SetMinSize(wxSize(FromDIP(12), FromDIP(12)));
        swatch_out->SetToolTip(tooltip);
        label_out = new wxStaticText(container_out, wxID_ANY, wxEmptyString);
        label_out->SetForegroundColour(local_picker_text);
        label_out->SetToolTip(tooltip);

        auto *content_sizer = new wxBoxSizer(wxHORIZONTAL);
        content_sizer->Add(swatch_out, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, inner_gap);
        content_sizer->Add(label_out, 0, wxALIGN_CENTER_VERTICAL);
        container_sizer->AddStretchSpacer(1);
        container_sizer->Add(content_sizer, 0, wxALIGN_CENTER_VERTICAL);
        container_sizer->AddStretchSpacer(1);
        container_out->SetSizer(container_sizer);
        container_out->SetToolTip(tooltip);
        container_out->SetCursor(wxCursor(wxCURSOR_HAND));
        swatch_out->SetCursor(wxCursor(wxCURSOR_HAND));
        label_out->SetCursor(wxCursor(wxCURSOR_HAND));
    };

    create_component_picker(m_picker_a_container, m_picker_a_swatch, m_picker_a_label, _L("Click to choose a physical filament color"));
    create_component_picker(m_picker_b_container, m_picker_b_swatch, m_picker_b_label, _L("Click to choose a physical filament color"));
    if (m_choice_c)
        create_component_picker(m_picker_c_container, m_picker_c_swatch, m_picker_c_label, _L("Click to choose a physical filament color"));
    if (m_choice_d)
        create_component_picker(m_picker_d_container, m_picker_d_swatch, m_picker_d_label, _L("Click to choose a physical filament color"));
    update_component_picker_visuals();

    // Check for pattern mode
    const std::string normalized_pattern = MixedFilamentManager::normalize_manual_pattern(m_mf);
    const bool pattern_row_mode = !normalized_pattern.empty();

    auto *picker_row = new wxBoxSizer(wxHORIZONTAL);
    if (!pattern_row_mode) {
        auto add_picker = [this, picker_row, gap](wxPanel *container, bool &first_picker) {
            if (!container)
                return;
            if (!first_picker)
                picker_row->Add(new wxStaticText(this, wxID_ANY, "+"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, std::max(FromDIP(2), gap / 2));
            picker_row->Add(container, 0, wxALIGN_CENTER_VERTICAL);
            first_picker = false;
        };

        bool first_picker = true;
        add_picker(m_picker_a_container, first_picker);
        add_picker(m_picker_b_container, first_picker);
        add_picker(m_picker_c_container, first_picker);
        add_picker(m_picker_d_container, first_picker);
    } else {
        if (m_picker_a_container) m_picker_a_container->Hide();
        if (m_picker_b_container) m_picker_b_container->Hide();
        if (m_picker_c_container) m_picker_c_container->Hide();
        if (m_picker_d_container) m_picker_d_container->Hide();
    }
    root->Add(picker_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);

    // Pattern controls (if pattern mode)
    if (pattern_row_mode) {
        auto *pattern_row = new wxBoxSizer(wxHORIZONTAL);
        auto *pattern_label = new wxStaticText(this, wxID_ANY, _L("Pattern"));
        pattern_label->SetForegroundColour(is_dark ? wxColour(236, 236, 236) : wxColour(20, 20, 20));
        pattern_row->Add(pattern_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
        m_pattern_ctrl = new wxTextCtrl(this, wxID_ANY, from_u8(normalized_pattern), wxDefaultPosition,
                                        wxSize(FromDIP(200), -1), wxTE_PROCESS_ENTER);
        if (m_mf.ui_mode == 1) {
            m_pattern_ctrl->SetToolTip(_L("Manual perimeter pattern. Separate wall groups with commas. "
                                          "Digits inside each group repeat by layer; 1 and 2 refer to components A and B. "
                                          "Use brackets for a multi-digit physical filament ID, for example [12]."));
        } else {
            m_pattern_ctrl->SetToolTip(_L("Manual repeating pattern. Enter physical filament IDs as comma-separated numbers. "
                                          "Values greater than 9 are supported. Example: 1,2,12,11."));
        }
        pattern_row->Add(m_pattern_ctrl, 1, wxALIGN_CENTER_VERTICAL);
        root->Add(pattern_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);

        auto *quick_buttons = new wxBoxSizer(wxHORIZONTAL);
        const size_t quick_button_count = std::min<size_t>(m_num_physical, 9);
        for (size_t fid = 0; fid < quick_button_count; ++fid) {
            const int filament_id = int(fid + 1);
            const bool component_alias = m_mf.ui_mode == 1 && filament_id <= 2;
            const int physical_filament_id = component_alias ?
                int(filament_id == 1 ? m_mf.component_a : m_mf.component_b) : filament_id;
            const wxString button_label = component_alias ?
                wxString(filament_id == 1 ? "A" : "B") : wxString::Format("%d", filament_id);
            wxButton *btn = new wxButton(this, wxID_ANY, button_label,
                                         wxDefaultPosition, wxSize(FromDIP(24), FromDIP(22)), wxBU_EXACTFIT);
            const size_t color_index = physical_filament_id > 0 ? size_t(physical_filament_id - 1) : m_palette.size();
            const wxColour chip_color = color_index < m_palette.size() ? m_palette[color_index] : wxColour("#26A69A");
            btn->SetBackgroundColour(chip_color);
            if (component_alias) {
                btn->SetToolTip(wxString::Format(
                    filament_id == 1 ? _L("Append component A (filament %d) to current wall group") :
                                       _L("Append component B (filament %d) to current wall group"),
                    physical_filament_id));
            } else {
                btn->SetToolTip(wxString::Format(
                    m_mf.ui_mode == 1 ? _L("Append filament %d to current wall group") :
                                        _L("Append filament %d to pattern"),
                    filament_id));
            }
            quick_buttons->Add(btn, 0, wxRIGHT, FromDIP(4));
            m_pattern_quick_buttons.emplace_back(btn);
        }
        auto *filaments_label = new wxStaticText(this, wxID_ANY, _L("Filaments"));
        filaments_label->SetForegroundColour(is_dark ? wxColour(236, 236, 236) : wxColour(20, 20, 20));
        picker_row->Add(filaments_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, std::max(FromDIP(3), gap / 2));
        picker_row->Add(quick_buttons, 0, wxALIGN_CENTER_VERTICAL);
    } else {
        // Blend selector for non-pattern mode
        const bool simple_mode = row_distribution_mode == int(MixedFilament::Simple);
        std::vector<unsigned int> selected_gradient_ids = simple_mode ? std::vector<unsigned int>() : initial_gradient_ids;
        if (selected_gradient_ids.size() < 3) selected_gradient_ids.clear();
        if (selected_gradient_ids.empty()) {
            selected_gradient_ids.emplace_back(unsigned(component_a));
            if (component_b != component_a) selected_gradient_ids.emplace_back(unsigned(component_b));
        }
        const bool multi_gradient_mode = selected_gradient_ids.size() >= 3;
        *m_selected_weight_state = normalize_gradient_weights(
            decode_gradient_weights(m_mf.gradient_component_weights, selected_gradient_ids.size()),
            selected_gradient_ids.size());

        wxColour color_a = (component_a >= 1 && component_a <= int(m_palette.size())) ? m_palette[component_a - 1] : wxColour("#26A69A");
        wxColour color_b = (component_b >= 1 && component_b <= int(m_palette.size())) ? m_palette[component_b - 1] : wxColour("#26A69A");
        m_blend_selector = new MixedGradientSelector(this, color_a, color_b, std::clamp(m_mf.mix_b_percent, 0, 100));
        m_blend_selector->SetBackgroundColour(panel_bg);
        m_blend_label = nullptr;
        picker_row->AddSpacer(gap);
        picker_row->Add(m_blend_selector, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxLEFT, gap);

        if (m_blend_selector) {
            std::vector<wxColour> corner_colors;
            corner_colors.reserve(selected_gradient_ids.size());
            for (const unsigned int id : selected_gradient_ids) {
                if (id >= 1 && id <= m_palette.size())
                    corner_colors.emplace_back(m_palette[id - 1]);
            }
            if (!simple_mode && corner_colors.size() >= 3)
                m_blend_selector->set_multi_preview(corner_colors, *m_selected_weight_state);
        }
    }

    // Preview
    auto *preview_row = new wxBoxSizer(wxHORIZONTAL);
    m_mix_preview = new MixedMixPreview(this);
    m_mix_preview->SetBackgroundColour(panel_bg);
    preview_row->Add(m_mix_preview, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxRIGHT, compact_gap);

    auto *bias_controls = new wxBoxSizer(wxHORIZONTAL);
    const float initial_surface_offset_value = mixed_filament_single_surface_offset_value(m_mf, m_nozzle_diameters);
    const double initial_bias_limit = mixed_filament_bias_limit_mm(m_mf, m_nozzle_diameters);
    const wxString bias_tooltip =
        _L("Positive bias recesses the second filament in the pair; negative bias recesses the first filament.\n\n"
           "The color chip shows which filament the current value affects.\n\n"
           "Manual patterns and Local-Z dithering ignore it.");

    auto *surface_offset_label = new wxStaticText(this, wxID_ANY, _L("Bias"));
    surface_offset_label->SetForegroundColour(is_dark ? wxColour(236, 236, 236) : wxColour(20, 20, 20));
    surface_offset_label->SetToolTip(bias_tooltip);
    bias_controls->Add(surface_offset_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, compact_gap);

    create_component_picker(m_surface_offset_target_container,
                            m_surface_offset_target_swatch,
                            m_surface_offset_target_label,
                            bias_tooltip);
    if (m_surface_offset_target_container)
        m_surface_offset_target_container->SetCursor(wxCursor(wxCURSOR_ARROW));
    if (m_surface_offset_target_swatch)
        m_surface_offset_target_swatch->SetCursor(wxCursor(wxCURSOR_ARROW));
    if (m_surface_offset_target_label)
        m_surface_offset_target_label->SetCursor(wxCursor(wxCURSOR_ARROW));
    bias_controls->Add(m_surface_offset_target_container, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, compact_gap);

    m_surface_offset_spin = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(58), -1),
                                                 wxSP_ARROW_KEYS | wxALIGN_RIGHT | wxTE_PROCESS_ENTER,
                                                 -initial_bias_limit, initial_bias_limit,
                                                 std::clamp(double(initial_surface_offset_value), -initial_bias_limit, initial_bias_limit), 0.001);
    m_surface_offset_spin->SetDigits(3);
    m_surface_offset_spin->SetToolTip(bias_tooltip);
    bias_controls->Add(m_surface_offset_spin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, compact_gap);

    auto *surface_offset_units = new wxStaticText(this, wxID_ANY, _L("mm"));
    surface_offset_units->SetForegroundColour(is_dark ? wxColour(210, 210, 210) : wxColour(72, 72, 72));
    surface_offset_units->SetToolTip(bias_tooltip);
    bias_controls->Add(surface_offset_units, 0, wxALIGN_CENTER_VERTICAL);
    if (m_bias_mode_enabled)
        preview_row->Add(bias_controls, 0, wxALIGN_CENTER_VERTICAL);
    else {
        surface_offset_label->Hide();
        if (m_surface_offset_target_container)
            m_surface_offset_target_container->Hide();
        if (m_surface_offset_spin)
            m_surface_offset_spin->Hide();
        surface_offset_units->Hide();
    }
    root->Add(preview_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);

    if (m_bias_mode_enabled) {
        const auto initial_surface_offset_pair =
            mixed_filament_single_surface_offset_pair(m_mf, initial_surface_offset_value, m_nozzle_diameters);
        m_mf.component_a_surface_offset = initial_surface_offset_pair.first;
        m_mf.component_b_surface_offset = initial_surface_offset_pair.second;
    }

    const bool initial_component_surface_offsets_supported = m_bias_mode_enabled &&
                                                             !pattern_row_mode &&
                                                             row_distribution_mode != int(MixedFilament::SameLayerPointillisme) &&
                                                             !m_preview_settings.local_z_mode;
    if (m_surface_offset_spin)
        m_surface_offset_spin->Enable(initial_component_surface_offsets_supported);

    const bool local_z_limit_supported = multi_gradient_row &&
                                         row_distribution_mode != int(MixedFilament::SameLayerPointillisme);
    if (local_z_limit_supported) {
        auto *local_z_limit_row = new wxBoxSizer(wxHORIZONTAL);
        m_local_z_limit_checkbox = new wxCheckBox(this, wxID_ANY, _L("Limit Local-Z"));
        m_local_z_limit_checkbox->SetValue(m_mf.local_z_max_sublayers >= 2);
        m_local_z_limit_checkbox->SetForegroundColour(is_dark ? wxColour(236, 236, 236) : wxColour(20, 20, 20));
        m_local_z_limit_checkbox->SetToolTip(
            _L("Store a per-color Local-Z cadence cap. It applies when Local-Z dithering mode is enabled in print settings."));
        local_z_limit_row->Add(m_local_z_limit_checkbox, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);

        auto *local_z_limit_label = new wxStaticText(this, wxID_ANY, _L("Max sublayers"));
        local_z_limit_label->SetForegroundColour(is_dark ? wxColour(236, 236, 236) : wxColour(20, 20, 20));
        local_z_limit_row->Add(local_z_limit_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, std::max(FromDIP(3), gap / 2));

        const int initial_local_z_limit = std::max(2, m_mf.local_z_max_sublayers > 0 ? m_mf.local_z_max_sublayers : 6);
        m_local_z_limit_spin = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(72), -1),
                                              wxSP_ARROW_KEYS | wxALIGN_RIGHT | wxTE_PROCESS_ENTER, 2, 999, initial_local_z_limit);
        m_local_z_limit_spin->SetToolTip(
            _L("Maximum number of Local-Z sublayers this color may use before its cadence repeats."));
        local_z_limit_row->Add(m_local_z_limit_spin, 0, wxALIGN_CENTER_VERTICAL);

        const bool enable_local_z_limit_controls = m_local_z_limit_checkbox->GetValue();
        m_local_z_limit_spin->Enable(enable_local_z_limit_controls);
        root->Add(local_z_limit_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);
    }

    m_breakdown_label = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_breakdown_label->SetForegroundColour(is_dark ? wxColour(210, 210, 210) : wxColour(72, 72, 72));
    m_breakdown_label->Wrap(FromDIP(360));
    root->Add(m_breakdown_label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, gap);

    // Bind events
    // Preserve a loaded legacy value until the user successfully submits a valid replacement.
    auto last_accepted_manual_pattern = std::make_shared<std::string>(normalized_pattern);
    auto apply_changes = [this, last_accepted_manual_pattern]() {
        std::string                   raw_manual_pattern;
        std::string                   normalized_manual_pattern;
        ManualPatternValidationResult manual_pattern_validation;
        if (m_pattern_ctrl) {
            raw_manual_pattern        = into_u8(m_pattern_ctrl->GetValue());
            MixedFilament edited_pattern = m_mf;
            edited_pattern.manual_pattern = raw_manual_pattern;
            normalized_manual_pattern = MixedFilamentManager::normalize_manual_pattern(edited_pattern);
            if (normalized_manual_pattern.empty()) {
                m_pattern_ctrl->ChangeValue(from_u8(*last_accepted_manual_pattern));
                return false;
            }

            edited_pattern.manual_pattern = normalized_manual_pattern;
            manual_pattern_validation =
                validate_manual_pattern_filament_types(edited_pattern, m_num_physical);
            if (manual_pattern_validation.error == ManualPatternValidationError::MissingSameType) {
                show_mixed_filament_type_toast(
                    _L("No other consumables of the same type are available for mixing. Please add consumables of the same type."));
                m_pattern_ctrl->ChangeValue(from_u8(*last_accepted_manual_pattern));
                return false;
            }
            if (manual_pattern_validation.error == ManualPatternValidationError::TypeMismatch) {
                show_mixed_filament_type_toast(
                    _L("Consumable types in the current mixing scheme are inconsistent. Please select another mixing scheme."));
                m_pattern_ctrl->ChangeValue(from_u8(*last_accepted_manual_pattern));
                return false;
            }
            if (manual_pattern_validation.error != ManualPatternValidationError::None) {
                m_pattern_ctrl->ChangeValue(from_u8(*last_accepted_manual_pattern));
                return false;
            }
        }

        m_has_changes = true;

        double surface_offset_value = 0.0;
        if (m_surface_offset_spin) {
            surface_offset_value = m_surface_offset_spin->GetValue();
#if !defined(wxHAS_NATIVE_SPINCTRLDOUBLE)
            if (wxTextCtrl *text = m_surface_offset_spin->GetText()) {
                double parsed_value = 0.0;
                if (text->GetValue().ToDouble(&parsed_value))
                    surface_offset_value = parsed_value;
            }
#endif
        }

        int a = std::clamp(m_choice_a->GetSelection() + 1, 1, int(m_num_physical));
        int b = std::clamp(m_choice_b->GetSelection() + 1, 1, int(m_num_physical));
        if (m_pattern_ctrl && m_mf.ui_mode != 1) {
            a = int(manual_pattern_validation.component_a);
            b = int(manual_pattern_validation.component_b);
            m_choice_a->SetSelection(a - 1);
            m_choice_b->SetSelection(b - 1);
        }
        if (a == b && m_num_physical > 1) {
            const std::vector<std::string> identities = current_editor_physical_filament_identities(m_num_physical);
            b = 0;
            if (size_t(a - 1) < identities.size() && !identities[size_t(a - 1)].empty()) {
                for (size_t i = 0; i < identities.size(); ++i) {
                    if (int(i + 1) != a && identities[i] == identities[size_t(a - 1)]) {
                        b = int(i + 1);
                        break;
                    }
                }
            }
            if (b == 0)
                b = (a == int(m_num_physical)) ? 1 : a + 1;
            m_choice_b->SetSelection(b - 1);
        }
        update_component_picker_visuals();

        if (m_local_z_limit_spin)
            m_local_z_limit_spin->Enable(m_local_z_limit_checkbox != nullptr &&
                                         m_local_z_limit_checkbox->GetValue());

        m_mf.component_a = unsigned(a);
        m_mf.component_b = unsigned(b);
        if (m_bias_mode_enabled) {
            const double bias_limit = mixed_filament_bias_limit_mm(m_mf, m_nozzle_diameters);
            const float clamped_surface_offset_value = std::clamp(float(surface_offset_value), -float(bias_limit), float(bias_limit));
            const auto surface_offset_pair =
                mixed_filament_single_surface_offset_pair(m_mf, clamped_surface_offset_value, m_nozzle_diameters);
            m_mf.component_a_surface_offset = surface_offset_pair.first;
            m_mf.component_b_surface_offset = surface_offset_pair.second;
            if (m_surface_offset_spin)
                m_surface_offset_spin->SetValue(clamped_surface_offset_value);
        }
        m_mf.local_z_max_sublayers =
            (m_local_z_limit_checkbox != nullptr && m_local_z_limit_checkbox->GetValue() && m_local_z_limit_spin != nullptr) ?
                std::max(2, m_local_z_limit_spin->GetValue()) :
                0;

        bool simple_mode = true;
        bool same_layer_mode = false;
        int preview_mix_b_percent = std::clamp(m_mf.mix_b_percent, 0, 100);
        std::vector<unsigned int> preview_sequence;

        if (m_pattern_ctrl) {
            m_mf.distribution_mode = int(MixedFilament::Simple);
            if (raw_manual_pattern != normalized_manual_pattern)
                m_pattern_ctrl->ChangeValue(from_u8(normalized_manual_pattern));
            m_mf.manual_pattern = normalized_manual_pattern;
            *last_accepted_manual_pattern = normalized_manual_pattern;
            m_mf.mix_b_percent = MixedFilamentManager::mix_percent_from_manual_pattern(m_mf, m_num_physical);
            preview_mix_b_percent = m_mf.mix_b_percent;
            m_mf.pointillism_all_filaments = false;
            m_mf.gradient_component_ids.clear();
            m_mf.gradient_component_weights.clear();
            preview_sequence = decode_manual_pattern_ids(m_mf, m_num_physical, m_preview_settings.wall_loops);
        } else {
            std::vector<unsigned int> selected_ids;
            selected_ids.reserve(4);
            auto add_unique = [&selected_ids](unsigned int id) {
                if (id == 0) return;
                if (std::find(selected_ids.begin(), selected_ids.end(), id) == selected_ids.end())
                    selected_ids.emplace_back(id);
            };
            add_unique(unsigned(a));
            add_unique(unsigned(b));
            if (m_choice_c && m_choice_c->GetSelection() > 0)
                add_unique(unsigned(m_choice_c->GetSelection()));
            if (m_choice_d && m_choice_d->GetSelection() > 0)
                add_unique(unsigned(m_choice_d->GetSelection()));
            const bool multi_gradient_mode = selected_ids.size() >= 3;
            m_mf.distribution_mode = multi_gradient_mode ? int(MixedFilament::LayerCycle) : int(MixedFilament::Simple);
            simple_mode = m_mf.distribution_mode == int(MixedFilament::Simple);
            m_mf.mix_b_percent = std::clamp(m_blend_selector ? m_blend_selector->value() : 50, 0, 100);
            m_mf.manual_pattern.clear();
            m_mf.pointillism_all_filaments = false;

            const wxColour color_a = (a >= 1 && a <= int(m_palette.size())) ? m_palette[size_t(a - 1)] : wxColour("#26A69A");
            const wxColour color_b = (b >= 1 && b <= int(m_palette.size())) ? m_palette[size_t(b - 1)] : wxColour("#26A69A");
            if (m_blend_selector) {
                if (!simple_mode && multi_gradient_mode) {
                    std::vector<wxColour> corner_colors;
                    corner_colors.reserve(selected_ids.size());
                    for (const unsigned int id : selected_ids) {
                        if (id >= 1 && id <= m_palette.size())
                            corner_colors.emplace_back(m_palette[id - 1]);
                    }
                    if (corner_colors.size() >= 3)
                        m_blend_selector->set_multi_preview(corner_colors, *m_selected_weight_state);
                    else
                        m_blend_selector->set_colors(color_a, color_b);
                } else {
                    m_blend_selector->set_colors(color_a, color_b);
                }
            }

            if (multi_gradient_mode) {
                const std::vector<int> decoded_weights =
                    decode_gradient_weights(m_mf.gradient_component_weights, selected_ids.size());
                if (m_selected_weight_state->size() != selected_ids.size())
                    *m_selected_weight_state = decoded_weights;
                *m_selected_weight_state = normalize_gradient_weights(*m_selected_weight_state, selected_ids.size());
                m_mf.gradient_component_ids = encode_gradient_ids(selected_ids);
                m_mf.gradient_component_weights = encode_gradient_weights(*m_selected_weight_state);
                preview_sequence = build_weighted_multi_sequence(selected_ids, *m_selected_weight_state);
            } else {
                m_mf.gradient_component_ids.clear();
                m_mf.gradient_component_weights.clear();
                preview_mix_b_percent = effective_local_z_preview_mix_b_percent(m_mf, m_preview_settings);
                preview_sequence = build_weighted_pair_sequence(m_mf.component_a, m_mf.component_b, preview_mix_b_percent, same_layer_mode);
            }
        }
        m_mf.custom = true;

        const std::vector<unsigned int> selected_gradient_ids = decode_gradient_ids(m_mf.gradient_component_ids);
        const bool component_surface_offsets_supported = m_bias_mode_enabled &&
                                                         (m_pattern_ctrl == nullptr) &&
                                                         !same_layer_mode &&
                                                         !m_preview_settings.local_z_mode;
        if (m_surface_offset_spin)
            m_surface_offset_spin->Enable(component_surface_offsets_supported);
        if (preview_sequence.empty())
            preview_sequence = build_weighted_pair_sequence(m_mf.component_a, m_mf.component_b, preview_mix_b_percent, same_layer_mode);

        if (m_blend_selector && selected_gradient_ids.size() >= 3) {
            std::vector<wxColour> corner_colors;
            corner_colors.reserve(selected_gradient_ids.size());
            for (const unsigned int id : selected_gradient_ids) {
                if (id >= 1 && id <= m_palette.size())
                    corner_colors.emplace_back(m_palette[id - 1]);
            }
            if (corner_colors.size() >= 3)
                m_blend_selector->set_multi_preview(corner_colors, *m_selected_weight_state);
        }

        if (Slic3r::mixed_filament_supports_bias_apparent_color(m_mf, m_preview_settings, m_bias_mode_enabled) &&
            m_mf.component_a >= 1 && m_mf.component_b >= 1 &&
            m_mf.component_a <= m_physical_colors.size() && m_mf.component_b <= m_physical_colors.size()) {
            const auto [apparent_pct_a, apparent_pct_b] =
                Slic3r::mixed_filament_apparent_pair_percentages(m_mf, m_preview_settings, m_nozzle_diameters, m_bias_mode_enabled);
            m_mf.display_color = MixedFilamentManager::blend_color(
                m_physical_colors[size_t(m_mf.component_a - 1)],
                m_physical_colors[size_t(m_mf.component_b - 1)],
                apparent_pct_a,
                apparent_pct_b);
        } else if (m_mf.ui_mode == 1 && !m_mf.manual_pattern.empty()) {
            const MixedFilamentDisplayContext display_context {
                m_num_physical, m_physical_colors, m_nozzle_diameters, m_preview_settings, m_bias_mode_enabled
            };
            m_mf.display_color = Slic3r::compute_mixed_filament_display_color(m_mf, display_context);
        } else if (selected_gradient_ids.size() >= 3 || !preview_sequence.empty()) {
            m_mf.display_color = blend_from_sequence(m_physical_colors, preview_sequence, "#26A69A");
            if (m_blend_label) {
                if (selected_gradient_ids.size() >= 3) {
                    m_blend_label->SetLabel(wxString::Format(_L("%d-color layer cycle"), int(selected_gradient_ids.size())));
                } else {
                    m_blend_label->SetLabel(wxString::Format(simple_mode ? _L("Simple %d%%/%d%%") : _L("%d%%/%d%%"),
                                                            100 - preview_mix_b_percent, preview_mix_b_percent));
                }
            }
        } else {
            m_mf.display_color = MixedFilamentManager::blend_color(
                m_physical_colors[size_t(a - 1)], m_physical_colors[size_t(b - 1)],
                100 - preview_mix_b_percent, preview_mix_b_percent);
            if (m_blend_label)
                m_blend_label->SetLabel(wxString::Format(simple_mode ? _L("Simple %d%%/%d%%") : _L("%d%%/%d%%"),
                                                        100 - preview_mix_b_percent, preview_mix_b_percent));
        }

        if (m_mix_preview) {
            const std::string bias_summary =
                mixed_filament_apparent_pair_summary(m_mf, m_preview_settings, m_nozzle_diameters, m_bias_mode_enabled);
            const std::string summary = bias_summary.empty() ? summarize_sequence(preview_sequence) : bias_summary;
            std::vector<double> preview_surface_offsets(m_palette.size() + 1, 0.0);
            if (m_bias_mode_enabled && m_mf.component_a >= 1 && m_mf.component_a < preview_surface_offsets.size())
                preview_surface_offsets[m_mf.component_a] = double(m_mf.component_a_surface_offset);
            if (m_bias_mode_enabled && m_mf.component_b >= 1 && m_mf.component_b < preview_surface_offsets.size())
                preview_surface_offsets[m_mf.component_b] = double(m_mf.component_b_surface_offset);
            m_mix_preview->set_data(m_palette, preview_sequence, same_layer_mode, preview_surface_offsets, wxColour(m_mf.display_color),
                                    _L("Preview"), summary.empty() ? wxString() : from_u8(summary));
        }
        update_local_z_breakdown();
        if (m_swatch) {
            m_swatch->SetBackgroundColour(wxColour(m_mf.display_color));
            m_swatch->Refresh();
        }
        if (m_on_change)
            m_on_change(m_mf);
        return true;
    };

    auto make_color_chip_bitmap = [this](const wxColour &color) {
        const int chip_size = FromDIP(14);
        wxBitmap bmp(chip_size, chip_size);
        wxMemoryDC dc(bmp);
        dc.SetBackground(wxBrush(wxColour(255, 255, 255)));
        dc.Clear();
        dc.SetPen(wxPen(wxColour(120, 120, 120)));
        dc.SetBrush(wxBrush(color));
        dc.DrawRectangle(0, 0, chip_size, chip_size);
        dc.SelectObject(wxNullBitmap);
        return bmp;
    };

    auto is_optional_component_choice = [this](const wxChoice *choice) {
        return choice != nullptr && choice->GetCount() == unsigned(m_num_physical + 1);
    };

    auto filament_id_from_selection = [this, is_optional_component_choice](const wxChoice *choice, int selection) -> unsigned int {
        if (choice == nullptr || selection < 0)
            return 0;
        if (is_optional_component_choice(choice))
            return selection == 0 ? 0u : unsigned(selection);
        return unsigned(selection + 1);
    };

    auto set_choice_to_filament_id = [this, is_optional_component_choice](wxChoice *choice, unsigned int filament_id) {
        if (choice == nullptr)
            return;
        if (is_optional_component_choice(choice)) {
            const int selection = int(std::min<unsigned int>(filament_id, unsigned(m_num_physical)));
            choice->SetSelection(selection);
        } else if (filament_id >= 1 && filament_id <= m_num_physical) {
            choice->SetSelection(int(filament_id - 1));
        }
    };

    auto validate_component_picker_selection = [this,
                                               filament_id_from_selection,
                                               set_choice_to_filament_id](wxChoice *backing_choice, int selection) {
        if (backing_choice == nullptr)
            return false;

        const std::vector<std::string> identities = current_editor_physical_filament_identities(m_num_physical);
        auto filament_identity = [&identities](unsigned int filament_id) -> std::string {
            if (filament_id == 0 || filament_id > identities.size())
                return {};
            return identities[size_t(filament_id - 1)];
        };

        if (backing_choice == m_choice_a) {
            const unsigned int first_id   = filament_id_from_selection(backing_choice, selection);
            const std::string  first_type = filament_identity(first_id);
            std::vector<unsigned int> same_type_ids;
            if (!first_type.empty()) {
                for (size_t i = 0; i < identities.size(); ++i) {
                    const unsigned int filament_id = unsigned(i + 1);
                    if (filament_id != first_id && identities[i] == first_type)
                        same_type_ids.emplace_back(filament_id);
                }
            }

            if (same_type_ids.empty()) {
                show_mixed_filament_type_toast(_L("No other consumables of the same type are available for mixing. Please add consumables of the same type."));
                return false;
            }

            backing_choice->SetSelection(selection);
            size_t next_same_type_id = 0;
            set_choice_to_filament_id(m_choice_b, same_type_ids[next_same_type_id++]);
            if (m_choice_c != nullptr && m_choice_c->GetSelection() > 0) {
                if (next_same_type_id < same_type_ids.size())
                    set_choice_to_filament_id(m_choice_c, same_type_ids[next_same_type_id++]);
                else
                    m_choice_c->SetSelection(0);
            }
            if (m_choice_d != nullptr && m_choice_d->GetSelection() > 0) {
                if (next_same_type_id < same_type_ids.size())
                    set_choice_to_filament_id(m_choice_d, same_type_ids[next_same_type_id++]);
                else
                    m_choice_d->SetSelection(0);
            }
            return true;
        }

        const unsigned int selected_id = filament_id_from_selection(backing_choice, selection);
        if (selected_id == 0) {
            backing_choice->SetSelection(selection);
            return true;
        }

        const unsigned int first_id   = filament_id_from_selection(m_choice_a, m_choice_a ? m_choice_a->GetSelection() : wxNOT_FOUND);
        const std::string  first_type = filament_identity(first_id);
        const std::string  selected_type = filament_identity(selected_id);
        if (selected_id == first_id)
            return false;
        if (first_type.empty() || selected_type.empty() || selected_type != first_type) {
            show_mixed_filament_type_toast(_L("Consumable types are inconsistent. Please add consumables of the same type."));
            return false;
        }

        backing_choice->SetSelection(selection);
        return true;
    };

    auto bind_component_picker_popup = [this, apply_changes, make_color_chip_bitmap, validate_component_picker_selection](wxWindow *target, wxChoice *backing_choice) {
        if (!target || !backing_choice)
            return;

        target->Bind(wxEVT_LEFT_UP, [this, apply_changes, make_color_chip_bitmap, validate_component_picker_selection, backing_choice](wxMouseEvent &) {
            if (m_num_physical == 0)
                return;

            const bool allow_none = backing_choice->GetCount() == unsigned(m_num_physical + 1);
            wxMenu menu;
            std::vector<int> item_ids;
            item_ids.reserve(m_num_physical + (allow_none ? 1 : 0));
            if (allow_none) {
                const int item_id = wxWindow::NewControlId();
                item_ids.emplace_back(item_id);
                menu.Append(item_id, backing_choice->GetSelection() == 0 ? _L("None (Selected)") : _L("None"));
            }
            for (size_t i = 0; i < m_num_physical; ++i) {
                const int item_id = wxWindow::NewControlId();
                item_ids.emplace_back(item_id);
                const int selection_index = allow_none ? int(i + 1) : int(i);
                const bool is_selected = selection_index == backing_choice->GetSelection();
                const wxString item_label = wxString::Format("F%d%s", int(i + 1), is_selected ? " (Selected)" : "");
                auto *menu_item = new wxMenuItem(&menu, item_id, item_label, wxEmptyString, wxITEM_NORMAL);
                const wxColour item_color = (i < m_palette.size()) ? m_palette[i] : wxColour("#26A69A");
                menu_item->SetBitmap(make_color_chip_bitmap(item_color));
                menu.Append(menu_item);
            }

            menu.Bind(wxEVT_COMMAND_MENU_SELECTED, [apply_changes, validate_component_picker_selection, backing_choice, item_ids](wxCommandEvent &evt) {
                const auto it = std::find(item_ids.begin(), item_ids.end(), evt.GetId());
                if (it == item_ids.end())
                    return;
                const int selection = int(std::distance(item_ids.begin(), it));
                if (!validate_component_picker_selection(backing_choice, selection))
                    return;
                apply_changes();
            });
            PopupMenu(&menu);
        });
    };

    bind_component_picker_popup(m_picker_a_container, m_choice_a);
    bind_component_picker_popup(m_picker_a_swatch, m_choice_a);
    bind_component_picker_popup(m_picker_a_label, m_choice_a);
    bind_component_picker_popup(m_picker_b_container, m_choice_b);
    bind_component_picker_popup(m_picker_b_swatch, m_choice_b);
    bind_component_picker_popup(m_picker_b_label, m_choice_b);
    bind_component_picker_popup(m_picker_c_container, m_choice_c);
    bind_component_picker_popup(m_picker_c_swatch, m_choice_c);
    bind_component_picker_popup(m_picker_c_label, m_choice_c);
    bind_component_picker_popup(m_picker_d_container, m_choice_d);
    bind_component_picker_popup(m_picker_d_swatch, m_choice_d);
    bind_component_picker_popup(m_picker_d_label, m_choice_d);

    m_choice_a->Bind(wxEVT_CHOICE, [apply_changes](wxCommandEvent&) { apply_changes(); });
    m_choice_b->Bind(wxEVT_CHOICE, [apply_changes](wxCommandEvent&) { apply_changes(); });
    if (m_choice_c)
        m_choice_c->Bind(wxEVT_CHOICE, [apply_changes](wxCommandEvent&) { apply_changes(); });
    if (m_choice_d)
        m_choice_d->Bind(wxEVT_CHOICE, [apply_changes](wxCommandEvent&) { apply_changes(); });
    if (m_blend_selector)
        m_blend_selector->Bind(wxEVT_SLIDER, [apply_changes](wxCommandEvent&) { apply_changes(); });
    if (m_local_z_limit_checkbox)
        m_local_z_limit_checkbox->Bind(wxEVT_CHECKBOX, [apply_changes](wxCommandEvent &) { apply_changes(); });
    if (m_local_z_limit_spin) {
        m_local_z_limit_spin->Bind(wxEVT_SPINCTRL, [apply_changes](wxCommandEvent &) { apply_changes(); });
        m_local_z_limit_spin->Bind(wxEVT_TEXT_ENTER, [apply_changes](wxCommandEvent &) { apply_changes(); });
        m_local_z_limit_spin->Bind(wxEVT_KILL_FOCUS, [apply_changes](wxFocusEvent &evt) {
            apply_changes();
            evt.Skip();
        });
    }
    if (m_surface_offset_spin) {
        m_surface_offset_spin->Bind(wxEVT_SPINCTRLDOUBLE, [apply_changes](wxSpinDoubleEvent &) { apply_changes(); });
        m_surface_offset_spin->Bind(wxEVT_TEXT_ENTER, [apply_changes](wxCommandEvent &) { apply_changes(); });
        m_surface_offset_spin->Bind(wxEVT_KILL_FOCUS, [apply_changes](wxFocusEvent &evt) {
            apply_changes();
            evt.Skip();
        });
    }

    if (m_blend_selector) {
        m_blend_selector->Bind(wxEVT_BUTTON, [this, apply_changes](wxCommandEvent&) {
            if (!m_blend_selector->is_multi_mode()) return;
            std::vector<unsigned int> selected_ids;
            auto add_unique = [&selected_ids](unsigned int id) { if (id > 0 && std::find(selected_ids.begin(), selected_ids.end(), id) == selected_ids.end()) selected_ids.emplace_back(id); };
            add_unique(unsigned(std::clamp(m_choice_a ? (m_choice_a->GetSelection() + 1) : 0, 1, int(m_num_physical))));
            add_unique(unsigned(std::clamp(m_choice_b ? (m_choice_b->GetSelection() + 1) : 0, 1, int(m_num_physical))));
            if (m_choice_c && m_choice_c->GetSelection() > 0) add_unique(unsigned(m_choice_c->GetSelection()));
            if (m_choice_d && m_choice_d->GetSelection() > 0) add_unique(unsigned(m_choice_d->GetSelection()));
            if (selected_ids.size() < 3) return;
            const std::vector<int> initial_weights = normalize_gradient_weights(*m_selected_weight_state, selected_ids.size());
            MixedGradientWeightsDialog dlg(this, selected_ids, m_palette, initial_weights);
            if (dlg.ShowModal() != wxID_OK) return;
            *m_selected_weight_state = dlg.normalized_weights();
            apply_changes();
        });
    }

    if (m_pattern_ctrl) {
        auto append_pattern_token = [this](int filament_id) {
            if (!m_pattern_ctrl || filament_id <= 0) return;
            std::string pattern = into_u8(m_pattern_ctrl->GetValue());
            if (m_mf.ui_mode != 1 && !pattern.empty()) {
                const char last = pattern.back();
                if (last != ',')
                    pattern.push_back(',');
            }
            pattern += std::to_string(filament_id);
            m_pattern_ctrl->ChangeValue(from_u8(pattern));
        };
        m_pattern_ctrl->Bind(wxEVT_TEXT_ENTER, [apply_changes](wxCommandEvent&) { apply_changes(); });
        m_pattern_ctrl->Bind(wxEVT_KILL_FOCUS, [apply_changes](wxFocusEvent &evt) { apply_changes(); evt.Skip(); });
        for (size_t fid = 0; fid < m_pattern_quick_buttons.size(); ++fid) {
            wxButton *btn = m_pattern_quick_buttons[fid];
            if (btn) {
                const int filament_id = int(fid + 1);
                btn->Bind(wxEVT_BUTTON, [apply_changes, append_pattern_token, filament_id](wxCommandEvent&) {
                    append_pattern_token(filament_id);
                    apply_changes();
                });
            }
        }
    }

    update_component_picker_visuals();
    SetSizer(root);
    Layout();
    SetMinSize(wxSize(-1, GetBestSize().GetHeight()));
    update_preview();
}

void MixedFilamentConfigPanel::update_component_picker_visuals()
{
    auto update_one = [this](wxChoice *choice, wxPanel *container, wxPanel *swatch, wxStaticText *label) {
        if (!choice)
            return;
        int sel = choice->GetSelection();
        const bool allow_none = choice->GetCount() == unsigned(m_num_physical + 1);
        if (sel < 0 && m_num_physical > 0) {
            sel = 0;
            choice->SetSelection(sel);
        }
        if (sel < 0)
            return;

        if (allow_none && sel == 0) {
            const wxColour none_color = wxGetApp().dark_mode() ? wxColour(86, 86, 92) : wxColour(224, 224, 224);
            if (swatch) {
                swatch->SetBackgroundColour(none_color);
                swatch->Refresh();
            }
            if (label)
                label->SetLabel(_L("None"));
            if (container) {
                container->Layout();
                container->Refresh();
            }
            return;
        }

        const int color_idx = allow_none ? sel - 1 : sel;
        const wxColour color = (color_idx >= 0 && size_t(color_idx) < m_palette.size()) ? m_palette[size_t(color_idx)] : wxColour("#26A69A");
        if (swatch) {
            swatch->SetBackgroundColour(color);
            swatch->Refresh();
        }
        if (label)
            label->SetLabel(wxString::Format("F%d", color_idx + 1));
        if (container) {
            container->Layout();
            container->Refresh();
        }
    };

    update_one(m_choice_a, m_picker_a_container, m_picker_a_swatch, m_picker_a_label);
    update_one(m_choice_b, m_picker_b_container, m_picker_b_swatch, m_picker_b_label);
    update_one(m_choice_c, m_picker_c_container, m_picker_c_swatch, m_picker_c_label);
    update_one(m_choice_d, m_picker_d_container, m_picker_d_swatch, m_picker_d_label);

    if (m_surface_offset_target_container || m_surface_offset_target_swatch || m_surface_offset_target_label || m_surface_offset_spin) {
        const int a_filament = std::clamp(m_choice_a ? (m_choice_a->GetSelection() + 1) : int(m_mf.component_a), 1, int(std::max<size_t>(1, m_num_physical)));
        const int b_filament = std::clamp(m_choice_b ? (m_choice_b->GetSelection() + 1) : int(m_mf.component_b), 1, int(std::max<size_t>(1, m_num_physical)));
        MixedFilament active_pair = m_mf;
        active_pair.component_a = unsigned(a_filament);
        active_pair.component_b = unsigned(b_filament);
        double signed_bias_value = mixed_filament_single_surface_offset_value(active_pair, m_nozzle_diameters);

        if (m_surface_offset_spin && m_bias_mode_enabled) {
            const double bias_limit = mixed_filament_bias_limit_mm(active_pair, m_nozzle_diameters);
            m_surface_offset_spin->SetRange(-bias_limit, bias_limit);
            signed_bias_value = m_surface_offset_spin->GetValue();
        }

        const int active_filament = signed_bias_value < -EPSILON ? a_filament : b_filament;
        const int color_idx = active_filament - 1;
        const wxColour color = (color_idx >= 0 && size_t(color_idx) < m_palette.size()) ? m_palette[size_t(color_idx)] : wxColour("#26A69A");
        if (m_surface_offset_target_swatch) {
            m_surface_offset_target_swatch->SetBackgroundColour(color);
            m_surface_offset_target_swatch->Refresh();
        }
        if (m_surface_offset_target_label)
            m_surface_offset_target_label->SetLabel(wxString::Format("F%d", active_filament));
        if (m_surface_offset_target_container) {
            m_surface_offset_target_container->Layout();
            m_surface_offset_target_container->Refresh();
        }
    }
}

void MixedFilamentConfigPanel::update_preview()
{
    const bool simple_mode = m_mf.distribution_mode == int(MixedFilament::Simple);
    const bool same_layer_mode = m_mf.distribution_mode == int(MixedFilament::SameLayerPointillisme);
    const std::string normalized_pattern = MixedFilamentManager::normalize_manual_pattern(m_mf);
    const bool pattern_row_mode = !normalized_pattern.empty();

    std::vector<unsigned int> initial_sequence;
    if (pattern_row_mode) {
        initial_sequence = decode_manual_pattern_ids(m_mf, m_num_physical, m_preview_settings.wall_loops);
    } else {
        std::vector<unsigned int> initial_gradient_ids = simple_mode ? std::vector<unsigned int>() : decode_gradient_ids(m_mf.gradient_component_ids);
        if (initial_gradient_ids.size() >= 3)
            initial_sequence = build_weighted_multi_sequence(initial_gradient_ids, *m_selected_weight_state);
        else
            initial_sequence = build_weighted_pair_sequence(m_mf.component_a,
                                                            m_mf.component_b,
                                                            effective_local_z_preview_mix_b_percent(m_mf, m_preview_settings),
                                                            same_layer_mode);

        if (m_blend_selector && initial_gradient_ids.size() >= 3) {
            std::vector<wxColour> corner_colors;
            corner_colors.reserve(initial_gradient_ids.size());
            for (const unsigned int id : initial_gradient_ids) {
                if (id >= 1 && id <= m_palette.size())
                    corner_colors.emplace_back(m_palette[id - 1]);
            }
            if (corner_colors.size() >= 3)
                m_blend_selector->set_multi_preview(corner_colors, *m_selected_weight_state);
        }
    }

    if (m_mix_preview) {
        const MixedFilamentDisplayContext display_context {
            m_num_physical, m_physical_colors, m_nozzle_diameters, m_preview_settings, m_bias_mode_enabled
        };
        m_mf.display_color = Slic3r::compute_mixed_filament_display_color(m_mf, display_context);

        const std::string bias_summary =
            mixed_filament_apparent_pair_summary(m_mf, m_preview_settings, m_nozzle_diameters, m_bias_mode_enabled);
        const std::string summary = bias_summary.empty() ? summarize_sequence(initial_sequence) : bias_summary;
        std::vector<double> preview_surface_offsets(m_palette.size() + 1, 0.0);
        if (m_bias_mode_enabled && m_mf.component_a >= 1 && m_mf.component_a < preview_surface_offsets.size())
            preview_surface_offsets[m_mf.component_a] = double(m_mf.component_a_surface_offset);
        if (m_bias_mode_enabled && m_mf.component_b >= 1 && m_mf.component_b < preview_surface_offsets.size())
            preview_surface_offsets[m_mf.component_b] = double(m_mf.component_b_surface_offset);
        m_mix_preview->set_data(m_palette, initial_sequence, same_layer_mode, preview_surface_offsets, wxColour(m_mf.display_color),
                                _L("Preview"), summary.empty() ? wxString() : from_u8(summary));
    }
    update_local_z_breakdown();
}

void MixedFilamentConfigPanel::update_local_z_breakdown()
{
    if (!m_breakdown_label)
        return;

    std::vector<int> weights = *m_selected_weight_state;
    const std::vector<unsigned int> ids = decode_gradient_ids(m_mf.gradient_component_ids);
    if (!ids.empty())
        weights = normalize_gradient_weights(weights, ids.size());

    const wxString breakdown = summarize_local_z_breakdown(m_mf, weights, m_preview_settings);
    m_breakdown_label->SetLabel(breakdown);
    m_breakdown_label->Wrap(FromDIP(360));
    m_breakdown_label->Show(!breakdown.empty());
    Layout();
}

} } // namespace Slic3r::GUI
