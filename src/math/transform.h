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
        return position == rhs.position && heading == rhs.heading &&
               scale == rhs.scale;
    }

    const glm::mat4 &as_matrix() const;

    Transform operator*(const Transform &rhs) const;
    Transform inverse() const;

    glm::vec3 get_local_up() const { return heading * math::GLOBAL_UP_AXIS; }
    glm::vec3 get_local_front() const {
        return heading * math::GLOBAL_FRONT_AXIS;
    }
    glm::vec3 get_local_right() const {
        return heading * math::GLOBAL_RIGHT_AXIS;
    }

    bool is_identity() const;

    void set_position(const glm::vec3 &pos);
    void set_heading(const glm::quat &h);
    void set_scale(const glm::vec3 &s);

    const glm::vec3 &get_position() const { return position; }
    const glm::quat &get_heading() const { return heading; }
    const glm::vec3 &get_scale() const { return scale; }

  private:
    glm::vec3 position{};
    glm::quat heading = glm::identity<glm::quat>();
    glm::vec3 scale{1.f};

    mutable glm::mat4 transformMatrix{1.f};
    mutable bool isDirty{false};
};
