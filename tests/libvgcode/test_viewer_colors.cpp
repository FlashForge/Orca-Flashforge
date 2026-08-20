#include <catch2/catch_all.hpp>

#include "Utils.hpp"

using namespace libvgcode;

TEST_CASE("Summary and color-print views tolerate a missing color-print palette", "[libvgcode][preview][colors]")
{
    const Palette tool_colors{ Color{ 1, 2, 3 }, Color{ 4, 5, 6 } };

    REQUIRE(get_palette_color({}, tool_colors, 1) == tool_colors[1]);
    REQUIRE(get_palette_color({}, {}, 1) == DUMMY_COLOR);
}

TEST_CASE("Color-print palette remains preferred when it is available", "[libvgcode][preview][colors]")
{
    const Palette tool_colors{ Color{ 1, 2, 3 } };
    const Palette color_print_colors{ Color{ 7, 8, 9 }, Color{ 10, 11, 12 } };

    REQUIRE(get_palette_color(color_print_colors, tool_colors, 3) == color_print_colors[1]);
}
