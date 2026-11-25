# The shared model is a static library: both editors link the exact same code.
add_library(proto_core STATIC
    src/model.cpp
    src/components.cpp
    src/commands.cpp
    src/assets.cpp
    src/serialization.cpp
    src/codegen.cpp
    src/geometry.cpp
)
# PUBLIC: anyone linking proto_core can include "proto_core/...".
target_include_directories(proto_core PUBLIC include)
# PRIVATE: json and stb_image are implementation details; the editors never see them.
target_include_directories(proto_core PRIVATE ${PROTO_THIRD_PARTY})
proto_strict_warnings(proto_core)

add_executable(proto_core_tests tests/core_tests.cpp)
target_link_libraries(proto_core_tests PRIVATE proto_core)
target_include_directories(proto_core_tests PRIVATE ${PROTO_THIRD_PARTY})
target_compile_definitions(proto_core_tests PRIVATE PROTO_SAMPLE_ASSETS="${PROTO_SAMPLE_ASSETS}")
proto_strict_warnings(proto_core_tests)
add_test(NAME proto_core_tests COMMAND proto_core_tests)
