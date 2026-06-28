# R3DO — Interactive 3D Ray Tracer

A real-time CPU ray tracer rendered into an X11 window. Navigate a 10×10×10 grid of abstract objects (spheres, boxes) using keyboard and mouse controls.

## Controls

| Key | Action |
|-----|--------|
| W/S | Move forward/backward |
| A/D | Strafe left/right |
| Q/E | Move up/down |
| ← → ↑ ↓ | Look around |
| Space | Full-quality re-render (4 samples/px) |
| Esc | Quit |
| F11 | Toggle windowed mode |
| **Mouse** | Click side menu buttons |
| **Left Click (scene)** | Remove object under crosshair |
| **Left Drag (scene)** | Look around (mouse look) |

The program starts at the resolution saved in `settings.cfg` (or 1920×1080 by default). Press F11 to toggle fullscreen. Use the **Settings** button on the start screen to change the resolution.

## Interactive Menu

A side panel on the right provides:

- **Add Sphere / Box / Cylinder / Cone** — places the chosen shape 3 units ahead in the first empty grid cell
- **Clear All** — removes all objects from the grid
- Object counter and camera position display

New objects cycle through a 10-color palette.

## Crosshair

A green `+` at screen center lets you target objects. Click anywhere in the 3D view (left of the menu panel) to delete the object under the crosshair. The crosshair persists across all renders and movements.

## Start Screen

On launch you'll see a start screen with **New Scene** (empty grid) and **Load Scene** (pick from existing saves). The default autosave (`saves/default.r3do`) is no longer loaded automatically — use Load to pick any save or New to start fresh.

## Save System

All scene changes are automatically saved to `saves/default.r3do` after every add, remove, or clear operation.

To manually snapshot a scene, click **Save** in the side panel and type a name in the terminal. The file is written to `saves/<name>.r3do`. On the start screen, **Load Scene** lists all `.r3do` files found in the `saves/` directory.

## Build

Requires libx11-dev.

```sh
g++ -std=c++11 -O3 main.cpp display.cpp ui.cpp saver.cpp render.cpp startscreen.cpp settings.cpp -lX11 -o renderer
./renderer
```

## How it works

- **DDA traversal**: rays step through the grid using a 3D Digital Differential Analyzer, visiting only the cells along the ray path (~6–12 per ray) instead of checking all 1000 cells.
- **Primitives**: spheres (`sphere.hpp`) and axis-aligned boxes (`box.hpp`) placed in grid cells — 10 spheres and 3 boxes by default.
- **Lighting**: simple directional diffuse shading with a single light source, gamma-corrected (sqrt) output.
- **Multi-sampling**: low quality (1 sample/px) during movement for speed; full quality (4 samples/px) on Space.

## Files

| File | Role |
|------|------|
| `main.cpp` | Entry point, game loop, event handling |
| `render.hpp` / `render.cpp` | Ray tracing (ray_color, render_scene, hit_center) |
| `startscreen.hpp` / `startscreen.cpp` | Start screen, load screen, save listing, settings screen |
| `settings.hpp` / `settings.cpp` | Resolution settings, load/save from `settings.cfg` |
| `camera.hpp` | Camera with WASD movement and yaw/pitch rotation |
| `display.hpp` / `display.cpp` | X11 window wrapper (keyboard, mouse, pixels, text) |
| `v3.hpp` | 3D vector math (Vec3, operators, cross, dot, unit) |
| `ray.hpp` | Ray class (origin, direction, at(t)) |
| `hittable.hpp` | Abstract Hittable interface + HitRecord |
| `sphere.hpp` | Sphere primitive (quadratic intersection) |
| `box.hpp` | Axis-aligned box primitive (slab method) |
| `cylinder.hpp` | Y-aligned capped cylinder |
| `cone.hpp` | Y-aligned capped cone |
| `grid.hpp` | 10×10×10 grid with DDA ray traversal |
| `ui.hpp` / `ui.cpp` | Side-panel menu (add sphere/box, clear) |
| `saver.hpp` / `saver.cpp` | Save/load scene to .r3do files |
