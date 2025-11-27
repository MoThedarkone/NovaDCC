#pragma once

#include "gizmo.h"
#include "scene.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include <glm/glm.hpp>
#include <functional>

namespace GizmoLib {

// Initialize/Shutdown (no-op wrappers for now)
void init();
void shutdown();

// Wrapper for ImGuizmo manipulation that preserves original signature and behavior
bool ManipulateScene(Scene& scene, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, bool useImGuizmo);

// Wrapper to draw the legacy/fallback gizmo
bool DrawFallbackGizmo(const glm::mat4& vp, const glm::vec2& viewPos, const glm::vec2& viewSize, Gizmo& fallback, Scene& scene);

// Helper to sync fallback gizmo operation with ImGuizmo
void SetFallbackOperation(Gizmo& fallback, ImGuizmo::OPERATION op);

// View manipulator wrapper
void ViewManipulate(const glm::mat4& view, float size, const ImVec2& pos, const ImVec2& sizePx, std::function<void(const glm::vec3&)> cameraPosCallback);

// Overlay helpers (drawn in screen/ImGui space)
void DrawAxisOverlay(const SceneEntity* ent, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size);
void DrawRotationArcs(Scene& scene, int entId, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size, ImGuizmo::MODE mode);

} // namespace GizmoLib
