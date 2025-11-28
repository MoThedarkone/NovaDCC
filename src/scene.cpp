#include <glad/glad.h>
#include "scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

struct AddCommand : Scene::Command {
    int id;
    primitives::PrimitiveType type;
    glm::vec3 pos;
    AddCommand(int i, primitives::PrimitiveType t, const glm::vec3& p) : id(i), type(t), pos(p) {}
    void undo(Scene& s) override { s.deleteSelected(); }
    void redo(Scene& s) override { s.addPrimitive(type, pos); }
};

Scene::Scene() {}
Scene::~Scene() {}

int Scene::addEntity(SceneEntity&& ent) {
    ent.id = m_nextId++;
    m_selectedId = ent.id;
    m_entities.push_back(std::move(ent));
    m_spawnCount++;
    return m_selectedId;
}

// Ensure addPrimitive delegates to addEntity to centralize logic
int Scene::addPrimitive(primitives::PrimitiveType type, const glm::vec3& pos) {
    SceneEntity e;
    e.type = type;
    if(type == primitives::PrimitiveType::Cube) e.mesh = std::make_unique<primitives::MeshGL>(primitives::createCubeMesh());
    else if(type == primitives::PrimitiveType::Sphere) e.mesh = std::make_unique<primitives::MeshGL>(primitives::createSphereMesh());
    else if(type == primitives::PrimitiveType::Cylinder) e.mesh = std::make_unique<primitives::MeshGL>(primitives::createCylinderMesh());
    else if(type == primitives::PrimitiveType::Plane) e.mesh = std::make_unique<primitives::MeshGL>(primitives::createPlaneMesh());
    e.position = pos;
    return addEntity(std::move(e));
}

int Scene::addCube(const glm::vec3& pos) { return addPrimitive(primitives::PrimitiveType::Cube, pos); }

void Scene::recordSpawnOnly() {
    // increment spawn counter but do not allocate meshes/entities
    m_spawnCount++;
}

void Scene::drawAll(unsigned int prog, const glm::mat4& vp) const {
    for (const auto& ent : m_entities) {
        if (!ent.mesh) continue;
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, ent.position);
        // apply rotation (degrees -> radians)
        glm::quat q = glm::quat(glm::radians(ent.rotation));
        model *= glm::toMat4(q);
        model = glm::scale(model, ent.scale);
        glm::mat4 mvp = vp * model;
        GLint loc = glGetUniformLocation(prog, "uMVP");
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp[0][0]);
        GLint col = glGetUniformLocation(prog, "uColor");
        if(ent.id == m_selectedId) {
            // brighter highlight for selected entity
            glUniform3f(col, 0.2f + ent.color.r, 0.2f + ent.color.g, 0.2f + ent.color.b);
        } else {
            glUniform3f(col, ent.color.r, ent.color.g, ent.color.b);
        }
        ent.mesh->draw();
    }
}

int Scene::getSelectedId() const { return m_selectedId; }

void Scene::selectEntity(int id) {
    // ensure id exists
    if(id == 0) { m_selectedId = 0; return; }
    auto it = std::find_if(m_entities.begin(), m_entities.end(), [id](const SceneEntity& e){ return e.id == id; });
    if(it != m_entities.end()) m_selectedId = id;
}

SceneEntity* Scene::findById(int id) {
    auto it = std::find_if(m_entities.begin(), m_entities.end(), [id](const SceneEntity& e){ return e.id == id; });
    if(it == m_entities.end()) return nullptr;
    return &(*it);
}

void Scene::deleteSelected() {
    if(m_selectedId == 0) return;
    auto it = std::find_if(m_entities.begin(), m_entities.end(), [this](const SceneEntity& e){ return e.id == m_selectedId; });
    if(it == m_entities.end()) return;
    m_entities.erase(it);
    m_selectedId = 0;
}

void Scene::translateSelected(const glm::vec3& delta) {
    SceneEntity* e = findById(m_selectedId);
    if(!e) return;
    e->position += delta;
}

void Scene::setSelectedPosition(const glm::vec3& pos) {
    SceneEntity* e = findById(m_selectedId);
    if(!e) return;
    e->position = pos;
}

// rotation/scale
void Scene::rotateSelected(const glm::vec3& deltaDegrees) {
    SceneEntity* e = findById(m_selectedId);
    if(!e) return;
    e->rotation += deltaDegrees;
}

void Scene::setSelectedRotation(const glm::vec3& eulerDeg) {
    SceneEntity* e = findById(m_selectedId);
    if(!e) return;
    e->rotation = eulerDeg;
}

void Scene::scaleSelected(const glm::vec3& scaleFactor) {
    SceneEntity* e = findById(m_selectedId);
    if(!e) return;
    e->scale *= scaleFactor;
}

void Scene::setSelectedScale(const glm::vec3& scale) {
    SceneEntity* e = findById(m_selectedId);
    if(!e) return;
    e->scale = scale;
}

void Scene::pushCommand(std::unique_ptr<Command> cmd) {
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.clear();
}

void Scene::undo() {
    if(m_undoStack.empty()) return;
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    cmd->undo(*this);
    m_redoStack.push_back(std::move(cmd));
}

void Scene::redo() {
    if(m_redoStack.empty()) return;
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    cmd->redo(*this);
    m_undoStack.push_back(std::move(cmd));
}

bool Scene::canUndo() const { return !m_undoStack.empty(); }
bool Scene::canRedo() const { return !m_redoStack.empty(); }

bool Scene::saveToFile(const std::string& path) const {
    std::ofstream f(path);
    if(!f) return false;
    for(const auto& e : m_entities){
        f << (int)e.type << " " << e.position.x << " " << e.position.y << " " << e.position.z << " "
          << e.rotation.x << " " << e.rotation.y << " " << e.rotation.z << " "
          << e.scale.x << " " << e.scale.y << " " << e.scale.z << "\n";
    }
    return true;
}

bool Scene::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if(!f) return false;
    m_entities.clear();
    int t; float px,py,pz; float rx,ry,rz; float sx,sy,sz;
    while(f >> t >> px >> py >> pz >> rx >> ry >> rz >> sx >> sy >> sz){
        int id = addPrimitive((primitives::PrimitiveType)t, glm::vec3(px,py,pz));
        SceneEntity* e = findById(id);
        if(e) { e->rotation = glm::vec3(rx,ry,rz); e->scale = glm::vec3(sx,sy,sz); }
    }
    return true;
}

void Scene::applyAdd(int id) { /* not implemented */ }
void Scene::applyRemove(int id, SceneEntity&& ent) { /* not implemented */ }

// New transform command implementation
Scene::Transform Scene::getEntityTransform(int id) const {
    Scene::Transform t;
    for(const auto& e : m_entities){
        if(e.id == id) { t.position = e.position; t.rotation = e.rotation; t.scale = e.scale; break; }
    }
    return t;
}

void Scene::setEntityTransform(int id, const Scene::Transform& t) {
    for(auto& e : m_entities){
        if(e.id == id) { e.position = t.position; e.rotation = t.rotation; e.scale = t.scale; break; }
    }
}

Scene::TransformCommand::TransformCommand(int i, const Transform& b, const Transform& a) : id(i), before(b), after(a) {}

void Scene::TransformCommand::undo(Scene& s) {
    s.setEntityTransform(id, before);
}

void Scene::TransformCommand::redo(Scene& s) {
    s.setEntityTransform(id, after);
}
