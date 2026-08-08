#include "ParameterUtils.hpp"
#include <algorithm>
#include <cassert>

namespace Slic3r {

std::vector<LayerPrintSequence> get_other_layers_print_sequence(int sequence_nums, const std::vector<int> &sequence)
{
    std::vector<LayerPrintSequence> res;
    if (sequence_nums == 0 || sequence.empty())
        return res;

    assert(sequence.size() % sequence_nums == 0);

    res.reserve(sequence_nums);
    size_t item_nums = sequence.size() / sequence_nums;

    for (int i = 0; i < sequence_nums; ++i) {
        std::vector<int> item;
        item.assign(sequence.begin() + i * item_nums, sequence.begin() + ((i + 1) * item_nums));

        assert(item.size() > 2);
        std::pair<std::pair<int, int>, std::vector<int>> res_item;
        res_item.first.first  = item[0];
        res_item.first.second = item[1];
        res_item.second.assign(item.begin() + 2, item.end());
        res.emplace_back(std::move(res_item));
    }

    return res;
}

void get_other_layers_print_sequence(const std::vector<LayerPrintSequence> &customize_sequences, int &sequence_nums, std::vector<int> &sequence)
{
    sequence_nums = 0;
    sequence.clear();
    if (customize_sequences.empty()) { return; }

    sequence_nums = (int) customize_sequences.size();
    for (const auto &customize_sequence : customize_sequences) {
        sequence.push_back(customize_sequence.first.first);
        sequence.push_back(customize_sequence.first.second);
        sequence.insert(sequence.end(), customize_sequence.second.begin(), customize_sequence.second.end());
    }
}

bool remap_and_normalize_filament_sequence(std::vector<int> &sequence,
                                           size_t total_filaments,
                                           const std::vector<unsigned int> &filament_id_remap)
{
    std::vector<int> normalized;
    normalized.reserve(total_filaments);
    std::vector<bool> seen(total_filaments + 1, false);

    for (const int old_id : sequence) {
        unsigned int mapped = 0;
        if (filament_id_remap.empty()) {
            if (old_id > 0)
                mapped = unsigned(old_id);
        } else if (old_id > 0 && size_t(old_id) < filament_id_remap.size()) {
            mapped = filament_id_remap[size_t(old_id)];
        }

        if (mapped == 0 || mapped > total_filaments || seen[mapped])
            continue;
        seen[mapped] = true;
        normalized.emplace_back(int(mapped));
    }

    for (size_t filament_id = 1; filament_id <= total_filaments; ++filament_id) {
        if (!seen[filament_id])
            normalized.emplace_back(int(filament_id));
    }

    if (normalized == sequence)
        return false;
    sequence = std::move(normalized);
    return true;
}

int get_index_for_extruder_parameter(const DynamicPrintConfig &config, const std::string &opt_key, int cur_extruder_id, ExtruderType extruder_type, NozzleVolumeType nozzle_volume_type)
{
    std::string  id_name, variant_name;
    unsigned int stride = 1;
    if (printer_options_with_variant_1.count(opt_key) > 0) { // printer parameter
        id_name      = "printer_extruder_id";
        variant_name = "printer_extruder_variant";
    } else if (printer_options_with_variant_2.count(opt_key) > 0) {
        id_name      = "printer_extruder_id";
        variant_name = "printer_extruder_variant";
        stride       = 2;
    } else if (filament_options_with_variant.count(opt_key) > 0) {
        // filament don't use id anymore
        // id_name      = "filament_extruder_id";
        variant_name = "filament_extruder_variant";
    } else if (print_options_with_variant.count(opt_key) > 0) {
        id_name      = "print_extruder_id";
        variant_name = "print_extruder_variant";
    } else {
        return 0;
    }

    // variant index
    int variant_index = config.get_index_for_extruder(cur_extruder_id + 1, id_name, extruder_type, nozzle_volume_type, variant_name, stride);
    if (variant_index < 0) {
        assert(false);
        return 0;
    }

    return variant_index;
}

}; // namespace Slic3r
