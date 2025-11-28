#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

class Scene;

class Animator {
public:
    Animator() = default;
    ~Animator() = default;

    // Advance animations by dt seconds and apply to the scene
    void update(Scene& scene, float dt);

    // Add animation helpers
    int addRotationAnimation(int entityId, const glm::vec3& axis, float degreesPerSec);
    int addTranslateAnimation(int entityId, const glm::vec3& velocityUnitsPerSec);
    int addScaleAnimation(int entityId, const glm::vec3& scaleDeltaPerSec);

    void removeAnimation(int animId);
    void removeAnimationsForEntity(int entityId);
    void clear();

    // Persistence
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

private:
    enum class Type { Rotation = 0, Translate = 1, Scale = 2 };

    struct Anim {
        int id = 0; // animation id
        int entityId = 0;
        Type type = Type::Rotation;
        // rotation
        glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f);
        float speedDeg = 0.0f;
        // translate
        glm::vec3 velocity = glm::vec3(0.0f);
        // scale (per-second additive delta)
        glm::vec3 scaleDelta = glm::vec3(0.0f);
    };

    std::vector<Anim> anims_;
    int nextAnimId_ = 1;
};

// Global animator instance (defined in animator.cpp)
extern Animator g_animator;

// Globals to control animator timestep mode (defined in main.cpp)
extern bool g_useFixedTimestep;
extern float g_fixedTimestep;
extern float g_timeAccumulator;
