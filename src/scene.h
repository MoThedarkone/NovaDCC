#pragma once

#include "primitive_factory.h"
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

struct SceneEntity {
    int id = 0;
    primitives::PrimitiveType type = primitives::PrimitiveType::Cube;
    std::unique_ptr<primitives::MeshGL> mesh;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // Euler angles in degrees (x=pitch,y=yaw,z=roll)
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 color = glm::vec3(0.8f, 0.2f, 0.2f);
};

class Scene {
public:
    Scene();
    ~Scene();

    int addCube(const glm::vec3& pos = glm::vec3(0.0f));
    int addPrimitive(primitives::PrimitiveType type, const glm::vec3& pos = glm::vec3(0.0f));
    // Record a spawn without allocating meshes (useful for testing/counting)
    void recordSpawnOnly();

    void drawAll(unsigned int prog, const glm::mat4& vp) const;

    // Selection / editing
    int getSelectedId() const;
    void selectEntity(int id);
    void deleteSelected();
    void translateSelected(const glm::vec3& delta);
    void setSelectedPosition(const glm::vec3& pos);

    // rotation/scale
    void rotateSelected(const glm::vec3& deltaDegrees);
    void setSelectedRotation(const glm::vec3& eulerDeg);
    void scaleSelected(const glm::vec3& scaleFactor);
    void setSelectedScale(const glm::vec3& scale);

    SceneEntity* findById(int id);

    // Expose entities for UI
    const std::vector<SceneEntity>& entities() const { return m_entities; }
    int getEntityCount() const { return (int)m_entities.size(); }
    int getSpawnCount() const { return m_spawnCount; }

    // Allow external code to add a fully formed entity
    int addEntity(SceneEntity&& ent);

    // Undo/Redo (simple command stack)
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Serialization (simple text-based skeleton)
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

    // Simple command base exposed so implementations can derive
    struct Command {
        virtual ~Command() = default;
        virtual void undo(Scene& s) = 0;
        virtual void redo(Scene& s) = 0;
    };

    // Transform snapshot used by commands
    struct Transform {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
    };

    // Command to set an entity transform (undo/redo)
    struct TransformCommand : Command {
        int id;
        Transform before;
        Transform after;
        TransformCommand(int i, const Transform& b, const Transform& a);
        void undo(Scene& s) override;
        void redo(Scene& s) override;
    };

    // Allow external code to push commands onto the stack
    void pushCommand(std::unique_ptr<Command> cmd);

    // Helpers to get/set entity transform by id
    Transform getEntityTransform(int id) const;
    void setEntityTransform(int id, const Transform& t);

private:
    std::vector<SceneEntity> m_entities;
    int m_nextId = 1;

    // selection
    int m_selectedId = 0;

    // spawn counter (total primitives created or recorded)
    int m_spawnCount = 0;

    // command stack
    std::vector<std::unique_ptr<Command>> m_undoStack;
    std::vector<std::unique_ptr<Command>> m_redoStack;

    // internal helpers
    void applyAdd(int id);
    void applyRemove(int id, SceneEntity&& ent);
};
