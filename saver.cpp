#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

void save_scene(const Grid& grid, const std::string& filename) {
    mkdir("saves", 0755);

    FILE* f = fopen(filename.c_str(), "w");
    if (!f) return;

    for (int i = 0; i < grid.nx; i++) {
        for (int j = 0; j < grid.ny; j++) {
            for (int k = 0; k < grid.nz; k++) {
                Hittable* obj = grid.get(i, j, k);
                if (!obj) continue;

                Vec3 c = obj->get_color();
                fprintf(f, "%s %d %d %d %.3f %.3f %.3f %d\n",
                        obj->type_name(), i, j, k, c.x, c.y, c.z, obj->is_visible() ? 1 : 0);
            }
        }
    }

    if (grid.terrain()) {
        fprintf(f, "terrain %s\n", grid.terrain_path().c_str());
    }

    fclose(f);
}

void load_scene(const std::string& filename, Grid& grid) {
    FILE* f = fopen(filename.c_str(), "r");
    if (!f) return;

    for (int i = 0; i < grid.nx; i++)
        for (int j = 0; j < grid.ny; j++)
            for (int k = 0; k < grid.nz; k++)
                grid.set(i, j, k, nullptr);

    char line[1024];
    char type[16];
    int i, j, k, v;
    float r, g, b;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 't' && line[1] == 'e' && line[2] == 'r') {
            // terrain line: "terrain <path>"
            char path_buf[512];
            if (sscanf(line, "terrain %511s", path_buf) == 1) {
                Mesh* m = load_obj(path_buf, Vec3(0.3, 0.5, 0.25), Vec3(0, 0, 0), 1.0);
                if (m) grid.set_terrain(m, path_buf);
            }
            continue;
        }

        int n = sscanf(line, "%15s %d %d %d %f %f %f %d", type, &i, &j, &k, &r, &g, &b, &v);
        if (n == 7) v = 1;
        else if (n < 7) continue;

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
        }
        if (obj) {
            obj->set_visible(v != 0);
            grid.set(i, j, k, obj);
        }
    }

    fclose(f);
}
