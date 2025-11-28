// src/main.cpp
// Minimal C++20 + OpenGL + ImGui application for NovaDCC / Studio3D
// - Requires: GLFW, glad, ImGui, glm
// - Renders a grid + cube, provides dockspace UI, viewport, orbit camera, wireframe toggle.
//
// Build: use the CMakeLists.txt you provided. Ensure dependencies (glfw, glad, imgui, glm) are
// available via vcpkg, submodules or system packages.

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ImGui includes: prefer installed layout (imgui/backends/...) when using vcpkg
#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "scene.h"
#include "gui_console.h"
#include "primitive_factory.h"
#include "gizmo.h"
#include "renderer.h"
#include "camera.h"
#include "gizmo_controller.h"
#include "viewport_window.h"
#include "ui_helpers.h"
#include "gizmo_lib.h"

#include "tools_window.h"
#include "assets_window.h"
#include "bottom_window.h"
#include "animator.h"

static Gizmo g_gizmo;

static int g_window_width = 1280;
static int g_window_height = 800;

// Camera instance
static Camera g_camera;
static bool g_showWireframe = false;
// Program handle provided by Renderer
static GLuint g_prog = 0;

// New: numeric widget toggle
static bool g_showNumericWidgets = false;

// Window visibility toggles (can be controlled from top menu)
static bool g_showToolsWindow = true;
static bool g_showAssetsWindow = true;
static bool g_showBottomWindow = true;
static bool g_showViewportWindow = true;
// Tool options (detailed tool panel shown from Edit menu)
static bool g_showToolOptions = true;

// Spawn helpers
static glm::vec2 g_spawnMousePos = glm::vec2(0.0f);
static bool g_spawnPending = false;
// Selected primitive type for spawning
static primitives::PrimitiveType g_spawnType = primitives::PrimitiveType::Cube;

// Pin states for windows (false = unpinned [ ], true = pinned [x])
static bool g_pinTools = false;
static bool g_pinAssets = false;
static bool g_pinBottom = false;
static bool g_pinViewport = false;

// ImGuizmo state
static ImGuizmo::OPERATION g_gizmoOperation = ImGuizmo::ROTATE;
static ImGuizmo::MODE g_gizmoMode = ImGuizmo::LOCAL;
static bool g_useImGuizmo = true;

// last view/proj used by tools and overlays
static glm::mat4 g_lastView = glm::mat4(1.0f);
static glm::mat4 g_lastProj = glm::mat4(1.0f);

// Track gizmo interaction for undo/redo
static bool g_imguizmoActive = false;
static int g_imguizmoEntity = 0;
static Scene::Transform g_imguizmoBefore;

// Offscreen framebuffer for viewport
static GLuint g_fbo = 0;
static GLuint g_fboColor = 0;
static GLuint g_fboDepth = 0;
static int g_fbo_width = 0;
static int g_fbo_height = 0;

static void destroyFBO() {
    if(g_fboDepth) { glDeleteRenderbuffers(1, &g_fboDepth); g_fboDepth = 0; }
    if(g_fboColor) { glDeleteTextures(1, &g_fboColor); g_fboColor = 0; }
    if(g_fbo) { glDeleteFramebuffers(1, &g_fbo); g_fbo = 0; }
    g_fbo_width = g_fbo_height = 0;
}

