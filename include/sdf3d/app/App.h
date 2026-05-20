#pragma once

#include "sdf3d/core/EventBus.h"
#include "sdf3d/renderer/Renderer.h"
#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/systems/DiagnosticsSystem.h"
#include "sdf3d/systems/JsonGraphSerializer.h"
#include "sdf3d/systems/MaterialSystem.h"
#include "sdf3d/ui/UI.h"
#include "sdf3d/ui/Viewport.h"

struct GLFWwindow;

namespace sdf3d {

/// Owns the application window, OpenGL context, ImGui state, and main loop.
class App {
public:
    App() = default;
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /// Initializes GLFW, OpenGL, Dear ImGui, and core runtime systems.
    bool init();

    /// Runs the frame loop until the window is closed.
    void run();

    /// Releases runtime systems in reverse initialization order.
    void shutdown();

private:
    void beginFrame();
    void drawMainMenu();
    void drawDockspace();
    void drawPanels();
    void endFrame();
    bool recompileScene(bool keepPreviousProgramOnFailure);
    bool refreshMaterials();

    GLFWwindow* m_window = nullptr;
    Renderer m_renderer;
    SceneGraph m_sceneGraph;
    GraphGroupRegistry m_groupRegistry;
    SdfCompiler m_sdfCompiler;
    JsonGraphSerializer m_graphSerializer;
    MaterialSystem m_materialSystem;
    EventBus m_eventBus;
    DiagnosticsSystem m_diagnostics;
    UI m_ui;
    Viewport m_viewport;
    bool m_initialized = false;
};

} // namespace sdf3d
