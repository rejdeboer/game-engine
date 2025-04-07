#include "intersection.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>

/**
 * @brief Checks if a ray intersects with an Axis-Aligned Bounding Box (AABB).
 *        Uses the Slab Test method.
 *
 * @param rayOrigin The starting point of the ray.
 * @param rayDirection The normalized direction vector of the ray. MUST NOT be
 * zero vector.
 * @param boxMin The minimum corner coordinates (x, y, z) of the AABB.
 * @param boxMax The maximum corner coordinates (x, y, z) of the AABB.
 * @param outIntersectionDistance (Output) If intersection occurs, the distance
 * 't' along the ray to the *first* intersection point (entry point).
 * @return true if the ray intersects the AABB (at a non-negative distance),
 * false otherwise.
 */
bool math::intersect_ray_aabb(const glm::vec3 &rayOrigin,
                              const glm::vec3 &rayDirection,
                              const glm::vec3 &boxMin, const glm::vec3 &boxMax,
                              float &outIntersectionDistance) {
    // Initialize intersection distances for the overall interval
    float tNear = -std::numeric_limits<float>::infinity();
    float tFar = std::numeric_limits<float>::infinity();

    // Process each axis (X, Y, Z)
    for (int i = 0; i < 3; ++i) { // Iterate through dimensions x, y, z
        // Check for division by zero if ray is parallel to slab planes
        if (std::abs(rayDirection[i]) < 1e-6f) { // Use a small epsilon
            // Ray is parallel to the slab planes for this axis.
            // If the origin is not within the slab bounds, it misses the box.
            if (rayOrigin[i] < boxMin[i] || rayOrigin[i] > boxMax[i]) {
                return false; // Misses the box entirely
            }
            // Otherwise, the ray is parallel AND inside the slab, so continue
            // checking other axes.
        } else {
            // Calculate intersection distances for this axis's slab
            float t1 = (boxMin[i] - rayOrigin[i]) / rayDirection[i];
            float t2 = (boxMax[i] - rayOrigin[i]) / rayDirection[i];

            // Ensure t1 is the entry distance and t2 is the exit distance
            if (t1 > t2) {
                std::swap(t1, t2); // Swap if needed
            }

            // Update the overall intersection interval [tNear, tFar]
            tNear = std::max(tNear, t1); // We want the latest entry time
            tFar = std::min(tFar, t2);   // We want the earliest exit time

            // Check for non-overlap
            if (tNear > tFar) {
                return false; // Box is missed because intervals don't overlap
            }
        }
    }

    // After checking all axes, the ray intersects the box if tFar >= 0
    // (meaning the exit point is not behind the ray origin)
    // and tNear <= tFar (which we already checked implicitly).
    // We also need tNear (the entry point) to be valid.
    // If tFar < 0, the entire intersection is behind the ray.

    if (tFar < 0.0f) {
        return false; // Intersection is entirely behind the ray's origin
    }

    // Intersection occurs. tNear holds the distance to the entry point.
    // If tNear is negative, the ray origin is inside the box,
    // so the first intersection is at t=0 (or tNear if you need the actual
    // entry point). For picking, we usually want the first hit distance >= 0.
    if (tNear < 0.0f) {
        outIntersectionDistance = 0.0f; // Origin is inside, hit distance is 0
        // Or if you need the actual entry point even if inside:
        // outIntersectionDistance = tNear;
    } else {
        outIntersectionDistance =
            tNear; // Hit distance is the calculated entry point
    }

    return true;
}

// --- Overload without output distance ---
bool math::intersect_ray_aabb(const glm::vec3 &rayOrigin,
                              const glm::vec3 &rayDirection,
                              const glm::vec3 &boxMin,
                              const glm::vec3 &boxMax) {
    float dummyDistance;
    return intersect_ray_aabb(rayOrigin, rayDirection, boxMin, boxMax,
                              dummyDistance);
}
