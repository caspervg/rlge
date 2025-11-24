# RLGE - Raylib Lightweight Game Engine

RLGE is a small C++23 2D game engine built on top of raylib, ImGui, and rlImGui. It provides a scene stack, entity/component model, event buses, a batched renderer, a collision system, and helper entities for getting simple games on screen quickly.

This repository also contains example games/demos:

- `examples/basic_game`: minimal moving-sprite scene with a stats overlay.
- `examples/snake`: small game showing scenes, audio, and event flow.
- `examples/particles`: configurable CPU particle emitters with live tuning.
- `examples/tilemap`: orthogonal Tiled map rendering with flip flag support.
- `examples/multiview`: split-screen + minimap rendering via multiple cameras.
- `examples/collision_debug`: collider shapes, layer masks, and debug overlays.

---

## Features

- Scene stack with enter/exit/pause/resume; per-scene tween + collision systems and local event bus.
- Entity/component model with helpers (`Transform`, sprites/animations, sprite sheets, tilemaps, particles, etc.).
- Two event buses: scene-local and shared game-wide, with queued dispatch and forwarding helpers.
- Camera system (follow/pan/zoom/rotate, screen <-> world helpers, view bounds) plus multi-view rendering.
- **Type-safe Input system** with support for keyboard, mouse, gamepad, and analog axes (see [INPUT_API_EXAMPLES.md](INPUT_API_EXAMPLES.md)).
- Batched render queue with layers (`Background`, `World`, `Foreground`, `UI`), z-sorting, per-view culling, and render stats.
- Collision system with layers/masks, triggers vs solids vs kinematic colliders, AABB/OBB/circle/polygon shapes, and optional ImGui debug overlay.
- Asset store for textures, prefab factory for named entity constructors, and an audio manager for sounds/music.
- Particle emitter component with configurable spawn/render functions and helper spawn shapes.
- Tilemap support using Tiled/JSON (via Tileson) with proper source-rect handling and per-tile flip flags.
- Optional debug overlays via ImGui (toggle with `F1` in the examples).

## Requirements

- CMake >= 3.25
- A C++23-capable compiler
- Git (for fetching dependencies)

All third-party libraries (raylib, ImGui, rlImGui) are fetched automatically by CMake via `FetchContent`.

## Building

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

This will produce the following executables:

- `rlge_basic_game`
- `rlge_particles`
- `rlge_snake`
- `rlge_tilemap`
- `rlge_multiview`
- `rlge_collision_debug`

On Windows, they will be under `build/` or a generator-specific subdirectory (for example `build/Debug`).

## Running the examples

Each executable runs a focused scenario; use `F1` to toggle the ImGui overlay in scenes that provide one.

- `rlge_basic_game`: moving sprite with camera follow and render stats.
- `rlge_snake`: two-scene flow, audio, and global game events.
- `rlge_particles`: two emitters with live tuning for spawn/rates/colors.
- `rlge_tilemap`: loads a Tiled map and renders it via the batched renderer.
- `rlge_multiview`: two independent world views plus a static minimap.
- `rlge_collision_debug`: move a collider through several shapes; enable collider drawing in the "Collisions" window.

## Using RLGE in your own game

### Bootstrapping a runtime

```cpp
rlge::WindowConfig cfg{
    .width = 960,
    .height = 540,
    .fps = 60,
    .title = "My Game"
};
rlge::Runtime runtime(cfg);

// Type-safe input bindings
runtime.input().bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
runtime.input().bind(rlge::Action::MoveRight, rlge::KeyCode::D);

runtime.pushScene<MyScene>();
runtime.run();
```

For more input examples including mouse, gamepad, and axis support, see [INPUT_API_EXAMPLES.md](INPUT_API_EXAMPLES.md).

### Scenes, views, and entities

