cmake_minimum_required(VERSION 3.25)
project(rlge)

set(CMAKE_CXX_STANDARD 23)


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
        GIT_TAG "4d8a61842903978bc42adf3347cd34f4e6524efc"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(rlimgui)
add_library(rlimgui STATIC ${rlimgui_SOURCE_DIR}/rlImGui.cpp)
target_include_directories(rlimgui PUBLIC ${rlimgui_SOURCE_DIR} PRIVATE ${raylib_SOURCE_DIR} ${imgui_SOURCE_DIR})
target_link_libraries(rlimgui PUBLIC imgui raylib)

set(PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src/")
file(GLOB RLGE_SOURCES "${PROJECT_INCLUDE}*.cpp")

add_library(rlge STATIC ${RLGE_SOURCES})
target_include_directories(rlge PUBLIC ${PROJECT_INCLUDE})
target_link_libraries(rlge PUBLIC raylib rlimgui)

# Example game executables
add_executable(rlge_basic_game examples/basic_game/main.cpp)
target_link_libraries(rlge_basic_game PRIVATE rlge raylib rlimgui imgui)

add_executable(rlge_snake examples/snake/main.cpp)
target_link_libraries(rlge_snake PRIVATE rlge raylib rlimgui imgui)
