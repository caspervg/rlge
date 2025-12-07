cmake_minimum_required(VERSION 3.25)
project(rlge)

set(CMAKE_CXX_STANDARD 23)

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")


# Adding Raylib / ImGui / rlImGui
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        raylib
        GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
        GIT_TAG "5.5"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
        GIT_TAG "e7d2d636affeeac1fc7175fb13472ca02d1bc0d1" # v1.92.4-docking
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_include_directories(imgui INTERFACE ${imgui_SOURCE_DIR})

FetchContent_Declare(
        rlimgui
        GIT_REPOSITORY "https://github.com/raylib-extras/rlImGui.git"
        GIT_TAG "main"
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(rlimgui)
add_library(rlimgui STATIC ${rlimgui_SOURCE_DIR}/rlImGui.cpp)
target_include_directories(rlimgui PUBLIC ${rlimgui_SOURCE_DIR} PRIVATE ${raylib_SOURCE_DIR} ${imgui_SOURCE_DIR})
target_link_libraries(rlimgui PUBLIC imgui raylib)

FetchContent_Declare(
        raygui
        GIT_REPOSITORY "https://github.com/raysan5/raygui.git"
        GIT_TAG "4.0"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(raygui)
set(RAYGUI_INCLUDE_DIR ${raygui_SOURCE_DIR}/src)

FetchContent_Declare(
        tileson
        GIT_REPOSITORY "https://github.com/SSBMTonberry/tileson.git"
        GIT_TAG "v1.4.0"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_GetProperties(tileson)
if (NOT tileson_POPULATED)
    FetchContent_Populate(tileson)
endif()

add_library(tileson INTERFACE)
target_include_directories(tileson INTERFACE ${tileson_SOURCE_DIR} ${tileson_SOURCE_DIR}/include)

FetchContent_Declare(
        box2d
        GIT_REPOSITORY "https://github.com/erincatto/box2d.git"
        GIT_TAG "v3.0.0"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(box2d)


set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/")
file(GLOB RLGE_SOURCES "${PROJECT_INCLUDE}*.cpp" "${PROJECT_INCLUDE}collision/*.cpp" "${PROJECT_INCLUDE}collision/shape/*.cpp")

add_library(rlge STATIC ${RLGE_SOURCES})
target_include_directories(rlge PUBLIC ${PROJECT_INCLUDE} "${PROJECT_INCLUDE}/collision/" "${PROJECT_INCLUDE}/collision/shape/" "${PROJECT_INCLUDE}/lighting/" ${RAYGUI_INCLUDE_DIR})
target_link_libraries(rlge PUBLIC raylib rlimgui tileson box2d)
target_sources(rlge PRIVATE
        src/raygui_impl.cpp
        src/lighting/lighting_system.cpp
        src/lighting/lit_scene.cpp
        src/lighting/lit_sprite.cpp
        src/box2d_physics.cpp
        src/box2d_debug.cpp)

# Example game executables
add_executable(rlge_basic_game examples/basic_game/main.cpp)
target_link_libraries(rlge_basic_game PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_particles examples/particles/main.cpp)
target_link_libraries(rlge_particles PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_snake
        examples/snake/main.cpp
        examples/snake/snake_game.cpp
        examples/snake/snake_fx.cpp
        examples/snake/snake_view.cpp
        examples/snake/snake_scene.cpp
        examples/snake/game_over_scene.cpp
        examples/snake/game_over_scene.h)
target_link_libraries(rlge_snake PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_tilemap examples/tilemap/main.cpp)
target_link_libraries(rlge_tilemap PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_multiview examples/multiview/main.cpp)
target_link_libraries(rlge_multiview PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_collision_debug examples/collision_debug/main.cpp)
target_link_libraries(rlge_collision_debug PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_breakout
        examples/breakout/main.cpp
        examples/breakout/breakout_scene.cpp
        examples/breakout/game_over_scene.cpp
        examples/breakout/breakout_level.cpp
        examples/breakout/breakout_game.cpp
        examples/breakout/powerup_manager.cpp
        examples/breakout/powerup.cpp
        examples/breakout/banner.cpp
        examples/breakout/safety_net.cpp
        examples/breakout/scoreboard.cpp
        examples/breakout/wall.cpp
        examples/breakout/paddle.cpp
        examples/breakout/brick.cpp
        examples/breakout/ball.cpp
)
target_link_libraries(rlge_breakout PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_shaders examples/shader/main.cpp)
target_link_libraries(rlge_shaders PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_lighting examples/lighting/main.cpp)
target_link_libraries(rlge_lighting PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_raygui examples/raygui/main.cpp examples/util/raygui_helpers.hpp)
target_include_directories(rlge_raygui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/examples)
target_link_libraries(rlge_raygui PRIVATE rlge raylib)
