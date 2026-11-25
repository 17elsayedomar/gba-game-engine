# Qt 6 is found through CMAKE_PREFIX_PATH (e.g. C:/Qt/6.8.3/msvc2022_64).
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets OpenGLWidgets)

# AUTOMOC runs Qt's "meta-object compiler", which generates the extra code that
# Q_OBJECT classes need for signals and slots.
set(CMAKE_AUTOMOC ON)

add_executable(qt_editor
    main.cpp
    editor_context.cpp editor_context.hpp
    main_window.cpp main_window.hpp
    scene_tree_model.cpp scene_tree_model.hpp
    scene_view.cpp scene_view.hpp
    inspector_panel.cpp inspector_panel.hpp
    canvas_widgets.cpp canvas_widgets.hpp
)
target_link_libraries(qt_editor PRIVATE proto_core Qt6::Widgets Qt6::OpenGLWidgets)
proto_strict_warnings(qt_editor)

# windeployqt copies the Qt DLLs and plugins the .exe needs next to it, so it
# runs straight from the build folder (and that folder is what a user would get).
# The Visual C++ runtime is left out for both prototypes; Windows usually has it.
get_target_property(_qmake_location Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_location}" DIRECTORY)
add_custom_command(TARGET qt_editor POST_BUILD
    COMMAND "${_qt_bin_dir}/windeployqt.exe" --no-translations --no-compiler-runtime
            --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:qt_editor>
    COMMENT "Copying Qt runtime files with windeployqt")
