#include "primitive_factory.h"
#include <glad/glad.h>
#include <vector>
#include <cmath>

namespace primitives {

MeshGL::~MeshGL() {
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
}

MeshGL::MeshGL(MeshGL&& other) noexcept {
    vao = other.vao; vbo = other.vbo; ebo = other.ebo; indexCount = other.indexCount;
    other.vao = other.vbo = other.ebo = 0; other.indexCount = 0;
}

MeshGL& MeshGL::operator=(MeshGL&& other) noexcept {
    if(this != &other){
        if (ebo) { glDeleteBuffers(1, &ebo); }
        if (vbo) { glDeleteBuffers(1, &vbo); }
        if (vao) { glDeleteVertexArrays(1, &vao); }
        vao = other.vao; vbo = other.vbo; ebo = other.ebo; indexCount = other.indexCount;
        other.vao = other.vbo = other.ebo = 0; other.indexCount = 0;
    }
    return *this;
}

void MeshGL::upload(const std::vector<float>& verts, const std::vector<unsigned int>& idx) {
    if (vao == 0) glGenVertexArrays(1, &vao);
    if (vbo == 0) glGenBuffers(1, &vbo);
    if (ebo == 0) glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    indexCount = (int)idx.size();
}

void MeshGL::draw() const {
    if (vao == 0 || indexCount == 0) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void makeCubeData(std::vector<float>& verts, std::vector<unsigned int>& idx) {
    verts = {
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f,1.0f,-1.0f,  -1.0f,1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f,-1.0f, 1.0f,  1.0f,1.0f, 1.0f,  -1.0f,1.0f, 1.0f
    };
    idx = {
        0,1,2, 2,3,0,
        4,6,5, 6,4,7,
        0,4,5, 5,1,0,
        3,2,6, 6,7,3,
        1,5,6, 6,2,1,
        0,3,7, 7,4,0
    };
}

// UV sphere generator (positions only)
void makeSphereData(std::vector<float>& verts, std::vector<unsigned int>& idx, int segments, int rings) {
    verts.clear(); idx.clear();
    if(segments < 3) segments = 3;
    if(rings < 2) rings = 2;
    for(int r=0;r<=rings;++r){
        float v = (float)r / (float)rings;
        float phi = v * 3.14159265359f;
        for(int s=0;s<=segments;++s){
            float u = (float)s / (float)segments;
            float theta = u * 2.0f * 3.14159265359f;
            float x = sinf(phi) * cosf(theta);
            float y = cosf(phi);
            float z = sinf(phi) * sinf(theta);
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
        }
    }
    for(int r=0;r<rings;++r){
        for(int s=0;s<segments;++s){
            int a = r * (segments + 1) + s;
            int b = a + segments + 1;
            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(a+1);
            idx.push_back(a+1);
            idx.push_back(b);
            idx.push_back(b+1);
        }
    }
}

void makeCylinderData(std::vector<float>& verts, std::vector<unsigned int>& idx, int segments, float height) {
    verts.clear(); idx.clear();
    if(segments < 3) segments = 3;
    float half = height * 0.5f;
    // side vertices
    for(int s=0;s<=segments;++s){
        float u = (float)s / (float)segments;
        float theta = u * 2.0f * 3.14159265359f;
        float x = cosf(theta);
        float z = sinf(theta);
        // bottom
        verts.push_back(x); verts.push_back(-half); verts.push_back(z);
        // top
        verts.push_back(x); verts.push_back(half); verts.push_back(z);
    }
    // indices for sides
    for(int s=0;s<segments;++s){
        int i0 = s*2;
        int i1 = i0+1;
        int i2 = ((s+1)%segments)*2;
        int i3 = i2+1;
        idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
        idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
    }
    // center points for caps
    int baseCenter = (int)verts.size()/3;
    verts.push_back(0.0f); verts.push_back(-half); verts.push_back(0.0f);
    int topCenter = (int)verts.size()/3;
    verts.push_back(0.0f); verts.push_back(half); verts.push_back(0.0f);
    // cap indices
    for(int s=0;s<segments;++s){
        int i0 = s*2; // bottom vertex
        int i2 = ((s+1)%segments)*2;
        idx.push_back(baseCenter); idx.push_back(i2); idx.push_back(i0);
        int i1 = s*2+1; int i3 = ((s+1)%segments)*2+1;
        idx.push_back(topCenter); idx.push_back(i1); idx.push_back(i3);
    }
}

void makePlaneData(std::vector<float>& verts, std::vector<unsigned int>& idx, float size) {
    float h = size * 0.5f;
    verts = { -h,0.0f,-h,  h,0.0f,-h,  h,0.0f,h,  -h,0.0f,h };
    idx = { 0,1,2, 2,3,0 };
}

MeshGL createCubeMesh() {
    MeshGL m;
    std::vector<float> v;
    std::vector<unsigned int> i;
    makeCubeData(v, i);
    m.upload(v, i);
    return m;
}

MeshGL createSphereMesh(int segments, int rings) {
    MeshGL m;
    std::vector<float> v; std::vector<unsigned int> i;
    makeSphereData(v, i, segments, rings);
    m.upload(v, i);
    return m;
}

MeshGL createCylinderMesh(int segments, float height) {
    MeshGL m;
    std::vector<float> v; std::vector<unsigned int> i;
    makeCylinderData(v, i, segments, height);
    m.upload(v, i);
    return m;
}

MeshGL createPlaneMesh(float size) {
    MeshGL m;
    std::vector<float> v; std::vector<unsigned int> i;
    makePlaneData(v, i, size);
    m.upload(v, i);
    return m;
}

} // namespace primitives
