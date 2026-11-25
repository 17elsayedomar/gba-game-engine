# Dear ImGui is compiled from source as a small static library. Only the files
# we need: the core, the docking-capable widgets, the SDL3 platform backend and
# the SDL_Renderer drawing backend.
set(IMGUI_DIR ${PROTO_THIRD_PARTY}/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
    ${IMGUI_DIR}/misc/imgui_stdlib.cpp
)
# Code includes <imgui.h>, <backends/...> and <misc/...> relative to this folder.
target_include_directories(imgui PUBLIC ${IMGUI_DIR})

# SDL3: the prebuilt Visual Studio package downloaded by prototypes/fetch_deps.ps1.
set(SDL3_DIR ${PROTO_THIRD_PARTY}/SDL3/cmake)
find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(imgui_editor
    main.cpp
    editor_app.cpp
    editor_app.hpp
)
target_link_libraries(imgui_editor PRIVATE proto_core imgui)
proto_strict_warnings(imgui_editor)

# Copy SDL3.dll next to the .exe so it runs straight from the build folder.
add_custom_command(TARGET imgui_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:imgui_editor>)
