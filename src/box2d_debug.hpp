#pragma once
#include <box2d/box2d.h>
#include "raylib.h"
#include "debug.hpp"

namespace rlge {
    // Debug drawer for Box2D that integrates with ImGui overlay
    class Box2DDebugDraw : public b2Draw, public HasDebugOverlay {
    public:
        Box2DDebugDraw() {
            SetFlags(e_shapeBit | e_jointBit | e_aabbBit | e_pairBit | e_centerOfMassBit);
        }

        void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
        void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
        void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) override;
        void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) override;
        void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
        void DrawTransform(const b2Transform& xf) override;
        void DrawPoint(const b2Vec2& p, float size, const b2Color& color) override;

        void debugOverlay() override;
        
        void setEnabled(bool enabled) { enabled_ = enabled; }
        [[nodiscard]] bool enabled() const { return enabled_; }

    private:
        bool enabled_ = false;
        bool drawShapes_ = true;
        bool drawJoints_ = true;
        bool drawAABBs_ = false;
        bool drawContactPoints_ = false;
        bool drawCenterOfMass_ = false;
    };
}
