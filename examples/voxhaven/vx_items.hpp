#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "raylib.h"

#include "vx_blocks.hpp"
#include "vx_inventory.hpp"

namespace vox {

    enum class ToolKind : std::uint8_t {
        None,
        Sword,
        Pickaxe,
        Axe,
        Armour,
    };

    enum class ArmourSlot : std::uint8_t {
        Head,
        Chest,
        Feet,
        Count,
    };

    constexpr int kArmourSlots = static_cast<int>(ArmourSlot::Count);

    struct ItemInfo {
        bool isItem = false;      // carried, never placed in the world
        ToolKind tool = ToolKind::None;
        int tier = 0;             // 1 wood, 2 stone, 3 iron
        int attack = 0;           // damage dealt to mobs
        int armour = 0;           // protection points when worn
        ArmourSlot slot = ArmourSlot::Head;
    };

    const ItemInfo& itemInfo(Block b);
    [[nodiscard]] bool isItem(Block b);

    // How much faster this held item breaks the given block. Matching the tool
    // to the material is the whole point of upgrading, so a mismatch is 1x.
    [[nodiscard]] float miningMultiplier(Block held, Block target);

    // Damage a held item deals; bare hands are the fallback.
    [[nodiscard]] int attackDamage(Block held);

    // --- Crafting ---------------------------------------------------------

    struct Ingredient {
        Block block = Block::Air;
        int count = 0;
    };

    struct Recipe {
        const char* name;
        Block result;
        int resultCount;
        std::array<Ingredient, 3> inputs;  // unused entries have count 0
        bool smelting;                     // shown under a separate heading
    };

    [[nodiscard]] const std::vector<Recipe>& recipes();
    [[nodiscard]] bool canCraft(const Inventory& inv, const Recipe& r);
    bool craft(Inventory& inv, const Recipe& r);

    // --- Worn armour ------------------------------------------------------

    class Equipment {
    public:
        // Equips the item, returning whatever was displaced (Air if nothing).
        Block equip(Block item);
        [[nodiscard]] Block worn(ArmourSlot s) const { return worn_[static_cast<int>(s)]; }
        [[nodiscard]] int points() const;                 // 0..8ish
        [[nodiscard]] int mitigate(int incoming) const;    // damage after armour
        void clear();

    private:
        std::array<Block, kArmourSlots> worn_{Block::Air, Block::Air, Block::Air};
    };

    // --- Crafting screen --------------------------------------------------

    struct CraftUiState {
        int hovered = -1;
        int scroll = 0;
        float openTime = -100.0f;
        void open(const float now) { openTime = now; hovered = -1; }
    };

    struct CraftContext {
        Rectangle viewport{};
        Texture2D atlas{};
        float time = 0.0f;
        Vector2 mouse{};
        bool mousePressed = false;
        float wheel = 0.0f;
    };

    // Draws the recipe book and applies clicks directly to the inventory.
    void drawCraftingScreen(const CraftContext& ctx, Inventory& inv, Equipment& gear,
                            CraftUiState& ui);

    // Small worn-armour readout for the HUD.
    void drawArmourStrip(Rectangle viewport, Texture2D atlas, const Equipment& gear, float scale);

} // namespace vox
