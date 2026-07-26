#pragma once
#include <array>
#include <cstddef>

#include "vx_blocks.hpp"

// VOXHAVEN - the player's inventory model.
//
// Deliberately raylib-free and free of global state so it can be reasoned about
// (and unit-tested) on its own. The single outside coupling is syncCreative(),
// which mirrors `vox::settings.creative` into the instance; the scene calls it
// once per frame so a settings-panel toggle takes effect immediately.
namespace vox {

    inline constexpr int kHotbarSlots = 9;
    inline constexpr int kStorageSlots = 27;
    inline constexpr int kInventorySlots = kHotbarSlots + kStorageSlots; // 36
    inline constexpr int kMaxStack = 64;

    // One inventory cell. `count == 0` (or an Air block) means empty - the two
    // are kept in sync by clear(), so callers only ever have to test empty().
    struct Slot {
        Block block = Block::Air;
        int count = 0;

        [[nodiscard]] bool empty() const { return count <= 0 || block == Block::Air; }
        [[nodiscard]] int room() const { return empty() ? kMaxStack : kMaxStack - count; }
        void clear() {
            block = Block::Air;
            count = 0;
        }
    };

    // Slot index layout, flat on purpose so drag-and-drop between the storage
    // grid and the hotbar is a plain index operation:
    //   0  .. 8   hotbar, left to right
    //   9  .. 35  storage, row-major over a 9-wide x 3-tall grid
    class Inventory {
    public:
        Inventory() = default;

        // --- Slot access -------------------------------------------------
        [[nodiscard]] static constexpr bool validIndex(const int i) {
            return i >= 0 && i < kInventorySlots;
        }
        [[nodiscard]] static constexpr bool isHotbarIndex(const int i) {
            return i >= 0 && i < kHotbarSlots;
        }

        // Out-of-range indices resolve to a scratch slot that is cleared first,
        // so UI code can hand us a -1 "no slot" hover without special-casing.
        [[nodiscard]] const Slot& slot(int index) const;
        [[nodiscard]] Slot& slot(int index);
        [[nodiscard]] const std::array<Slot, kInventorySlots>& slots() const { return slots_; }

        // --- Stock management --------------------------------------------
        // Fills matching partial stacks first, then empty slots (hotbar before
        // storage so picked-up blocks land where the player can use them).
        // Adds as much as fits; returns false if anything was left over.
        bool add(Block b, int n = 1);

        // Number of items that could NOT be stored (0 == everything fit).
        [[nodiscard]] int addLeftover(Block b, int n);

        // All-or-nothing: nothing is removed unless the full amount is present.
        // Always succeeds without removing anything in creative mode.
        bool consume(Block b, int n = 1);

        // Same, but restricted to the currently selected hotbar slot.
        bool consumeSelected(int n = 1);

        [[nodiscard]] int countOf(Block b) const;
        [[nodiscard]] bool has(Block b, int n = 1) const { return countOf(b) >= n; }
        void clear();

        // --- Selection ----------------------------------------------------
        void select(int index);                 // clamped to 0..8
        void scrollSelection(int delta);        // wraps; +1 moves right
        [[nodiscard]] int selected() const { return selected_; }
        [[nodiscard]] Block selectedBlock() const { return slots_[static_cast<std::size_t>(selected_)].block; }
        [[nodiscard]] int selectedCount() const { return slots_[static_cast<std::size_t>(selected_)].count; }

        // --- Rearranging (drag & drop) ------------------------------------
        // Merge `from` into `to` when they hold the same block, otherwise swap.
        void swapSlots(int a, int b);

        // Move up to `n` items from one slot to another; returns how many moved.
        int moveStack(int from, int to, int n = kMaxStack);

        // Removes and returns the larger half of a stack (right-click split).
        [[nodiscard]] Slot splitStack(int index);

        // Minecraft-style click semantics against a cursor-held stack. The HUD
        // owns `held` (it lives in UI state, not in the inventory) and just
        // forwards clicks here, which keeps all the item math in one place.
        //   left  : pick up all / drop all / merge / swap
        //   right : pick up half / drop one / swap
        void clickSlot(int index, Slot& held, bool rightClick);

        // --- Presets -------------------------------------------------------
        // A modest survival loadout: tools are not modelled, so this is blocks.
        void giveStarterKit();

        // Mirror vox::settings.creative. Entering creative snapshots the
        // survival contents and fills every slot from kPlaceable; leaving it
        // restores the snapshot, so toggling the setting is non-destructive.
        void syncCreative();
        void setCreative(bool on);
        [[nodiscard]] bool creative() const { return creative_; }

    private:
        void fillCreative_();

        std::array<Slot, kInventorySlots> slots_{};
        std::array<Slot, kInventorySlots> survivalBackup_{};
        int selected_ = 0;
        bool creative_ = false;
        mutable Slot scratch_{}; // returned for out-of-range slot() calls
    };

} // namespace vox
