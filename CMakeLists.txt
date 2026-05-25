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
    src/core/EventBus.cpp
    src/core/ResourceManager.cpp
    src/math/Math.cpp
    src/renderer/FboRenderer.cpp
    src/renderer/PathTraceAccumulation.cpp
    src/renderer/Renderer.cpp
    src/renderer/ShaderManager.cpp
    src/renderer/UniformUploader.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/scene/SceneGraph.cpp
    src/scene/SdfCompiler.cpp
    src/systems/CompilerSystem.cpp
    src/systems/DiagnosticsSystem.cpp
    src/systems/glsl_emitter/GlslEmitterBooleans.cpp
    src/systems/glsl_emitter/GlslEmitterDomain.cpp
    src/systems/glsl_emitter/GlslEmitterDispatch.cpp
    src/systems/glsl_emitter/GlslEmitterFormatting.cpp
    src/systems/glsl_emitter/GlslEmitterMaterials.cpp
    src/systems/glsl_emitter/GlslEmitterMath.cpp
    src/systems/glsl_emitter/GlslEmitterPrimitives.cpp
    src/systems/glsl_emitter/GlslEmitterSceneAssembly.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/GraphSerializer.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/GraphSystemSerialization.cpp
    src/systems/JsonGraphSerializer.cpp
    src/systems/json_graph_serializer/JsonGraphSerializerConversions.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
    src/ui/AddMenu.cpp
    src/ui/GraphCanvas.cpp
    src/ui/GraphEditorCore.cpp
    src/ui/MaterialGraphPanel.cpp
    src/ui/node_editor/NodeEditorActions.cpp
    src/ui/node_editor/NodeEditorCanvas.cpp
    src/ui/node_editor/NodeEditorInputLinks.cpp
    src/ui/node_editor/NodeEditorLayout.cpp
    src/ui/node_editor/NodeEditorLinks.cpp
    src/ui/node_editor/NodeEditor.cpp
    src/ui/node_editor/NodeEditorProperties.cpp
    src/ui/node_editor/NodeEditorPreviews.cpp
    src/ui/node_editor/NodeEditorView.cpp
    src/ui/PropertiesPanel.cpp
    src/ui/SceneOutliner.cpp
    src/ui/UI.cpp
    src/ui/Viewport.cpp
    src/ui/viewport/TranslateGizmo.cpp
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
    src/systems/CompilerSystem.cpp
    src/systems/glsl_emitter/GlslEmitterBooleans.cpp
    src/systems/glsl_emitter/GlslEmitterDomain.cpp
    src/systems/glsl_emitter/GlslEmitterDispatch.cpp
    src/systems/glsl_emitter/GlslEmitterFormatting.cpp
    src/systems/glsl_emitter/GlslEmitterMaterials.cpp
    src/systems/glsl_emitter/GlslEmitterMath.cpp
    src/systems/glsl_emitter/GlslEmitterPrimitives.cpp
    src/systems/glsl_emitter/GlslEmitterSceneAssembly.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
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

add_executable(sdf3d_graph_tests
    tests/test_sdf_graph.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemSerialization.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/SelectionSystem.cpp
    src/ui/node_editor/NodeEditorLayout.cpp
)

target_compile_features(sdf3d_graph_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_graph_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_graph_tests PRIVATE
    glm::glm
    imgui
)

if(MSVC)
    target_compile_options(sdf3d_graph_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_graph_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_graph_compiler_tests
    tests/test_sdf_graph_compiler.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/scene/SdfCompiler.cpp
    src/systems/CompilerSystem.cpp
    src/systems/glsl_emitter/GlslEmitterBooleans.cpp
    src/systems/glsl_emitter/GlslEmitterDomain.cpp
    src/systems/glsl_emitter/GlslEmitterDispatch.cpp
    src/systems/glsl_emitter/GlslEmitterFormatting.cpp
    src/systems/glsl_emitter/GlslEmitterMaterials.cpp
    src/systems/glsl_emitter/GlslEmitterMath.cpp
    src/systems/glsl_emitter/GlslEmitterPrimitives.cpp
    src/systems/glsl_emitter/GlslEmitterSceneAssembly.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
)

target_compile_features(sdf3d_graph_compiler_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_graph_compiler_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_graph_compiler_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_graph_compiler_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_graph_compiler_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_uniform_uploader_tests
    tests/test_uniform_uploader.cpp
    src/renderer/UniformUploader.cpp
)

target_compile_features(sdf3d_uniform_uploader_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_uniform_uploader_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_uniform_uploader_tests PRIVATE
    glad_gl_core_46
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_uniform_uploader_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_uniform_uploader_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_fbo_renderer_tests
    tests/test_fbo_renderer.cpp
    src/renderer/FboRenderer.cpp
)

target_compile_features(sdf3d_fbo_renderer_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_fbo_renderer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_fbo_renderer_tests PRIVATE
    glad_gl_core_46
)

if(MSVC)
    target_compile_options(sdf3d_fbo_renderer_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_fbo_renderer_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_path_trace_accumulation_tests
    tests/test_path_trace_accumulation.cpp
    src/renderer/FboRenderer.cpp
    src/renderer/PathTraceAccumulation.cpp
)

target_compile_features(sdf3d_path_trace_accumulation_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_path_trace_accumulation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_path_trace_accumulation_tests PRIVATE
    glad_gl_core_46
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_path_trace_accumulation_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_path_trace_accumulation_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_shader_manager_tests
    tests/test_shader_manager.cpp
    src/renderer/ShaderManager.cpp
)

