// Tests for mixed-filament project-config round-trip persistence.
//
// The full-3MF (store_bbs_3mf / load_bbs_3mf) round-trip requires a heavy
// PlateDataPtrs + Model + PresetBundle setup that is not yet scaffolded in
// fff_print tests.  We therefore exercise the *persistence layer* directly:
//
//   MixedFilamentManager::serialize_custom_entries()
//     → stored in project_config["mixed_filament_definitions"]
//     → PresetBundle::sync_mixed_filaments_to_config()   (mirrors store path)
//     → PresetBundle::sync_mixed_filaments_from_config() (mirrors load path)
//
// This is the code path that bbs_3mf.cpp invokes when writing/reading the
// project config block, so a regression here would break 3MF persistence.
//
#include <catch2/catch_all.hpp>

#include "libslic3r/MixedFilament.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "test_data.hpp"

#include <boost/filesystem.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace Slic3r;

// ---------------------------------------------------------------------------
// Helper: build a two-filament PresetBundle with colours set.
// ---------------------------------------------------------------------------
namespace {

static PresetBundle make_bundle_2(const std::string &col_a = "#FF0000",
                                  const std::string &col_b = "#0000FF")
{
    PresetBundle bundle;
    bundle.filament_presets = {"Default Filament", "Default Filament"};
    bundle.project_config.option<ConfigOptionStrings>("filament_colour")->values = {col_a, col_b};
    return bundle;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test 1 - empty definitions do not create implicit mixed rows
//   Build bundle -> sync to config string -> reload in fresh bundle -> compare
// ---------------------------------------------------------------------------
TEST_CASE("Mixed filament 3MF round-trip: empty definitions do not auto-create entries",
          "[MixedFilamentRoundTrip]")
{
    PresetBundle origin = make_bundle_2();
    origin.sync_mixed_filaments_from_config();

    const auto &orig_mixed = origin.mixed_filaments.mixed_filaments();
    REQUIRE(orig_mixed.empty());

    // Sync to config (mirrors what store_bbs_3mf does).
    origin.sync_mixed_filaments_to_config();
    const std::string serialized = origin.project_config.opt_string("mixed_filament_definitions");
    CHECK(serialized.empty());

    // Reload into a fresh bundle with the same colours.
    PresetBundle loaded = make_bundle_2();
    loaded.project_config.option<ConfigOptionString>("mixed_filament_definitions")->value = serialized;
    loaded.sync_mixed_filaments_from_config();

    const auto &load_mixed = loaded.mixed_filaments.mixed_filaments();
    CHECK(load_mixed.empty());
    CHECK(loaded.mixed_filaments.total_filaments(loaded.filament_presets.size()) == loaded.filament_presets.size());
}

TEST_CASE("Mixed filament 3MF round-trip: empty definitions clear stale manager rows",
          "[MixedFilamentRoundTrip]")
{
    PresetBundle bundle = make_bundle_2();
    const auto &colors = bundle.project_config.option<ConfigOptionStrings>("filament_colour")->values;
    bundle.mixed_filaments.add_custom_filament(1, 2, 50, colors);
    REQUIRE(!bundle.mixed_filaments.mixed_filaments().empty());

    bundle.project_config.option<ConfigOptionString>("mixed_filament_definitions")->value.clear();
    bundle.sync_mixed_filaments_from_config();
    CHECK(bundle.mixed_filaments.mixed_filaments().empty());
    CHECK(bundle.mixed_filaments.total_filaments(bundle.filament_presets.size()) == bundle.filament_presets.size());

    bundle.sync_mixed_filaments_to_config();
    CHECK(bundle.project_config.opt_string("mixed_filament_definitions").empty());
}

// ---------------------------------------------------------------------------
// Test 2 — custom entry round-trip
//   2 physical + 1 custom mixed entry: ratio_a, ratio_b, mix_b_percent, stable_id
// ---------------------------------------------------------------------------
TEST_CASE("Mixed filament 3MF round-trip: custom entry count, components, ratio, stable_id",
          "[MixedFilamentRoundTrip]")
{
    const std::vector<std::string> colors = {"#FF0000", "#00FF00"};

    MixedFilamentManager mgr;
    // Add two custom entries to exercise the multi-entry path.
    mgr.add_custom_filament(1, 2, 25, colors);
    mgr.add_custom_filament(1, 2, 75, colors);

    const auto &entries = mgr.mixed_filaments();
    REQUIRE(entries.size() == 2);

    const uint64_t stable_id_0 = entries[0].stable_id;
    const uint64_t stable_id_1 = entries[1].stable_id;
    CHECK(stable_id_0 != stable_id_1);

    // Serialize and reload.
    const std::string serialized = mgr.serialize_custom_entries();
    REQUIRE(!serialized.empty());

    MixedFilamentManager loaded;
    loaded.load_custom_entries(serialized, colors);

    const auto &reloaded = loaded.mixed_filaments();
    REQUIRE(reloaded.size() == 2);

    // Order must be preserved.
    CHECK(reloaded[0].component_a  == 1);
    CHECK(reloaded[0].component_b  == 2);
    CHECK(reloaded[0].mix_b_percent == 25);
    CHECK(reloaded[0].stable_id    == stable_id_0);

    CHECK(reloaded[1].component_a  == 1);
    CHECK(reloaded[1].component_b  == 2);
    CHECK(reloaded[1].mix_b_percent == 75);
    CHECK(reloaded[1].stable_id    == stable_id_1);
}

// ---------------------------------------------------------------------------
// Test 3 — PresetBundle project_config string path (mirrors 3MF store+load)
//   Verify that sync_mixed_filaments_to_config + sync_mixed_filaments_from_config
//   preserves a custom entry end-to-end through the project_config string.
// ---------------------------------------------------------------------------
TEST_CASE("Mixed filament 3MF round-trip: PresetBundle project_config string path",
          "[MixedFilamentRoundTrip]")
{
    PresetBundle origin = make_bundle_2("#FFFF00", "#FF00FF");
    origin.sync_mixed_filaments_from_config();

    // Add a custom entry explicitly.
    const auto &colors = origin.project_config.option<ConfigOptionStrings>("filament_colour")->values;
    origin.mixed_filaments.add_custom_filament(1, 2, 40, colors);

    const size_t num_entries_before = origin.mixed_filaments.mixed_filaments().size();

    // Count custom entries in origin.
    size_t custom_count_before = 0;
    uint64_t custom_stable_id = 0;
    for (const auto &mf : origin.mixed_filaments.mixed_filaments()) {
        if (mf.custom && mf.enabled && !mf.deleted) {
            ++custom_count_before;
            custom_stable_id = mf.stable_id;
        }
    }
    REQUIRE(custom_count_before == 1);

    // Sync to config string — mirrors bbs_3mf store path.
    origin.sync_mixed_filaments_to_config();
    const std::string defs = origin.project_config.opt_string("mixed_filament_definitions");
    REQUIRE(!defs.empty());

    // Reload — mirrors bbs_3mf load path.
    PresetBundle loaded = make_bundle_2("#FFFF00", "#FF00FF");
    loaded.project_config.option<ConfigOptionString>("mixed_filament_definitions")->value = defs;
    loaded.sync_mixed_filaments_from_config();

    const auto &reloaded = loaded.mixed_filaments.mixed_filaments();
    REQUIRE(reloaded.size() == num_entries_before);

    // Custom entry must survive with its stable_id intact.
    size_t custom_count_after = 0;
    uint64_t reloaded_stable_id = 0;
    for (const auto &mf : reloaded) {
        if (mf.custom && mf.enabled && !mf.deleted) {
            ++custom_count_after;
            reloaded_stable_id = mf.stable_id;
        }
    }
    CHECK(custom_count_after == custom_count_before);
    CHECK(reloaded_stable_id == custom_stable_id);
}

// ---------------------------------------------------------------------------
// Test 4 — total_filaments counts physical + mixed correctly after round-trip
// ---------------------------------------------------------------------------
TEST_CASE("Mixed filament 3MF round-trip: total_filaments count is stable after reload",
          "[MixedFilamentRoundTrip]")
{
    PresetBundle origin = make_bundle_2();
    origin.sync_mixed_filaments_from_config();
    const auto &colors = origin.project_config.option<ConfigOptionStrings>("filament_colour")->values;
    origin.mixed_filaments.add_custom_filament(1, 2, 50, colors);

    const size_t num_physical = origin.filament_presets.size();
    const size_t total_before = origin.mixed_filaments.total_filaments(num_physical);
    REQUIRE(total_before == 3u);

    origin.sync_mixed_filaments_to_config();
    const std::string defs = origin.project_config.opt_string("mixed_filament_definitions");

    PresetBundle loaded = make_bundle_2();
    loaded.project_config.option<ConfigOptionString>("mixed_filament_definitions")->value = defs;
    loaded.sync_mixed_filaments_from_config();

    const size_t total_after = loaded.mixed_filaments.total_filaments(loaded.filament_presets.size());
    CHECK(total_after == total_before);
}

TEST_CASE("Mixed pattern and weighted color keep Local-Z wipe-tower planning in sync",
          "[MixedFilament][LocalZ][WipeTower]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_num_extruders(4);
    config.set_num_filaments(4);

    const std::vector<std::string> colors = {
        "#E53935", "#43A047", "#1E88E5", "#FDD835"
    };
    config.option<ConfigOptionFloats>("nozzle_diameter")->values  = {0.4, 0.4, 0.4, 0.4};
    ConfigOptionEnumsGenericNullable *nozzle_types =
        config.option<ConfigOptionEnumsGenericNullable>("nozzle_type");
    REQUIRE(nozzle_types != nullptr);
    REQUIRE_FALSE(nozzle_types->values.empty());
    const int nozzle_type = nozzle_types->values.front();
    nozzle_types->values.assign(4, nozzle_type);
    config.option<ConfigOptionFloats>("filament_diameter")->values = {1.75, 1.75, 1.75, 1.75};
    config.option<ConfigOptionStrings>("filament_colour")->values = colors;
    config.option<ConfigOptionInts>("filament_map")->values       = {1, 2, 3, 4};
    config.option<ConfigOptionInts>("filament_printable")->values = {15, 15, 15, 15};
    config.option<ConfigOptionEnum<FilamentMapMode>>("filament_map_mode")->value = FilamentMapMode::fmmManual;

    MixedFilamentManager manager;
    manager.add_custom_filament(1, 2, 50, colors);
    MixedFilament &pattern = manager.mixed_filaments().back();
    pattern.manual_pattern = MixedFilamentManager::normalize_manual_pattern(
        "1,2,3,4,2,2,2,3,4,3,3,4,1,4,4,4,4,4,4,4,4,4,4,4,4,4,3,3,3,3");
    pattern.distribution_mode = int(MixedFilament::Simple);

    manager.add_custom_filament(4, 2, 33, colors);
    MixedFilament &weighted = manager.mixed_filaments().back();
    weighted.manual_pattern.clear();
    weighted.gradient_component_ids = MixedFilamentManager::encode_gradient_component_ids({4, 2, 3});
    weighted.gradient_component_weights = "50/25/25";
    weighted.distribution_mode = int(MixedFilament::LayerCycle);

    config.option<ConfigOptionString>("mixed_filament_definitions")->value =
        manager.serialize_custom_entries();
    config.set("layer_height", 0.20);
    config.set("initial_layer_print_height", 0.20);
    config.set("enable_prime_tower", true);
    config.set("single_extruder_multi_material", false);
    config.set("purge_in_prime_tower", false);
    config.set("dithering_local_z_mode", true);
    config.set("dithering_local_z_whole_objects", true);
    config.set("dithering_local_z_direct_multicolor", true);
    config.option<ConfigOptionEnum<WipeTowerType>>("wipe_tower_type")->value = WipeTowerType::Type1;

    // full_print_config() copies the static defaults, whose generic enum-vector
    // options do not carry the definition map normally attached while loading a
    // preset. Attach those maps so export_gcode() can serialize this synthetic
    // test configuration in the same way as a GUI-loaded C5 preset.
    for (const std::string &key : config.keys()) {
        ConfigOption *option = config.option(key);
        if (option == nullptr || option->type() != coEnums)
            continue;

        const ConfigOptionDef *definition = print_config_def.get(key);
        REQUIRE(definition != nullptr);
        if (option->nullable())
            dynamic_cast<ConfigOptionEnumsGenericNullable *>(option)->keys_map = definition->enum_keys_map;
        else
            dynamic_cast<ConfigOptionEnumsGeneric *>(option)->keys_map = definition->enum_keys_map;
    }

    Model model;
    const auto add_cube = [&model](const char *name, int virtual_filament_id, double x) {
        ModelObject *object = model.add_object();
        object->name = name;
        object->add_volume(make_cube(20., 20., 6.));
        object->add_instance();
        object->instances.front()->set_offset(Vec3d(x, 40., 0.));
        object->config.set("extruder", virtual_filament_id);
    };
    add_cube("pattern", 5, 40.);
    add_cube("weighted", 6, 80.);

    for (ModelObject *object : model.objects)
        object->ensure_on_bed();

    // Prime the physical C5 tool configuration before loading project-level
    // virtual colors, matching the GUI's printer-then-project apply order.
    DynamicPrintConfig printer_config = config;
    printer_config.option<ConfigOptionString>("mixed_filament_definitions")->value.clear();

    Model empty_model;
    Print print;
    print.is_BBL_printer() = false;
    print.is_flashforge_printer() = true;
    print.set_plate_origin(Vec3d::Zero());
    print.set_status_silent();
    print.apply(empty_model, printer_config, true);
    REQUIRE(print.config().filament_diameter.size() == 4);

    print.apply(model, config, true);
    REQUIRE(print.objects().size() == 2);
    REQUIRE(print.mixed_filament_manager().mixed_filament_from_id(5, 4) != nullptr);
    REQUIRE(print.mixed_filament_manager().mixed_filament_from_id(6, 4) != nullptr);

    REQUIRE_NOTHROW(print.process());
    const Vec2d requested_wipe_tower_position(
        config.option<ConfigOptionFloats>("wipe_tower_x")->get_at(0),
        config.option<ConfigOptionFloats>("wipe_tower_y")->get_at(0));
    const Vec2d corrected_wipe_tower_position(
        print.config().wipe_tower_x.get_at(0), print.config().wipe_tower_y.get_at(0));
    REQUIRE_FALSE(is_approx(requested_wipe_tower_position, corrected_wipe_tower_position));
    CHECK(is_approx(print.resolve_wipe_tower_position(requested_wipe_tower_position),
                    corrected_wipe_tower_position));

    const bool exercised_local_z = std::any_of(
        print.objects().begin(), print.objects().end(), [](const PrintObject *object) {
            return std::any_of(object->local_z_intervals().begin(), object->local_z_intervals().end(),
                               [](const LocalZInterval &interval) {
                                   return interval.has_mixed_paint && interval.sublayer_count > 1;
                               });
        });
    REQUIRE(exercised_local_z);

    const auto &wipe_tower_tool_changes = print.wipe_tower_data().tool_changes;
    REQUIRE(wipe_tower_tool_changes.size() > 2);
    CHECK(std::any_of(wipe_tower_tool_changes[2].begin(), wipe_tower_tool_changes[2].end(),
                      [](const WipeTower::ToolChangeResult &toolchange) {
                          return toolchange.initial_tool == 2 && toolchange.new_tool == 2;
                      }));

    const boost::filesystem::path gcode_path =
        boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("orca-mixed-%%%%-%%%%.gcode");
    GCodeProcessorResult gcode_result;
    REQUIRE_NOTHROW(print.export_gcode(gcode_path.string(), &gcode_result, nullptr));
    REQUIRE(boost::filesystem::file_size(gcode_path) > 0);
    boost::filesystem::remove(gcode_path);

    DynamicPrintConfig moved_config = config;
    const Vec2d user_wipe_tower_position(80., 70.);
    moved_config.option<ConfigOptionFloats>("wipe_tower_x")->values[0] = user_wipe_tower_position.x();
    moved_config.option<ConfigOptionFloats>("wipe_tower_y")->values[0] = user_wipe_tower_position.y();
    print.apply(model, moved_config, true);
    CHECK(is_approx(Vec2d(print.config().wipe_tower_x.get_at(0), print.config().wipe_tower_y.get_at(0)),
                    user_wipe_tower_position));
    CHECK(is_approx(print.resolve_wipe_tower_position(requested_wipe_tower_position),
                    requested_wipe_tower_position));
}

TEST_CASE("Snapmaker perimeter groups emit different physical tools for different walls",
          "[MixedFilament][Compatibility][GCode]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.set_num_extruders(2);
    config.set_num_filaments(2);

    const std::vector<std::string> colors = {"#E53935", "#1E88E5"};
    config.option<ConfigOptionFloats>("nozzle_diameter")->values   = {0.4, 0.4};
    config.option<ConfigOptionFloats>("filament_diameter")->values = {1.75, 1.75};
    config.option<ConfigOptionFloats>("filament_density")->values = {1.24, 1.27};
    config.option<ConfigOptionFloats>("filament_cost")->values    = {20., 25.};
    config.option<ConfigOptionStrings>("filament_colour")->values = colors;
    config.option<ConfigOptionStrings>("filament_ids")->values    = {"physical-red", "physical-blue"};
    config.option<ConfigOptionStrings>("filament_type")->values   = {"PLA", "PETG"};
    config.option<ConfigOptionFloats>("flush_volumes_matrix")->values = {0., 280., 280., 0.};
    config.option<ConfigOptionInts>("filament_map")->values       = {1, 2};
    config.option<ConfigOptionInts>("filament_printable")->values = {3, 3};
    config.option<ConfigOptionEnum<FilamentMapMode>>("filament_map_mode")->value = FilamentMapMode::fmmManual;
    ConfigOptionEnumsGenericNullable *nozzle_types =
        config.option<ConfigOptionEnumsGenericNullable>("nozzle_type");
    REQUIRE(nozzle_types != nullptr);
    REQUIRE_FALSE(nozzle_types->values.empty());
    nozzle_types->values.assign(2, nozzle_types->values.front());

    MixedFilamentManager manager;
    manager.add_custom_filament(1, 2, 50, colors);
    REQUIRE(manager.mixed_filaments().size() == 1);
    MixedFilament &grouped = manager.mixed_filaments().front();
    grouped.ui_mode = 1;
    grouped.manual_pattern = "1,2";
    grouped.distribution_mode = int(MixedFilament::Simple);

    config.option<ConfigOptionString>("mixed_filament_definitions")->value =
        manager.serialize_custom_entries();
    config.option<ConfigOptionInt>("wall_filament")->value = 3;
    config.option<ConfigOptionInt>("sparse_infill_filament")->value = 3;
    config.option<ConfigOptionInt>("solid_infill_filament")->value = 3;
    config.set("wall_loops", 2);
    config.set("layer_height", 0.20);
    config.set("initial_layer_print_height", 0.20);
    config.set("single_extruder_multi_material", false);
    config.set("enable_prime_tower", false);

    for (const std::string &key : config.keys()) {
        ConfigOption *option = config.option(key);
        if (option == nullptr || option->type() != coEnums)
            continue;
        const ConfigOptionDef *definition = print_config_def.get(key);
        REQUIRE(definition != nullptr);
        if (option->nullable())
            dynamic_cast<ConfigOptionEnumsGenericNullable *>(option)->keys_map = definition->enum_keys_map;
        else
            dynamic_cast<ConfigOptionEnumsGeneric *>(option)->keys_map = definition->enum_keys_map;
    }

    Model model;
    ModelObject *object = model.add_object();
    object->name = "grouped-perimeters";
    object->add_volume(make_cube(12., 12., 1.));
    object->add_instance();
    object->instances.front()->set_offset(Vec3d(40., 40., 0.));
    object->ensure_on_bed();

    DynamicPrintConfig printer_config = config;
    printer_config.option<ConfigOptionString>("mixed_filament_definitions")->value.clear();

    Model empty_model;
    Print print;
    print.is_BBL_printer() = false;
    print.is_flashforge_printer() = true;
    print.set_status_silent();
    print.apply(empty_model, printer_config, true);
    print.apply(model, config, true);
    REQUIRE(print.mixed_filament_manager().mixed_filament_from_id(3, 2) != nullptr);
    REQUIRE_NOTHROW(print.process());

    const boost::filesystem::path gcode_path =
        boost::filesystem::temp_directory_path() /
        boost::filesystem::unique_path("orca-grouped-perimeters-%%%%-%%%%.gcode");
    GCodeProcessorResult result;
    REQUIRE_NOTHROW(print.export_gcode(gcode_path.string(), &result, nullptr));

    std::ifstream gcode_file(gcode_path.string());
    REQUIRE(gcode_file.good());
    const std::string gcode((std::istreambuf_iterator<char>(gcode_file)),
                            std::istreambuf_iterator<char>());
    gcode_file.close();
    const size_t executable_block = gcode.find("; EXECUTABLE_BLOCK_START");
    const size_t filament_stats   = gcode.find("; filament used [g] =");
    const size_t total_weight     = gcode.find("; total filament used [g] =");
    const size_t estimated_time   = gcode.find("; estimated printing time (normal mode) =");
    const size_t config_block     = gcode.find("; CONFIG_BLOCK_START");
    const size_t filament_metadata = gcode.find(
        "; <filament id=\"1\" tray_info_idx=\"physical-red\" type=\"PLA\" color=\"#E53935\" used_m=\"");

    REQUIRE(executable_block != std::string::npos);
    REQUIRE(filament_stats != std::string::npos);
    REQUIRE(total_weight != std::string::npos);
    REQUIRE(estimated_time != std::string::npos);
    REQUIRE(config_block != std::string::npos);
    REQUIRE(filament_metadata != std::string::npos);
    CHECK(filament_stats < executable_block);
    CHECK(total_weight < executable_block);
    CHECK(estimated_time < executable_block);
    CHECK(config_block < executable_block);
    CHECK(filament_metadata < executable_block);
    CHECK(gcode.find("; <filament id=\"2\" tray_info_idx=\"physical-blue\" type=\"PETG\" color=\"#1E88E5\" used_m=\"") != std::string::npos);
    CHECK(gcode.find("; <filament id=\"3\"") == std::string::npos);

    std::map<unsigned int, std::set<unsigned int>> perimeter_tools_by_layer;
    std::map<unsigned int, std::vector<unsigned int>> perimeter_order_by_layer;
    std::set<unsigned int> infill_tools;
    for (const GCodeProcessorResult::MoveVertex &move : result.moves) {
        if ((move.extrusion_role == erExternalPerimeter || move.extrusion_role == erPerimeter) &&
            move.type == EMoveType::Extrude && move.delta_extruder > 0.f) {
            perimeter_tools_by_layer[move.layer_id].insert(unsigned(move.extruder_id));
            std::vector<unsigned int> &order = perimeter_order_by_layer[move.layer_id];
            if (order.empty() || order.back() != unsigned(move.extruder_id))
                order.emplace_back(unsigned(move.extruder_id));
        }
        if (move.type == EMoveType::Extrude && move.delta_extruder > 0.f && is_infill(move.extrusion_role))
            infill_tools.insert(unsigned(move.extruder_id));
    }
    CHECK(std::any_of(perimeter_tools_by_layer.begin(), perimeter_tools_by_layer.end(),
                      [](const auto &layer_tools) {
                          return layer_tools.second.count(0) != 0 && layer_tools.second.count(1) != 0;
                      }));
    CHECK(std::any_of(perimeter_order_by_layer.begin(), perimeter_order_by_layer.end(),
                      [](const auto &layer_order) {
                          return layer_order.second.size() >= 2 &&
                                 layer_order.second.front() == 0 && layer_order.second[1] == 1;
                      }));
    CHECK(infill_tools == std::set<unsigned int>{1});

    boost::filesystem::remove(gcode_path);
}
