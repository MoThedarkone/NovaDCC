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

static Gizmo g_gizmo;

static int g_window_width = 1280;
static int g_window_height = 800;

// Camera instance
static Camera g_camera;
static bool g_mouseMiddleDown = false;
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
static bool g_showToolOptions = false;

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

// Pin icon textures (generated at runtime)
static GLuint g_pinTexPinned = 0;
static GLuint g_pinTexUnpinned = 0;

static void ensurePinTextures() {
    if(g_pinTexPinned && g_pinTexUnpinned) return;
    const int W = 16, H = 16;
    std::vector<unsigned char> pixPinned(W*H*4, 0);
    std::vector<unsigned char> pixUnpinned(W*H*4, 0);
    auto setPix = [&](std::vector<unsigned char>& buf, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a){
        if(x<0||x>=W||y<0||y>=H) return;
        int idx = (y*W + x) * 4;
        buf[idx+0] = r; buf[idx+1] = g; buf[idx+2] = b; buf[idx+3] = a;
    };
    // Pinned icon: circular head + vertical stem (dark color)
    for(int y=0;y<H;++y){
        for(int x=0;x<W;++x){
            float dx = x - 8.0f; float dy = y - 5.0f;
            float d2 = dx*dx + dy*dy;
            if(d2 <= 4.5f*4.5f) {
                setPix(pixPinned, x, y, 40, 120, 180, 255);
            }
            // stem
            if(x >= 7 && x <= 9 && y >= 9 && y <= 13) setPix(pixPinned, x, y, 40,120,180,255);
        }
    }
    // Unpinned icon: same head but lighter and tilted stem
    for(int y=0;y<H;++y){
        for(int x=0;x<W;++x){
            float dx = x - 8.0f; float dy = y - 5.0f;
            float d2 = dx*dx + dy*dy;
            if(d2 <= 4.5f*4.5f) {
                setPix(pixUnpinned, x, y, 220,220,220,255);
            }
        }
    }
    // draw tilted stem pixels for unpinned
    int x0 = 10, y0 = 7; int x1 = 13, y1 = 13;
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx + dy; int cx = x0, cy = y0;
    while(true){ setPix(pixUnpinned, cx, cy, 220,220,220,255); if(cx==x1 && cy==y1) break; int e2 = 2*err; if(e2>=dy){ err += dy; cx += sx;} if(e2<=dx){ err += dx; cy += sy;} }

    // Upload textures
    glGenTextures(1, &g_pinTexPinned);
    glBindTexture(GL_TEXTURE_2D, g_pinTexPinned);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixPinned.data());

    glGenTextures(1, &g_pinTexUnpinned);
    glBindTexture(GL_TEXTURE_2D, g_pinTexUnpinned);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixUnpinned.data());

    glBindTexture(GL_TEXTURE_2D, 0);
}

// Helper to draw a pin button in the top-right of the current window (titlebar/tab area).
static void ShowHeaderPin(const char* id, bool &pinned, float pin_w = 18.0f, float pin_h = 18.0f) {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();

    // Ensure textures exist
    ensurePinTextures();
    ImTextureID tex = (ImTextureID)(intptr_t)(pinned ? g_pinTexPinned : g_pinTexUnpinned);

    const float extraRightOffset = 6.0f;

    // If a menu bar is available, place the button into it so it aligns with titlebar controls
    if (ImGui::BeginMenuBar()) {
        // compute X inside window coordinates: align to right edge
        float posX = ImGui::GetWindowWidth() - pin_w - style.WindowPadding.x - extraRightOffset;
        ImGui::SetCursorPosX(posX);
        if (ImGui::ImageButton(id, tex, ImVec2(16,16), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0))) {
            pinned = !pinned;
        }
        // subtle hover outline handled by ImageButton internally
        ImGui::EndMenuBar();
        return;
    }

    // Fallback: draw in foreground and use invisible item for input (absolute coords)
    float tabH = ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
    ImVec2 pinPos;
    pinPos.x = winPos.x + winSize.x - pin_w - style.WindowPadding.x - extraRightOffset;
    pinPos.y = winPos.y - tabH + (tabH - pin_h) * 0.5f;
    if (pinPos.y < 0.0f) pinPos.y = winPos.y + style.FramePadding.y;
    ImVec2 pinMax = ImVec2(pinPos.x + pin_w, pinPos.y + pin_h);

    ImDrawList* fg = ImGui::GetForegroundDrawList();
    ImVec2 imgA = ImVec2(pinPos.x + (pin_w - 16.0f) * 0.5f, pinPos.y + (pin_h - 16.0f) * 0.5f);
    ImVec2 imgB = ImVec2(imgA.x + 16.0f, imgA.y + 16.0f);
    fg->AddImage(tex, imgA, imgB, ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,255));

    ImVec2 prevCursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(pinPos);
    ImGui::InvisibleButton(id, ImVec2(pin_w, pin_h));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) pinned = !pinned;
    if (hovered) fg->AddRect(pinPos, pinMax, IM_COL32(255,255,255,48), 2.0f);
    ImGui::SetCursorScreenPos(prevCursor);
}

