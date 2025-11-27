#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    Camera();

    glm::mat4 getView() const;
    glm::mat4 getProjection(float aspect) const;
    glm::vec3 getPosition() const;

    // Input helpers
    void onScroll(double yoff);
    void beginMiddleDrag(const glm::vec2& pos);
    void updateMiddleDrag(const glm::vec2& pos, bool alt);
    void endMiddleDrag();

    // Handle viewport-specific input (queries GLFW for mouse/buttons when mouse is over viewport)
    void handleViewportInput(GLFWwindow* window, bool mouseOnViewport);

    // Set camera world position and recompute spherical params relative to current target
    void setPosition(const glm::vec3& camPos);

private:
    float distance_ = 6.0f;
    glm::vec2 angles_ = glm::vec2(0.3f, -1.0f); // pitch, yaw
    glm::vec3 target_ = glm::vec3(0.0f);

    // dragging state
    bool dragging_ = false;
    glm::vec2 lastMouse_ = glm::vec2(0.0f);
};
