#pragma once

#include "sdf3d/renderer/Renderer.h"
#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/scene/SdfCompiler.h"
#include "sdf3d/ui/UI.h"
#include "sdf3d/ui/Viewport.h"

#include <string>
#include <vector>

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

    GLFWwindow* m_window = nullptr;
    Renderer m_renderer;
    SceneGraph m_sceneGraph;
    SdfCompiler m_sdfCompiler;
    UI m_ui;
    Viewport m_viewport;
    std::vector<std::string> m_runtimeErrors;
    bool m_initialized = false;
};

} // namespace sdf3d
