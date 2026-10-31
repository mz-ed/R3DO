#include "render_modes.hpp"
#include "billboard.hpp"
#include "overhead.hpp"
#include "render.hpp"

// --- adapters to the uniform signature ---

static void raytrace_mode(Grid& grid, Camera& cam, DisplayWin& display,
                          const Vec3& light_dir, int samples) {
    render_scene(grid, cam, display, display.width(), display.height(), samples, light_dir);
}

static void sculpt_mode(Grid& grid, Camera& cam, DisplayWin& display,
                        const Vec3& light_dir, int samples) {
    // Same tracer as Raytrace; the brush engine lives in sculpt/*.cpp and is
    // driven by the main loop while this mode is active.
    render_scene(grid, cam, display, display.width(), display.height(), samples, light_dir);
}

static void billboard_mode(Grid& grid, Camera& cam, DisplayWin& display,
                           const Vec3& light_dir, int) {
    (void)light_dir; // sprites use the object color; no lighting
    render_billboard(grid, cam, display, light_dir);
}

static void overhead_mode(Grid& grid, Camera& cam, DisplayWin& display,
                          const Vec3& light_dir, int) {
    (void)light_dir; // top-down marker view; no lighting
    render_overhead(grid, cam, display);
}

// Registered last so existing .r3do saves keep their indices.
const RenderMode MODES[] = {
    { "Raytrace",  raytrace_mode,   false },
    { "Billboard", billboard_mode,  false },
    { "Overhead",  overhead_mode,   false },
    { "Sculpt",    sculpt_mode,     true  },
    { "Static",    static_mode,     false },
};
