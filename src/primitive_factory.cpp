#include "primitive_factory.h"
#include <glad/glad.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace primitives {

MeshGL::~MeshGL() {
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
}

MeshGL::MeshGL(MeshGL&& other) noexcept {
    vao = other.vao; vbo = other.vbo; ebo = other.ebo; indexCount = other.indexCount;
    aabbMin = other.aabbMin; aabbMax = other.aabbMax;
    cpuPositions = std::move(other.cpuPositions);
    cpuIndices = std::move(other.cpuIndices);
    bvhNodes = std::move(other.bvhNodes);
    other.vao = other.vbo = other.ebo = 0; other.indexCount = 0;
}

MeshGL& MeshGL::operator=(MeshGL&& other) noexcept {
    if(this != &other){
        if (ebo) { glDeleteBuffers(1, &ebo); }
        if (vbo) { glDeleteBuffers(1, &vbo); }
        if (vao) { glDeleteVertexArrays(1, &vao); }
        vao = other.vao; vbo = other.vbo; ebo = other.ebo; indexCount = other.indexCount;
        aabbMin = other.aabbMin; aabbMax = other.aabbMax;
        cpuPositions = std::move(other.cpuPositions);
        cpuIndices = std::move(other.cpuIndices);
        bvhNodes = std::move(other.bvhNodes);
        other.vao = other.vbo = other.ebo = 0; other.indexCount = 0;
    }
    return *this;
}

// Simple helper to compute triangle centroid bounding box split
static void computeTriangleAABB(const std::vector<glm::vec3>& pos, const std::vector<unsigned int>& idx, int triStart, int triCount, glm::vec3& outMin, glm::vec3& outMax, glm::vec3& outCentroidMin, glm::vec3& outCentroidMax) {
    outMin = glm::vec3(FLT_MAX); outMax = glm::vec3(-FLT_MAX);
    outCentroidMin = glm::vec3(FLT_MAX); outCentroidMax = glm::vec3(-FLT_MAX);
    for(int t=0;t<triCount;++t) {
        int i0 = idx[(triStart+t)*3+0];
        int i1 = idx[(triStart+t)*3+1];
        int i2 = idx[(triStart+t)*3+2];
        glm::vec3 v0 = pos[i0]; glm::vec3 v1 = pos[i1]; glm::vec3 v2 = pos[i2];
        glm::vec3 triMin = glm::min(v0, glm::min(v1,v2));
        glm::vec3 triMax = glm::max(v0, glm::max(v1,v2));
        outMin = glm::min(outMin, triMin);
        outMax = glm::max(outMax, triMax);
        glm::vec3 centroid = (v0 + v1 + v2) / 3.0f;
        outCentroidMin = glm::min(outCentroidMin, centroid);
        outCentroidMax = glm::max(outCentroidMax, centroid);
    }
}

// Build BVH recursively into mesh.bvhNodes. Node stores [start,count] in primitive (triangle) list stored as contiguous idx triples.
static int buildBVHRecursive(MeshGL& mesh, int triStart, int triCount) {
    MeshGL::BVHNode node;
    glm::vec3 triMin, triMax, centMin, centMax;
    computeTriangleAABB(mesh.cpuPositions, mesh.cpuIndices, triStart, triCount, triMin, triMax, centMin, centMax);
    node.min = triMin; node.max = triMax; node.start = triStart; node.count = triCount; node.left = -1; node.right = -1;
    int myIndex = (int)mesh.bvhNodes.size();
    mesh.bvhNodes.push_back(node);
    if(triCount <= 8) {
        // leaf
        return myIndex;
    }
    // choose split axis by centroid extent
    glm::vec3 ext = centMax - centMin;
    int axis = 0; if(ext.y > ext.x) axis = 1; if(ext.z > ext[axis]) axis = 2;
    // partition triangles by centroid median
    int mid = triStart + triCount/2;
    // simple nth_element on indices of triangles by centroid
    std::vector<int> triIndices(triCount);
    for(int i=0;i<triCount;++i) triIndices[i] = triStart + i;
    std::nth_element(triIndices.begin(), triIndices.begin() + triCount/2, triIndices.end(), [&](int a, int b){
        glm::vec3 ca = (mesh.cpuPositions[mesh.cpuIndices[a*3+0]] + mesh.cpuPositions[mesh.cpuIndices[a*3+1]] + mesh.cpuPositions[mesh.cpuIndices[a*3+2]]) / 3.0f;
        glm::vec3 cb = (mesh.cpuPositions[mesh.cpuIndices[b*3+0]] + mesh.cpuPositions[mesh.cpuIndices[b*3+1]] + mesh.cpuPositions[mesh.cpuIndices[b*3+2]]) / 3.0f;
        return ca[axis] < cb[axis];
    });
    // create a temporary copy of triangle indices order
    std::vector<unsigned int> newIdx;
    newIdx.reserve(mesh.cpuIndices.size());
    for(int ti : triIndices) {
        newIdx.push_back(mesh.cpuIndices[ti*3+0]);
        newIdx.push_back(mesh.cpuIndices[ti*3+1]);
        newIdx.push_back(mesh.cpuIndices[ti*3+2]);
    }
    // replace the relevant range in cpuIndices
    for(int i=0;i<triCount*3;++i) mesh.cpuIndices[triStart*3 + i] = newIdx[i];
    int leftStart = triStart;
    int leftCount = triCount/2;
    int rightStart = triStart + leftCount;
    int rightCount = triCount - leftCount;
    int leftNode = buildBVHRecursive(mesh, leftStart, leftCount);
    int rightNode = buildBVHRecursive(mesh, rightStart, rightCount);
    // update this node's left/right children
    mesh.bvhNodes[myIndex].left = leftNode;
    mesh.bvhNodes[myIndex].right = rightNode;
    mesh.bvhNodes[myIndex].start = triStart; mesh.bvhNodes[myIndex].count = triCount;
    return myIndex;
}

