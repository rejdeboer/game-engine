#include "transform.h"

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

    _position = translationVec;
    _heading = rotationQuat;
    _scale = scaleVec;
    _isDirty = true;
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
    if (!_isDirty) {
        return transformMatrix;
    }

    transformMatrix = glm::translate(I, _position);
    if (_heading != glm::identity<glm::quat>()) {
        transformMatrix *= glm::mat4_cast(_heading);
    }
    transformMatrix = glm::scale(transformMatrix, _scale);
    _isDirty = false;
    return transformMatrix;
}

void Transform::position(const glm::vec3 &pos) {
    _position = pos;
    _isDirty = true;
}

void Transform::heading(const glm::quat &h) {
    // non-normalized quaternions cause all sorts of issues
    _heading = glm::normalize(h);
    _isDirty = true;
}

void Transform::scale(const glm::vec3 &s) {
    _scale = s;
    _isDirty = true;
}

bool Transform::is_identity() const { return as_matrix() == I; }
