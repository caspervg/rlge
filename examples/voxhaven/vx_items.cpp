#include "vx_items.hpp"

#include <algorithm>
#include <cmath>

#include "vx_config.hpp"
#include "vx_hud.hpp"

namespace vox {

    namespace {
        // Only entries that differ from the default (a plain block) are listed;
        // everything else falls through to a zeroed ItemInfo.
        struct Entry {
            Block block;
            ItemInfo info;
        };

        constexpr Entry kEntries[] = {
            {Block::Stick,      {true, ToolKind::None, 0, 0, 0, ArmourSlot::Head}},
            {Block::Coal,       {true, ToolKind::None, 0, 0, 0, ArmourSlot::Head}},
            {Block::IronIngot,  {true, ToolKind::None, 0, 0, 0, ArmourSlot::Head}},
            {Block::GoldIngot,  {true, ToolKind::None, 0, 0, 0, ArmourSlot::Head}},

            {Block::WoodSword,  {true, ToolKind::Sword, 1, 4, 0, ArmourSlot::Head}},
            {Block::StoneSword, {true, ToolKind::Sword, 2, 5, 0, ArmourSlot::Head}},
            {Block::IronSword,  {true, ToolKind::Sword, 3, 7, 0, ArmourSlot::Head}},

            {Block::WoodPick,   {true, ToolKind::Pickaxe, 1, 2, 0, ArmourSlot::Head}},
            {Block::StonePick,  {true, ToolKind::Pickaxe, 2, 3, 0, ArmourSlot::Head}},
            {Block::IronPick,   {true, ToolKind::Pickaxe, 3, 4, 0, ArmourSlot::Head}},

            {Block::WoodAxe,    {true, ToolKind::Axe, 1, 3, 0, ArmourSlot::Head}},
            {Block::StoneAxe,   {true, ToolKind::Axe, 2, 4, 0, ArmourSlot::Head}},
            {Block::IronAxe,    {true, ToolKind::Axe, 3, 6, 0, ArmourSlot::Head}},

            {Block::IronHelmet,     {true, ToolKind::Armour, 3, 0, 2, ArmourSlot::Head}},
            {Block::IronChestplate, {true, ToolKind::Armour, 3, 0, 5, ArmourSlot::Chest}},
            {Block::IronBoots,      {true, ToolKind::Armour, 3, 0, 1, ArmourSlot::Feet}},
        };

        const ItemInfo kNotAnItem{};

        std::vector<Recipe> buildRecipes() {
            using B = Block;
            return {
                {"planks",          B::Planks, 4,      {{{B::Wood, 1}}},                         false},
                {"sticks",          B::Stick, 4,       {{{B::Planks, 2}}},                       false},
                {"lantern",         B::Lantern, 2,     {{{B::Planks, 4}, {B::Coal, 1}}},         false},
                {"bricks",          B::Bricks, 4,      {{{B::Clay, 4}}},                         false},

                // "Smelting" needs coal as fuel, which gives coal a purpose.
                {"glass",           B::Glass, 1,       {{{B::Sand, 1}, {B::Coal, 1}}},           true},
                {"iron ingot",      B::IronIngot, 1,   {{{B::IronOre, 1}, {B::Coal, 1}}},        true},
                {"gold ingot",      B::GoldIngot, 1,   {{{B::GoldOre, 1}, {B::Coal, 1}}},        true},

                {"wooden sword",    B::WoodSword, 1,   {{{B::Planks, 2}, {B::Stick, 1}}},        false},
                {"stone sword",     B::StoneSword, 1,  {{{B::Cobble, 2}, {B::Stick, 1}}},        false},
                {"iron sword",      B::IronSword, 1,   {{{B::IronIngot, 2}, {B::Stick, 1}}},     false},

                {"wooden pickaxe",  B::WoodPick, 1,    {{{B::Planks, 3}, {B::Stick, 2}}},        false},
                {"stone pickaxe",   B::StonePick, 1,   {{{B::Cobble, 3}, {B::Stick, 2}}},        false},
                {"iron pickaxe",    B::IronPick, 1,    {{{B::IronIngot, 3}, {B::Stick, 2}}},     false},

                {"wooden axe",      B::WoodAxe, 1,     {{{B::Planks, 3}, {B::Stick, 2}}},        false},
                {"stone axe",       B::StoneAxe, 1,    {{{B::Cobble, 3}, {B::Stick, 2}}},        false},
                {"iron axe",        B::IronAxe, 1,     {{{B::IronIngot, 3}, {B::Stick, 2}}},     false},

                {"iron helmet",     B::IronHelmet, 1,     {{{B::IronIngot, 5}}},                 false},
                {"iron chestplate", B::IronChestplate, 1, {{{B::IronIngot, 8}}},                 false},
                {"iron boots",      B::IronBoots, 1,      {{{B::IronIngot, 4}}},                 false},
            };
        }
    } // namespace

