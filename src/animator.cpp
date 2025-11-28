#include "animator.h"
#include "scene.h"
#include "log.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <fstream>

Animator g_animator;

void Animator::update(Scene& scene, float dt) {
    if(anims_.empty()) return;
    for(const auto& a : anims_) {
        SceneEntity* e = scene.findById(a.entityId);
        if(!e) continue;
        switch(a.type) {
            case Type::Rotation: {
                glm::vec3 delta = a.axis * (a.speedDeg * dt);
                e->rotation += delta;
                break;
            }
            case Type::Translate: {
                e->position += a.velocity * dt;
                break;
            }
            case Type::Scale: {
                e->scale += a.scaleDelta * dt;
                // clamp scale to small positive values
                e->scale.x = std::max(0.0001f, e->scale.x);
                e->scale.y = std::max(0.0001f, e->scale.y);
                e->scale.z = std::max(0.0001f, e->scale.z);
                break;
            }
        }
    }
}

int Animator::addRotationAnimation(int entityId, const glm::vec3& axis, float degreesPerSec) {
    Anim a; a.id = nextAnimId_++; a.entityId = entityId; a.type = Type::Rotation; a.axis = axis; a.speedDeg = degreesPerSec;
    anims_.push_back(a);
    LOG_INFO("Added rotation anim id=" << a.id << " ent=" << entityId << " spd=" << degreesPerSec);
    return a.id;
}

int Animator::addTranslateAnimation(int entityId, const glm::vec3& velocityUnitsPerSec) {
    Anim a; a.id = nextAnimId_++; a.entityId = entityId; a.type = Type::Translate; a.velocity = velocityUnitsPerSec;
    anims_.push_back(a);
    LOG_INFO("Added translate anim id=" << a.id << " ent=" << entityId << " vel=" << velocityUnitsPerSec.x << "," << velocityUnitsPerSec.y << "," << velocityUnitsPerSec.z);
    return a.id;
}

int Animator::addScaleAnimation(int entityId, const glm::vec3& scaleDeltaPerSec) {
    Anim a; a.id = nextAnimId_++; a.entityId = entityId; a.type = Type::Scale; a.scaleDelta = scaleDeltaPerSec;
    anims_.push_back(a);
    LOG_INFO("Added scale anim id=" << a.id << " ent=" << entityId << " delta=" << scaleDeltaPerSec.x << "," << scaleDeltaPerSec.y << "," << scaleDeltaPerSec.z);
    return a.id;
}

void Animator::removeAnimation(int animId) {
    auto it = std::remove_if(anims_.begin(), anims_.end(), [animId](const Anim& a){ return a.id == animId; });
    if(it != anims_.end()) { anims_.erase(it, anims_.end()); LOG_INFO("Removed anim id=" << animId); }
}

void Animator::removeAnimationsForEntity(int entityId) {
    auto it = std::remove_if(anims_.begin(), anims_.end(), [entityId](const Anim& a){ return a.entityId == entityId; });
    if(it != anims_.end()) { anims_.erase(it, anims_.end()); LOG_INFO("Removed animations for entity=" << entityId); }
}

void Animator::clear() { anims_.clear(); }

bool Animator::saveToFile(const std::string& path) const {
    std::ofstream f(path);
    if(!f) return false;
    // Format: TYPE id entityId ...
    for(const auto& a : anims_) {
        if(a.type == Type::Rotation) {
            f << "ROT " << a.id << " " << a.entityId << " " << a.axis.x << " " << a.axis.y << " " << a.axis.z << " " << a.speedDeg << "\n";
        } else if(a.type == Type::Translate) {
            f << "TRN " << a.id << " " << a.entityId << " " << a.velocity.x << " " << a.velocity.y << " " << a.velocity.z << "\n";
        } else if(a.type == Type::Scale) {
            f << "SCL " << a.id << " " << a.entityId << " " << a.scaleDelta.x << " " << a.scaleDelta.y << " " << a.scaleDelta.z << "\n";
        }
    }
    LOG_INFO("Saved " << anims_.size() << " animations to " << path);
    return true;
}

bool Animator::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if(!f) return false;
    anims_.clear();
    std::string tag;
    while(f >> tag) {
        if(tag == "ROT") {
            Anim a; f >> a.id >> a.entityId >> a.axis.x >> a.axis.y >> a.axis.z >> a.speedDeg;
            a.type = Type::Rotation;
            anims_.push_back(a);
            nextAnimId_ = std::max(nextAnimId_, a.id + 1);
        } else if(tag == "TRN") {
            Anim a; f >> a.id >> a.entityId >> a.velocity.x >> a.velocity.y >> a.velocity.z;
            a.type = Type::Translate;
            anims_.push_back(a);
            nextAnimId_ = std::max(nextAnimId_, a.id + 1);
        } else if(tag == "SCL") {
            Anim a; f >> a.id >> a.entityId >> a.scaleDelta.x >> a.scaleDelta.y >> a.scaleDelta.z;
            a.type = Type::Scale;
            anims_.push_back(a);
            nextAnimId_ = std::max(nextAnimId_, a.id + 1);
        } else {
            std::string line; std::getline(f, line);
        }
    }
    LOG_INFO("Loaded animations from " << path << ", count=" << anims_.size());
    return true;
}
