#include "renderer.h"
#include <vector>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
static GLuint g_prog = 0;
// FBO resources
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

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){ char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); std::cerr << "Shader compile error: " << log << '\n'; }
    return s;
}
static GLuint createProgram(const char* vs, const char* fs) {
    GLuint vsid = compileShader(GL_VERTEX_SHADER, vs);
    GLuint fsid = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vsid);
    glAttachShader(prog, fsid);
    glLinkProgram(prog);
    GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if(!ok){ char log[1024]; glGetProgramInfoLog(prog, sizeof(log), nullptr, log); std::cerr << "Program link error: " << log << '\n'; }
    glDeleteShader(vsid); glDeleteShader(fsid);
    return prog;
}

const char* VS_SIMPLE = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main(){ gl_Position = uMVP * vec4(aPos,1.0); }
)glsl";
const char* FS_SIMPLE = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main(){ FragColor = vec4(uColor,1.0); }
)glsl";

} // anonymous

namespace Renderer {

void init() {
    if(g_prog) return;
    g_prog = createProgram(VS_SIMPLE, FS_SIMPLE);
}

void destroy() {
    if(g_prog) { glDeleteProgram(g_prog); g_prog = 0; }
    if(s_fboDepth) { glDeleteRenderbuffers(1, &s_fboDepth); s_fboDepth = 0; }
    if(s_fboColor) { glDeleteTextures(1, &s_fboColor); s_fboColor = 0; }
    if(s_fbo) { glDeleteFramebuffers(1, &s_fbo); s_fbo = 0; }
}

GLuint getProgram() { return g_prog; }

void renderGrid(const glm::mat4& vp) {
    std::vector<float> lines;
    const int half = 20; const float step = 0.5f;
    for(int i=-half;i<=half;i++){
        float x = i*step;
        lines.push_back(x); lines.push_back(0.0f); lines.push_back(-half*step);
        lines.push_back(x); lines.push_back(0.0f); lines.push_back(half*step);
        float z = i*step;
        lines.push_back(-half*step); lines.push_back(0.0f); lines.push_back(z);
        lines.push_back(half*step);  lines.push_back(0.0f); lines.push_back(z);
    }
    GLuint tmpVBO=0, tmpVAO=0; glGenBuffers(1,&tmpVBO); glGenVertexArrays(1,&tmpVAO);
    glBindVertexArray(tmpVAO); glBindBuffer(GL_ARRAY_BUFFER, tmpVBO);
    glBufferData(GL_ARRAY_BUFFER, lines.size()*sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glUseProgram(g_prog);
    GLint loc = glGetUniformLocation(g_prog, "uMVP"); glm::mat4 model = glm::mat4(1.0f); glm::mat4 mvp = vp * model;
    glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]); GLint col = glGetUniformLocation(g_prog, "uColor"); glUniform3f(col, 0.6f, 0.6f, 0.6f);
    glDrawArrays(GL_LINES, 0, (GLsizei)(lines.size()/3));
    glBindVertexArray(0); glDeleteBuffers(1,&tmpVBO); glDeleteVertexArrays(1,&tmpVAO);
}

void drawOriginMarker(const glm::mat4& vp) {
    std::vector<float> verts = {0.0f,0.0f,0.0f, 0.6f,0.0f,0.0f, 0.0f,0.0f,0.0f, 0.0f,0.6f,0.0f, 0.0f,0.0f,0.0f, 0.0f,0.0f,0.6f};
    GLuint tmpVBO=0, tmpVAO=0; glGenBuffers(1,&tmpVBO); glGenVertexArrays(1,&tmpVAO);
    glBindVertexArray(tmpVAO); glBindBuffer(GL_ARRAY_BUFFER, tmpVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glUseProgram(g_prog); GLint loc = glGetUniformLocation(g_prog, "uMVP"); glm::mat4 model = glm::mat4(1.0f); glm::mat4 mvp = vp * model; glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]); GLint col = glGetUniformLocation(g_prog, "uColor");
    glUniform3f(col, 1.0f, 0.0f, 0.0f); glDrawArrays(GL_LINES, 0, 2);
    glUniform3f(col, 1.0f, 0.9f, 0.2f); glDrawArrays(GL_LINES, 2, 2);
    glUniform3f(col, 0.0f, 0.0f, 1.0f); glDrawArrays(GL_LINES, 4, 2);
    glBindVertexArray(0); glDeleteBuffers(1,&tmpVBO); glDeleteVertexArrays(1,&tmpVAO);
}

