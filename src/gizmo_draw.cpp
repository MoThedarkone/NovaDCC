#include "gizmo.h"
#include "scene.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

// Simple on-screen gizmo: draws 3 axis lines at selected entity position in screen space
// Handles simple dragging along an axis using mouse delta in screen space projected to world.

static glm::vec3 projectToScreen(const glm::vec3& worldPos, const glm::mat4& vp, const glm::vec2& viewPos, const glm::vec2& viewSize) {
    glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
    if(clip.w == 0.0f) return glm::vec3(-1.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    glm::vec2 screen = glm::vec2((ndc.x * 0.5f + 0.5f) * viewSize.x + viewPos.x, (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y + viewPos.y);
    return glm::vec3(screen, ndc.z);
}

static glm::vec3 unprojectFromScreen(const glm::vec3& screenPos, const glm::mat4& invVP, const glm::vec2& viewPos, const glm::vec2& viewSize) {
    // screenPos: x,y in pixels relative to window, z is NDC depth
    glm::vec2 ndc;
    ndc.x = (screenPos.x - viewPos.x) / viewSize.x * 2.0f - 1.0f;
    ndc.y = 1.0f - (screenPos.y - viewPos.y) / viewSize.y * 2.0f;
    glm::vec4 clip = glm::vec4(ndc, screenPos.z, 1.0f);
    glm::vec4 world = invVP * clip;
    if(world.w == 0.0f) return glm::vec3(0.0f);
    return glm::vec3(world) / world.w;
}

bool Gizmo::drawGizmo(const glm::mat4& vp, const glm::vec2& viewPos, const glm::vec2& viewSize, Scene& scene) {
    int sel = scene.getSelectedId();
    if(sel == 0) return false;
    SceneEntity* ent = scene.findById(sel);
    if(!ent || !ent->mesh) return false;

    // ImGui draw list for overlay
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    glm::vec3 worldPos = ent->position;
    glm::vec3 screenPos = projectToScreen(worldPos, vp, viewPos, viewSize);
    if(screenPos.x < 0 || screenPos.y < 0) return false;

    // Axis lengths in pixels
    const float axisLen = 80.0f;

    // Compute endpoints
    glm::vec3 xEnd = screenPos + glm::vec3(axisLen, 0.0f, 0.0f);
    glm::vec3 yEnd = screenPos + glm::vec3(0.0f, -axisLen, 0.0f);
    glm::vec3 zEnd = screenPos + glm::vec3(axisLen * 0.7f, axisLen * 0.7f, 0.0f);

    ImU32 colX = IM_COL32(220,80,80,255);
    ImU32 colY = IM_COL32(80,220,80,255);
    ImU32 colZ = IM_COL32(80,120,220,255);

    // Draw lines
    dl->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(xEnd.x, xEnd.y), colX, 3.0f);
    dl->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(yEnd.x, yEnd.y), colY, 3.0f);
    dl->AddLine(ImVec2(screenPos.x, screenPos.y), ImVec2(zEnd.x, zEnd.y), colZ, 3.0f);

    // Hit testing and dragging
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mpos(io.MousePos.x, io.MousePos.y);
    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    static bool wasDragging = false;
    static Axis dragAxis = Axis::None;
    static glm::vec3 initialPos;
    static glm::vec3 initialRot;
    static glm::vec3 initialScale;
    static glm::vec2 startMouse;

    auto nearPointDist = [&](const glm::vec3& p)->float{
        return glm::length(glm::vec2(p.x - mpos.x, p.y - mpos.y));
    };

    float dX = nearPointDist(xEnd);
    float dY = nearPointDist(yEnd);
    float dZ = nearPointDist(zEnd);
    float mind = std::min(std::min(dX,dY),dZ);

    if(!wasDragging && mouseDown) {
        // start drag if near any axis
        const float thresh = 12.0f;
        if(mind < thresh) {
            if(mind == dX) dragAxis = Axis::X;
            else if(mind == dY) dragAxis = Axis::Y;
            else dragAxis = Axis::Z;
            wasDragging = true;
            initialPos = ent->position;
            initialRot = ent->rotation;
            initialScale = ent->scale;
            startMouse = mpos;
            dragging_ = true;
        }
    }

    if(wasDragging && !mouseDown) {
        // commit change based on current operation
        wasDragging = false;
        dragging_ = false;
        // push undo command
        Scene::Transform before; before.position = initialPos; before.rotation = initialRot; before.scale = initialScale;
        Scene::Transform after; after.position = ent->position; after.rotation = ent->rotation; after.scale = ent->scale;
        scene.pushCommand(std::unique_ptr<Scene::Command>(new Scene::TransformCommand(sel, before, after)));
        dragAxis = Axis::None;
        return true; // committed change
    }

    if(wasDragging && mouseDown) {
        // compute mouse delta and modify based on operation type
        glm::vec2 delta = mpos - startMouse;
        float moveScale = 0.01f * (/*scale with view size*/ (viewSize.y));
        float rotScale = 0.3f; // degrees per pixel
        float scaleScale = 0.005f; // scale factor per pixel

        if(op_ == Operation::Translate) {
            glm::vec3 newPos = initialPos;
            if(dragAxis == Axis::X) newPos.x = initialPos.x + delta.x * moveScale;
            if(dragAxis == Axis::Y) newPos.y = initialPos.y - delta.y * moveScale;
            if(dragAxis == Axis::Z) newPos.z = initialPos.z + (delta.x - delta.y) * 0.01f;
            ent->position = newPos;
        } else if(op_ == Operation::Rotate) {
            // Simple rotation: mouse X affects rotation around Y, mouse Y affects rotation around X
            glm::vec3 newRot = initialRot;
            if(dragAxis == Axis::X) newRot.x = initialRot.x + delta.y * rotScale;
            if(dragAxis == Axis::Y) newRot.y = initialRot.y + delta.x * rotScale;
            if(dragAxis == Axis::Z) newRot.z = initialRot.z + (delta.x - delta.y) * rotScale;
            ent->rotation = newRot;
        } else if(op_ == Operation::Scale) {
            glm::vec3 newScale = initialScale;
            if(dragAxis == Axis::X) newScale.x = std::max(0.001f, initialScale.x + delta.x * scaleScale);
            if(dragAxis == Axis::Y) newScale.y = std::max(0.001f, initialScale.y - delta.y * scaleScale);
            if(dragAxis == Axis::Z) newScale.z = std::max(0.001f, initialScale.z + (delta.x - delta.y) * scaleScale);
            ent->scale = newScale;
        }
        return true;
    }

    return false;
}
