#pragma once

#include "scene.h"
#include <glm/glm.hpp>
#include "imgui.h"
#include "ImGuizmo.h"
#include <functional>

namespace GizmoController {
    void init();
    void shutdown();

    // Manipulate the selected entity using ImGuizmo. Returns true if ImGuizmo handled interaction.
    bool manipulate(Scene& scene, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, bool useImGuizmo);

    // Show a small view manipulator (for Tools panel). If the manipulator changes the camera, callback must be used externally.
    void viewManipulate(const glm::mat4& view, float size, const ImVec2& pos, const ImVec2& sizePx, std::function<void(const glm::vec3&)> cameraPosCallback);
}
