#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "global_axis.h"

class Transform {
  public:
    Transform() = default;
    Transform(const glm::mat4 &m);

    bool operator==(const Transform &rhs) const {
        return _position == rhs._position && _heading == rhs._heading &&
               _scale == rhs._scale;
    }

    const glm::mat4 &as_matrix() const;

    Transform operator*(const Transform &rhs) const;
    Transform inverse() const;

    glm::vec3 get_local_up() const { return _heading * math::GLOBAL_UP_AXIS; }
    glm::vec3 get_local_front() const {
        return _heading * math::GLOBAL_FRONT_AXIS;
    }
    glm::vec3 get_local_right() const {
        return _heading * math::GLOBAL_RIGHT_AXIS;
    }

    bool is_identity() const;

    void position(const glm::vec3 &pos);
    void heading(const glm::quat &h);
    void scale(const glm::vec3 &s);

    const glm::vec3 &position() const { return _position; }
    const glm::quat &heading() const { return _heading; }
    const glm::vec3 &scale() const { return _scale; }

  private:
    glm::vec3 _position{};
    glm::quat _heading = glm::identity<glm::quat>();
    glm::vec3 _scale{1.f};

    mutable glm::mat4 transformMatrix{1.f};
    mutable bool _isDirty{false};
};
