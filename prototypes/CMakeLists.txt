# =============================================================================
# Editor-framework comparison prototypes (decision D-003). Throwaway code.
#
#   proto_core/    shared, UI-independent model (no Qt, no ImGui) + tests
#   imgui_editor/  Dear ImGui + SDL3 prototype
#   qt_editor/     Qt 6 Widgets prototype
#
# See prototypes/README.md for build and run instructions.
# =============================================================================
cmake_minimum_required(VERSION 3.25)
project(gbe_editor_prototypes LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PROTO_BUILD_IMGUI "Build the Dear ImGui prototype" ON)
option(PROTO_BUILD_QT "Build the Qt 6 prototype" ON)

set(PROTO_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(PROTO_SAMPLE_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/sample_assets")

# Our own code gets strict warnings; third-party code is compiled without them.
function(proto_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

enable_testing()
add_subdirectory(proto_core)

if(PROTO_BUILD_IMGUI)
    add_subdirectory(imgui_editor)
endif()
if(PROTO_BUILD_QT)
    add_subdirectory(qt_editor)
endif()
