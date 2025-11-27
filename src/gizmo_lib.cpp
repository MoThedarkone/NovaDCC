#include "gizmo_lib.h"
#include "gizmo_controller.h"
#include "renderer.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

namespace GizmoLib {

void init() {}
void shutdown() {}

bool ManipulateScene(Scene& scene, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size, ImGuizmo::OPERATION op, ImGuizmo::MODE mode, bool useImGuizmo) {
    return GizmoController::manipulate(scene, view, proj, vp_pos, vp_size, op, mode, useImGuizmo);
}

bool DrawFallbackGizmo(const glm::mat4& vp, const glm::vec2& viewPos, const glm::vec2& viewSize, Gizmo& fallback, Scene& scene) {
    return fallback.drawGizmo(vp, viewPos, viewSize, scene);
}

void SetFallbackOperation(Gizmo& fallback, ImGuizmo::OPERATION op) {
    if(op == ImGuizmo::TRANSLATE) fallback.setOperation(Gizmo::Operation::Translate);
    else if(op == ImGuizmo::ROTATE) fallback.setOperation(Gizmo::Operation::Rotate);
    else if(op == ImGuizmo::SCALE) fallback.setOperation(Gizmo::Operation::Scale);
}

void ViewManipulate(const glm::mat4& view, float size, const ImVec2& pos, const ImVec2& sizePx, std::function<void(const glm::vec3&)> cameraPosCallback) {
    GizmoController::viewManipulate(view, size, pos, sizePx, cameraPosCallback);
}

void DrawAxisOverlay(const SceneEntity* ent, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size) {
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

    glm::vec4 fb_viewport = glm::vec4(0, 0, (int)vp_size.x, (int)vp_size.y);
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

// helper used by DrawRotationArcs
static float pointSegmentDist2_f(const ImVec2& p, const ImVec2& a, const ImVec2& b) {
    ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
    ImVec2 ap = ImVec2(p.x - a.x, p.y - a.y);
    float ab2 = ab.x*ab.x + ab.y*ab.y;
    if(ab2 == 0.0f) return ap.x*ap.x + ap.y*ap.y;
    float t = (ap.x*ab.x + ap.y*ab.y) / ab2;
    t = std::max(0.0f, std::min(1.0f, t));
    ImVec2 proj = ImVec2(a.x + ab.x * t, a.y + ab.y * t);
    return (p.x - proj.x)*(p.x - proj.x) + (p.y - proj.y)*(p.y - proj.y);
}

void DrawRotationArcs(Scene& scene, int entId, const glm::mat4& view, const glm::mat4& proj, const ImVec2& vp_pos, const ImVec2& vp_size, ImGuizmo::MODE mode) {
    SceneEntity* ent = scene.findById(entId);
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

    // Drag state (static to persist across frames)
    static bool dragging = false;
    static int dragAxis = -1; // 0=x,1=y,2=z
    static Scene::Transform beforeTransform;
    static ImVec2 dragStartMouse;

    for(int ai=0; ai<3; ++ai) {
        glm::vec3 axis = axes[ai].axis;
        glm::vec3 ex, ey;
        if(mode == ImGuizmo::LOCAL) {
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
        glm::vec4 fb_viewport = glm::vec4(0, 0, (int)vp_size.x, (int)vp_size.y);
        for(int s=0;s<=samples;++s){
            float t = (float)s / (float)samples * 2.0f * PI;
            glm::vec3 p_obj = glm::vec3(0.0f) + (ex * cosf(t)) + (ey * sinf(t));
            glm::vec4 p_world = model * glm::vec4(p_obj, 1.0f);
            glm::vec3 p_screen = glm::project(glm::vec3(p_world), view, proj, fb_viewport);
            ImVec2 p_imgui = ImVec2(vp_pos.x + p_screen.x, vp_pos.y + (vp_size.y - p_screen.y));
            pts.push_back(p_imgui);
        }

        ImU32 col = (mode == ImGuizmo::LOCAL) ? axes[ai].localCol : axes[ai].worldCol;
        float minDist2 = FLT_MAX;
        for(int i=0;i<(int)pts.size()-1;++i){ float d2 = pointSegmentDist2_f(mouse, pts[i], pts[i+1]); if(d2 < minDist2) minDist2 = d2; }
        bool hover = minDist2 <= (16.0f*16.0f);
        float thickness = hover ? 4.0f : 2.0f;
        ImU32 drawCol = hover ? IM_COL32(255,255,255,255) : col;
        dl->AddPolyline(pts.data(), (int)pts.size(), drawCol, false, thickness);

        // Begin drag
        if(!dragging && hover && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            dragging = true; dragAxis = ai; beforeTransform = scene.getEntityTransform(entId); dragStartMouse = mouse;
        }

        if(dragging && dragAxis == ai) {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                // compute simple delta angle based on mouse X movement
                ImVec2 delta = ImVec2(mouse.x - dragStartMouse.x, mouse.y - dragStartMouse.y);
                float ang = (delta.x - delta.y) * 0.3f; // degrees per pixel
                Scene::Transform nt = beforeTransform;
                if(ai==0) nt.rotation.x = beforeTransform.rotation.x + ang;
                if(ai==1) nt.rotation.y = beforeTransform.rotation.y + ang;
                if(ai==2) nt.rotation.z = beforeTransform.rotation.z + ang;
                scene.setEntityTransform(entId, nt);
            } else {
                // mouse released -> commit
                dragging = false;
                Scene::Transform after = scene.getEntityTransform(entId);
                scene.pushCommand(std::unique_ptr<Scene::Command>(new Scene::TransformCommand(entId, beforeTransform, after)));
                dragAxis = -1;
            }
        }
    }
}

} // namespace GizmoLib
