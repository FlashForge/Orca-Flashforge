#include <catch2/catch_all.hpp>

#include <memory>

#include "libslic3r/GCode.hpp"

using namespace Slic3r;

SCENARIO("Origin manipulation", "[GCode]") {
	Slic3r::GCode gcodegen;
	WHEN("set_origin to (10,0)") {
    	gcodegen.set_origin(Vec2d(10,0));
    	REQUIRE(gcodegen.origin() == Vec2d(10, 0));
    }
	WHEN("set_origin to (10,0) and translate by (5, 5)") {
		gcodegen.set_origin(Vec2d(10,0));
		gcodegen.set_origin(gcodegen.origin() + Vec2d(5, 5));
		THEN("origin returns reference to point") {
    		REQUIRE(gcodegen.origin() == Vec2d(15,5));
    	}
    }
}

TEST_CASE("Wipe tower integration accepts an explicitly empty planned layer", "[GCode][WipeTower]")
{
    PrintConfig print_config;
    const std::vector<WipeTower::ToolChangeResult> priming;
    const std::vector<std::vector<WipeTower::ToolChangeResult>> tool_changes(1);
    const std::vector<std::vector<WipeTower::ToolChangeResult>> local_z_tool_changes(1);
    const WipeTower::ToolChangeResult final_purge{};
    const std::vector<unsigned int> used_filaments{0};

    WipeTowerIntegration wipe_tower(print_config, 0, Vec3d::Zero(), priming, tool_changes,
                                    local_z_tool_changes, final_purge, used_filaments);
    GCode gcodegen;
    wipe_tower.next_layer();

    CHECK(wipe_tower.is_empty_wipe_tower_gcode(gcodegen, 0, true));
    CHECK(wipe_tower.tool_change(gcodegen, 0, true).empty());
}