void MeshGL::upload(const std::vector<float>& verts, const std::vector<unsigned int>& idx) {
    // compute AABB from vertex positions (assume verts.size() % 3 == 0)
    cpuPositions.clear(); cpuIndices.clear(); bvhNodes.clear();
    if(!verts.empty()){
        glm::vec3 mn(verts[0], verts[1], verts[2]);
        glm::vec3 mx = mn;
        size_t vcount = verts.size()/3;
        cpuPositions.reserve(vcount);
        for(size_t i=0;i<vcount;++i){
            glm::vec3 p(verts[i*3+0], verts[i*3+1], verts[i*3+2]);
            cpuPositions.push_back(p);
            mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
            mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
        }
        aabbMin = mn; aabbMax = mx;
    } else {
        aabbMin = glm::vec3(-1.0f); aabbMax = glm::vec3(1.0f);
    }

    cpuIndices = idx; // copy indices

    // Build BVH over triangles (triCount)
    int triCount = (int)cpuIndices.size() / 3;
    if(triCount > 0) {
        // ensure indices are grouped as triples starting at 0..triCount-1
        // build simple recursive BVH
        buildBVHRecursive(*this, 0, triCount);
    }

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

// Moller-Trumbore helper
static bool rayTriangleIntersect(const glm::vec3& orig, const glm::vec3& dir, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t) {
    const float EPSILON = 1e-8f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(dir, edge2);
    float a = glm::dot(edge1, h);
    if(fabs(a) < EPSILON) return false;
    float f = 1.0f / a;
    glm::vec3 s = orig - v0;
    float u = f * glm::dot(s, h);
    if(u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(dir, q);
    if(v < 0.0f || u + v > 1.0f) return false;
    float tt = f * glm::dot(edge2, q);
    if(tt > EPSILON) { t = tt; return true; }
    return false;
}

// AABB intersection test
static bool rayIntersectsAABB(const glm::vec3& orig, const glm::vec3& dir, const glm::vec3& minB, const glm::vec3& maxB, float& tmin_out, float& tmax_out) {
    float tmin = (minB.x - orig.x) / dir.x; float tmax = (maxB.x - orig.x) / dir.x;
    if(tmin > tmax) std::swap(tmin,tmax);
    float tymin = (minB.y - orig.y) / dir.y; float tymax = (maxB.y - orig.y) / dir.y;
    if(tymin > tymax) std::swap(tymin, tymax);
    if((tmin > tymax) || (tymin > tmax)) return false;
    if(tymin > tmin) tmin = tymin;
    if(tymax < tmax) tmax = tymax;
    float tzmin = (minB.z - orig.z) / dir.z; float tzmax = (maxB.z - orig.z) / dir.z;
    if(tzmin > tzmax) std::swap(tzmin, tzmax);
    if((tmin > tzmax) || (tzmin > tmax)) return false;
    if(tzmin > tmin) tmin = tzmin;
    if(tzmax < tmax) tmax = tzmax;
    tmin_out = tmin; tmax_out = tmax;
    return true;
}

bool meshRayIntersect(const MeshGL& mesh, const glm::mat4& model, const glm::vec3& orig, const glm::vec3& dir, float& outT, glm::vec3& outPoint) {
    if(mesh.bvhNodes.empty()) return false;
    // traverse BVH stack
    float bestT = FLT_MAX; bool hit = false;
    struct StackItem { int node; };
    std::vector<StackItem> stack; stack.reserve(64);
    stack.push_back({0});
    while(!stack.empty()) {
        int nodeIdx = stack.back().node; stack.pop_back();
        const auto& node = mesh.bvhNodes[nodeIdx];
        // Transform node AABB by model matrix (approx): transform corners
        glm::vec3 nmin = glm::vec3(model * glm::vec4(node.min, 1.0f));
        glm::vec3 nmax = glm::vec3(model * glm::vec4(node.max, 1.0f));
        float tmin, tmax;
        if(!rayIntersectsAABB(orig, dir, nmin, nmax, tmin, tmax)) continue;
        if(tmin > bestT) continue;
        if(node.left == -1 && node.right == -1) {
            // leaf: test triangles in range
            int triStart = node.start; int triCount = node.count;
            for(int ti=0; ti<triCount; ++ti) {
                int idx0 = mesh.cpuIndices[(triStart+ti)*3+0];
                int idx1 = mesh.cpuIndices[(triStart+ti)*3+1];
                int idx2 = mesh.cpuIndices[(triStart+ti)*3+2];
                glm::vec3 v0 = glm::vec3(model * glm::vec4(mesh.cpuPositions[idx0], 1.0f));
                glm::vec3 v1 = glm::vec3(model * glm::vec4(mesh.cpuPositions[idx1], 1.0f));
                glm::vec3 v2 = glm::vec3(model * glm::vec4(mesh.cpuPositions[idx2], 1.0f));
                float t;
                if(rayTriangleIntersect(orig, dir, v0, v1, v2, t)) {
                    if(t < bestT) { bestT = t; outPoint = orig + dir * t; hit = true; }
                }
            }
        } else {
            // push children
            if(node.left != -1) stack.push_back({node.left});
            if(node.right != -1) stack.push_back({node.right});
        }
    }
    if(hit) { outT = bestT; return true; }
    return false;
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
