#include "stack.hpp"

#include <algorithm>
#include <numeric>

namespace rlge::ui {

    Vector2 Stack::measureContent() const {
        auto totalMain = 0.0f;
        auto maxCross = 0.0f;
        std::size_t visibleCount = 0;

        for (const auto& child : children_) {
            if (!child->visible())
                continue;

            ++visibleCount;
            const auto measured = child->measureContent();
            const auto& childLayout = child->layout();

            auto childMain = mainAxis(childLayout.size) > 0.0f
                ? mainAxis(childLayout.size)
                : mainAxis(measured);
            auto childCross = crossAxis(childLayout.size) > 0.0f
                ? crossAxis(childLayout.size)
                : crossAxis(measured);

            // Removed double-counting of padding: childMain and childCross already include padding from measureContent().

            totalMain += childMain;
            maxCross = std::max(maxCross, childCross);
        }

        if (visibleCount > 1) {
            totalMain += config_.spacing * static_cast<float>(visibleCount - 1);
        }

        return makeVec(totalMain, maxCross);
    }

    void Stack::computeLayout(const Rectangle& parentBounds) {
        // Set own bounds (ignore base auto-child layout; we handle children below).
        const auto width = layout_.size.x > 0.0f ? layout_.size.x : parentBounds.width;
        const auto height = layout_.size.y > 0.0f ? layout_.size.y : parentBounds.height;
        computedBounds_ = {parentBounds.x, parentBounds.y, width, height};
        layoutDirty_ = false;

        if (children_.empty())
            return;

        const auto content = contentBounds();
        const auto availableMain = mainAxis({content.width, content.height});
        const auto availableCross = crossAxis({content.width, content.height});

        struct ChildInfo {
            Widget* widget;
            float mainSize;
            float crossSize;
        };

        std::vector<ChildInfo> visibleChildren;
        visibleChildren.reserve(children_.size());

        for (const auto& child : children_) {
            if (!child->visible())
                continue;

            const auto measured = child->measureContent();
            const auto& childLayout = child->layout();

            auto mainSize = mainAxis(childLayout.size) > 0.0f
                ? mainAxis(childLayout.size)
                : mainAxis(measured) + mainAxis(childLayout.padding) * 2.0f;

            auto crossSize = crossAxis(childLayout.size) > 0.0f
                ? crossAxis(childLayout.size)
                : crossAxis(measured) + crossAxis(childLayout.padding) * 2.0f;

            visibleChildren.push_back({child.get(), mainSize, crossSize});
        }

        if (visibleChildren.empty())
            return;

        const auto totalChildMain = std::accumulate(
            visibleChildren.begin(), visibleChildren.end(), 0.0f,
            [](float sum, const ChildInfo& info) { return sum + info.mainSize; });

        const auto totalSpacing = config_.spacing * static_cast<float>(visibleChildren.size() - 1);
        const auto totalContentMain = totalChildMain + totalSpacing;
        const auto freeSpace = availableMain - totalContentMain;

        auto currentMain = 0.0f;
        auto extraSpacing = 0.0f;

        switch (config_.distribution) {
        case Distribution::Start:
            currentMain = 0.0f;
            break;
        case Distribution::Center:
            currentMain = freeSpace * 0.5f;
            break;
        case Distribution::End:
            currentMain = freeSpace;
            break;
        case Distribution::SpaceBetween:
            currentMain = 0.0f;
            if (visibleChildren.size() > 1) {
                extraSpacing = freeSpace / static_cast<float>(visibleChildren.size() - 1);
            }
            break;
        case Distribution::SpaceAround:
            extraSpacing = freeSpace / static_cast<float>(visibleChildren.size());
            currentMain = extraSpacing * 0.5f;
            break;
        case Distribution::SpaceEvenly:
            extraSpacing = freeSpace / static_cast<float>(visibleChildren.size() + 1);
            currentMain = extraSpacing;
            break;
        }

        for (auto& info : visibleChildren) {
            auto crossPos = 0.0f;
            auto childCross = info.crossSize;

            switch (config_.alignment) {
            case Alignment::Start:
                crossPos = 0.0f;
                break;
            case Alignment::Center:
                crossPos = (availableCross - childCross) * 0.5f;
                break;
            case Alignment::End:
                crossPos = availableCross - childCross;
                break;
            case Alignment::Stretch:
                crossPos = 0.0f;
                childCross = availableCross;
                break;
            }

            const auto childPos = makeVec(currentMain, crossPos);
            const auto childSize = makeVec(info.mainSize, childCross);

            const Rectangle childBounds{
                content.x + childPos.x,
                content.y + childPos.y,
                childSize.x,
                childSize.y
            };

            info.widget->computeLayout(childBounds);

            currentMain += info.mainSize + config_.spacing + extraSpacing;
        }
    }

} // namespace rlge::ui