target_compile_features(sdf3d_shader_manager_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_shader_manager_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_shader_manager_tests PRIVATE
    glad_gl_core_46
    glfw
)

if(MSVC)
    target_compile_options(sdf3d_shader_manager_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_shader_manager_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_diagnostics_tests
    tests/test_diagnostics_system.cpp
    src/systems/DiagnosticsSystem.cpp
)

target_compile_features(sdf3d_diagnostics_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_diagnostics_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(sdf3d_diagnostics_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_diagnostics_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_event_bus_tests
    tests/test_event_bus.cpp
    src/core/EventBus.cpp
)

target_compile_features(sdf3d_event_bus_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_event_bus_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(sdf3d_event_bus_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_event_bus_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_selection_tests
    tests/test_selection_system.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfNodeDefinition.cpp
    src/scene/SceneGraph.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/SelectionSystem.cpp
)

target_compile_features(sdf3d_selection_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_selection_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_selection_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_selection_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_selection_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_graph_system_tests
    tests/test_graph_system.cpp
    src/core/EventBus.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfNodeDefinition.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/SelectionSystem.cpp
)

target_compile_features(sdf3d_graph_system_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_graph_system_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_graph_system_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_graph_system_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_graph_system_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_graph_serializer_tests
    tests/test_graph_serializer.cpp
    src/systems/GraphSerializer.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/GraphSystemSerialization.cpp
    src/systems/JsonGraphSerializer.cpp
    src/systems/json_graph_serializer/JsonGraphSerializerConversions.cpp
    src/scene/SdfCompiler.cpp
    src/scene/SdfGraphCompiler.cpp
    src/systems/CompilerSystem.cpp
    src/systems/glsl_emitter/GlslEmitterBooleans.cpp
    src/systems/glsl_emitter/GlslEmitterDomain.cpp
    src/systems/glsl_emitter/GlslEmitterDispatch.cpp
    src/systems/glsl_emitter/GlslEmitterFormatting.cpp
    src/systems/glsl_emitter/GlslEmitterMaterials.cpp
    src/systems/glsl_emitter/GlslEmitterMath.cpp
    src/systems/glsl_emitter/GlslEmitterPrimitives.cpp
    src/systems/glsl_emitter/GlslEmitterSceneAssembly.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfNodeDefinition.cpp
)

target_compile_features(sdf3d_graph_serializer_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_graph_serializer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_graph_serializer_tests PRIVATE
    glm::glm
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(sdf3d_graph_serializer_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_graph_serializer_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_glsl_emitter_tests
    tests/test_glsl_emitter.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/systems/glsl_emitter/GlslEmitterBooleans.cpp
    src/systems/glsl_emitter/GlslEmitterDomain.cpp
    src/systems/glsl_emitter/GlslEmitterDispatch.cpp
    src/systems/glsl_emitter/GlslEmitterFormatting.cpp
    src/systems/glsl_emitter/GlslEmitterMaterials.cpp
    src/systems/glsl_emitter/GlslEmitterMath.cpp
    src/systems/glsl_emitter/GlslEmitterPrimitives.cpp
    src/systems/glsl_emitter/GlslEmitterSceneAssembly.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
)

target_compile_features(sdf3d_glsl_emitter_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_glsl_emitter_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_glsl_emitter_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_glsl_emitter_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_glsl_emitter_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_compiler_system_tests
    tests/test_compiler_system.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/systems/CompilerSystem.cpp
    src/systems/glsl_emitter/GlslEmitterBooleans.cpp
    src/systems/glsl_emitter/GlslEmitterDomain.cpp
    src/systems/glsl_emitter/GlslEmitterDispatch.cpp
    src/systems/glsl_emitter/GlslEmitterFormatting.cpp
    src/systems/glsl_emitter/GlslEmitterMaterials.cpp
    src/systems/glsl_emitter/GlslEmitterMath.cpp
    src/systems/glsl_emitter/GlslEmitterPrimitives.cpp
    src/systems/glsl_emitter/GlslEmitterSceneAssembly.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
)

target_compile_features(sdf3d_compiler_system_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_compiler_system_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_compiler_system_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_compiler_system_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_compiler_system_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_material_system_tests
    tests/test_material_system.cpp
    src/scene/SdfGraph.cpp
    src/scene/SdfGraphCompiler.cpp
    src/scene/SdfNodeDefinition.cpp
    src/systems/GlslNodeNames.cpp
    src/systems/GraphSystem.cpp
    src/systems/GraphSystemTransforms.cpp
    src/systems/GraphSystemHighlight.cpp
    src/systems/GraphSystemValidity.cpp
    src/systems/MaterialSystem.cpp
    src/systems/SelectionSystem.cpp
)

target_compile_features(sdf3d_material_system_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_material_system_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_material_system_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_material_system_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_material_system_tests PRIVATE -Wall -Wextra)
endif()

add_executable(sdf3d_material_graph_tests
    tests/test_material_graph.cpp
)

target_compile_features(sdf3d_material_graph_tests PRIVATE cxx_std_17)

target_include_directories(sdf3d_material_graph_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(sdf3d_material_graph_tests PRIVATE
    glm::glm
)

if(MSVC)
    target_compile_options(sdf3d_material_graph_tests PRIVATE /W4)
else()
    target_compile_options(sdf3d_material_graph_tests PRIVATE -Wall -Wextra)
endif()
