#pragma once

#include <glm/glm.hpp>
#include "scene.h"
#include <glad/glad.h>
#include "camera.h"
#include "imgui.h"

namespace Renderer {
    // Initialize renderer (compile shaders, setup resources)
    void init();
    void destroy();

    // Program handle
    GLuint getProgram();

    // Draw helpers used by main
    void renderGrid(const glm::mat4& vp);
    void drawOriginMarker(const glm::mat4& vp);
    void drawAxisLines(const glm::mat4& vp);
    void drawSelectionBox(const glm::mat4& vp, const SceneEntity* ent);

    // Offscreen FBO management and scene rendering
    // Renders the given scene into an offscreen texture sized to the provided viewport (logical pixels)
    // Outputs view and projection matrices used for the render into out_view/out_proj.
    void renderScene(Scene& scene, const Camera& camera, const ImVec2& viewport_pos, const ImVec2& viewport_size, bool wireframe, glm::mat4& out_view, glm::mat4& out_proj);

    // Get the color texture produced by the last render (suitable for ImGui::Image)
    GLuint getColorTexture();
}
