# R3DO — Interactive 3D Ray Tracer

A real-time CPU ray tracer rendered into an X11 window. Navigate a 10×10×10 grid of abstract objects (spheres, boxes) using keyboard controls.

## Controls

| Key | Action |
|-----|--------|
| W/S | Move forward/backward |
| A/D | Strafe left/right |
| Q/E | Move up/down |
| ← → ↑ ↓ | Look around |
| Space | Teleport to origin |
| Esc | Quit |

## Build

Requires libx11-dev.

```sh
g++ -std=c++11 -O3 main.cpp display.cpp -lX11 -o renderer
./renderer
```

## How it works

- **DDA traversal**: rays step through the grid using a 3D Digital Differential Analyzer, visiting only the cells along the ray path (~6–12 per ray) instead of checking all 216 cells.
- **Primitives**: spheres (`sphere.hpp`) and axis-aligned boxes (`box.hpp`) placed in grid cells — 10 spheres and 3 boxes.
- **Lighting**: simple directional diffuse shading with a single light source, gamma-corrected (sqrt) output.
- **Viewport**: 64° FOV, samples 2 rays per pixel for basic anti-aliasing.

## Files

| File | Role |
|------|------|
| `main.cpp` | Scene setup, camera, render loop, event handling |
| `display.hpp` / `display.cpp` | X11 window wrapper (keyboard, mouse, pixels) |
| `v3.hpp` | 3D vector math (Vec3, operators, cross, unit) |
| `hittable.hpp` | Abstract Hittable interface + Ray + HitRecord |
| `sphere.hpp` | Sphere primitive |
| `box.hpp` | Axis-aligned box primitive (slab method) |
| `grid.hpp` | 10×10×10 grid with DDA ray traversal |
