#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

Camera::Camera() {}

glm::mat4 Camera::getView() const {
    glm::vec3 camPos;
    camPos.x = target_.x + distance_ * cos(angles_.x) * sin(angles_.y);
    camPos.y = target_.y + distance_ * sin(angles_.x);
    camPos.z = target_.z + distance_ * cos(angles_.x) * cos(angles_.y);
    return glm::lookAt(camPos, target_, glm::vec3(0,1,0));
}

glm::mat4 Camera::getProjection(float aspect) const {
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
}

glm::vec3 Camera::getPosition() const {
    glm::vec3 camPos;
    camPos.x = target_.x + distance_ * cos(angles_.x) * sin(angles_.y);
    camPos.y = target_.y + distance_ * sin(angles_.x);
    camPos.z = target_.z + distance_ * cos(angles_.x) * cos(angles_.y);
    return camPos;
}

void Camera::onScroll(double yoff) {
    distance_ *= (1.0f - (float)yoff*0.12f);
    if(distance_ < 0.2f) distance_ = 0.2f;
}

void Camera::beginMiddleDrag(const glm::vec2& pos) {
    dragging_ = true; lastMouse_ = pos;
}

void Camera::updateMiddleDrag(const glm::vec2& pos, bool alt) {
    if(!dragging_) return;
    glm::vec2 delta = pos - lastMouse_;
    lastMouse_ = pos;
    if(alt) {
        float panSpeed = 0.0015f * distance_;
        glm::vec3 camPos = getPosition();
        glm::vec3 forward = glm::normalize(target_ - camPos);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
        glm::vec3 upVec = glm::normalize(glm::cross(right, forward));
        target_ += (-right * delta.x * panSpeed) + (upVec * delta.y * panSpeed);
    } else {
        angles_.x += delta.y * 0.008f;
        angles_.y += delta.x * 0.008f;
    }
}

void Camera::endMiddleDrag() {
    dragging_ = false;
}

void Camera::setPosition(const glm::vec3& camPos) {
    glm::vec3 delta = camPos - target_;
    float dist = glm::length(delta);
    if(dist < 1e-6f) return;
    distance_ = dist;
    float pitch = asinf(glm::clamp(delta.y / dist, -1.0f, 1.0f));
    float yaw = atan2f(delta.x, delta.z);
    angles_.x = pitch; angles_.y = yaw;
}

void Camera::handleViewportInput(GLFWwindow* window, bool mouseOnViewport) {
    static bool middleDown = false;
    if(!mouseOnViewport) {
        if(middleDown) { endMiddleDrag(); middleDown = false; }
        return;
    }
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        glm::vec2 cur((float)mx, (float)my);
        if(!middleDown) {
            middleDown = true;
            beginMiddleDrag(cur);
        } else {
            bool alt = (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
            updateMiddleDrag(cur, alt);
        }
    } else {
        if(middleDown) endMiddleDrag();
        middleDown = false;
    }
}
