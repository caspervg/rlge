#include "narrow_phase.hpp"

#include <vector>

#include "raylib.h"
#include "raymath.h"
#include "shape/box_collider.hpp"
#include "shape/circle_collider.hpp"
#include "shape/obb_collider.hpp"
#include "shape/polygon_collider.hpp"

namespace rlge::narrow_phase {

    static void projectPoints(const std::vector<Vector2>& pts, const Vector2 axis, float& outMin, float& outMax) {
        const float len = Vector2Length(axis);
        if (len == 0.0f || pts.empty()) {
            outMin = 0.0f;
            outMax = 0.0f;
            return;
        }

        const Vector2 nAxis = Vector2Scale(axis, 1.0f / len);

        float minProj = Vector2DotProduct(pts[0], nAxis);
        float maxProj = minProj;

        const size_t count = pts.size();
        for (size_t i = 1; i < count; ++i) {
            const float p = Vector2DotProduct(pts[i], nAxis);
            if (p < minProj) {
                minProj = p;
            } else if (p > maxProj) {
                maxProj = p;
            }
        }

        outMin = minProj;
        outMax = maxProj;
    }

    static Vector2 polygonCenter(const std::vector<Vector2>& pts) {
        Vector2 c{0.0f, 0.0f};
        if (pts.empty()) {
            return c;
        }

        for (const auto& p : pts) {
            c = Vector2Add(c, p);
        }

        const float inv = 1.0f / static_cast<float>(pts.size());
        return Vector2Scale(c, inv);
    }

    static void projectCircle(Vector2 center, float radius, Vector2 axis, float& outMin, float& outMax) {
        const float len = Vector2Length(axis);
        if (len == 0.0f) {
            outMin = 0.0f;
            outMax = 0.0f;
            return;
        }

        const Vector2 nAxis = Vector2Scale(axis, 1.0f / len);
        const float centerProj = Vector2DotProduct(center, nAxis);
        outMin = centerProj - radius;
        outMax = centerProj + radius;
    }

    static bool satTestEdges(const std::vector<Vector2>& a,
                             const std::vector<Vector2>& b,
                             const std::vector<Vector2>& pts,
                             bool& ioHasAxis,
                             float& ioBestDepth,
                             Vector2& ioBestAxis) {
        const size_t count = pts.size();
        for (size_t i = 0; i < count; ++i) {
            const Vector2 p0 = pts[i];
            const Vector2 p1 = pts[(i + 1) % count];
            const Vector2 edge = Vector2Subtract(p1, p0);

            if (Vector2Length(edge) == 0.0f) {
                continue;
            }

            Vector2 axis{-edge.y, edge.x};

            float minA, maxA;
            float minB, maxB;
            projectPoints(a, axis, minA, maxA);
            projectPoints(b, axis, minB, maxB);

            if (maxA < minB || maxB < minA) {
                return false;
            }

            const float overlap = (maxA < maxB ? maxA : maxB) - (minA > minB ? minA : minB);
            if (!ioHasAxis || overlap < ioBestDepth) {
                ioHasAxis = true;
                ioBestDepth = overlap;
                const float len = Vector2Length(axis);
                ioBestAxis = Vector2Scale(axis, 1.0f / len);
            }
        }
        return true;
    }

    static bool satPolygons(const std::vector<Vector2>& a,
                            const std::vector<Vector2>& b,
                            float& outDepth,
                            Vector2& outAxis) {
        auto hasAxis = false;
        auto bestDepth = 0.0f;
        Vector2 bestAxis{0.0f, 0.0f};

        if (!satTestEdges(a, b, a, hasAxis, bestDepth, bestAxis)) {
            return false;
        }
        if (!satTestEdges(a, b, b, hasAxis, bestDepth, bestAxis)) {
            return false;
        }

        if (!hasAxis) {
            return false;
        }

        outDepth = bestDepth;
        outAxis = bestAxis;
        return true;
    }

