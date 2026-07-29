#include <catch2/catch_all.hpp>

#include "Layers.hpp"
#include "PathVertex.hpp"

using namespace libvgcode;

TEST_CASE("Pause markers do not replace the preview color of a layer", "[libvgcode][preview][pause]")
{
    Layers layers;

    PathVertex extrusion;
    extrusion.layer_id    = 0;
    extrusion.type        = EMoveType::Extrude;
    extrusion.role        = EGCodeExtrusionRole::Perimeter;
    extrusion.position[2] = 0.2f;
    layers.update(extrusion, 0);

    PathVertex pause;
    pause.layer_id = 0;
    pause.type     = EMoveType::PausePrint;
    layers.update(pause, 1);

    REQUIRE_FALSE(layers.layer_contains_colorprint_options(0));
}

TEST_CASE("Custom G-code markers keep their special preview layer treatment", "[libvgcode][preview][custom_gcode]")
{
    Layers layers;

    PathVertex custom_gcode;
    custom_gcode.layer_id = 0;
    custom_gcode.type     = EMoveType::CustomGCode;
    layers.update(custom_gcode, 0);

    REQUIRE(layers.layer_contains_colorprint_options(0));
}