// ImGuizmo state
static ImGuizmo::OPERATION g_gizmoOperation = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE g_gizmoMode = ImGuizmo::LOCAL;
static bool g_useImGuizmo = true;

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

static void scroll_callback(GLFWwindow* /*w*/, double /*xoff*/, double yoff) {
    g_camera.onScroll(yoff);
}

// add last view/proj globals used by ImGuizmo and overlays
static glm::mat4 g_lastView = glm::mat4(1.0f);
static glm::mat4 g_lastProj = glm::mat4(1.0f);

// forward declarations for overlay helpers (defined later in this file)
static void drawAxisOverlay(const SceneEntity* ent, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size);
static void drawRotationArcs(const SceneEntity* ent, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size);

// helper prototypes used by overlays
static float sqr(float x);
static float pointSegmentDist2(const ImVec2& p, const ImVec2& a, const ImVec2& b);

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
    glfwSetScrollCallback(window, scroll_callback);

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

    // Scene instance (use Scene API to manage entities)
    Scene scene;
    bool recordOnly = false; // if true, only increment spawn counter without creating meshes/entities

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
            if (ImGui::BeginMenu("View")) {
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
            ImGuiWindowFlags toolsFlags = 0;
            toolsFlags |= ImGuiWindowFlags_MenuBar;
            if (g_pinTools) toolsFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
            ImGui::Begin("Tools", &g_showToolsWindow, toolsFlags);
            // pin button in title/top-right
            ShowHeaderPin("pin_tools", g_pinTools);

         ImGui::Text("Primitives");
         ImGui::Checkbox("Record spawn only", &recordOnly);
         if(ImGui::Button("Cube")) {
            g_spawnType = primitives::PrimitiveType::Cube;
            if(recordOnly) scene.recordSpawnOnly();
            else {
                // schedule spawn at current mouse position (will resolve during render when view/proj available)
                ImGuiIO& io = ImGui::GetIO();
                g_spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y);
                g_spawnPending = true;
            }
         }
         ImGui::SameLine();
         if(ImGui::Button("Sphere")) {
            g_spawnType = primitives::PrimitiveType::Sphere;
            if(recordOnly) scene.recordSpawnOnly();
            else { ImGuiIO& io = ImGui::GetIO(); g_spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); g_spawnPending = true; }
         }
         ImGui::SameLine();
         if(ImGui::Button("Cylinder")) {
            g_spawnType = primitives::PrimitiveType::Cylinder;
            if(recordOnly) scene.recordSpawnOnly();
            else { ImGuiIO& io = ImGui::GetIO(); g_spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); g_spawnPending = true; }
         }
         ImGui::SameLine();
         if(ImGui::Button("Plane")) {
            g_spawnType = primitives::PrimitiveType::Plane;
            if(recordOnly) scene.recordSpawnOnly();
            else { ImGuiIO& io = ImGui::GetIO(); g_spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); g_spawnPending = true; }
         }
         ImGui::SameLine();
         if(ImGui::Button("Delete")) { scene.deleteSelected(); }
         ImGui::Separator();

         // Tools area: if tool options is requested, render gizmo controls here
         ImGui::SameLine();
         ImGui::Checkbox("Wireframe", &g_showWireframe); // quick inline toggle remains
         ImGui::Separator();
         // Numeric transform panel (always visible in Tools)
         ImGui::Text("Transform (selected)");
         SceneEntity* selEntTools = scene.findById(scene.getSelectedId());
         if(selEntTools) {
             glm::vec3 posVal = selEntTools->position;
             glm::vec3 rotVal = selEntTools->rotation;
             glm::vec3 sclVal = selEntTools->scale;
             if(ImGui::DragFloat3("Position", &posVal.x, 0.05f)) {
                 scene.setSelectedPosition(posVal);
             }
             if(ImGui::DragFloat3("Rotation", &rotVal.x, 0.5f)) {
                 scene.setSelectedRotation(rotVal);
             }
             if(ImGui::DragFloat3("Scale", &sclVal.x, 0.01f, 0.0001f)) {
                 sclVal.x = std::max(0.0001f, sclVal.x);
                 sclVal.y = std::max(0.0001f, sclVal.y);
                 sclVal.z = std::max(0.0001f, sclVal.z);
                 scene.setSelectedScale(sclVal);
             }
         } else {
             ImGui::TextDisabled("No entity selected");
         }

         if(g_showToolOptions) {
             ImGui::Text("Gizmo");
             ImGui::Checkbox("Use ImGuizmo", &g_useImGuizmo);
             if(ImGui::RadioButton("Translate", g_gizmoOperation == ImGuizmo::TRANSLATE)) g_gizmoOperation = ImGuizmo::TRANSLATE;
             ImGui::SameLine();
             if(ImGui::RadioButton("Rotate", g_gizmoOperation == ImGuizmo::ROTATE)) g_gizmoOperation = ImGuizmo::ROTATE;
             ImGui::SameLine();
             if(ImGui::RadioButton("Scale", g_gizmoOperation == ImGuizmo::SCALE)) g_gizmoOperation = ImGuizmo::SCALE;
             if(ImGui::RadioButton("Local", g_gizmoMode == ImGuizmo::LOCAL)) g_gizmoMode = ImGuizmo::LOCAL;
             ImGui::SameLine();
             if(ImGui::RadioButton("World", g_gizmoMode == ImGuizmo::WORLD)) g_gizmoMode = ImGuizmo::WORLD;
             ImGui::Checkbox("Show numeric fields", &g_showNumericWidgets);
             ImGui::Separator();

             // Keep fallback gizmo in sync with ImGuizmo operation
             if(g_gizmoOperation == ImGuizmo::TRANSLATE) g_gizmo.setOperation(Gizmo::Operation::Translate);
             else if(g_gizmoOperation == ImGuizmo::ROTATE) g_gizmo.setOperation(Gizmo::Operation::Rotate);
             else if(g_gizmoOperation == ImGuizmo::SCALE) g_gizmo.setOperation(Gizmo::Operation::Scale);

             // Show orientation manipulator (ImGuizmo) inside Tools panel
             // Prepare view matrix array
             float viewMatArr[16];
             memcpy(viewMatArr, &g_lastView[0][0], sizeof(viewMatArr));
             ImGuiIO& io = ImGui::GetIO();
             float fbSx = io.DisplayFramebufferScale.x;
             float fbSy = io.DisplayFramebufferScale.y;
             ImVec2 toolsPos = ImGui::GetCursorScreenPos();
             // Use logical (unscaled) widget size here and let SetRect receive framebuffer-scaled values
             float w = 80.0f, h = 80.0f;
              // position manipulator top-right of Tools panel
              ImVec2 manipPos = ImVec2(toolsPos.x + ImGui::GetContentRegionAvail().x - w - 8.0f, toolsPos.y + 4.0f);
            // Ensure ImGuizmo uses the current ImGui drawlist and rectangle for the Tools panel
            // so the manipulator can receive mouse events when rendered inside the panel.
            GizmoController::viewManipulate(g_lastView, 8.0f, ImVec2(manipPos.x, manipPos.y), ImVec2(w, h), [&](const glm::vec3& camPos){ g_camera.setPosition(camPos); });

            } // end if(g_showToolOptions)

        ImGui::End();
        }

        // Assets panel (right)
        if(g_showAssetsWindow) {
             ImGuiWindowFlags assetsFlags = 0;
             assetsFlags |= ImGuiWindowFlags_MenuBar;
             if (g_pinAssets) assetsFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
             ImGui::Begin("Assets", &g_showAssetsWindow, assetsFlags);
             // pin button in title/top-right
             ShowHeaderPin("pin_assets", g_pinAssets);

         ImGui::Text("Asset Browser (placeholder)");
         if(ImGui::Button("Import...")) { /* TODO */ }
         ImGui::Separator();

        // Show entity list for selection
        ImGui::Text("Entities:");
        const auto& ents = scene.entities();
        for(size_t i = 0; i < ents.size(); ++i) {
            std::string label = "Entity " + std::to_string(ents[i].id);
            bool selected = (scene.getSelectedId() == ents[i].id);
            if(ImGui::Selectable(label.c_str(), selected)) {
                scene.selectEntity(ents[i].id);
            }
        }

        ImGui::End();
        }

        // Bottom panel (tabs)
        if(g_showBottomWindow) {
             ImGuiWindowFlags bottomFlags = 0;
             bottomFlags |= ImGuiWindowFlags_MenuBar;
             if (g_pinBottom) bottomFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
             ImGui::Begin("Bottom", &g_showBottomWindow, bottomFlags);
             // pin button in title/top-right
             ShowHeaderPin("pin_bottom", g_pinBottom);

         if(ImGui::BeginTabBar("Tabs")){
            if(ImGui::BeginTabItem("Asset Viewer")) {
                ImGui::TextWrapped("Preview will appear here.");
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Console")) {
                // Console controls
                if(ImGui::Button("Clear")) {
                    GuiConsole::instance().clear();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Console output captured from stdout/stderr");

                ImGui::Separator();
                ImGui::BeginChild("ConsoleChild", ImVec2(0, ImGui::GetContentRegionAvail().y - 10), true, ImGuiWindowFlags_HorizontalScrollbar);
                auto lines = GuiConsole::instance().lines();
                for (const auto &l : lines) {
                    ImGui::TextUnformatted(l.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
        }

         // Viewport window (central)
        {
             ImGuiWindowFlags viewportFlags = 0;
             viewportFlags |= ImGuiWindowFlags_MenuBar;
             if (g_pinViewport) viewportFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
             ImGui::Begin("Viewport", &g_showViewportWindow, viewportFlags);
             // pin button in title/top-right
             ShowHeaderPin("pin_viewport", g_pinViewport);

         // Reserve the full area and get position/size for rendering
         ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
         ImVec2 viewport_size = ImGui::GetContentRegionAvail();
         // Determine whether mouse is over the viewport (screen coords)
         ImVec2 mpos = ImGui::GetIO().MousePos;
         bool mouseOnViewport = (mpos.x >= viewport_pos.x && mpos.x <= (viewport_pos.x + viewport_size.x) &&
                               mpos.y >= viewport_pos.y && mpos.y <= (viewport_pos.y + viewport_size.y));
        // create or resize FBO to match available content region
        int fb_w = (int)viewport_size.x;
        int fb_h = (int)viewport_size.y;
        if(fb_w > 0 && fb_h > 0) createFBO(fb_w, fb_h);

        // Handle middle-mouse orbit/pan only when mouse is over the viewport area
        if(mouseOnViewport) {
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
                double mx, my; glfwGetCursorPos(window, &mx, &my);
                glm::vec2 cur((float)mx, (float)my);
                if(!g_mouseMiddleDown) {
                    g_mouseMiddleDown = true;
                    g_camera.beginMiddleDrag(cur);
                } else {
                    bool alt = (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
                    g_camera.updateMiddleDrag(cur, alt);
                }
             } else {
                 if(g_mouseMiddleDown) g_camera.endMiddleDrag();
                 g_mouseMiddleDown = false;
             }
         }

        // Render into offscreen FBO (or fallback to default framebuffer)
        if(g_fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
            glViewport(0, 0, g_fbo_width, g_fbo_height);
            glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Camera
            glm::mat4 view = g_camera.getView();
            float aspect = (float)g_fbo_width / (g_fbo_height > 0 ? (float)g_fbo_height : 1.0f);
            glm::mat4 proj = g_camera.getProjection(aspect);
            glm::mat4 vp = proj * view;

            // store last view/proj so other UI (Tools) can access the camera orientation
            g_lastView = view;
            g_lastProj = proj;

            // If a spawn was requested earlier, resolve screen->world and spawn on ground plane (y=0)
            if(g_spawnPending) {
                // spawn at center of viewport
                float mx = viewport_size.x * 0.5f;
                float my = viewport_size.y * 0.5f;
                // convert to framebuffer coordinates (origin bottom-left)
                float fbx = mx;
                float fby = (float)g_fbo_height - my;
                glm::vec3 nearP = glm::unProject(glm::vec3(fbx, fby, 0.0f), view, proj, glm::vec4(0,0,g_fbo_width,g_fbo_height));
                glm::vec3 farP  = glm::unProject(glm::vec3(fbx, fby, 1.0f), view, proj, glm::vec4(0,0,g_fbo_width,g_fbo_height));
                glm::vec3 dir = glm::normalize(farP - nearP);
                // intersect with y=0 plane
                float t = 0.0f;
                if(fabs(dir.y) > 1e-6f) t = -nearP.y / dir.y;
                glm::vec3 spawnPos = nearP + dir * t;
                // Snap spawn to integer grid and position cube center at y=0.5 so cube sits on grid
                glm::vec3 snapped;
                snapped.x = std::floor(spawnPos.x) + 0.5f;
                snapped.y = 0.5f;
                snapped.z = std::floor(spawnPos.z) + 0.5f;
                int newId = scene.addPrimitive(g_spawnType, snapped);
                // set per-primitive default scale so spawned objects have sensible size
                glm::vec3 defaultScale(1.0f);
                switch(g_spawnType) {
                    case primitives::PrimitiveType::Cube:    defaultScale = glm::vec3(0.5f); break;
                    case primitives::PrimitiveType::Sphere:  defaultScale = glm::vec3(0.5f); break;
                    case primitives::PrimitiveType::Cylinder:defaultScale = glm::vec3(0.5f); break;
                    case primitives::PrimitiveType::Plane:   defaultScale = glm::vec3(1.0f); break;
                    default: defaultScale = glm::vec3(1.0f); break;
                }
                scene.setSelectedScale(defaultScale);
                 // optionally select the new cube (addCube already selects)
                 g_spawnPending = false;
            }

            // Render grid + scene entities into FBO
            glUseProgram(g_prog);
            if(g_showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            Renderer::renderGrid(vp);

            // Use Scene to draw all entities
            scene.drawAll(g_prog, vp);

            // ImGuizmo manipulation when rendering to FBO
            if(g_useImGuizmo && scene.getSelectedId() != 0) {
                bool active = GizmoController::manipulate(scene, view, proj, viewport_pos, viewport_size, g_gizmoOperation, g_gizmoMode, g_useImGuizmo);
                if(active) {
                    if(!g_imguizmoActive) { g_imguizmoActive = true; g_imguizmoEntity = scene.getSelectedId(); g_imguizmoBefore = scene.getEntityTransform(g_imguizmoEntity); }
                } else {
                    if(g_imguizmoActive) { Scene::Transform after = scene.getEntityTransform(g_imguizmoEntity); scene.pushCommand(std::unique_ptr<Scene::Command>(new Scene::TransformCommand(g_imguizmoEntity, g_imguizmoBefore, after))); g_imguizmoActive = false; }
                }
            } else {
                // fallback gizmo
                g_gizmo.drawGizmo(vp, glm::vec2(viewport_pos.x, viewport_pos.y), glm::vec2(viewport_size.x, viewport_size.y), scene);
            }

            // Draw selection box when rotating or scaling to improve visibility
            SceneEntity* selEnt = scene.findById(scene.getSelectedId());
            if(selEnt && (g_gizmoOperation == ImGuizmo::ROTATE || g_gizmoOperation == ImGuizmo::SCALE)) {
                Renderer::drawSelectionBox(vp, selEnt);
                // draw axis direction overlay (screen-space labels/lines)
                drawAxisOverlay(selEnt, view, proj, viewport_pos, viewport_size);
            }
            // Draw rotation arcs only when rotating
            if(selEnt && g_gizmoOperation == ImGuizmo::ROTATE) {
                drawRotationArcs(selEnt, view, proj, viewport_pos, viewport_size);
            }

            // Draw long axis lines for orientation (already added)
            // drawAxisLines(vp); // keep if desired (already called)

            // Done rendering to FBO
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        } else {
            // Fallback: render to default framebuffer if FBO not available
            glViewport(0, 0, g_window_width, g_window_height);
            glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Camera
            glm::mat4 view = g_camera.getView();
            float aspect = (float)g_window_width / (g_window_height > 0 ? (float)g_window_height : 1.0f);
            glm::mat4 proj = g_camera.getProjection(aspect);
            glm::mat4 vp = proj * view;

            // Render grid
            glUseProgram(g_prog);
            if(g_showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            Renderer::renderGrid(vp);

            // Use Scene to draw all entities
            scene.drawAll(g_prog, vp);

            // ViewManipulate intentionally removed from viewport fallback; widget will be shown in Tools panel

            // ImGuizmo manipulation (fallback path)
            if(g_useImGuizmo && scene.getSelectedId() != 0) {
                 // Use GizmoController to manipulate selected entity
                 bool active = GizmoController::manipulate(scene, view, proj, viewport_pos, viewport_size, g_gizmoOperation, g_gizmoMode, g_useImGuizmo);
                 // manage undo stack similar to previous logic
                 if(active) {
                     if(!g_imguizmoActive) { g_imguizmoActive = true; g_imguizmoEntity = scene.getSelectedId(); g_imguizmoBefore = scene.getEntityTransform(g_imguizmoEntity); }
                 } else {
                     if(g_imguizmoActive) { Scene::Transform after = scene.getEntityTransform(g_imguizmoEntity); scene.pushCommand(std::unique_ptr<Scene::Command>(new Scene::TransformCommand(g_imguizmoEntity, g_imguizmoBefore, after))); g_imguizmoActive = false; }
                 }
            } else {
                // Draw on-screen gizmo overlay (ImGui coords) - keep legacy gizmo for fallback
                g_gizmo.drawGizmo(vp, glm::vec2(viewport_pos.x, viewport_pos.y), glm::vec2(viewport_size.x, viewport_size.y), scene);
            }
        }

        // Show the rendered texture inside the same ImGui Viewport window
        ImVec2 show_size = viewport_size;
        if(g_fboColor && show_size.x > 0 && show_size.y > 0) {
            ImGui::Image((ImTextureID)(intptr_t)g_fboColor, show_size, ImVec2(0,1), ImVec2(1,0));
        } else {
            ImGui::Dummy(show_size);
        }

        ImGui::End(); // Viewport
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
    destroyFBO();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    Renderer::destroy();
    return 0;
}

// Implementations for overlays (axis labels and rotation arcs)
static void drawAxisOverlay(const SceneEntity* ent, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size) {
    if(!ent) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, ent->position);
    glm::quat q = glm::quat(glm::radians(ent->rotation));
    model *= glm::toMat4(q);
    model = glm::scale(model, ent->scale);

    glm::vec3 origin = glm::vec3(0.0f);
    glm::vec3 ax = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 ay = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 az = glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec4 o_w = model * glm::vec4(origin, 1.0f);
    glm::vec4 x_w = model * glm::vec4(ax, 1.0f);
    glm::vec4 y_w = model * glm::vec4(ay, 1.0f);
    glm::vec4 z_w = model * glm::vec4(az, 1.0f);

    glm::vec4 fb_viewport = glm::vec4(0, 0, (g_fbo ? g_fbo_width : g_window_width), (g_fbo ? g_fbo_height : g_window_height));
    glm::vec3 o_s = glm::project(glm::vec3(o_w), view, proj, fb_viewport);
    glm::vec3 x_s = glm::project(glm::vec3(x_w), view, proj, fb_viewport);
    glm::vec3 y_s = glm::project(glm::vec3(y_w), view, proj, fb_viewport);
    glm::vec3 z_s = glm::project(glm::vec3(z_w), view, proj, fb_viewport);

    ImVec2 o_p = ImVec2(vp_pos.x + o_s.x, vp_pos.y + (vp_size.y - o_s.y));
    ImVec2 x_p = ImVec2(vp_pos.x + x_s.x, vp_pos.y + (vp_size.y - x_s.y));
    ImVec2 y_p = ImVec2(vp_pos.x + y_s.x, vp_pos.y + (vp_size.y - y_s.y));
    ImVec2 z_p = ImVec2(vp_pos.x + z_s.x, vp_pos.y + (vp_size.y - z_s.y));

    dl->AddLine(o_p, x_p, IM_COL32(255,80,80,220), 3.0f);
    dl->AddLine(o_p, y_p, IM_COL32(255,230,60,220), 3.0f);
    dl->AddLine(o_p, z_p, IM_COL32(80,160,255,220), 3.0f);

    dl->AddText(font, fontSize, ImVec2(x_p.x + 4, x_p.y - fontSize*0.5f), IM_COL32(255,80,80,255), "X");
    dl->AddText(font, fontSize, ImVec2(y_p.x + 4, y_p.y - fontSize*0.5f), IM_COL32(255,230,60,255), "Y");
    dl->AddText(font, fontSize, ImVec2(z_p.x + 4, z_p.y - fontSize*0.5f), IM_COL32(80,160,255,255), "Z");
}

static void drawRotationArcs(const SceneEntity* ent, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size) {
    if(!ent) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 mouse = ImGui::GetIO().MousePos;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, ent->position);
    glm::quat q = glm::quat(glm::radians(ent->rotation));
    model *= glm::toMat4(q);
    model = glm::scale(model, ent->scale);

    float radius = std::max({ ent->scale.x, ent->scale.y, ent->scale.z }) * 1.5f;
    const int samples = 96;
    const float PI = 3.14159265358979323846f;

    struct AxisInfo { glm::vec3 axis; ImU32 worldCol; ImU32 localCol; };
    AxisInfo axes[3] = {
        { glm::vec3(1,0,0), IM_COL32(255,80,80,220), IM_COL32(255,140,140,220) },
        { glm::vec3(0,1,0), IM_COL32(255,230,60,220), IM_COL32(255,200,110,220) },
        { glm::vec3(0,0,1), IM_COL32(80,160,255,220), IM_COL32(140,190,255,220) }
    };

    for(int ai=0; ai<3; ++ai) {
        glm::vec3 axis = axes[ai].axis;
        glm::vec3 ex, ey;
        if(g_gizmoMode == ImGuizmo::LOCAL) {
            glm::vec3 ax_w = glm::normalize(glm::vec3(glm::toMat4(q) * glm::vec4(axis, 0.0f)));
            if (fabs(ax_w.y) < 0.9f) ex = glm::normalize(glm::cross(ax_w, glm::vec3(0,1,0)));
            else ex = glm::normalize(glm::cross(ax_w, glm::vec3(1,0,0)));
            ey = glm::normalize(glm::cross(ax_w, ex));
            ex *= radius; ey *= radius;
        } else {
            glm::vec3 ax_w = axis;
            if (fabs(ax_w.y) < 0.9f) ex = glm::normalize(glm::cross(ax_w, glm::vec3(0,1,0)));
            else ex = glm::normalize(glm::cross(ax_w, glm::vec3(1,0,0)));
            ey = glm::normalize(glm::cross(ax_w, ex));
            ex *= radius; ey *= radius;
        }

        std::vector<ImVec2> pts; pts.reserve(samples+1);
        glm::vec4 fb_viewport = glm::vec4(0, 0, (g_fbo ? g_fbo_width : g_window_width), (g_fbo ? g_fbo_height : g_window_height));
        for(int s=0;s<=samples;++s){
            float t = (float)s / (float)samples * 2.0f * PI;
            glm::vec3 p_obj = glm::vec3(0.0f) + (ex * cosf(t)) + (ey * sinf(t));
            glm::vec4 p_world = model * glm::vec4(p_obj, 1.0f);
            glm::vec3 p_screen = glm::project(glm::vec3(p_world), view, proj, fb_viewport);
            ImVec2 p_imgui = ImVec2(vp_pos.x + p_screen.x, vp_pos.y + (vp_size.y - p_screen.y));
            pts.push_back(p_imgui);
        }

        ImU32 col = (g_gizmoMode == ImGuizmo::LOCAL) ? axes[ai].localCol : axes[ai].worldCol;
        float minDist2 = FLT_MAX;
        for(int i=0;i<(int)pts.size()-1;++i){ float d2 = pointSegmentDist2(mouse, pts[i], pts[i+1]); if(d2 < minDist2) minDist2 = d2; }
        bool hover = minDist2 <= (16.0f*16.0f);
        float thickness = hover ? 4.0f : 2.0f;
        ImU32 drawCol = hover ? IM_COL32(255,255,255,255) : col;
        dl->AddPolyline(pts.data(), (int)pts.size(), drawCol, false, thickness);
        if(hover) {
            float bestD2 = FLT_MAX; ImVec2 bestP;
            for(int i=0;i<(int)pts.size()-1;++i){ ImVec2 a = pts[i]; ImVec2 b = pts[i+1]; ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y); ImVec2 ap = ImVec2(mouse.x - a.x, mouse.y - a.y); float ab2 = ab.x*ab.x + ab.y*ab.y; float t = (ab2==0.0f)?0.0f:((ap.x*ab.x + ap.y*ab.y)/ab2); t = std::max(0.0f, std::min(1.0f,t)); ImVec2 projp = ImVec2(a.x + ab.x*t, a.y + ab.y*t); float d2 = (mouse.x - projp.x)*(mouse.x - projp.x) + (mouse.y - projp.y)*(mouse.y - projp.y); if(d2 < bestD2){ bestD2 = d2; bestP = projp; } }
            dl->AddCircleFilled(bestP, 6.0f, IM_COL32(255,255,255,255));
            const char* lbl = (ai==0?"X":(ai==1?"Y":"Z"));
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bestP.x + 8, bestP.y - ImGui::GetFontSize()*0.5f), IM_COL32(255,255,255,255), lbl);
        }
    }
}

// helper: squared and point-to-segment squared distance (used by rotation arcs)
static float sqr(float x) { return x*x; }
static float pointSegmentDist2(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
    ImVec2 ap = ImVec2(p.x - a.x, p.y - a.y);
    float ab2 = ab.x*ab.x + ab.y*ab.y;
    if(ab2 == 0.0f) return ap.x*ap.x + ap.y*ap.y;
    float t = (ap.x*ab.x + ap.y*ab.y) / ab2;
    t = std::max(0.0f, std::min(1.0f, t));
    ImVec2 proj = ImVec2(a.x + ab.x * t, a.y + ab.y * t);
    return (p.x - proj.x)*(p.x - proj.x) + (p.y - proj.y)*(p.y - proj.y);
}
