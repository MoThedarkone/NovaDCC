#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace primitives {

enum class PrimitiveType { Cube, Sphere, Cylinder, Plane };

// Small GL mesh helper (owns VAO/VBO/EBO)
struct MeshGL {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    int indexCount = 0;

    // Axis-aligned bounding box in mesh/model local space
    glm::vec3 aabbMin = glm::vec3(-1.0f);
    glm::vec3 aabbMax = glm::vec3(1.0f);

    // Keep CPU-side copies for operations like ray-mesh intersection
    std::vector<glm::vec3> cpuPositions;
    std::vector<unsigned int> cpuIndices;

    MeshGL() = default;
    ~MeshGL();

    // non-copyable
    MeshGL(const MeshGL&) = delete;
    MeshGL& operator=(const MeshGL&) = delete;

    // movable
    MeshGL(MeshGL&& other) noexcept;
    MeshGL& operator=(MeshGL&& other) noexcept;

    // upload vertex (vec3) and index buffers
    void upload(const std::vector<float>& verts, const std::vector<unsigned int>& idx);
    void draw() const;
};

// Return CPU-side data for primitives (positions only, 3 floats per vertex)
void makeCubeData(std::vector<float>& verts, std::vector<unsigned int>& idx);
void makeSphereData(std::vector<float>& verts, std::vector<unsigned int>& idx, int segments = 24, int rings = 16);
void makeCylinderData(std::vector<float>& verts, std::vector<unsigned int>& idx, int segments = 24, float height = 2.0f);
void makePlaneData(std::vector<float>& verts, std::vector<unsigned int>& idx, float size = 2.0f);

// Helpers that create initialized MeshGL for primitives
MeshGL createCubeMesh();
MeshGL createSphereMesh(int segments = 24, int rings = 16);
MeshGL createCylinderMesh(int segments = 24, float height = 2.0f);
MeshGL createPlaneMesh(float size = 2.0f);

} // namespace primitives