    static CollisionManifold polygonCollisionFromPoints(const std::vector<Vector2>& ptsA,
                                                        const std::vector<Vector2>& ptsB) {
        CollisionManifold m{};

        if (ptsA.size() < 3 || ptsB.size() < 3) {
            m.colliding = false;
            return m;
        }

        float depth = 0.0f;
        Vector2 axis{0.0f, 0.0f};

        if (!satPolygons(ptsA, ptsB, depth, axis)) {
            m.colliding = false;
            return m;
        }

        const Vector2 centerA = polygonCenter(ptsA);
        const Vector2 centerB = polygonCenter(ptsB);
        const Vector2 dir = Vector2Subtract(centerB, centerA);

        if (Vector2DotProduct(axis, dir) < 0.0f) {
            axis = Vector2Negate(axis);
        }

        m.colliding = true;
        m.normal = axis;
        m.depth = depth;
        return m;
    }

    CollisionManifold boxBox(const BoxCollider& a, const BoxCollider& b) {
        CollisionManifold m;
        const Rectangle A = a.axisAlignedWorldBounds();
        const Rectangle B = b.axisAlignedWorldBounds();

        const auto Ax2 = A.x + A.width;
        const auto Ay2 = A.y + A.height;
        const auto Bx2 = B.x + B.width;
        const auto By2 = B.y + B.height;

        if (!CheckCollisionRecs(A, B)) {
            // For safety, do a broad phase check first
            m.colliding = false;
            return m;
        }

        const auto overlapX1 = Ax2 - B.x;
        const auto overlapX2 = Bx2 - A.x;
        const auto depthX = std::min(overlapX1, overlapX2);
        const auto overlapY1 = Ay2 - B.y;
        const auto overlapY2 = By2 - A.y;
        const auto depthY = std::min(overlapY1, overlapY2);

        if (depthX < depthY) {
            m.depth = depthX;
            m.normal = {A.x < B.x ? -1.f : 1.f, 0.f};
        }
        else {
            m.depth = depthY;
            m.normal = {0.f, A.y < B.y ? -1.f : 1.f};
        }
        m.colliding = true;
        return m;
    }

    CollisionManifold boxCircle(const BoxCollider& box, const CircleCollider& c) {
        CollisionManifold m;

        const auto b = box.axisAlignedWorldBounds();
        const auto center = c.center();
        const auto radius = c.radius();

        const auto closest = Vector2Clamp(
            center,
            Vector2{b.x, b.y},
            Vector2{b.x + b.width, b.y + b.height});

        const auto diff = center - closest;
        const auto dSq = Vector2DotProduct(diff, diff);

        if (dSq > radius * radius) {
            m.colliding = false;
            return m;
        }

        const auto dist = sqrtf(dSq);
        if (dist == 0.0f) {
            // Circle center is inside the box, choose the shallowest axis
            const auto left = center.x - b.x;
            const auto right = b.x + b.width - center.x;
            const auto top = center.y - b.y;
            const auto bottom = b.y + b.height - center.y;

            const auto minX = std::min(left, right);
            const auto minY = std::min(top, bottom);

            if (minX < minY) {
                m.normal = (left < right) ? Vector2{-1.f, 0.f} : Vector2{1.f, 0.f};
                m.depth = minX;
            } else {
                m.normal = (top < bottom) ? Vector2{0.f, -1.f} : Vector2{0.f, 1.f};
                m.depth = minY;
            }
        } else {
            // Partial overlap
            m.depth = radius - dist;
            m.normal = diff / dist;
        }
        m.colliding = true;
        return m;
    }

    CollisionManifold circleCircle(const CircleCollider& a, const CircleCollider& b) {
        CollisionManifold m;

        const auto pa = a.center();
        const auto pb = b.center();

        const auto d = pb - pa;
        const auto dSq = Vector2DotProduct(d, d);
        const auto rSum = a.radius() + b.radius();

        if (dSq >= rSum * rSum) {
            m.colliding = false;
            return m;
        }

        const auto dist = sqrtf(dSq);
        if (dist == 0.0f) {
            // Overlapping perfectly
            m.depth = rSum;
            m.normal = {1.0f, 0.0f};
        } else {
            m.depth = rSum - dist;
            m.normal = d / dist;
        }

        m.colliding = true;
        return m;
    }

