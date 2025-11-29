#pragma once

#include <ranges>
#include <set>
#include <unordered_map>
#include <vector>

#include "collider.hpp"
#include "raylib.h"

namespace rlge {
    class Collider;

    class SpatialGrid {
    public:
        explicit SpatialGrid(const float cellSize = 100.0f)
            : cellSize_(cellSize) {}

        // Insert collider into the grid based on its bounds
        void insert(Collider* c) {
            const Rectangle bounds = c->axisAlignedWorldBounds();

            // Find all grid cells this collider overlaps
            const auto minGridX = static_cast<int>(std::floor(bounds.x / cellSize_));
            const auto maxGridX = static_cast<int>(std::floor((bounds.x + bounds.width) / cellSize_));
            const auto minGridY = static_cast<int>(std::floor(bounds.y / cellSize_));
            const auto maxGridY = static_cast<int>(std::floor((bounds.y + bounds.height) / cellSize_));

            // Add collider to all overlapping cells
            for (int gx = minGridX; gx <= maxGridX; ++gx) {
                for (int gy = minGridY; gy <= maxGridY; ++gy) {
                    uint64_t key = (static_cast<uint64_t>(gx) << 32) |
                                  (static_cast<uint32_t>(gy) & 0xFFFFFFFFu);
                    grid_[key].colliders.push_back(c);
                }
            }
        }

        // Get all potential collision pairs from grid
        std::vector<std::pair<Collider*, Collider*>> getPotentialPairs() {
            std::vector<std::pair<Collider*, Collider*>> pairs;
            std::set<std::pair<Collider*, Collider*>> seenPairs;  // Avoid duplicates

            // For each cell, check colliders within that cell
            for (auto& [colliders] : grid_ | std::views::values) {
                for (size_t i = 0; i < colliders.size(); ++i) {
                    for (size_t j = i + 1; j < colliders.size(); ++j) {
                        auto a = colliders[i];
                        auto b = colliders[j];

                        // Normalize a pair (smaller address first) to avoid duplicates
                        if (a > b) std::swap(a, b);

                        if (seenPairs.insert({a, b}).second) {
                            pairs.emplace_back(a, b);
                        }
                    }
                }
            }
            return pairs;
        }

        void clear() {
            grid_.clear();
        }

    private:
        struct Cell {
            std::vector<Collider*> colliders;
        };

        std::unordered_map<uint64_t, Cell> grid_;
        float cellSize_;

        // Convert world position to grid cell key
        uint64_t positionToKey(const float x, const float y) const {
            auto const gridX = static_cast<int>(std::floor(x / cellSize_));
            auto const gridY = static_cast<int>(std::floor(y / cellSize_));
            // Pack into a 64-bit key: upper 32 bits = X, lower 32 bits = Y
            return (static_cast<uint64_t>(gridX) << 32) |
                   (static_cast<uint32_t>(gridY) & 0xFFFFFFFFu);
        }
    };
}