#include <catch2/catch_all.hpp>

#include <boost/filesystem.hpp>

#include <cstring>

#include "libslic3r/Model.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/Utils.hpp"
#include "libslic3r/Zipper.hpp"

using namespace Slic3r;

namespace {

namespace fs = boost::filesystem;

class TempFile {
public:
    explicit TempFile(const char *pattern) : path(fs::temp_directory_path() / fs::unique_path(pattern)) {}

    ~TempFile()
    {
        boost::system::error_code ec;
        fs::remove(path, ec);
    }

    fs::path path;
};

class TemporaryDirGuard {
public:
    TemporaryDirGuard()
        : old_path(temporary_dir())
    {
        set_temporary_dir(fs::temp_directory_path().string());
    }

    ~TemporaryDirGuard() { set_temporary_dir(old_path); }

private:
    std::string old_path;
};

size_t vector_size(const DynamicPrintConfig &config, const char *key)
{
    const auto *option = dynamic_cast<const ConfigOptionVectorBase *>(config.option(key));
    REQUIRE(option != nullptr);
    return option->size();
}

void write_surface_3mf(const fs::path &path, bool closed = true)
{
    static constexpr const char *content_types = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
</Types>)xml";

    static constexpr const char *relationships = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>
</Relationships>)xml";

    static constexpr const char *closed_model = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">
  <resources>
    <object id="1" type="surface" name="Closed surface">
      <mesh>
        <vertices>
          <vertex x="0" y="0" z="0"/>
          <vertex x="10" y="0" z="0"/>
          <vertex x="0" y="10" z="0"/>
          <vertex x="0" y="0" z="10"/>
        </vertices>
        <triangles>
          <triangle v1="0" v2="2" v3="1"/>
          <triangle v1="0" v2="1" v3="3"/>
          <triangle v1="1" v2="2" v3="3"/>
          <triangle v1="2" v2="0" v3="3"/>
        </triangles>
      </mesh>
    </object>
  </resources>
  <build><item objectid="1"/></build>
</model>)xml";

    static constexpr const char *open_model = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">
  <resources>
    <object id="1" type="surface" name="Open surface">
      <mesh>
        <vertices>
          <vertex x="0" y="0" z="0"/>
          <vertex x="10" y="0" z="0"/>
          <vertex x="0" y="10" z="0"/>
          <vertex x="0" y="0" z="10"/>
        </vertices>
        <triangles>
          <triangle v1="0" v2="2" v3="1"/>
          <triangle v1="0" v2="1" v3="3"/>
          <triangle v1="1" v2="2" v3="3"/>
        </triangles>
      </mesh>
    </object>
  </resources>
  <build><item objectid="1"/></build>
</model>)xml";

    const char *model = closed ? closed_model : open_model;

    Zipper zipper(path.string());
    zipper.add_entry("[Content_Types].xml", content_types, std::char_traits<char>::length(content_types));
    zipper.add_entry("_rels/.rels", relationships, std::char_traits<char>::length(relationships));
    zipper.add_entry("3D/3dmodel.model", model, std::char_traits<char>::length(model));
    zipper.finalize();
}

} // namespace

TEST_CASE("Four-extruder project normalization preserves one physical filament", "[Config][3mf][Regression]")
{
    DynamicPrintConfig config = DynamicPrintConfig::full_print_config();
    config.option<ConfigOptionBool>("single_extruder_multi_material")->value = false;
    config.option<ConfigOptionFloats>("nozzle_diameter")->values = {0.4, 0.4, 0.4, 0.4};
    config.option<ConfigOptionFloats>("filament_diameter")->values = {1.75};
    config.set_key_value("filament_settings_id", new ConfigOptionStrings({"Test PLA"}));
    config.set_key_value("filament_ids", new ConfigOptionStrings({"GFL99"}));
    config.option<ConfigOptionStrings>("filament_colour", true)->values = {"#F2754E"};
    config.option<ConfigOptionStrings>("filament_extruder_variant")->values = {"Direct Drive Standard"};
    config.set_key_value("filament_self_index", new ConfigOptionInts({1}));

    Preset::normalize(config);

    // Per-extruder filament parameters may still be expanded for a
    // multi-tool printer, but the physical filament identity must not be.
    CHECK(vector_size(config, "filament_settings_id") == 1);
    CHECK(vector_size(config, "filament_ids") == 1);
    CHECK(vector_size(config, "filament_colour") == 1);
    CHECK(vector_size(config, "filament_extruder_variant") == 1);
    CHECK(vector_size(config, "filament_self_index") == 1);
    CHECK(vector_size(config, "nozzle_diameter") == 4);
}

TEST_CASE("BBS 3MF importer handles surface objects", "[3mf][Surface][Regression]")
{
    TemporaryDirGuard temporary_dir_guard;

    SECTION("closed surface is imported as a printable solid")
    {
        TempFile file("orcaslicer-surface-%%%%-%%%%.3mf");
        write_surface_3mf(file.path);

        DynamicPrintConfig config;
        ConfigSubstitutionContext substitutions{ForwardCompatibilitySubstitutionRule::Disable};
        Model model = Model::read_from_file(
            file.path.string(), &config, &substitutions,
            LoadStrategy::LoadModel | LoadStrategy::AddDefaultInstances);

        REQUIRE(model.objects.size() == 1);
        REQUIRE(model.objects.front()->volumes.size() == 1);
        CHECK(model.objects.front()->volumes.front()->mesh().facets_count() == 4);
        CHECK(model.objects.front()->instances.size() == 1);
    }

    SECTION("open surface is rejected instead of being silently filled")
    {
        TempFile file("orcaslicer-open-surface-%%%%-%%%%.3mf");
        // Removing one tetrahedron face leaves three boundary edges.
        write_surface_3mf(file.path, false);

        DynamicPrintConfig config;
        ConfigSubstitutionContext substitutions{ForwardCompatibilitySubstitutionRule::Disable};
        CHECK_THROWS(Model::read_from_file(
            file.path.string(), &config, &substitutions,
            LoadStrategy::LoadModel | LoadStrategy::AddDefaultInstances));
    }
}
