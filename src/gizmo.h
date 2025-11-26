#pragma once
#include <glm/glm.hpp>

class Scene;

class Gizmo {
public:
    enum class Axis { None, X, Y, Z };

    // Numeric widgets for editing transform components in the Tools panel
    bool translationWidget(class Scene& scene, glm::vec3& newPos);
    bool rotationWidget(class Scene& scene, glm::vec3& newEulerDeg);
    bool scaleWidget(class Scene& scene, glm::vec3& newScale);

    // On-screen gizmo drawing (legacy/fallback). Returns true when an edit was committed.
    bool drawGizmo(const glm::mat4& vp, const glm::vec2& viewPos, const glm::vec2& viewSize, class Scene& scene);

private:
    bool dragging_ = false;
};
