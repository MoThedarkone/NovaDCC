#include "tools_window.h"
#include "gizmo_controller.h"
#include "animator.h"
#include <cstring>
#include <functional>

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
                     Camera& cameraRef) {
    ImGuiWindowFlags toolsFlags = 0;
    toolsFlags |= ImGuiWindowFlags_MenuBar;
    if (pinTools) toolsFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Tools", &showToolsWindow, toolsFlags);
    ShowHeaderPin("pin_tools", pinTools);

    ImGui::Text("Primitives");
    ImGui::Checkbox("Record spawn only", &recordOnly);
    if(ImGui::Button("Cube")) {
        spawnType = primitives::PrimitiveType::Cube;
        if(recordOnly) scene.recordSpawnOnly();
        else { ImGuiIO& io = ImGui::GetIO(); spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); spawnPending = true; }
    }
    ImGui::SameLine();
    if(ImGui::Button("Sphere")) {
        spawnType = primitives::PrimitiveType::Sphere;
        if(recordOnly) scene.recordSpawnOnly();
        else { ImGuiIO& io = ImGui::GetIO(); spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); spawnPending = true; }
    }
    ImGui::SameLine();
    if(ImGui::Button("Cylinder")) {
        spawnType = primitives::PrimitiveType::Cylinder;
        if(recordOnly) scene.recordSpawnOnly();
        else { ImGuiIO& io = ImGui::GetIO(); spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); spawnPending = true; }
    }
    ImGui::SameLine();
    if(ImGui::Button("Plane")) {
        spawnType = primitives::PrimitiveType::Plane;
        if(recordOnly) scene.recordSpawnOnly();
        else { ImGuiIO& io = ImGui::GetIO(); spawnMousePos = glm::vec2(io.MousePos.x, io.MousePos.y); spawnPending = true; }
    }
    ImGui::SameLine();
    if(ImGui::Button("Delete")) { scene.deleteSelected(); }
    ImGui::Separator();

    ImGui::SameLine();
    ImGui::Checkbox("Wireframe", &showWireframe);
    ImGui::Separator();

    // Numeric transform panel
    ImGui::Text("Transform (selected)");
    SceneEntity* selEnt = scene.findById(scene.getSelectedId());
    if(selEnt) {
        glm::vec3 pos = selEnt->position;
        glm::vec3 rot = selEnt->rotation;
        glm::vec3 scl = selEnt->scale;
        if(ImGui::DragFloat3("Position", &pos.x, 0.05f)) scene.setSelectedPosition(pos);
        if(ImGui::DragFloat3("Rotation", &rot.x, 0.5f)) scene.setSelectedRotation(rot);
        if(ImGui::DragFloat3("Scale", &scl.x, 0.01f, 0.0001f)) {
            scl.x = std::max(0.0001f, scl.x);
            scl.y = std::max(0.0001f, scl.y);
            scl.z = std::max(0.0001f, scl.z);
            scene.setSelectedScale(scl);
        }

        // Animator controls for selected entity
        ImGui::Separator();
        ImGui::Text("Animator");
        static float spinSpeed = 45.0f;
        ImGui::DragFloat("Spin speed (deg/s)", &spinSpeed, 1.0f, -360.0f, 360.0f);
        if(ImGui::Button("Start spin on selected")) {
            g_animator.addRotationAnimation(selEnt->id, glm::vec3(0.0f,1.0f,0.0f), spinSpeed);
        }
        ImGui::SameLine();
        if(ImGui::Button("Stop spin on selected")) {
            g_animator.removeAnimationsForEntity(selEnt->id);
        }

        // Translate/Scale quick adds
        static glm::vec3 translateVel = glm::vec3(0.0f, 0.0f, 0.0f);
        ImGui::DragFloat3("Translate vel (units/s)", &translateVel.x, 0.01f);
        if(ImGui::Button("Start translate on selected")) { g_animator.addTranslateAnimation(selEnt->id, translateVel); }
        ImGui::SameLine();
        if(ImGui::Button("Stop translate on selected")) { g_animator.removeAnimationsForEntity(selEnt->id); }

        static glm::vec3 scaleDelta = glm::vec3(0.0f, 0.0f, 0.0f);
        ImGui::DragFloat3("Scale delta (per s)", &scaleDelta.x, 0.01f);
        if(ImGui::Button("Start scale on selected")) { g_animator.addScaleAnimation(selEnt->id, scaleDelta); }
        ImGui::SameLine();
        if(ImGui::Button("Stop scale on selected")) { g_animator.removeAnimationsForEntity(selEnt->id); }
    } else ImGui::TextDisabled("No entity selected");

    // Animator persistence and timestep controls
    ImGui::Separator();
    ImGui::Text("Animator settings");
    static char savePath[260] = "animations.txt";
    ImGui::InputText("Save path", savePath, sizeof(savePath));
    if(ImGui::Button("Save animations")) { g_animator.saveToFile(savePath); }
    ImGui::SameLine();
    if(ImGui::Button("Load animations")) { g_animator.loadFromFile(savePath); }

    ImGui::Checkbox("Use fixed animator timestep", &g_useFixedTimestep);
    ImGui::SameLine();
    ImGui::DragFloat("Fixed timestep (s)", &g_fixedTimestep, 0.001f, 0.001f, 0.5f);

    if(showToolOptions) {
        ImGui::Text("Gizmo");
        ImGui::Checkbox("Use ImGuizmo", &useImGuizmo);
        if(ImGui::RadioButton("Translate", gizmoOperation == ImGuizmo::TRANSLATE)) gizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if(ImGui::RadioButton("Rotate", gizmoOperation == ImGuizmo::ROTATE)) gizmoOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if(ImGui::RadioButton("Scale", gizmoOperation == ImGuizmo::SCALE)) gizmoOperation = ImGuizmo::SCALE;
        if(ImGui::RadioButton("Local", gizmoMode == ImGuizmo::LOCAL)) gizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if(ImGui::RadioButton("World", gizmoMode == ImGuizmo::WORLD)) gizmoMode = ImGuizmo::WORLD;
        ImGui::Checkbox("Show numeric fields", &showNumericWidgets);
        ImGui::Separator();

        if(gizmoOperation == ImGuizmo::TRANSLATE) fallbackGizmo.setOperation(Gizmo::Operation::Translate);
        else if(gizmoOperation == ImGuizmo::ROTATE) fallbackGizmo.setOperation(Gizmo::Operation::Rotate);
        else if(gizmoOperation == ImGuizmo::SCALE) fallbackGizmo.setOperation(Gizmo::Operation::Scale);

        // Orientation manipulator
        float viewMatArr[16]; memcpy(viewMatArr, &lastView[0][0], sizeof(viewMatArr));
        ImGuiIO& io = ImGui::GetIO();
        float fbSx = io.DisplayFramebufferScale.x; float fbSy = io.DisplayFramebufferScale.y;
        ImVec2 toolsPos = ImGui::GetCursorScreenPos();
        float w = 80.0f, h = 80.0f;
        ImVec2 manipPos = ImVec2(toolsPos.x + ImGui::GetContentRegionAvail().x - w - 8.0f, toolsPos.y + 4.0f);
        GizmoController::viewManipulate(lastView, 8.0f, ImVec2(manipPos.x, manipPos.y), ImVec2(w, h), [&](const glm::vec3& camPos){ cameraRef.setPosition(camPos); });
    }

    ImGui::End();
}
