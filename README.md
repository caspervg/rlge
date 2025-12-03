# RLGE - Raylib Lightweight Game Engine

RLGE is a small C++23 2D game engine built on top of raylib, ImGui, and rlImGui. It provides a scene stack, entity/component model, event buses, a batched renderer, a collision system, and helper entities for getting simple games on screen quickly.

This repository also contains example games/demos:

- `examples/basic_game`: minimal moving-sprite scene with a stats overlay.
- `examples/snake`: small game showing scenes, audio, and event flow.
- `examples/particles`: configurable CPU particle emitters with live tuning.
- `examples/breakout`: paddle/brick demo using the physics body + collision events.
- `examples/tilemap`: orthogonal Tiled map rendering with flip flag support.
- `examples/multiview`: split-screen + minimap rendering via multiple cameras.
- `examples/collision_debug`: collider shapes, layer masks, and debug overlays.
- `examples/shader_demo`: layer and per-entity shaders with live ImGui controls.
- `examples/lighting_demo`: normal-mapped sprites lit by multiple point lights, with a movable picture-in-picture view.

---

## Features

- Scene stack with enter/exit/pause/resume; per-scene tween + collision systems and local event bus.
- Entity/component model with helpers (`Transform`, sprites/animations, sprite sheets, tilemaps, particles, etc.).
- Two event buses: scene-local and shared game-wide, with queued dispatch and forwarding helpers.
- Camera system (follow/pan/zoom/rotate, screen <-> world helpers, view bounds) plus multi-view rendering.
- Dynamic render layers with optional shaders and typed uniform bindings; per-entity shader effects for custom visuals.
- 2D lighting pipeline (`LitScene`/`LitSprite`) with ambient + multiple point lights, normal-map support, and view-aware light data.
- Type-safe Input system with support for keyboard, mouse, gamepad, and analog axes.
- Batched render queue with layers (`Background`, `World`, `Foreground`, `UI`), z-sorting, per-view culling, and render stats.
- Collision system with layers/masks, triggers vs solids vs kinematic colliders, AABB/OBB/circle/polygon shapes, and optional ImGui debug overlay.
- Lightweight PhysicsBody component for gravity/forces/bounce with ground detection and collision response.
- Asset store for textures/shaders (file or in-memory), prefab factory for named entity constructors, and an audio manager for sounds/music.
- Particle emitter component with configurable spawn/render functions and helper spawn shapes.
- Tilemap support using Tiled/JSON (via Tileson) with proper source-rect handling and per-tile flip flags.
- Retained-mode UI stack (panels, stacks, labels, buttons) with layout, text binding helpers, hit testing, and click callbacks for HUDs/menus.
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
- `rlge_breakout`
- `rlge_shader_demo`
- `rlge_lighting_demo`

On Windows, they will be under `build/` or a generator-specific subdirectory (for example `build/Debug`).

## Running the examples

Each executable runs a focused scenario; use `F1` to toggle the ImGui overlay in scenes that provide one.

- `rlge_basic_game`: moving sprite with camera follow and render stats.
- `rlge_snake`: two-scene flow, audio, and global game events.
- `rlge_particles`: two emitters with live tuning for spawn/rates/colors.
- `rlge_breakout`: physics-backed paddle/brick collisions, game over flow, and global events.
- `rlge_tilemap`: loads a Tiled map and renders it via the batched renderer.
- `rlge_multiview`: two independent world views plus a static minimap.
- `rlge_collision_debug`: move a collider through several shapes; enable collider drawing in the "Collisions" window.
- `rlge_shader_demo`: layer-level wave shader + per-entity flash shader with ImGui sliders.
- `rlge_lighting_demo`: point/torch/mouse-follow lights over normal-mapped sprites; resize the PIP viewport with WASD and pan its camera with arrows.

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

// Type-safe input bindings (keyboard, mouse, gamepad supported)
runtime.input().bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
runtime.input().bind(rlge::Action::MoveRight, rlge::KeyCode::D);
runtime.input().bind(rlge::Action::Jump, rlge::KeyCode::Space);

// Mouse and gamepad bindings
runtime.input().bindMouse(rlge::Action::Fire, rlge::MouseButton::Left);
runtime.input().bindGamepad(rlge::Action::Jump, 0, rlge::GamepadButton::A);

runtime.pushScene<MyScene>();
runtime.run();
```

### Input system

The Input class is template-based, allowing custom action enums:

```cpp
// Use default actions
runtime.input().bind(rlge::Action::MoveLeft, rlge::KeyCode::A);
if (input.down(rlge::Action::MoveLeft)) { ... }