static void createFBO(int w, int h) {
    if(w <= 0 || h <= 0) return;
    if(g_fbo && g_fbo_width == w && g_fbo_height == h) return;
    destroyFBO();
    g_fbo_width = w; g_fbo_height = h;
    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);

    glGenTextures(1, &g_fboColor);
    glBindTexture(GL_TEXTURE_2D, g_fboColor);
    // ensure correct alignment for the texture data
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_fboColor, 0);

    glGenRenderbuffers(1, &g_fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, g_fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_fboDepth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Failed to create framebuffer, status: " << status << '\n';
        destroyFBO();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void framebuffer_size_callback(GLFWwindow* /*w*/, int w, int h) {
    g_window_width = w; g_window_height = h;
    glViewport(0,0,w,h);
}

bool g_useFixedTimestep = false;
float g_fixedTimestep = 1.0f / 60.0f;
float g_timeAccumulator = 0.0f;

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    if(!glfwInit()){
        std::cerr << "Failed to init GLFW\n"; return -1;
    }
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(g_window_width, g_window_height, "NovaDCC - Prototype", nullptr, nullptr);
    if(!window){ std::cerr << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Camera now manages scroll events itself
    g_camera.installCallbacks(window);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr << "Failed to initialize GLAD\n"; return -1;
    }

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Disable docking at runtime to prevent tabbed/docked headers interfering with our custom pin
#ifdef ImGuiConfigFlags_DockingEnable
    io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
#endif

    // Ensure ImGuizmo uses the same ImGui context
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

    // Increase gizmo visual sizes for better visibility (rotation/scale handles)
    {
        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.TranslationLineThickness = 3.0f;
        style.TranslationLineArrowSize = 18.0f;
        style.RotationLineThickness = 3.0f;
        style.RotationOuterLineThickness = 4.0f;
        style.ScaleLineThickness = 3.0f;
        style.ScaleLineCircleSize = 12.0f;
        style.CenterCircleSize = 8.0f;
    }

    // Enable viewports only if ImGui was built with that feature
#if defined(IMGUI_ENABLE_VIEWPORTS) || defined(IMGUI_HAS_VIEWPORTS)
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // install stdout/stderr capture into GUI console
    GuiConsole::instance().installStdStreams();

    // Initialize renderer resources
    Renderer::init();
    g_prog = Renderer::getProgram();
    glEnable(GL_DEPTH_TEST);

    // Default camera: look at origin from a 45-degree-ish direction
    g_camera.setPosition(glm::vec3(5.0f, 5.0f, 5.0f));

    // Ensure fallback Gizmo matches ImGuizmo default operation
    GizmoLib::SetFallbackOperation(g_gizmo, g_gizmoOperation);

    // Scene instance (use Scene API to manage entities)
    Scene scene;
    bool recordOnly = false; // if true, only increment spawn counter without creating meshes/entities

    // Animator: leave empty; user can add animations via Tools panel or code
    // g_animator.addRotationAnimation(...)

    // Docking state - docking is disabled in this build; windows are regular ImGui windows
    // bool dock_initialized = false;
    // (docking removed) 

    // Main loop
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Begin ImGuizmo frame (needed for proper gizmo interaction)
        ImGuizmo::BeginFrame();

        // Main application menu bar (File/Edit/View)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New")) { /* TODO */ }
                if (ImGui::MenuItem("Open")) { /* TODO */ }
                if (ImGui::MenuItem("Save")) { /* TODO */ }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, scene.canUndo())) { scene.undo(); }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, scene.canRedo())) { scene.redo(); }
                ImGui::Separator();
                // Panel visibility toggles
                ImGui::MenuItem("Tools Panel", NULL, &g_showToolsWindow);
                ImGui::MenuItem("Tool Options...", NULL, &g_showToolOptions);
                ImGui::MenuItem("Assets Panel", NULL, &g_showAssetsWindow);
                ImGui::MenuItem("Bottom Panel", NULL, &g_showBottomWindow);
                ImGui::MenuItem("Viewport Panel", NULL, &g_showViewportWindow);

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View") ) {
                ImGui::MenuItem("Wireframe", NULL, &g_showWireframe);
                ImGui::MenuItem("Use ImGuizmo", NULL, &g_useImGuizmo);
                ImGui::MenuItem("Show numeric fields", NULL, &g_showNumericWidgets);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Docking removed: do not create a dockspace. Windows will be regular ImGui windows.

        // Tools panel (left)
        if(g_showToolsWindow) {
            DrawToolsWindow(scene, g_camera, g_showToolsWindow, g_pinTools, g_spawnType, g_spawnMousePos, g_spawnPending, recordOnly, g_showWireframe, g_showToolOptions, g_gizmoOperation, g_gizmoMode, g_useImGuizmo, g_showNumericWidgets, g_gizmo, g_lastView, g_camera);
        }

        // Assets panel (right)
        if(g_showAssetsWindow) {
            DrawAssetsWindow(scene, g_showAssetsWindow, g_pinAssets);
        }

        // Bottom panel (tabs)
        if(g_showBottomWindow) {
            DrawBottomWindow(g_showBottomWindow, g_pinBottom);
        }

        // Viewport window (central) - extracted
        if(g_showViewportWindow) {
            ImGuiWindowFlags viewportFlags = 0;
            viewportFlags |= ImGuiWindowFlags_MenuBar;
            if (g_pinViewport) viewportFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
            ImGui::Begin("Viewport", &g_showViewportWindow, viewportFlags);
            ShowHeaderPin("pin_viewport", g_pinViewport);

            ViewportContext vctx;
            vctx.scene = &scene;
            vctx.camera = &g_camera;
            vctx.prog = &g_prog;
            vctx.showWireframe = &g_showWireframe;
            vctx.gizmo = &g_gizmo;
            vctx.gizmoOperation = &g_gizmoOperation;
            vctx.gizmoMode = &g_gizmoMode;
            vctx.useImGuizmo = &g_useImGuizmo;
            vctx.imguizmoActive = &g_imguizmoActive;
            vctx.imguizmoEntity = &g_imguizmoEntity;
            vctx.imguizmoBefore = &g_imguizmoBefore;
            vctx.lastView = &g_lastView;
            vctx.lastProj = &g_lastProj;
            vctx.pinViewport = &g_pinViewport;
            vctx.showViewportWindow = &g_showViewportWindow;
            vctx.window_width = &g_window_width;
            vctx.window_height = &g_window_height;
            vctx.spawnMousePos = &g_spawnMousePos;
            vctx.spawnPending = &g_spawnPending;
            vctx.spawnType = &g_spawnType;
            vctx.showHeaderPin = ShowHeaderPin;

            DrawViewportWindow(vctx);
            ImGui::End(); // Viewport
        }

        // Update animator: either use fixed timestep accumulator or ImGui::GetIO().DeltaTime
        float dt = ImGui::GetIO().DeltaTime;
        if(g_useFixedTimestep) {
            g_timeAccumulator += dt;
            while(g_timeAccumulator >= g_fixedTimestep) {
                g_animator.update(scene, g_fixedTimestep);
                g_timeAccumulator -= g_fixedTimestep;
            }
        } else {
            if(dt > 0.0f) g_animator.update(scene, dt);
        }

        // Prepare full-window GL state for ImGui rendering
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, g_window_width, g_window_height);

        // Clear both color and depth to remove previous UI/scene pixels before ImGui draw
        glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Ensure blending is enabled with the standard alpha blend for ImGui
        GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Disable depth test for UI rendering and remember previous state
        GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        if(depthEnabled) glDisable(GL_DEPTH_TEST);

        // ImGui render
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Restore depth test state for correctness next frame
        if(depthEnabled) glEnable(GL_DEPTH_TEST);
        if(!blendEnabled) glDisable(GL_BLEND);

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    Renderer::destroy();
    return 0;
}
