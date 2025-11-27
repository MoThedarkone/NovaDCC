#pragma once

#include "scene.h"
#include "camera.h"
#include "gizmo.h"
#include "primitive_factory.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "ui_helpers.h"
#include "gizmo_controller.h"

void DrawToolsWindow(Scene& scene,
                     Camera& camera,
                     bool& showToolsWindow,
                     bool& pinTools,
                     primitives::PrimitiveType& spawnType,
                     glm::vec2& spawnMousePos,
                     bool& spawnPending,
                     bool& recordOnly,
                     bool& showWireframe,
                     bool& showToolOptions,
                     ImGuizmo::OPERATION& gizmoOperation,
                     ImGuizmo::MODE& gizmoMode,
                     bool& useImGuizmo,
                     bool& showNumericWidgets,
                     Gizmo& fallbackGizmo,
                     const glm::mat4& lastView,
                     Camera& cameraRef);