// Or define your own
enum class MyAction { Shoot, Reload, Sprint };
rlge::Input<MyAction> customInput;
customInput.bind(MyAction::Shoot, rlge::KeyCode::Space);
```

Query input with `down()` (held), `pressed()` (just pressed), or `released()` (just released). Supports keyboard, mouse (`bindMouse`, `mouseDown`, `mousePressed`, `mousePosition`), gamepad (`bindGamepad`, `gamepadDown`, `gamepadPressed`), and analog axes (`bindAxis`, `axisValue`, `setAxisDeadZone`).

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

### Lighting

- Derive from `LitScene` to render the world into a target, accumulate lights, and combine lighting before drawing UI/debug overlays.
- Add lights via `lighting().addPointLight(position, radius, color, intensity)` and set ambient with `lighting().setAmbient(color)`.
- Use `LitSprite(diffuse, normalMap, w, h, lighting)` to shade sprites with per-pixel normals; light data is converted per active view.
- Override `drawUnlit()` in a `LitScene` to draw content that should bypass lighting (HUDs, outlines, debug helpers).

### Shaders (layers and entities)

- Load shaders through the asset store from disk or memory: `assets().loadShader("wave", "wave.vert", "wave.frag")` or `loadShaderFromMemory`.
- Apply a shader to a render layer and bind typed uniforms:
  ```cpp
  struct WaveParams { float time{0.0f}; float amplitude{0.02f}; };

  auto& shader = assets().loadFragmentShader("wave", "wave.frag");
  ShaderParams<WaveParams> params(shader);
  params.bind("u_time", &WaveParams::time)
        .bind("u_amplitude", &WaveParams::amplitude);

  auto waterLayer = layers().create("Water", 5 /*sort*/, true);
  layers().setShaderParams(waterLayer, std::move(params));

  // Later in update
  if (auto layer = layers().get(waterLayer)) {
      if (auto* wrapper = dynamic_cast<ShaderParamsWrapper<WaveParams>*>(layer->get().shaderParams.get())) {
          wrapper->get().params().time += dt;
      }
  }
  ```
- Add per-entity effects with `ShaderEffect<T>`; batching is bypassed so custom visuals are isolated:
  ```cpp
  static const char* kFlashFrag = "...";
  auto& flash = assets().loadFragmentShader("flash", kFlashFrag);
  auto& e = spawn<MyEntity>();
  e.add<ShaderEffect<FlashParams>>(flash)
      .bind("u_intensity", &FlashParams::intensity)
      .bind("u_flashColor", &FlashParams::flashColor);
  e.get<ShaderEffect<FlashParams>>()->params().intensity = 0.8f;
  ```
- Layer shaders and per-entity shaders can be used together; per-entity shaders wrap only that draw call while layer shaders affect everything on the layer.

### Scene transitions

- For instant swaps, call `runtime.pushScene<NewScene>()` and `runtime.popScene()`.
- For smooth transitions, use `runtime.transitionTo<NextScene>(std::make_unique<FadeTransition>(0.35f));`. The runtime handles the out/in phases.
- Built-ins: `FadeTransition(duration, color)` and `SlideTransition(duration, direction)`. Draws are submitted to the UI layer so they cover the frame.
- Create custom transitions by deriving from `Transition`, overriding `draw(RenderQueue&, screenW, screenH)`, and pushing your own shapes or effects. Set the phase with `setPhase(TransitionPhase::Out/In)` if you need different visuals per half.

### Events, assets, audio, and prefabs

- Scene-local events: `sceneEvents().publish/subscribe/enqueue`.
- Game-wide events: `runtime.services().gameEvents()`; scenes can forward specific types with `forwardGameEvent<T>()`.
- Assets: `assets().loadTexture(id, path)` and `assets().texture(id)`.
- Audio: `audio().loadSound/playSound`, `audio().loadMusic/playMusic/stopMusic`, call `audio().update()` (runtime does this).
- Prefabs: register entity constructors once (`runtime.services().prefabs().registerPrefab("enemy", fn)`) and instantiate by name.

### UI overlays and HUD

- Each scene owns a retained-mode UI tree (`ui().root()`), laid out in screen space and wired to input processing.
- Compose overlays with panels, vertical/horizontal stacks, labels, spacers, and buttons; configure sizing, padding, spacing, alignment, and distribution.
- Bind dynamic text with `ui::bind` and handle clicks via callbacks or `ui().wasClicked(id)`.
- Example:
  ```cpp
  auto& root = ui().root();
  root.clearChildren();
  auto& hud = root.addChild<ui::Panel>(ui::LayoutConfig{
      .size = {200, 60}, .padding = {12, 10}, .anchor = {0.0f, 0.0f}});
  hud.id("hud");
  hud.addChild<ui::Label>(
      ui::bind([score] { return std::format("Score: {}", score); }),
      ui::LayoutConfig{},
      ui::LabelStyle{.color = {255, 255, 255, 255}, .fontSize = 22});
  hud.addChild<ui::Button>("Reset", ui::LayoutConfig{.size = {80, 32}})
      .id("reset_btn")
      .onClick([this] { resetGame(); });
  ```

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
- [cpptoml](https://github.com/skystrife/cpptoml) - parsing TOML config files for the Breakout example (MIT License)

### Assets

- Basic game sprites/background: Generated myself with a very basic Python script.
- Snake sprites: _[Snake Game Assets](https://cosme.itch.io/snake)_ by Cosme, from itch.io ([CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)).
- Snake sound effects: Generated myself with a very basic Python script.
- Snake background music: _[Snake around the Sun](https://freemusicarchive.org/music/crowander/circles/snake-around-the-sun/)_ by Crowander, from the Free Music Archive ([CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/)).
- Snake UI font: _[Falling Sky Black](https://www.fontspace.com/falling-sky-font-f22358)_ by KineticPlasma Fonts, from FontSpace.com ([SIL Open Font License](https://openfontlicense.org/open-font-license-official-text/)).
