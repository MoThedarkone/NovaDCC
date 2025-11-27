#include "gizmo_controller.h"
#include <cstring>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace GizmoController {

void init() {
    // nothing for now
}

void shutdown() {
}

bool manipulate(Scene& scene, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, bool useImGuizmo) {
    if(!useImGuizmo) return false;
    if(scene.getSelectedId() == 0) return false;

    ImGuiIO& io = ImGui::GetIO();
    float fbSx = io.DisplayFramebufferScale.x;
    float fbSy = io.DisplayFramebufferScale.y;
    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(vp_pos.x * fbSx, vp_pos.y * fbSy, vp_size.x * fbSx, vp_size.y * fbSy);

    SceneEntity* ent = scene.findById(scene.getSelectedId());
    if(!ent) return false;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, ent->position);
    model *= glm::toMat4(glm::quat(glm::radians(ent->rotation)));
    model = glm::scale(model, ent->scale);

    float viewMat[16]; float projMat[16]; float modelMat[16];
    memcpy(viewMat, &view[0][0], sizeof(viewMat));
    memcpy(projMat, &proj[0][0], sizeof(projMat));
    memcpy(modelMat, &model[0][0], sizeof(modelMat));

    ImGuizmo::Manipulate(viewMat, projMat, op, mode, modelMat, NULL);
    if(ImGuizmo::IsUsing()) {
        float t[3], r[3], s[3];
        ImGuizmo::DecomposeMatrixToComponents(modelMat, t, r, s);
        // push undo on begin is handled by main (g_imguizmoActive)
        scene.setSelectedPosition(glm::vec3(t[0], t[1], t[2]));
        scene.setSelectedRotation(glm::vec3(r[0], r[1], r[2]));
        scene.setSelectedScale(glm::vec3(s[0], s[1], s[2]));
        return true;
    }
    return false;
}

void viewManipulate(const glm::mat4& view, float size, const ImVec2& pos, const ImVec2& sizePx, std::function<void(const glm::vec3&)> cameraPosCallback) {
    ImGuiIO& io = ImGui::GetIO();
    float fbSx = io.DisplayFramebufferScale.x;
    float fbSy = io.DisplayFramebufferScale.y;
    float viewMatArr[16]; memcpy(viewMatArr, &view[0][0], sizeof(viewMatArr));
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(pos.x * fbSx, pos.y * fbSy, sizePx.x * fbSx, sizePx.y * fbSy);
    ImGuizmo::ViewManipulate(viewMatArr, size, ImVec2(pos.x, pos.y), sizePx, 0);
    if(ImGuizmo::IsUsing()) {
        glm::mat4 newView; memcpy(&newView[0][0], viewMatArr, sizeof(viewMatArr));
        glm::mat4 invView = glm::inverse(newView);
        glm::vec3 newCamPos = glm::vec3(invView[3]);
        if(cameraPosCallback) cameraPosCallback(newCamPos);
    }
}

} // namespace GizmoController