- Derive from `Scene`, override lifecycle methods, and call `Scene::update(dt)` so tweens/entities/collisions run.
- Use `setSingleView(camera)` for a full-screen camera or `runtime().addView(...)` to build split-screen/minimap layouts.
- Spawn entities with `spawn<T>()`; add components like `Transform`, `Sprite`, `SpriteAnim`, `SheetSprite`, `ParticleEmitter`, or colliders to them.
- Scenes own local event buses (`sceneEvents()`), tween and collision systems, and auto-cleaned view handles.

```cpp
class MyScene : public rlge::Scene {
public:
    explicit MyScene(rlge::Runtime& r) : Scene(r) {}

    void enter() override {
        camera_ = rlge::Camera();
        setSingleView(camera_);

        auto& tex = assets().loadTexture("player", "player.png");
        auto& player = spawn<PlayerEntity>(tex);
        player.get<rlge::Transform>()->position = {100.0f, 200.0f};

        // Forward a global event into the scene-local bus if needed.
        forwardGameEvent<MyEvent>();
    }

private:
    rlge::Camera camera_;
};
```

### Rendering

- Submit sprites via `RenderQueue::submitSprite(layer, z, texture, src, dest, origin, rotation, tint)`.
- Use layers to control draw order and z to sort within a layer; world layers are flushed per view, UI once per frame.
- `RenderEntity` is a convenience base that exposes `rq()`, `assets()`, `input()`, `audio()`, and `events()`.
- Render stats (`rq().stats()`) are handy for debug overlays.

### Events, assets, audio, and prefabs

- Scene-local events: `sceneEvents().publish/subscribe/enqueue`.
- Game-wide events: `runtime.services().gameEvents()`; scenes can forward specific types with `forwardGameEvent<T>()`.
- Assets: `assets().loadTexture(id, path)` and `assets().texture(id)`.
- Audio: `audio().loadSound/playSound`, `audio().loadMusic/playMusic/stopMusic`, call `audio().update()` (runtime does this).
- Prefabs: register entity constructors once (`runtime.services().prefabs().registerPrefab("enemy", fn)`) and instantiate by name.

### Collisions

- Access the per-scene system via `scene().collisions()`. Add colliders to entities:
  ```cpp
  add<rlge::BoxCollider>(scene().collisions(),
                         rlge::ColliderType::Solid,
                         rlge::ColliderLayerMask::LAYER_PLAYER,
                         rlge::ColliderLayerMask::LAYER_WORLD,
                         Rectangle{-8, -8, 16, 16},
                         false /*trigger*/);
  ```
- Shapes: axis-aligned boxes, oriented boxes, circles, polygons. Configure trigger/solid/kinematic types and layer masks.
- Register callbacks with `setOnCollision` for game logic; resolution is handled for non-trigger solids/kinematics.
- Toggle the ImGui "Collisions" window (F1) to draw collider shapes/AABBs.

### Particles, tweens, and tilemaps

- `ParticleEmitter` (component) lets you supply spawn and render callbacks; helpers like `spawnInBox`/`spawnOnLine` build common patterns.
- `tweens().add(Tween(duration, applyFn, easeFn))` for simple time-based animations (`easeLinear`, `easeOutQuad` included).
- Load Tiled maps with `Tilemap::loadTMX(scene, texture, "map.tmj", "LayerName")`; flip flags and spacing/margins are handled automatically.

Look at the examples for small, focused patterns that combine these systems.

## Attributions

### Libraries

- [raylib](https://www.raylib.com/) - windowing, input, rendering (zlib License)
- [Dear ImGui](https://github.com/ocornut/imgui) - immediate-mode GUI (MIT License)
- [rlImGui](https://github.com/raylib-extras/rlImGui) - ImGui integration for raylib (zlib License)

### Assets

- Basic game sprites/background: Generated myself with a very basic Python script.
- Snake sprites: _[Snake Game Assets](https://cosme.itch.io/snake)_ by Cosme, from itch.io ([CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)).
- Snake sound effects: Generated myself with a very basic Python script.
- Snake background music: _[Snake around the Sun](https://freemusicarchive.org/music/crowander/circles/snake-around-the-sun/)_ by Crowander, from the Free Music Archive ([CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/)).