    const ItemInfo& itemInfo(const Block b) {
        for (const Entry& e : kEntries) {
            if (e.block == b)
                return e.info;
        }
        return kNotAnItem;
    }

    bool isItem(const Block b) { return b >= kFirstItem && b < Block::Count; }

    float miningMultiplier(const Block held, const Block target) {
        const ItemInfo& tool = itemInfo(held);
        if (tool.tool == ToolKind::None || tool.tool == ToolKind::Armour)
            return 1.0f;

        const SoundGroup group = blockInfo(target).sound;
        const bool matched =
            (tool.tool == ToolKind::Pickaxe && group == SoundGroup::Stone) ||
            (tool.tool == ToolKind::Axe && group == SoundGroup::Wood);
        if (!matched)
            return 1.0f;

        // Wood 2x, stone 4x, iron 7x on the right material.
        switch (tool.tier) {
        case 1: return 2.0f;
        case 2: return 4.0f;
        default: return 7.0f;
        }
    }

    int attackDamage(const Block held) {
        const ItemInfo& info = itemInfo(held);
        return info.attack > 0 ? info.attack : 2; // bare hands
    }

    const std::vector<Recipe>& recipes() {
        static const std::vector<Recipe> table = buildRecipes();
        return table;
    }

    bool canCraft(const Inventory& inv, const Recipe& r) {
        if (settings.creative)
            return true;
        for (const Ingredient& in : r.inputs) {
            if (in.count <= 0)
                continue;
            if (inv.countOf(in.block) < in.count)
                return false;
        }
        return true;
    }

    bool craft(Inventory& inv, const Recipe& r) {
        if (!canCraft(inv, r))
            return false;
        if (!settings.creative) {
            // Consume only after the whole recipe is known to be affordable, so
            // a failed craft can never eat half the ingredients.
            for (const Ingredient& in : r.inputs) {
                if (in.count > 0)
                    inv.consume(in.block, in.count);
            }
        }
        return inv.add(r.result, r.resultCount);
    }

    // ---------------------------------------------------------- Equipment

    Block Equipment::equip(const Block item) {
        const ItemInfo& info = itemInfo(item);
        if (info.tool != ToolKind::Armour)
            return Block::Air;
        const int idx = static_cast<int>(info.slot);
        const Block displaced = worn_[idx];
        worn_[idx] = item;
        return displaced;
    }

    int Equipment::points() const {
        int total = 0;
        for (const Block b : worn_)
            total += itemInfo(b).armour;
        return total;
    }

    int Equipment::mitigate(const int incoming) const {
        // Each point shaves 5%, capped so armour never makes you invulnerable.
        const float reduction = std::min(0.04f * static_cast<float>(points()), 0.60f);
        const int out = static_cast<int>(std::lround(static_cast<float>(incoming) * (1.0f - reduction)));
        return std::max(incoming > 0 ? 1 : 0, out);
    }

    void Equipment::clear() { worn_.fill(Block::Air); }

    // ------------------------------------------------------------- UI

    namespace {
        constexpr Color kPanel{18, 20, 30, 240};
        constexpr Color kRowIdle{34, 38, 52, 210};
        constexpr Color kRowHot{58, 64, 86, 235};
        constexpr Color kEdge{92, 100, 130, 255};

        void panelRect(const Rectangle r, const Color fill, const Color edge) {
            DrawRectangleRec(r, fill);
            DrawRectangleLinesEx(r, 2.0f, edge);
        }
    } // namespace

