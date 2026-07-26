#pragma once
#include <cstdint>
#include <vector>

#include "raylib.h"

#include "vx_world.hpp"

namespace vox {

    enum class MobKind : std::uint8_t {
        // Passive
        Pig,
        Sheep,
        Cow,
        Chicken,
        // Hostile
        Zombie,
        Skeleton,
        Creeper,
        Spider,
        Count
    };

    constexpr int kMobKindCount = static_cast<int>(MobKind::Count);

    struct MobStats {
        const char* name;
        bool hostile;
        float width;        // AABB footprint (blocks)
        float height;
        float speed;        // blocks/second when moving
        int maxHealth;
        int damage;         // per hit against the player
        float attackRange;  // centre distance at which it can hit
        float senseRange;   // distance at which a hostile notices the player
        bool burnsInSun;    // undead scorch in daylight
    };

    const MobStats& mobStats(MobKind kind);

    // Representative colour, used for death particles and HUD accents.
    Color mobColor(MobKind kind);

    // A live creature. Simple AABB physics against the voxel world, a small
    // behaviour state machine, and an animation phase driving the limb swing.
    struct Mob {
        MobKind kind = MobKind::Pig;
        Vector3 pos{};        // feet centre
        Vector3 vel{};
        float yaw = 0.0f;     // radians, facing
        float health = 10.0f;
        bool onGround = false;
        bool dead = false;

        // Behaviour
        float wanderTimer = 0.0f;
        float targetYaw = 0.0f;
        bool moving = false;
        float attackCooldown = 0.0f;
        float hurtFlash = 0.0f;   // white flash after taking damage
        float burnTimer = 0.0f;
        float burning = 0.0f;      // >0 while scorching in daylight
        float knockback = 0.0f;

        // Animation
        float limbPhase = 0.0f;
        float limbSwing = 0.0f;   // 0..1 amount
        float deathTimer = 0.0f;  // >0 while toppling over

        [[nodiscard]] Vector3 centre() const;
        [[nodiscard]] BoundingBox bounds() const;
    };

    // Owns every creature in the world: spawning, simulation, rendering.
    class MobManager {
    public:
        void init(std::uint32_t seed);   // builds the texture atlas (needs a GL context)
        void shutdown();

        // dayLight 0..1 drives sun-burning and which mobs may spawn.
        void update(World& world, Vector3 playerPos, float dayFraction, float dt);

        // Immediate-mode 3D draw; call from inside a submit3D callback.
        void draw(const Camera3D& cam, float dayFactor) const;

        // Player swings at whatever they are looking at. Returns true on a hit.
        bool attack(Vector3 eye, Vector3 dir, float reach, int damage);

        // Damage the manager wants to deal to the player this frame, then cleared.
        [[nodiscard]] int takePlayerDamage() { const int d = playerDamage_; playerDamage_ = 0; return d; }
        // Direction of the last hit, for knockback.
        [[nodiscard]] Vector3 lastHitDir() const { return lastHitDir_; }

        [[nodiscard]] const std::vector<Mob>& mobs() const { return mobs_; }
        [[nodiscard]] int passiveCount() const;
        [[nodiscard]] int hostileCount() const;
        [[nodiscard]] Texture2D atlas() const { return atlas_; }

        // Feedback for the scene: a mob died at this spot this frame.
        struct Death {
            Vector3 pos;
            MobKind kind;
        };
        std::vector<Death> deaths;

        void clear() { mobs_.clear(); }

        // Drops one of every kind in a ring around a point. Creative-mode dev
        // tool: it makes the whole bestiary inspectable without waiting on the
        // spawn cycle.
        void spawnSampler(const World& world, Vector3 centre, float facingYaw);

    private:
        void simulate_(World& world, Mob& mob, Vector3 playerPos, float dayFactor, float dt);
        void moveAxis_(const World& world, Mob& mob, int axis, float amount) const;
        [[nodiscard]] bool collides_(const World& world, const Mob& mob, Vector3 at) const;
        void trySpawn_(World& world, Vector3 playerPos, float dayFactor);

        std::vector<Mob> mobs_;
        Texture2D atlas_{};
        std::uint32_t seed_ = 1;
        float spawnTimer_ = 0.0f;
        int playerDamage_ = 0;
        Vector3 lastHitDir_{0.0f, 0.0f, 0.0f};
    };

} // namespace vox
