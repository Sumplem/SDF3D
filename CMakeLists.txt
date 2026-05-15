cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

project(SDF3D
    VERSION 0.1.0
    LANGUAGES CXX
)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

add_executable(sdf3d
    src/main.cpp
    src/app/App.cpp
    src/core/ResourceManager.cpp
    src/math/Math.cpp
    src/renderer/Renderer.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/scene/SceneGraph.cpp
    src/scene/SdfCompiler.cpp
    src/ui/AddMenu.cpp
    src/ui/NodeEditor.cpp
    src/ui/PropertiesPanel.cpp
    src/ui/SceneOutliner.cpp
    src/ui/UI.cpp
    src/ui/Viewport.cpp
)

target_compile_features(sdf3d PRIVATE cxx_std_17)

target_include_directories(sdf3d PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(sdf3d PRIVATE /W4)
else()
    target_compile_options(sdf3d PRIVATE -Wall -Wextra)
endif()

include(cmake/dependencies.cmake)

# AGENT: Assets are copied next to the executable so runtime paths stay relative
# to the binary location instead of depending on a source-tree working directory.
add_custom_command(TARGET sdf3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:sdf3d>/assets
)

add_executable(sdf3d_tests
    tests/sdf_node_tests.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/scene/SdfCompiler.cpp
)

target_compile_features(sdf3d_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_tests PRIVATE -Wall -Wextra)
endif()
