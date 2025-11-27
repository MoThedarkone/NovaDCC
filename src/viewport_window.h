#pragma once

#include "scene.h"
#include "camera.h"
#include "gizmo.h"
#include <glm/glm.hpp>
#include "imgui.h"
#include "ImGuizmo.h"
#include <functional>
#include "primitive_factory.h"

struct ViewportContext {
    Scene* scene;
    Camera* camera;
    GLuint* prog;
    bool* showWireframe;
    Gizmo* gizmo;
    ImGuizmo::OPERATION* gizmoOperation;
    ImGuizmo::MODE* gizmoMode;
    bool* useImGuizmo;
    bool* imguizmoActive;
    int* imguizmoEntity;
    Scene::Transform* imguizmoBefore;
    glm::mat4* lastView;
    glm::mat4* lastProj;
    bool* pinViewport;
    bool* showViewportWindow;
    int* window_width;
    int* window_height;
    // pin textures (for ShowHeaderPin rendering via callback)
    GLuint* pinTexPinned;
    GLuint* pinTexUnpinned;
    // Spawn helpers
    glm::vec2* spawnMousePos;
    bool* spawnPending;
    primitives::PrimitiveType* spawnType;
    // callback to draw the header pin (main.cpp still owns the implementation)
    std::function<void(const char*, bool&, float, float)> showHeaderPin;
};

// Draw the viewport UI, render scene into an offscreen FBO and handle gizmo/spawn logic.
// The function preserves behavior of original code and uses values supplied via ViewportContext.
void DrawViewportWindow(ViewportContext& ctx);