void drawAxisLines(const glm::mat4& vp) {
    std::vector<float> verts; verts.push_back(-100.0f); verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(100.0f); verts.push_back(0.0f); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back(-100.0f); verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(100.0f); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(-100.0f); verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(100.0f);
    GLuint tmpVBO=0, tmpVAO=0; glGenBuffers(1,&tmpVBO); glGenVertexArrays(1,&tmpVAO);
    glBindVertexArray(tmpVAO); glBindBuffer(GL_ARRAY_BUFFER, tmpVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glUseProgram(g_prog); GLint loc = glGetUniformLocation(g_prog, "uMVP"); glm::mat4 model = glm::mat4(1.0f); glm::mat4 mvp = vp * model; glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]); GLint col = glGetUniformLocation(g_prog, "uColor");
    glUniform3f(col, 1.0f, 0.2f, 0.2f); glDrawArrays(GL_LINES, 0, 2);
    glUniform3f(col, 1.0f, 0.9f, 0.2f); glDrawArrays(GL_LINES, 2, 2);
    glUniform3f(col, 0.2f, 0.4f, 1.0f); glDrawArrays(GL_LINES, 4, 2);
    glBindVertexArray(0); glDeleteBuffers(1,&tmpVBO); glDeleteVertexArrays(1,&tmpVAO);
}

void drawSelectionBox(const glm::mat4& vp, const SceneEntity* ent) {
    if(!ent || !ent->mesh) return;
    glm::mat4 model = glm::mat4(1.0f); model = glm::translate(model, ent->position); model *= glm::toMat4(glm::quat(glm::radians(ent->rotation))); model = glm::scale(model, ent->scale); glm::mat4 mvp = vp * model;
    std::vector<float> lines = {
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,
        1.0f,-1.0f,-1.0f,   1.0f, 1.0f,-1.0f,
        1.0f, 1.0f,-1.0f,  -1.0f, 1.0f,-1.0f,
        -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f,-1.0f, 1.0f,
        1.0f,-1.0f, 1.0f,   1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f, -1.0f,-1.0f, 1.0f,
        1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,
        1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f, -1.0f, 1.0f, 1.0f
    };
    GLuint tmpVBO=0, tmpVAO=0; glGenBuffers(1,&tmpVBO); glGenVertexArrays(1,&tmpVAO); glBindVertexArray(tmpVAO); glBindBuffer(GL_ARRAY_BUFFER, tmpVBO);
    glBufferData(GL_ARRAY_BUFFER, lines.size()*sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glUseProgram(g_prog); GLint loc = glGetUniformLocation(g_prog, "uMVP"); glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]); GLint col = glGetUniformLocation(g_prog, "uColor"); glLineWidth(3.0f); glUniform3f(col, 1.0f, 0.2f, 1.0f); glDrawArrays(GL_LINES, 0, (GLsizei)(lines.size()/3)); glLineWidth(1.0f);
    glBindVertexArray(0); glDeleteBuffers(1,&tmpVBO); glDeleteVertexArrays(1,&tmpVAO);
}

// Render scene into offscreen texture sized to viewport (ImGui logical pixels). Returns view/proj & color texture
void renderScene(Scene& scene, const Camera& camera, const ImVec2& viewport_pos, const ImVec2& viewport_size, bool wireframe, glm::mat4& out_view, glm::mat4& out_proj) {
    int w = (int)viewport_size.x;
    int h = (int)viewport_size.y;
    ensureFBO(w, h);
    if(!s_fbo) return; // failed to create

    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    glViewport(0, 0, s_fbo_w, s_fbo_h);
    glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera.getView();
    float aspect = (float)s_fbo_w / (s_fbo_h > 0 ? (float)s_fbo_h : 1.0f);
    glm::mat4 proj = camera.getProjection(aspect);
    glm::mat4 vp = proj * view;

    out_view = view; out_proj = proj;

    glUseProgram(g_prog);
    if(wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderGrid(vp);
    scene.drawAll(g_prog, vp);
    drawAxisLines(vp);
    drawOriginMarker(vp);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint getColorTexture() { return s_fboColor; }

} // namespace Renderer
