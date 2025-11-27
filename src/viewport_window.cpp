#include "viewport_window.h"
#include "renderer.h"
#include "gizmo_controller.h"
#include "gizmo_lib.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

// Local FBO state (kept internal to this module)
static GLuint s_fbo = 0;
static GLuint s_fboColor = 0;
static GLuint s_fboDepth = 0;
static int s_fbo_w = 0;
static int s_fbo_h = 0;

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

        // Spawn at origin when requested
        if(ctx.spawnPending && *ctx.spawnPending) {
            glm::vec3 pos(0.0f, 0.5f, 0.0f);
            ctx.scene->addPrimitive(*ctx.spawnType, pos);
            *ctx.spawnPending = false;
        }

        // ImGuizmo manipulation
        if(*ctx.useImGuizmo && ctx.scene->getSelectedId() != 0) {
            // GizmoController::manipulate signature: (Scene&, const glm::mat4&, const glm::mat4&, const ImVec2&, const ImVec2&, ImGuizmo::OPERATION, ImGuizmo::MODE, bool)
            bool active = GizmoController::manipulate(*ctx.scene, view, proj, viewport_pos, viewport_size, *ctx.gizmoOperation, *ctx.gizmoMode, *ctx.useImGuizmo);
            if(active) {
                if(!*ctx.imguizmoActive) { *ctx.imguizmoActive = true; *ctx.imguizmoEntity = ctx.scene->getSelectedId(); *ctx.imguizmoBefore = ctx.scene->getEntityTransform(*ctx.imguizmoEntity); }
            } else {
                if(*ctx.imguizmoActive) { Scene::Transform after = ctx.scene->getEntityTransform(*ctx.imguizmoEntity); ctx.scene->pushCommand(std::unique_ptr<Scene::Command>(new Scene::TransformCommand(*ctx.imguizmoEntity, *ctx.imguizmoBefore, after))); *ctx.imguizmoActive = false; }
            }
        } else {
            ctx.gizmo->drawGizmo(vp, glm::vec2(viewport_pos.x, viewport_pos.y), glm::vec2(viewport_size.x, viewport_size.y), *ctx.scene);
        }

        // Selection visuals
        SceneEntity* sel = ctx.scene->findById(ctx.scene->getSelectedId());
        if(sel && (*ctx.gizmoOperation == ImGuizmo::ROTATE || *ctx.gizmoOperation == ImGuizmo::SCALE)) {
            Renderer::drawSelectionBox(vp, sel);
            // overlays -> use GizmoLib
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
