#include "vx_inventory.hpp"

#include <algorithm>
#include <utility>

#include "vx_config.hpp"

namespace vox {

    namespace {
        // Blocks a fresh survival player starts with. Enough to build a shelter
        // and light it without trivialising the first night.
        constexpr std::array<std::pair<Block, int>, 6> kStarterKit = {{
            {Block::Planks, 32},
            {Block::Cobble, 32},
            {Block::Dirt, 16},
            {Block::Wood, 8},
            {Block::Glass, 8},
            {Block::Lantern, 4},
        }};
    } // namespace

    // ------------------------------------------------------------ Slot access

    const Slot& Inventory::slot(const int index) const {
        if (!validIndex(index)) {
            scratch_.clear();
            return scratch_;
        }
        return slots_[static_cast<std::size_t>(index)];
    }

    Slot& Inventory::slot(const int index) {
        if (!validIndex(index)) {
            scratch_.clear();
            return scratch_;
        }
        return slots_[static_cast<std::size_t>(index)];
    }

    // -------------------------------------------------------------- Stock

    int Inventory::addLeftover(const Block b, int n) {
        if (b == Block::Air || n <= 0)
            return std::max(n, 0);

        // Pass 1: top up partial stacks of the same block so the grid does not
        // fragment into many half-stacks.
        for (auto& s : slots_) {
            if (n <= 0)
                break;
            if (s.empty() || s.block != b)
                continue;
            const int take = std::min(n, s.room());
            s.count += take;
            n -= take;
        }
        // Pass 2: empty slots, hotbar first (slots_ is ordered that way).
        for (auto& s : slots_) {
            if (n <= 0)
                break;
            if (!s.empty())
                continue;
            s.block = b;
            s.count = std::min(n, kMaxStack);
            n -= s.count;
        }
        return n;
    }

    bool Inventory::add(const Block b, const int n) {
        return addLeftover(b, n) == 0;
    }

    int Inventory::countOf(const Block b) const {
        if (b == Block::Air)
            return 0;
        int total = 0;
        for (const auto& s : slots_) {
            if (!s.empty() && s.block == b)
                total += s.count;
        }
        return total;
    }

    bool Inventory::consume(const Block b, const int n) {
        if (creative_)
            return true; // infinite blocks: never touch the stacks
        if (n <= 0)
            return true;
        if (countOf(b) < n)
            return false; // all-or-nothing, so a failed place leaves no partial spend

        int remaining = n;
        // Drain in slot order (hotbar first) so what the player sees selected is
        // what gets spent.
        for (auto& s : slots_) {
            if (remaining <= 0)
                break;
            if (s.empty() || s.block != b)
                continue;
            const int take = std::min(remaining, s.count);
            s.count -= take;
            remaining -= take;
            if (s.count <= 0)
                s.clear();
        }
        return true;
    }

    bool Inventory::consumeSelected(const int n) {
        if (creative_)
            return true;
        if (n <= 0)
            return true;
        Slot& s = slots_[static_cast<std::size_t>(selected_)];
        if (s.empty() || s.count < n)
            return false;
        s.count -= n;
        if (s.count <= 0)
            s.clear();
        return true;
    }

    void Inventory::clear() {
        for (auto& s : slots_)
            s.clear();
    }

    // ---------------------------------------------------------- Selection

    void Inventory::select(const int index) {
        selected_ = std::clamp(index, 0, kHotbarSlots - 1);
    }

    void Inventory::scrollSelection(const int delta) {
        // Wrap with a positive modulus so large negative deltas behave.
        int i = (selected_ + delta) % kHotbarSlots;
        if (i < 0)
            i += kHotbarSlots;
        selected_ = i;
    }

    // -------------------------------------------------------- Rearranging

    void Inventory::swapSlots(const int a, const int b) {
        if (a == b || !validIndex(a) || !validIndex(b))
            return;
        Slot& sa = slots_[static_cast<std::size_t>(a)];
        Slot& sb = slots_[static_cast<std::size_t>(b)];
        if (!sa.empty() && !sb.empty() && sa.block == sb.block) {
            const int take = std::min(sa.count, sb.room());
            sb.count += take;
            sa.count -= take;
            if (sa.count <= 0)
                sa.clear();
            return;
        }
        std::swap(sa, sb);
    }

    int Inventory::moveStack(const int from, const int to, const int n) {
        if (from == to || !validIndex(from) || !validIndex(to) || n <= 0)
            return 0;
        Slot& src = slots_[static_cast<std::size_t>(from)];
        Slot& dst = slots_[static_cast<std::size_t>(to)];
        if (src.empty())
            return 0;
        if (!dst.empty() && dst.block != src.block)
            return 0; // caller wants swapSlots() for that case

        if (dst.empty())
            dst.block = src.block;
        const int moved = std::min({n, src.count, dst.room()});
        dst.count += moved;
        src.count -= moved;
        if (src.count <= 0)
            src.clear();
        if (dst.count <= 0)
            dst.clear();
        return moved;
    }

    Slot Inventory::splitStack(const int index) {
        Slot out{};
        if (!validIndex(index))
            return out;
        Slot& s = slots_[static_cast<std::size_t>(index)];
        if (s.empty())
            return out;
        // Odd stacks favour the cursor, matching player expectation.
        const int take = (s.count + 1) / 2;
        out.block = s.block;
        out.count = take;
        s.count -= take;
        if (s.count <= 0)
            s.clear();
        return out;
    }

    void Inventory::clickSlot(const int index, Slot& held, const bool rightClick) {
        if (!validIndex(index))
            return;
        Slot& s = slots_[static_cast<std::size_t>(index)];

        if (rightClick) {
            if (held.empty()) {
                held = splitStack(index);
                return;
            }
            if (s.empty()) {
                s.block = held.block;
                s.count = 1;
                if (--held.count <= 0)
                    held.clear();
                return;
            }
            if (s.block == held.block && s.room() > 0) {
                s.count += 1;
                if (--held.count <= 0)
                    held.clear();
                return;
            }
            std::swap(s, held);
            return;
        }

        if (held.empty()) {
            held = s;
            s.clear();
            return;
        }
        if (s.empty()) {
            s = held;
            held.clear();
            return;
        }
        if (s.block == held.block) {
            const int take = std::min(held.count, s.room());
            s.count += take;
            held.count -= take;
            if (held.count <= 0)
                held.clear();
            return;
        }
        std::swap(s, held);
    }

    // ------------------------------------------------------------- Presets

    void Inventory::giveStarterKit() {
        for (const auto& [block, count] : kStarterKit)
            add(block, count);
    }

    void Inventory::fillCreative_() {
        clear();
        // kPlaceable is authored in creative-menu order, so a straight copy puts
        // the common building blocks on the hotbar and the rest in storage.
        for (std::size_t i = 0; i < kPlaceable.size() && i < slots_.size(); ++i) {
            slots_[i].block = kPlaceable[i];
            slots_[i].count = kMaxStack;
        }
    }

    void Inventory::setCreative(const bool on) {
        if (on == creative_)
            return;
        creative_ = on;
        if (on) {
            survivalBackup_ = slots_; // so toggling the setting is non-destructive
            fillCreative_();
        } else {
            slots_ = survivalBackup_;
        }
        select(selected_);
    }

    void Inventory::syncCreative() {
        setCreative(settings.creative);
    }

} // namespace vox
