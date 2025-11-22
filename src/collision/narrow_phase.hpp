#pragma once
#include "collider_types.hpp"

namespace rlge {
    class PolygonCollider;
    class CircleCollider;
    class BoxCollider;
    class ObbCollider;

    namespace narrow_phase {
        CollisionManifold boxBox(const BoxCollider& a, const BoxCollider& b);
        CollisionManifold boxCircle(const BoxCollider& box, const CircleCollider& c);
        CollisionManifold circleCircle(const CircleCollider& a, const CircleCollider& b);
        CollisionManifold boxObb(const ObbCollider& obb, const BoxCollider& box);
        CollisionManifold obbObb(const ObbCollider& a, const ObbCollider& b);

        CollisionManifold polyPoly(const PolygonCollider& a, const PolygonCollider& b);
        CollisionManifold polyCircle(const PolygonCollider& p, const CircleCollider& c);
        CollisionManifold polyBox(const PolygonCollider& p, const BoxCollider& b);
        CollisionManifold obbPolygon(const ObbCollider& obb, const PolygonCollider& p);
        CollisionManifold obbCircle(const ObbCollider& obb, const CircleCollider& c);
    }
}