    static CollisionManifold polygonCircleFromPoints(const std::vector<Vector2>& pts,
                                                     const CircleCollider& c) {
        CollisionManifold m{};

        if (pts.size() < 3) {
            m.colliding = false;
            return m;
        }

        const Vector2 center = c.center();
        const float radius = c.radius();

        bool hasAxis = false;
        float bestDepth = 0.0f;
        Vector2 bestAxis{0.0f, 0.0f};

        // Polygon edge normals
        const size_t count = pts.size();
        for (size_t i = 0; i < count; ++i) {
            const Vector2 p0 = pts[i];
            const Vector2 p1 = pts[(i + 1) % count];
            const Vector2 edge = Vector2Subtract(p1, p0);

            if (Vector2Length(edge) == 0.0f) {
                continue;
            }

            Vector2 axis{-edge.y, edge.x};

            float minPoly, maxPoly;
            projectPoints(pts, axis, minPoly, maxPoly);

            float minCircle, maxCircle;
            projectCircle(center, radius, axis, minCircle, maxCircle);

            if (maxPoly < minCircle || maxCircle < minPoly) {
                m.colliding = false;
                return m;
            }

            const float overlap = (maxPoly < maxCircle ? maxPoly : maxCircle) -
                                  (minPoly > minCircle ? minPoly : minCircle);

            if (!hasAxis || overlap < bestDepth) {
                hasAxis = true;
                bestDepth = overlap;
                const float len = Vector2Length(axis);
                bestAxis = Vector2Scale(axis, 1.0f / len);
            }
        }

        // Axis from nearest polygon vertex to the circle center
        float minDistSq = 0.0f;
        Vector2 closestVertex{0.0f, 0.0f};
        for (size_t i = 0; i < count; ++i) {
            const Vector2 v = pts[i];
            const Vector2 diff = Vector2Subtract(center, v);
            const auto dSq = Vector2DotProduct(diff, diff);
            if (i == 0 || dSq < minDistSq) {
                minDistSq = dSq;
                closestVertex = v;
            }
        }

        Vector2 vertexAxis = Vector2Subtract(center, closestVertex);
        if (Vector2Length(vertexAxis) > 0.0f) {
            float minPoly, maxPoly;
            projectPoints(pts, vertexAxis, minPoly, maxPoly);

            float minCircle, maxCircle;
            projectCircle(center, radius, vertexAxis, minCircle, maxCircle);

            if (maxPoly < minCircle || maxCircle < minPoly) {
                m.colliding = false;
                return m;
            }

            const float overlap = (maxPoly < maxCircle ? maxPoly : maxCircle) -
                                  (minPoly > minCircle ? minPoly : minCircle);

            if (!hasAxis || overlap < bestDepth) {
                hasAxis = true;
                bestDepth = overlap;
                const float len = Vector2Length(vertexAxis);
                bestAxis = Vector2Scale(vertexAxis, 1.0f / len);
            }
        }

        if (!hasAxis) {
            m.colliding = false;
            return m;
        }

        const Vector2 polyCenter = polygonCenter(pts);
        const Vector2 dir = Vector2Subtract(center, polyCenter);
        if (Vector2DotProduct(bestAxis, dir) < 0.0f) {
            bestAxis = Vector2Negate(bestAxis);
        }

        m.colliding = true;
        m.normal = bestAxis;
        m.depth = bestDepth;
        return m;
    }

    CollisionManifold boxObb(const ObbCollider& obb, const BoxCollider& box) {
        return polygonCollisionFromPoints(obb.points(), box.points());
    }

    CollisionManifold obbObb(const ObbCollider& a, const ObbCollider& b) {
        return polygonCollisionFromPoints(a.points(), b.points());
    }

    CollisionManifold polyPoly(const PolygonCollider& a, const PolygonCollider& b) {
        return polygonCollisionFromPoints(a.points(), b.points());
    }

    CollisionManifold polyCircle(const PolygonCollider& p, const CircleCollider& c) {
        return polygonCircleFromPoints(p.points(), c);
    }

    CollisionManifold polyBox(const PolygonCollider& p, const BoxCollider& b) {
        // We want the manifold normal to point from the box to the polygon
        // for the common case where the box is the moving collider. By
        // feeding the box points as the first argument, the SAT helper
        // orients the normal from box -> poly.
        return polygonCollisionFromPoints(b.points(), p.points());
    }

    CollisionManifold obbPolygon(const ObbCollider& obb, const PolygonCollider& p) {
        return polygonCollisionFromPoints(obb.points(), p.points());
    }

    CollisionManifold obbCircle(const ObbCollider& obb, const CircleCollider& c) {
        return polygonCircleFromPoints(obb.points(), c);
    }

}
