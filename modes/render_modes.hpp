#ifndef RENDER_MODES_H
#define RENDER_MODES_H

#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include "v3.hpp"

// One entry per render mode. Adding a mode = write its render function in its
// own file, then append one line to the MODES table in modes.cpp. Names for
// the sidebar combo, the mode count for clamps/cycling and the sculpt-panel
// special cases all come from this table.
//
// WARNING: the array order is the saved scene format (mode index stored in
// .r3do files). Only append; never reorder or renumber.
struct RenderMode {
    const char* name;
    void (*render)(Grid& grid, Camera& cam, DisplayWin& display,
                   const Vec3& light_dir, int samples);
    bool uses_mouse_for_strokes; // sculpt-style: LMB/RMB edit geometry, so the
                                 // gizmo, aim-delete and camera-look are skipped
};

extern const RenderMode MODES[];
inline int mode_count() { return 5; }

// non-[fn]-table mode function defined in static.cpp
void static_mode(Grid& grid, Camera& cam, DisplayWin& display,
                 const Vec3& light_dir, int samples);

// Current mode helpers (index validated like ui.render_mode()).
inline const RenderMode& get_mode(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= mode_count()) idx = mode_count() - 1;
    return MODES[idx];
}

#endif
