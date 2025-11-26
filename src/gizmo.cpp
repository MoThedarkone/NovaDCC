#include "gizmo.h"
#include "scene.h"
#include "imgui.h"
#include <glm/glm.hpp>

// Helper: project a world-space position to ImGui screen coordinates using VP matrix and viewport rect
static glm::vec3 projectToScreen(const glm::vec3& worldPos, const glm::mat4& vp, const glm::vec2& viewPos, const glm::vec2& viewSize) {
    glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
    if (clip.w == 0.0f) return glm::vec3(-1.0f, -1.0f, -1.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w; // -1..1
    // Convert NDC to window coords (ImGui uses top-left origin)
    float sx = (ndc.x * 0.5f + 0.5f) * viewSize.x + viewPos.x;
    float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y + viewPos.y;
    return glm::vec3(sx, sy, ndc.z);
}

bool Gizmo::translationWidget(Scene& scene, glm::vec3& newPos) {
    int sel = scene.getSelectedId();
    if(sel == 0) return false;
    SceneEntity* ent = scene.findById(sel);
    if(!ent) return false;

    glm::vec3 pos = ent->position;
    float p[3] = { pos.x, pos.y, pos.z };
    ImGui::PushID(sel);
    bool changed = ImGui::InputFloat3("Position", p);
    ImGui::PopID();
    if(changed) {
        newPos = glm::vec3(p[0], p[1], p[2]);
        return true;
    }
    return false;
}

bool Gizmo::rotationWidget(Scene& scene, glm::vec3& newEulerDeg) {
    int sel = scene.getSelectedId();
    if(sel == 0) return false;
    SceneEntity* ent = scene.findById(sel);
    if(!ent) return false;

    glm::vec3 rot = ent->rotation;
    float r[3] = { rot.x, rot.y, rot.z };
    ImGui::PushID(sel+1000);
    bool changed = ImGui::InputFloat3("Rotation (deg)", r);
    ImGui::PopID();
    if(changed) {
        newEulerDeg = glm::vec3(r[0], r[1], r[2]);
        return true;
    }
    return false;
}

bool Gizmo::scaleWidget(Scene& scene, glm::vec3& newScale) {
    int sel = scene.getSelectedId();
    if(sel == 0) return false;
    SceneEntity* ent = scene.findById(sel);
    if(!ent) return false;

    glm::vec3 s = ent->scale;
    float sc[3] = { s.x, s.y, s.z };
    ImGui::PushID(sel+2000);
    bool changed = ImGui::InputFloat3("Scale", sc);
    ImGui::PopID();
    if(changed) {
        newScale = glm::vec3(sc[0], sc[1], sc[2]);
        return true;
    }
    return false;
}

// drawGizmo implementation is provided in gizmo_draw.cpp to avoid duplicate symbols
