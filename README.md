# RLGE – Raylib Lightweight Game Engine

RLGE is a small C++23 game engine built on top of _raylib_, _ImGui_, and _rlImGui_. It focuses on straightforward 2D games with scenes, entities, components, an event bus, and a simple rendering queue.

This repository also contains example games/demos:

- `examples/basic_game` – a minimal “move the sprite” scene.
- `examples/snake` – a more complete Snake game showcasing scenes, events, and UI.
- `examples/particles` - a tech demo to show the particle emission system.

---

## Features

- Scene stack with enter/exit/pause/resume lifecycle.
- Entity/component model (`Scene`, `Entity`, `Component`, `Transform`, sprites, tilemaps, etc.).
- Event bus (`EventBus`) with publish/subscribe and queued events.
- Camera system that can follow entities.
- Input binding system mapping named actions to keys.
- Asset store for textures.
- Tween system, collision system hooks, and a particle emitter.
- Optional debug overlays via ImGui.

## Requirements

- CMake ≥ 3.25
- A C++23-capable compiler
- Git (for fetching dependencies)

All third‑party libraries (raylib, ImGui, rlImGui) are fetched automatically by CMake via `FetchContent`.

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

On Windows, they will be under `build/` or a generator‑specific subdirectory (e.g. `build/Debug`).

## Running the examples

### Basic Game

The “basic game” example shows a moving sprite, a background, and an ImGui debug overlay.

- Executable: `rlge_basic_game`
- Input:
  - `A` / `D` – move left/right
  - `W` – rotate the sprite

### Snake Game

The Snake example demonstrates a more complete setup with game logic, audio, the event bus, two scenes, and a game‑over flow.

- Executable: `rlge_snake`
- Input:
  - `W` / `A` / `S` / `D` – move the snake (left/right/up/down)
  - `Enter` – restart from the Game Over screen


### Particles Demo

The particles example demonstrates a configurable particle emitter with pluggable spawn functions and render callbacks, plus a live ImGui debug UI to play with the particle parameters.

- Executable: `rlge_particles`
- Input:
  - Move the mouse to move the main emitter in world space.
  - Press `F1` to toggle the ImGui debug overlay and tweak emitter parameters.

## Using RLGE in your own game (overview)

At a high level, to build a new game on RLGE:

1. Create a `Runtime` instance with your desired window size and title:
   ```cpp
   rlge::Runtime runtime(width, height, 60, "My Game");
   ```
2. Bind input actions:
   ```cpp
   runtime.input().bind("left", KEY_A);
   runtime.input().bind("right", KEY_D);
   ```
3. Implement a `Scene` subclass for your game logic:
   ```cpp
   class MyScene : public rlge::Scene {
   public:
       explicit MyScene(rlge::Runtime& r) : Scene(r) {}
       void enter() override {
           // load assets, spawn entities
       }
       void update(float dt) override {
           Scene::update(dt);
           // game logic
       }
   };
   ```
4. Push your scene and run the runtime:
   ```cpp
   runtime.pushScene<MyScene>();
   runtime.run();
   ```

Look at `examples/basic_game/main.cpp`, `examples/snake`, and `examples/particles/main.cpp` for concrete patterns.

## Attributions

### Libraries

- [raylib](https://www.raylib.com/) – windowing, input, rendering (zlib License)
- [Dear ImGui](https://github.com/ocornut/imgui) – immediate‑mode GUI (MIT License)
- [rlImGui](https://github.com/raylib-extras/rlImGui) – ImGui integration for raylib (zlib License)

### Assets

- Basic game sprites/background: Generated myself with a very basic Python script.
- Snake sprites: _[Snake Game Assets](https://cosme.itch.io/snake)_ by Cosme, from itch.io ([CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/)).
- Snake sound effects: Generated myself with a very basic Python script.
- Snake background music: _[Snake around the Sun](https://freemusicarchive.org/music/crowander/circles/snake-around-the-sun/)_ by Crowander, from the Free Music Archive ([CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/)).
