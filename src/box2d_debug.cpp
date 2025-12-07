#include "box2d_debug.hpp"
#include "imgui.h"

namespace rlge {
    namespace {
        Color toRaylibColor(const b2Color& c) {
            return Color{
                static_cast<unsigned char>(c.r * 255),
                static_cast<unsigned char>(c.g * 255),
                static_cast<unsigned char>(c.b * 255),
                static_cast<unsigned char>(c.a * 255)
            };
        }
    }

    void Box2DDebugDraw::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) {
        if (!enabled_ || !drawShapes_) return;

        Color rayColor = toRaylibColor(color);
        for (int32 i = 0; i < vertexCount; ++i) {
            const auto& v1 = vertices[i];
            const auto& v2 = vertices[(i + 1) % vertexCount];
            DrawLineEx({v1.x, v1.y}, {v2.x, v2.y}, 1.0f, rayColor);
        }
    }

    void Box2DDebugDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) {
        if (!enabled_ || !drawShapes_) return;

        Color rayColor = toRaylibColor(color);
        Color fillColor = rayColor;
        fillColor.a = 128;

        // Draw filled polygon as triangle fan
        if (vertexCount >= 3) {
            for (int32 i = 1; i < vertexCount - 1; ++i) {
                DrawTriangle(
                    {vertices[0].x, vertices[0].y},
                    {vertices[i].x, vertices[i].y},
                    {vertices[i + 1].x, vertices[i + 1].y},
                    fillColor
                );
            }
        }

        // Draw outline
        DrawPolygon(vertices, vertexCount, color);
    }

    void Box2DDebugDraw::DrawCircle(const b2Vec2& center, float radius, const b2Color& color) {
        if (!enabled_ || !drawShapes_) return;

        Color rayColor = toRaylibColor(color);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius, rayColor);
    }

    void Box2DDebugDraw::DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) {
        if (!enabled_ || !drawShapes_) return;

        Color rayColor = toRaylibColor(color);
        Color fillColor = rayColor;
        fillColor.a = 128;

        DrawCircleV({center.x, center.y}, radius, fillColor);
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y), radius, rayColor);
        
        // Draw axis line
        Vector2 p = {center.x + radius * axis.x, center.y + radius * axis.y};
        DrawLineEx({center.x, center.y}, p, 1.0f, rayColor);
    }

    void Box2DDebugDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) {
        if (!enabled_ || !drawShapes_) return;

        Color rayColor = toRaylibColor(color);
        DrawLineEx({p1.x, p1.y}, {p2.x, p2.y}, 1.0f, rayColor);
    }

    void Box2DDebugDraw::DrawTransform(const b2Transform& xf) {
        if (!enabled_ || !drawCenterOfMass_) return;

        constexpr float axisScale = 0.4f;
        b2Vec2 p1 = xf.p;
        
        // Red for X axis
        b2Vec2 p2 = p1 + axisScale * xf.q.GetXAxis();
        DrawLineEx({p1.x, p1.y}, {p2.x, p2.y}, 2.0f, RED);
        
        // Green for Y axis
        p2 = p1 + axisScale * xf.q.GetYAxis();
        DrawLineEx({p1.x, p1.y}, {p2.x, p2.y}, 2.0f, GREEN);
    }

    void Box2DDebugDraw::DrawPoint(const b2Vec2& p, float size, const b2Color& color) {
        if (!enabled_ || !drawContactPoints_) return;

        Color rayColor = toRaylibColor(color);
        DrawCircleV({p.x, p.y}, size, rayColor);
    }

    void Box2DDebugDraw::debugOverlay() {
        ImGui::Begin("Box2D Physics");
        ImGui::Checkbox("Enable Debug Draw", &enabled_);
        
        if (enabled_) {
            ImGui::Separator();
            ImGui::Text("Draw Options:");
            ImGui::Checkbox("Shapes", &drawShapes_);
            ImGui::Checkbox("Joints", &drawJoints_);
            ImGui::Checkbox("AABBs", &drawAABBs_);
            ImGui::Checkbox("Contact Points", &drawContactPoints_);
            ImGui::Checkbox("Center of Mass", &drawCenterOfMass_);
            
            uint32 flags = 0;
            if (drawShapes_) flags |= e_shapeBit;
            if (drawJoints_) flags |= e_jointBit;
            if (drawAABBs_) flags |= e_aabbBit;
            if (drawContactPoints_) flags |= e_pairBit;
            if (drawCenterOfMass_) flags |= e_centerOfMassBit;
            SetFlags(flags);
        }
        
        ImGui::End();
    }
}
