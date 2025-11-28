#include "viewport_window.h"
#include "renderer.h"
#include "gizmo_controller.h"
#include "gizmo_lib.h"
#include "primitive_factory.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cfloat>
#include <iostream>

// Local FBO state (kept internal to this module)
static GLuint s_fbo = 0;
static GLuint s_fboColor = 0;
static GLuint s_fboDepth = 0;
static int s_fbo_w = 0;
static int s_fbo_h = 0;

// Extern spawn placement mode
SpawnPlacementMode g_spawnPlacementMode = SpawnPlacementMode::Origin;
// Align spawned object to surface normal (defined here to back extern)
bool g_spawnAlignToNormal = false;

static void ensureFBO(int w, int h) {
    if(w <= 0 || h <= 0) return;
    if(s_fbo && s_fbo_w == w && s_fbo_h == h) return;
    if(s_fboDepth) { glDeleteRenderbuffers(1, &s_fboDepth); s_fboDepth = 0; }
    if(s_fboColor) { glDeleteTextures(1, &s_fboColor); s_fboColor = 0; }
    if(s_fbo) { glDeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
    s_fbo_w = w; s_fbo_h = h;
    glGenFramebuffers(1, &s_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glGenTextures(1, &s_fboColor);
    glBindTexture(GL_TEXTURE_2D, s_fboColor);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_fboColor, 0);
    glGenRenderbuffers(1, &s_fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, s_fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_fboDepth);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Failed to create framebuffer, status: " << status << '\n';
        if(s_fboDepth) { glDeleteRenderbuffers(1, &s_fboDepth); s_fboDepth = 0; }
        if(s_fboColor) { glDeleteTextures(1, &s_fboColor); s_fboColor = 0; }
        if(s_fbo) { glDeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
        s_fbo_w = s_fbo_h = 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Helper: unproject screen point to world ray (origin, dir)
static void screenPointToRay(const glm::vec2& screenPos, const ImVec2& vp_pos, const ImVec2& vp_size, const glm::mat4& view, const glm::mat4& proj, glm::vec3& outOrigin, glm::vec3& outDir) {
    // NDC
    float x = (screenPos.x - vp_pos.x) / vp_size.x * 2.0f - 1.0f;
    float y = 1.0f - (screenPos.y - vp_pos.y) / vp_size.y * 2.0f;
    glm::vec4 nearPoint = glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = glm::vec4(x, y, 1.0f, 1.0f);
    glm::mat4 inv = glm::inverse(proj * view);
    glm::vec4 nearWorld = inv * nearPoint; nearWorld /= nearWorld.w;
    glm::vec4 farWorld = inv * farPoint; farWorld /= farWorld.w;
    outOrigin = glm::vec3(nearWorld);
    outDir = glm::normalize(glm::vec3(farWorld - nearWorld));
}

// Helper: intersect ray with plane y = planeY. returns true if hit and sets outPoint
static bool intersectRayPlane(const glm::vec3& origin, const glm::vec3& dir, float planeY, glm::vec3& outPoint) {
    if(fabs(dir.y) < 1e-6f) return false;
    float t = (planeY - origin.y) / dir.y;
    if(t < 0.0f) return false;
    outPoint = origin + dir * t;
    return true;
}

// Moller-Trumbore ray-triangle intersection
static bool rayIntersectsTriangle(const glm::vec3& orig, const glm::vec3& dir, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& outT, float& outU, float& outV) {
    const float EPSILON = 1e-8f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(dir, edge2);
    float a = glm::dot(edge1, h);
    if (a > -EPSILON && a < EPSILON) return false; // parallel
    float f = 1.0f / a;
    glm::vec3 s = orig - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(dir, q);
    if (v < 0.0f || u + v > 1.0f) return false;
    float t = f * glm::dot(edge2, q);
    if (t > EPSILON) {
        outT = t; outU = u; outV = v; return true;
    }
    return false;
}

// Find closest intersection of ray with all entities' meshes; returns true if hit and sets outPoint, outNormal and hitEntityId
static bool rayIntersectSceneMeshes(Scene& scene, const glm::vec3& origin, const glm::vec3& dir, glm::vec3& outPoint, glm::vec3& outNormal, int& hitEntityId) {
    float bestT = FLT_MAX;
    bool hitAny = false;
    for(const auto& ent : scene.entities()) {
        if(!ent.mesh) continue;
        if(ent.mesh->cpuPositions.empty() || ent.mesh->cpuIndices.empty()) continue;
        // transform mesh vertex positions to world space using entity transform
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, ent.position);
        glm::quat q = glm::quat(glm::radians(ent.rotation));
        model *= glm::toMat4(q);
        model = glm::scale(model, ent.scale);
        const auto& pos = ent.mesh->cpuPositions;
        const auto& idx = ent.mesh->cpuIndices;
        for(size_t i=0;i+2<idx.size(); i+=3) {
            glm::vec3 v0 = glm::vec3(model * glm::vec4(pos[idx[i+0]], 1.0f));
            glm::vec3 v1 = glm::vec3(model * glm::vec4(pos[idx[i+1]], 1.0f));
            glm::vec3 v2 = glm::vec3(model * glm::vec4(pos[idx[i+2]], 1.0f));
            float t,u,v;
            if(rayIntersectsTriangle(origin, dir, v0, v1, v2, t, u, v)) {
                if(t < bestT) {
                    bestT = t; outPoint = origin + dir * t; hitEntityId = ent.id; hitAny = true;
                    // compute triangle normal
                    glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                    outNormal = n;
                }
            }
        }
    }
    return hitAny;
}

void DrawViewportWindow(ViewportContext& ctx) {
    if(!ctx.scene || !ctx.camera) return;
    ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
    ImVec2 viewport_size = ImGui::GetContentRegionAvail();
    ImVec2 mpos = ImGui::GetIO().MousePos;
    bool mouseOnViewport = (mpos.x >= viewport_pos.x && mpos.x <= (viewport_pos.x + viewport_size.x) && mpos.y >= viewport_pos.y && mpos.y <= (viewport_pos.y + viewport_size.y));

    int fb_w = (int)viewport_size.x; int fb_h = (int)viewport_size.y;
    ensureFBO(fb_w, fb_h);

    // Delegate viewport input handling to Camera
    ctx.camera->handleViewportInput(glfwGetCurrentContext(), mouseOnViewport);

    GLuint fboToUse = s_fbo;
    GLuint fboColor = s_fboColor;

    // Static preview meshes for ghost placement
    static bool s_previewInit = false;
    static primitives::MeshGL s_cubePreview;
    static primitives::MeshGL s_spherePreview;
    static primitives::MeshGL s_cylinderPreview;
    static primitives::MeshGL s_planePreview;
    if(!s_previewInit) {
        s_cubePreview = primitives::createCubeMesh();
        s_spherePreview = primitives::createSphereMesh();
        s_cylinderPreview = primitives::createCylinderMesh();
        s_planePreview = primitives::createPlaneMesh();
        s_previewInit = true;
    }

    if(fboToUse) {
        glBindFramebuffer(GL_FRAMEBUFFER, fboToUse);
        glViewport(0,0,s_fbo_w,s_fbo_h);
        glClearColor(0.09f,0.09f,0.11f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = ctx.camera->getView();
        float aspect = (float)s_fbo_w / (s_fbo_h>0? (float)s_fbo_h : 1.0f);
        glm::mat4 proj = ctx.camera->getProjection(aspect);
        glm::mat4 vp = proj * view;
        *ctx.lastView = view; *ctx.lastProj = proj;

        glUseProgram(*ctx.prog);
        if(*ctx.showWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        Renderer::renderGrid(vp);
        ctx.scene->drawAll(*ctx.prog, vp);

        // compute live preview position if armed and in click modes
        bool havePreview = false;
        glm::vec3 previewPos(0.0f);
        glm::vec3 previewNormal(0.0f, 1.0f, 0.0f);
        if(*ctx.spawnPending && g_spawnPlacementMode != SpawnPlacementMode::Origin && mouseOnViewport) {
            ImVec2 m = ImGui::GetIO().MousePos;
            glm::vec2 sp = glm::vec2(m.x, m.y);
            glm::vec3 rayOrig, rayDir;
            screenPointToRay(sp, viewport_pos, viewport_size, view, proj, rayOrig, rayDir);
            if(g_spawnPlacementMode == SpawnPlacementMode::ClickPlane) {
                glm::vec3 hit;
                if(intersectRayPlane(rayOrig, rayDir, 0.0f, hit)) { previewPos = hit; havePreview = true; previewNormal = glm::vec3(0,1,0); }
            } else if(g_spawnPlacementMode == SpawnPlacementMode::ClickMesh) {
                glm::vec3 hit; glm::vec3 nrm; int entId = 0;
                if(rayIntersectSceneMeshes(*ctx.scene, rayOrig, rayDir, hit, nrm, entId)) { previewPos = hit; previewNormal = nrm; havePreview = true; }
            }
        }

        // Draw preview ghost if available
        if(havePreview) {
            // use program and set uniforms
            GLint loc = glGetUniformLocation(*ctx.prog, "uMVP");
            GLint col = glGetUniformLocation(*ctx.prog, "uColor");
            // choose mesh and scale
            primitives::MeshGL* pm = nullptr;
            switch(*ctx.spawnType) {
                case primitives::PrimitiveType::Cube: pm = &s_cubePreview; break;
                case primitives::PrimitiveType::Sphere: pm = &s_spherePreview; break;
                case primitives::PrimitiveType::Cylinder: pm = &s_cylinderPreview; break;
                case primitives::PrimitiveType::Plane: pm = &s_planePreview; break;
            }
            if(pm) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), previewPos);
                // orient to normal if requested
                if(g_spawnAlignToNormal) {
                    glm::vec3 up = glm::vec3(0,1,0);
                    glm::vec3 axis = glm::cross(up, previewNormal);
                    float dot = glm::dot(up, previewNormal);
                    float angle = acosf(glm::clamp(dot, -1.0f, 1.0f));
                    if(glm::length(axis) > 1e-6f) {
                        axis = glm::normalize(axis);
                        model *= glm::toMat4(glm::angleAxis(angle, axis));
                    }
                }
                // small uniform scale for preview
                model = glm::scale(model, glm::vec3(0.5f));
                glm::mat4 mvp = vp * model;
                glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);
                // draw wireframe with blending
                GLboolean prevBlend = glIsEnabled(GL_BLEND);
                GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                if(!prevDepth) glEnable(GL_DEPTH_TEST);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glUniform3f(col, 0.9f, 0.9f, 0.2f);
                pm->draw();
                // restore
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                if(!prevBlend) glDisable(GL_BLEND);
                if(!prevDepth) glDisable(GL_DEPTH_TEST);
            }
        }

        // Spawn when requested
        if(ctx.spawnPending && *ctx.spawnPending) {
            glm::vec3 spawnPos(0.0f);
            glm::vec3 spawnNormal(0.0f, 1.0f, 0.0f);
            if(g_spawnPlacementMode == SpawnPlacementMode::Origin) {
                spawnPos = glm::vec3(0.0f);
            } else if(g_spawnPlacementMode == SpawnPlacementMode::ClickPlane) {
                // Wait for user click inside viewport
                if(mouseOnViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 m = ImGui::GetIO().MousePos; // actual click position
                    glm::vec2 sp = glm::vec2(m.x, m.y);
                    glm::vec3 rayOrigin, rayDir;
                    screenPointToRay(sp, viewport_pos, viewport_size, view, proj, rayOrigin, rayDir);
                    glm::vec3 hit;
                    if(intersectRayPlane(rayOrigin, rayDir, 0.0f, hit)) { spawnPos = hit; spawnNormal = glm::vec3(0,1,0); } else spawnPos = glm::vec3(0.0f);
                    *ctx.spawnPending = false;
                } else {
                    // not clicked yet; wait
                }
            } else if(g_spawnPlacementMode == SpawnPlacementMode::ClickMesh) {
                if(mouseOnViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 m = ImGui::GetIO().MousePos;
                    glm::vec2 sp = glm::vec2(m.x, m.y);
                    glm::vec3 rayOrigin, rayDir;
                    screenPointToRay(sp, viewport_pos, viewport_size, view, proj, rayOrigin, rayDir);
                    glm::vec3 hit; glm::vec3 nrm; int hitEnt = 0;
                    if(rayIntersectSceneMeshes(*ctx.scene, rayOrigin, rayDir, hit, nrm, hitEnt)) { spawnPos = hit; spawnNormal = nrm; } else spawnPos = glm::vec3(0.0f);
                    *ctx.spawnPending = false;
                } else {
                    // wait for click
                }
            }
            // Only add if spawnPending was cleared (i.e., we had a click) or Origin mode
            if(g_spawnPlacementMode == SpawnPlacementMode::Origin) {
                int id = ctx.scene->addPrimitive(*ctx.spawnType, spawnPos);
                *ctx.spawnPending = false;
            } else if(!*ctx.spawnPending) {
                int id = ctx.scene->addPrimitive(*ctx.spawnType, spawnPos);
                // align to normal if requested
                if(g_spawnAlignToNormal && id != 0) {
                    glm::quat q = glm::rotation(glm::vec3(0,1,0), spawnNormal);
                    glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
                    Scene::Transform t = ctx.scene->getEntityTransform(id);
                    t.rotation = euler;
                    ctx.scene->setEntityTransform(id, t);
                }
            }
        }

        // ImGuizmo manipulation
        if(*ctx.useImGuizmo && ctx.scene->getSelectedId() != 0) {
            bool active = GizmoController::manipulate(*ctx.scene, view, proj, viewport_pos, viewport_size, *ctx.gizmoOperation, *ctx.gizmoMode, *ctx.useImGuizmo);
            if(active) {
                if(!*ctx.imguizmoActive) { *ctx.imguizmoActive = true; *ctx.imguizmoEntity = ctx.scene->getSelectedId(); *ctx.imguizmoBefore = ctx.scene->getEntityTransform(*ctx.imguizmoEntity); }
            } else {
                if(*ctx.imguizmoActive) {
                    Scene::Transform after = ctx.scene->getEntityTransform(*ctx.imguizmoEntity);
                    const Scene::Transform& before = *ctx.imguizmoBefore;
                    if(after.position != before.position || after.rotation != before.rotation || after.scale != before.scale) {
                        ctx.scene->pushCommand(std::unique_ptr<Scene::Command>(new Scene::TransformCommand(*ctx.imguizmoEntity, *ctx.imguizmoBefore, after)));
                    }
                    *ctx.imguizmoActive = false;
                }
            }
        } else {
            ctx.gizmo->drawGizmo(vp, glm::vec2(viewport_pos.x, viewport_pos.y), glm::vec2(viewport_size.x, viewport_size.y), *ctx.scene);
        }

        // Selection visuals
        SceneEntity* sel = ctx.scene->findById(ctx.scene->getSelectedId());
        if(sel && (*ctx.gizmoOperation == ImGuizmo::ROTATE || *ctx.gizmoOperation == ImGuizmo::SCALE)) {
            Renderer::drawSelectionBox(vp, sel);
            GizmoLib::DrawAxisOverlay(sel, view, proj, viewport_pos, viewport_size);
            if(*ctx.gizmoOperation == ImGuizmo::ROTATE) GizmoLib::DrawRotationArcs(*ctx.scene, sel->id, view, proj, viewport_pos, viewport_size, *ctx.gizmoMode);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Show rendered texture
    ImVec2 show_size = viewport_size;
    if(fboColor && show_size.x > 0 && show_size.y > 0) ImGui::Image((ImTextureID)(intptr_t)fboColor, show_size, ImVec2(0,1), ImVec2(1,0));
    else ImGui::Dummy(show_size);
}
