cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(libvgcode_tests LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../catch2 ${CMAKE_CURRENT_BINARY_DIR}/catch2)

    add_library(libvgcode_layers STATIC
        ${CMAKE_CURRENT_LIST_DIR}/../../src/libvgcode/src/Layers.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../src/libvgcode/src/Range.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../../src/libvgcode/src/Utils.cpp
    )
    target_include_directories(libvgcode_layers PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../../src/libvgcode/include
        ${CMAKE_CURRENT_LIST_DIR}/../../src/libvgcode/src
    )

    include(CTest)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/../catch2/extras)
    include(Catch)

    set(_LIBVGCODE_TEST_LIBRARY libvgcode_layers)
else()
    set(_LIBVGCODE_TEST_LIBRARY libvgcode)
endif()

get_filename_component(_TEST_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)

set(_TEST_SOURCES
    test_layers.cpp
    test_viewer_colors.cpp
)

add_executable(${_TEST_NAME}_tests
    ${_TEST_SOURCES}
)

target_link_libraries(${_TEST_NAME}_tests PRIVATE
    ${_LIBVGCODE_TEST_LIBRARY}
    Catch2::Catch2WithMain
)
target_include_directories(${_TEST_NAME}_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libvgcode/include
    ${CMAKE_SOURCE_DIR}/src/libvgcode/src
)
set_property(TARGET ${_TEST_NAME}_tests PROPERTY FOLDER "tests")

catch_discover_tests(${_TEST_NAME}_tests)
