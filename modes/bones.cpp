#include "render_modes.hpp"
#include "billboard.hpp"
#include "render.hpp"
#include "v3.hpp"
#include <vector>

// Bones mode: standard billboard view with every skeleton overlaid — joints
// drawn as dots, links as lines, selected bone highlighted, joint names as
// tiny labels near the joints. Gizmo/selection keep working (strokes disabled),
// so bones can be picked in the OBJECTS dropdown while editing the chain.

struct BoneOverlay {
    Vec3 from;                       // parent joint (or child itself for root)
    Vec3 to;
    const char* name;
    int depth;
};

void render_bones_mode(Grid& grid, Camera& cam, DisplayWin& display,
                       const Vec3& light_dir, int samples) {
    render_billboard(grid, cam, display, light_dir);
    (void)samples;

    int w = display.width(), h = display.height();
    double vw = VIEWPORT_HEIGHT * w / h;

    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);

    auto project = [&](const Vec3& p, int& sx, int& sy) -> bool {
        Vec3 rel = p - cam.pos;
        double depth = dot(rel, fwd);
        if (depth <= 0.05) return false;
        double rx = dot(rel, rgt);
        double ry = dot(rel, up);
        sx = (int)(w / 2.0 + (rx / depth) * FOCAL_LENGTH * w / vw + 0.5);
        sy = (int)(h / 2.0 - (ry / depth) * FOCAL_LENGTH * h / VIEWPORT_HEIGHT + 0.5);
        return sx >= -50 && sy >= -50 && sx < w + 50 && sy < h + 50;
    };

    auto& fobjs = grid.free_objects();
    for (size_t fi = 0; fi < fobjs.size(); fi++) {
        Hittable* obj = fobjs[fi].get();
        const Skeleton* sk = obj ? obj->skeleton() : nullptr;
        if (!sk || sk->bones.empty()) continue;

        // live world geometry: bind verts follow drag (translate), pose keys
        // are applied per-frame by the animator, so re-project from the head.
        std::vector<Vec3> joints((size_t)sk->bones.size());
        for (int bi = 0; bi < (int)sk->bones.size(); bi++) joints[(size_t)bi] = sk->joint(bi);



        // links
        for (int bi = 0; bi < (int)sk->bones.size(); bi++) {
            const auto& b = sk->bones[(size_t)bi];
            int ax, ay, bx, by;
            if (!project(joints[(size_t)bi], ax, ay)) continue;
            if (b.parent >= 0) {
                const Vec3& jpp = joints[(size_t)b.parent];
                if (project(jpp, bx, by))
                    display.draw_line(ax, ay, bx, by, 0x34d2eb);   // bright link
            }
            // joint dot
            int d = 3;
            display.fill_rect(ax - d / 2, ay - d / 2, d, d,
                              b.parent >= 0 ? 0x34d2eb : 0xffd23f);
            // tiny name tag
            char tag[72];
            snprintf(tag, sizeof(tag), "%d:%s", bi, b.name.c_str());
            display.draw_text(ax + 5, ay - 5, tag, 0xbfeef7);
        }
    }
}
