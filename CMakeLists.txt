cmake_minimum_required(VERSION 3.25)
project(rlge)

set(CMAKE_CXX_STANDARD 23)

if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Adding Raylib / ImGui / rlImGui (configurable)
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

option(RLGE_FETCH_RAYLIB "Fetch raylib for rlge" ON)
option(RLGE_FETCH_IMGUI "Fetch ImGui for rlge" ON)
option(RLGE_FETCH_RLIMGUI "Fetch rlImGui for rlge" ON)

set(RLGE_RAYLIB_TAG "5.5" CACHE STRING "raylib tag/branch/hash to fetch")
set(RLGE_IMGUI_TAG "e7d2d636affeeac1fc7175fb13472ca02d1bc0d1" CACHE STRING "ImGui tag/hash to fetch")
set(RLGE_RLIMGUI_TAG "main" CACHE STRING "rlImGui tag/branch to fetch")

if (RLGE_FETCH_RAYLIB)
    if (NOT TARGET raylib)
        FetchContent_Declare(
                raylib
                GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
                GIT_TAG "${RLGE_RAYLIB_TAG}"
                GIT_SHALLOW TRUE
                GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(raylib)
    endif()
else()
    if (NOT TARGET raylib)
        message(FATAL_ERROR "raylib target not found. Provide it or enable RLGE_FETCH_RAYLIB.")
    endif()
endif()

if (TARGET raylib AND NOT TARGET rlge::raylib)
    add_library(rlge::raylib ALIAS raylib)
endif()

if (RLGE_FETCH_IMGUI)
    if (NOT TARGET imgui)
        FetchContent_Declare(
                imgui
                GIT_REPOSITORY "https://github.com/ocornut/imgui.git"
                GIT_TAG "${RLGE_IMGUI_TAG}" # e.g., docking tag
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
    endif()
else()
    if (NOT TARGET imgui)
        message(FATAL_ERROR "imgui target not found. Provide it or enable RLGE_FETCH_IMGUI.")
    endif()
endif()

if (TARGET imgui AND NOT TARGET rlge::imgui)
    add_library(rlge::imgui ALIAS imgui)
endif()

if (RLGE_FETCH_RLIMGUI)
    if (NOT TARGET rlimgui)
        FetchContent_Declare(
                rlimgui
                GIT_REPOSITORY "https://github.com/raylib-extras/rlImGui.git"
                GIT_TAG "${RLGE_RLIMGUI_TAG}"
                GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(rlimgui)
        add_library(rlimgui STATIC ${rlimgui_SOURCE_DIR}/rlImGui.cpp)
        target_include_directories(rlimgui PUBLIC ${rlimgui_SOURCE_DIR} PRIVATE ${imgui_SOURCE_DIR})
        if (DEFINED raylib_SOURCE_DIR)
            target_include_directories(rlimgui PRIVATE ${raylib_SOURCE_DIR})
        endif()
        target_link_libraries(rlimgui PUBLIC imgui raylib)
    endif()
else()
    if (NOT TARGET rlimgui)
        message(FATAL_ERROR "rlimgui target not found. Provide it or enable RLGE_FETCH_RLIMGUI.")
    endif()
endif()

if (TARGET rlimgui AND NOT TARGET rlge::rlimgui)
    add_library(rlge::rlimgui ALIAS rlimgui)
endif()

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


set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/")
file(GLOB RLGE_SOURCES "${PROJECT_INCLUDE}*.cpp" "${PROJECT_INCLUDE}collision/*.cpp" "${PROJECT_INCLUDE}collision/shape/*.cpp")

add_library(rlge STATIC ${RLGE_SOURCES})
target_include_directories(rlge PUBLIC ${PROJECT_INCLUDE} "${PROJECT_INCLUDE}/collision/" "${PROJECT_INCLUDE}/collision/shape/" "${PROJECT_INCLUDE}/lighting/" ${RAYGUI_INCLUDE_DIR})
target_link_libraries(rlge PUBLIC rlge::raylib rlge::rlimgui tileson)
target_sources(rlge PRIVATE
        src/raygui_impl.cpp
        src/lighting/lighting_system.cpp
        src/lighting/lit_scene.cpp
        src/lighting/lit_sprite.cpp)

option(RLGE_BUILD_EXAMPLES "Build the bundled RLGE example executables" ON)

if(RLGE_BUILD_EXAMPLES)
    # Example game executables
    add_executable(rlge_basic_game examples/basic_game/main.cpp)
    target_link_libraries(rlge_basic_game PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_anim_2d
        examples/anim_2d/anim_demo_scene.cpp
        examples/anim_2d/sea_creature.cpp
        examples/anim_2d/hud.cpp
        examples/anim_2d/main.cpp)
    target_link_libraries(rlge_anim_2d PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_particles examples/particles/main.cpp)
    target_link_libraries(rlge_particles PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_snake
            examples/snake/main.cpp
            examples/snake/snake_game.cpp
            examples/snake/snake_fx.cpp
            examples/snake/snake_view.cpp
            examples/snake/snake_scene.cpp
            examples/snake/game_over_scene.cpp
            examples/snake/game_over_scene.h)
    target_link_libraries(rlge_snake PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_tilemap examples/tilemap/main.cpp)
    target_link_libraries(rlge_tilemap PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_multiview examples/multiview/main.cpp)
    target_link_libraries(rlge_multiview PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_hybrid3d examples/hybrid3d/main.cpp)
    target_link_libraries(rlge_hybrid3d PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_collision_debug examples/collision_debug/main.cpp)
    target_link_libraries(rlge_collision_debug PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

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
    target_link_libraries(rlge_breakout PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_shaders examples/shader/main.cpp)
    target_link_libraries(rlge_shaders PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_lighting examples/lighting/main.cpp)
    target_link_libraries(rlge_lighting PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_raygui examples/raygui/main.cpp examples/util/raygui_helpers.hpp)
    target_include_directories(rlge_raygui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/examples)
    target_link_libraries(rlge_raygui PRIVATE rlge rlge::raylib)

    add_executable(rlge_shader_learning examples/shader_learn/main.cpp)
    target_link_libraries(rlge_shader_learning PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_neon_surge
            examples/neon_surge/main.cpp
            examples/neon_surge/ns_assets.cpp
            examples/neon_surge/ns_game.cpp
            examples/neon_surge/backdrop.cpp
            examples/neon_surge/fx.cpp
            examples/neon_surge/player.cpp
            examples/neon_surge/bullet.cpp
            examples/neon_surge/enemy.cpp
            examples/neon_surge/pickup.cpp
            examples/neon_surge/hud.cpp
            examples/neon_surge/arena_scene.cpp
            examples/neon_surge/menu_scene.cpp
            examples/neon_surge/game_over_scene.cpp)
    target_link_libraries(rlge_neon_surge PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)

    add_executable(rlge_voxhaven
            examples/voxhaven/main.cpp
            examples/voxhaven/vx_blocks.cpp
            examples/voxhaven/vx_worldgen.cpp
            examples/voxhaven/vx_light.cpp
            examples/voxhaven/vx_inventory.cpp
            examples/voxhaven/vx_hud.cpp
            examples/voxhaven/vx_mobs.cpp
            examples/voxhaven/vx_world.cpp
            examples/voxhaven/vx_mesher.cpp
            examples/voxhaven/vx_player.cpp
            examples/voxhaven/vx_sfx.cpp
            examples/voxhaven/vox_scene.cpp)
    target_link_libraries(rlge_voxhaven PRIVATE rlge rlge::raylib rlge::rlimgui rlge::imgui)
endif()
