#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

static const char* MAGIC = "R3DO1";

static bool deserialize_stream(FILE* f, Grid& grid, Camera* cam,
                               int* render_mode, int* palette_idx);

// Quote a path if needed; always quoting is simplest and safe.
static std::string quote_path(const std::string& p) {
    return "\"" + p + "\"";
}

// Parse a possibly-quoted path from a string. Advances *pp past the token.
static bool parse_quoted(const char** pp, std::string& out) {
    const char* p = *pp;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        p++;
        const char* end = strchr(p, '"');
        if (!end) return false;
        out.assign(p, end - p);
        *pp = end + 1;
        return true;
    }
    const char* start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    out.assign(start, p - start);
    *pp = p;
    return !out.empty();
}

// Load a mesh and shift it so its bounding-box center lands on target_center.
static Mesh* load_mesh_centered(const char* path, const Vec3& color,
                                const Vec3& target_center, double scale) {
    Mesh* m = load_obj(path, color, Vec3(0, 0, 0), scale);
    if (!m) return nullptr;
    Vec3 cur = m->get_center();
    Vec3 shift = target_center - cur;
    const auto& src = m->get_triangles();
    std::vector<Triangle> tris;
    tris.reserve(src.size());
    for (const auto& t : src)
        tris.push_back({t.v0 + shift, t.v1 + shift, t.v2 + shift});
    Vec3 col = m->color;
    delete m;
    return new Mesh(std::move(tris), col);
}

std::string serialize_scene(const Grid& grid, const Camera& cam,
                            int render_mode, int palette_idx) {
    std::string out;
    out.reserve(4096);
    char buf[512];

    snprintf(buf, sizeof(buf), "%s\ncamera %.4f %.4f %.4f %.4f %.4f\nmode %d\npalette %d\n",
             MAGIC, cam.pos.x, cam.pos.y, cam.pos.z, cam.yaw, cam.pitch,
             render_mode, palette_idx);
    out += buf;

    for (int i = 0; i < grid.nx; i++) {
        for (int j = 0; j < grid.ny; j++) {
            for (int k = 0; k < grid.nz; k++) {
                Hittable* obj = grid.get(i, j, k);
                if (!obj) continue;
                Vec3 c = obj->get_color();
                snprintf(buf, sizeof(buf), "%s %d %d %d %.3f %.3f %.3f %d\n",
                         obj->type_name(), i, j, k, c.x, c.y, c.z,
                         obj->is_visible() ? 1 : 0);
                out += buf;
            }
        }
    }

    for (const auto& f : grid.free_objects_) {
        if (!f) continue;
        Vec3 c = f->get_color();
        Vec3 ctr = f->get_center();
        // Free objects are meshes; scale is derived from cell size on load.
        snprintf(buf, sizeof(buf), "mesh %s %.4f %.4f %.4f %.3f %.3f %.3f\n",
                 quote_path(f->mesh_path()).c_str(),
                 ctr.x, ctr.y, ctr.z, c.x, c.y, c.z);
        out += buf;
    }

    if (grid.terrain()) {
        snprintf(buf, sizeof(buf), "terrain %s\n", quote_path(grid.terrain_path()).c_str());
        out += buf;
    }

    return out;
}

bool deserialize_scene(const std::string& data, Grid& grid, Camera* cam,
                       int* render_mode, int* palette_idx) {
    FILE* f = fmemopen((void*)data.data(), data.size(), "r");
    if (!f) return false;

    bool result = deserialize_stream(f, grid, cam, render_mode, palette_idx);
    fclose(f);
    return result;
}

static bool deserialize_stream(FILE* f, Grid& grid, Camera* cam,
                        int* render_mode, int* palette_idx) {
    grid.clear();

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        // Trim trailing newline for cleaner parsing
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;

        if (strncmp(line, "camera ", 7) == 0) {
            float x, y, z, yaw, pitch;
            if (cam && sscanf(line + 7, "%f %f %f %f %f", &x, &y, &z, &yaw, &pitch) == 5) {
                cam->pos = Vec3(x, y, z);
                cam->yaw = yaw;
                cam->pitch = pitch;
            }
            continue;
        }
        if (strncmp(line, "mode ", 5) == 0) {
            int m;
            if (render_mode && sscanf(line + 5, "%d", &m) == 1)
                *render_mode = m;
            continue;
        }
        if (strncmp(line, "palette ", 8) == 0) {
            int pi;
            if (palette_idx && sscanf(line + 8, "%d", &pi) == 1)
                *palette_idx = pi;
            continue;
        }
        if (strncmp(line, "terrain", 7) == 0 &&
            (line[7] == ' ' || line[7] == '\t')) {
            const char* p = line + 7;
            std::string path;
            if (parse_quoted(&p, path)) {
                Mesh* m = load_mesh_centered(path.c_str(), Vec3(0.3, 0.5, 0.25),
                                             Vec3(0, 0, 0), 1.0);
                if (m) grid.set_terrain(m, path);
            }
            continue;
        }
        if (strncmp(line, "mesh", 4) == 0 &&
            (line[4] == ' ' || line[4] == '\t')) {
            const char* p = line + 4;
            std::string path;
            if (!parse_quoted(&p, path)) continue;
            float cx, cy, cz, r, g, b;
            if (sscanf(p, "%f %f %f %f %f %f", &cx, &cy, &cz, &r, &g, &b) != 6) continue;
            Mesh* m = load_mesh_centered(path.c_str(), Vec3(r, g, b),
                                         Vec3(cx, cy, cz), grid.cell_size * 0.4);
            if (m) grid.add_free(m);
            continue;
        }

        // Cell object line: "type i j k r g b [vis]"
        char type[16];
        int i, j, k, v;
        float r, g, b;
        int n = sscanf(line, "%15s %d %d %d %f %f %f %d",
                       type, &i, &j, &k, &r, &g, &b, &v);
        if (n == 7) v = 1;
        else if (n < 7) continue;

        // Bounds-check indices before using them
        if (i < 0 || i >= grid.nx || j < 0 || j >= grid.ny || k < 0 || k >= grid.nz)
            continue;

        Vec3 color(r, g, b);
        Vec3 c = grid.cell_center(i, j, k);
        double cs = grid.cell_size;

        Hittable* obj = nullptr;
        if (strcmp(type, "sphere") == 0) {
            obj = new Sphere(c, cs * 0.45, color);
        } else if (strcmp(type, "box") == 0) {
            Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
            obj = new Box(c - half, c + half, color);
        } else if (strcmp(type, "cylinder") == 0) {
            obj = new Cylinder(c, cs * 0.38, cs * 0.8, color);
        } else if (strcmp(type, "cone") == 0) {
            obj = new Cone(c, cs * 0.38, cs * 0.8, color);
        } else {
            continue;
        }
        obj->set_visible(v != 0);
        grid.set(i, j, k, obj);
    }

    return true;
}

void save_scene(const Grid& grid, const Camera& cam, const std::string& filename,
                int render_mode, int palette_idx) {
    mkdir("saves", 0755);

    std::string data = serialize_scene(grid, cam, render_mode, palette_idx);

    FILE* f = fopen(filename.c_str(), "w");
    if (!f) return;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
}

bool load_scene(const std::string& filename, Grid& grid, Camera* cam,
                int* render_mode, int* palette_idx) {
    FILE* f = fopen(filename.c_str(), "r");
    if (!f) return false;
    bool ok = deserialize_stream(f, grid, cam, render_mode, palette_idx);
    fclose(f);
    return ok;
}