    void drawCraftingScreen(const CraftContext& ctx, Inventory& inv, Equipment& gear,
                            CraftUiState& ui) {
        const Rectangle vp = ctx.viewport;
        const float s = hudScale(vp);
        const auto& table = recipes();
        const int count = static_cast<int>(table.size());

        DrawRectangleRec(vp, Fade(BLACK, 0.72f));

        const float rowH = 34.0f * s;
        const float pad = 18.0f * s;
        const float headH = 52.0f * s;
        const int visible = std::max(4, std::min(count,
            static_cast<int>((vp.height * 0.80f - headH - pad) / rowH)));
        const float pw = std::min(560.0f * s, vp.width * 0.94f);
        const float ph = headH + rowH * static_cast<float>(visible) + pad;
        const Rectangle pr{vp.x + (vp.width - pw) * 0.5f, vp.y + (vp.height - ph) * 0.5f, pw, ph};
        panelRect(pr, kPanel, kEdge);

        hudTextShadow({pr.x + pad, pr.y + 14.0f * s}, "CRAFTING", 22.0f * s, pal::hudText);
        {
            const char* hint = "CLICK TO CRAFT     WHEEL TO SCROLL     C OR ESC TO CLOSE";
            const Vector2 e = hudMeasure(hint, 11.0f * s);
            hudText({pr.x + pr.width - e.x - pad, pr.y + 22.0f * s}, hint, 11.0f * s, pal::hudDim);
        }

        // Scroll, clamped so the list can never run past its own end.
        const int maxScroll = std::max(0, count - visible);
        if (std::fabs(ctx.wheel) > 0.1f)
            ui.scroll -= static_cast<int>(ctx.wheel);
        ui.scroll = std::clamp(ui.scroll, 0, maxScroll);

        ui.hovered = -1;
        for (int row = 0; row < visible; ++row) {
            const int i = row + ui.scroll;
            if (i >= count)
                break;
            const Recipe& r = table[static_cast<std::size_t>(i)];
            const Rectangle rr{pr.x + pad, pr.y + headH + rowH * static_cast<float>(row),
                               pr.width - pad * 2.0f, rowH - 4.0f * s};
            const bool hot = CheckCollisionPointRec(ctx.mouse, rr);
            const bool affordable = canCraft(inv, r);
            if (hot)
                ui.hovered = i;

            DrawRectangleRec(rr, hot ? kRowHot : kRowIdle);
            if (!affordable)
                DrawRectangleRec(rr, Fade(BLACK, 0.35f));

            // Result icon, name, and the ingredient list.
            const float icon = rr.height - 6.0f * s;
            drawBlockIcon(ctx.atlas, r.result, {rr.x + 4.0f * s, rr.y + 3.0f * s, icon, icon},
                          affordable ? 1.0f : 0.45f);
            const Color nameCol = affordable ? pal::hudText : pal::hudDim;
            hudText({rr.x + icon + 12.0f * s, rr.y + rr.height * 0.5f - 7.0f * s},
                    TextFormat(r.resultCount > 1 ? "%s x%d" : "%s", r.name, r.resultCount),
                    13.0f * s, nameCol);
            if (r.smelting) {
                hudText({rr.x + icon + 12.0f * s, rr.y + rr.height * 0.5f + 4.0f * s}, "SMELTED",
                        9.0f * s, Fade(pal::hudAccent, 0.8f));
            }

            float ix = rr.x + rr.width - 8.0f * s;
            for (int k = static_cast<int>(r.inputs.size()) - 1; k >= 0; --k) {
                const Ingredient& in = r.inputs[static_cast<std::size_t>(k)];
                if (in.count <= 0)
                    continue;
                const bool have = settings.creative || inv.countOf(in.block) >= in.count;
                const auto label = TextFormat("%d", in.count);
                const Vector2 le = hudMeasure(label, 11.0f * s);
                ix -= le.x;
                hudText({ix, rr.y + rr.height * 0.5f - 5.0f * s}, label, 11.0f * s,
                        have ? pal::hudText : Color{224, 110, 110, 255});
                ix -= icon + 3.0f * s;
                drawBlockIcon(ctx.atlas, in.block, {ix, rr.y + 3.0f * s, icon, icon},
                              have ? 1.0f : 0.4f);
                ix -= 10.0f * s;
            }

            if (hot && ctx.mousePressed && affordable) {
                if (craft(inv, r)) {
                    // Armour goes straight on: there is no equipment grid, and a
                    // freshly forged helmet sitting unused in a slot is a trap.
                    if (itemInfo(r.result).tool == ToolKind::Armour) {
                        gear.equip(r.result);
                        inv.consume(r.result, 1);
                    }
                }
            }
        }

        // Scroll indicator.
        if (maxScroll > 0) {
            const float trackH = rowH * static_cast<float>(visible);
            const Rectangle track{pr.x + pr.width - 6.0f * s, pr.y + headH, 3.0f * s, trackH};
            DrawRectangleRec(track, Fade(BLACK, 0.4f));
            const float frac = static_cast<float>(visible) / static_cast<float>(count);
            const float y = trackH * (static_cast<float>(ui.scroll) / static_cast<float>(count));
            DrawRectangleRec({track.x, track.y + y, track.width, trackH * frac},
                             Fade(pal::hudAccent, 0.8f));
        }
    }

    void drawArmourStrip(const Rectangle viewport, const Texture2D atlas, const Equipment& gear,
                         const float scale) {
        const float icon = 22.0f * scale;
        const float x = viewport.x + 24.0f * scale;
        const float y = viewport.y + viewport.height - 118.0f * scale;
        for (int i = 0; i < kArmourSlots; ++i) {
            const Rectangle r{x + static_cast<float>(i) * (icon + 4.0f * scale), y, icon, icon};
            DrawRectangleRec(r, Fade(BLACK, 0.42f));
            DrawRectangleLinesEx(r, 1.0f, Fade(WHITE, 0.18f));
            const Block worn = gear.worn(static_cast<ArmourSlot>(i));
            if (worn != Block::Air)
                drawBlockIcon(atlas, worn, r, 1.0f);
        }
        if (const int pts = gear.points(); pts > 0) {
            hudText({x + static_cast<float>(kArmourSlots) * (icon + 4.0f * scale) + 6.0f * scale,
                     y + icon * 0.3f},
                    TextFormat("%d", pts), 12.0f * scale, pal::hudText);
        }
    }

} // namespace vox
