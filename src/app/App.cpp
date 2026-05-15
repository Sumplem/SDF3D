#include "sdf3d/app/App.h"

#include "sdf3d/core/ResourceManager.h"
#include "sdf3d/math/Math.h"

#include <filesystem>
#include <iostream>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace sdf3d {
namespace {

void glfwErrorCallback(int error, const char* description)
{
    std::cerr << "[SDF3D][GLFW] " << error << ": " << description << '\n';
}

SdfCompileResult compileScene(const SceneGraph& sceneGraph, const SdfCompiler& compiler)
{
    // AGENT: Graph output becomes authoritative once graph UI starts creating
    // nodes, while tree fallback keeps current panels functional during migration.
    if (sceneGraph.graph().outputNode() != 0) {
        return compiler.compile(sceneGraph.graph());
    }

    return compiler.compile(sceneGraph.root());
}

} // namespace

App::~App()
{
    shutdown();
}

bool App::init()
{
    glfwSetErrorCallback(glfwErrorCallback);

    if (glfwInit() != GLFW_TRUE) {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    m_window = glfwCreateWindow(1280, 720, "SDF3D", nullptr, nullptr);
    if (m_window == nullptr) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        std::cerr << "[SDF3D] Failed to load OpenGL 4.6 core functions.\n";
        shutdown();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) {
        shutdown();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 460")) {
        shutdown();
        return false;
    }

    // AGENT: ResourceManager is initialized only after GLAD has loaded the
    // context, because its guaranteed fallbacks allocate OpenGL objects.
    ResourceManager::instance().init(std::filesystem::weakly_canonical(executableDir() / "assets"));

    if (!m_renderer.init(ResourceManager::instance().assetsPath("shaders"))) {
        shutdown();
        return false;
    }

    m_eventBus.subscribe<SceneDirtyEvent>([this](const SceneDirtyEvent&) {
        recompileScene(true);
    });

    if (!recompileScene(false)) {
        shutdown();
        return false;
    }

    m_initialized = true;
    return true;
}

void App::run()
{
    while (m_window != nullptr && glfwWindowShouldClose(m_window) == GLFW_FALSE) {
        glfwPollEvents();

        beginFrame();
        drawMainMenu();
        drawDockspace();
        drawPanels();
        if (m_ui.consumeSceneDirty()) {
            m_eventBus.emit(SceneDirtyEvent{});
        }
        ResourceManager::instance().flushErrors();
        endFrame();
    }

    shutdown();
}

void App::shutdown()
{
    if (!m_initialized && m_window == nullptr) {
        return;
    }

    m_renderer.shutdown();
    m_eventBus.clear();
    ResourceManager::instance().shutdown();

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
    m_initialized = false;
}

void App::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void App::drawMainMenu()
{
    m_ui.drawMainMenu(m_sceneGraph);
}

void App::drawDockspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("SDF3D Dockspace", nullptr, flags);
    ImGui::PopStyleVar(2);

    const ImGuiID dockspaceId = ImGui::GetID("SDF3DMainDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void App::drawPanels()
{
    m_ui.drawPanels(m_sceneGraph, m_diagnostics.typedEntries());
    m_viewport.draw(m_renderer);
}

void App::endFrame()
{
    ImGui::Render();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}

bool App::recompileScene(bool keepPreviousProgramOnFailure)
{
    m_diagnostics.clear();
    const SdfCompileResult sceneGlsl = compileScene(m_sceneGraph, m_sdfCompiler);
    for (const std::string& error : sceneGlsl.errors) {
        m_diagnostics.add(DiagnosticSeverity::Warning, "SdfCompiler", error);
        std::cerr << "[SDF3D][SdfCompiler] " << error << '\n';
    }

    // AGENT: Dirty-frame shader reload failure keeps the previous linked
    // program alive, so a bad graph edit reports diagnostics without blanking.
    if (!m_renderer.reloadScene(sceneGlsl.glsl)) {
        if (!m_renderer.lastError().empty()) {
            m_diagnostics.add(DiagnosticSeverity::Error, "Renderer", m_renderer.lastError());
        }
        return keepPreviousProgramOnFailure;
    }

    m_diagnostics.add(DiagnosticSeverity::Info, "Renderer", "Shader validation passed.");
    m_renderer.setMaterials(sceneGlsl.materials);
    return true;
}

} // namespace sdf3d
