#include "transform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace {
static const auto I = glm::mat4{1.f};
}

Transform::Transform(const glm::mat4 &m) {
    glm::vec3 scaleVec;
    glm::quat rotationQuat;
    glm::vec3 translationVec;
    glm::vec3 skewVec;
    glm::vec4 perspectiveVec;
    glm::decompose(m, scaleVec, rotationQuat, translationVec, skewVec,
                   perspectiveVec);

    position = translationVec;
    heading = rotationQuat;
    scale = scaleVec;
    isDirty = true;
}

Transform Transform::operator*(const Transform &rhs) const {
    if (is_identity()) {
        return rhs;
    }
    if (rhs.is_identity()) {
        return *this;
    }
    return Transform(as_matrix() * rhs.as_matrix());
}

Transform Transform::inverse() const {
    if (is_identity()) {
        return Transform{};
    }
    return Transform(glm::inverse(as_matrix()));
}

const glm::mat4 &Transform::as_matrix() const {
    if (!isDirty) {
        return transformMatrix;
    }

    transformMatrix = glm::translate(I, position);
    if (heading != glm::identity<glm::quat>()) {
        transformMatrix *= glm::mat4_cast(heading);
    }
    transformMatrix = glm::scale(transformMatrix, scale);
    isDirty = false;
    return transformMatrix;
}

void Transform::set_position(const glm::vec3 &pos) {
    position = pos;
    isDirty = true;
}

void Transform::set_heading(const glm::quat &h) {
    // non-normalized quaternions cause all sorts of issues
    heading = glm::normalize(h);
    isDirty = true;
}

void Transform::set_scale(const glm::vec3 &s) {
    scale = s;
    isDirty = true;
}

bool Transform::is_identity() const { return as_matrix() == I; }
