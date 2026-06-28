#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
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
                fprintf(f, "%s %d %d %d %.3f %.3f %.3f\n",
                        obj->type_name(), i, j, k, c.x, c.y, c.z);
            }
        }
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

    char type[16];
    int i, j, k;
    float r, g, b;

    while (fscanf(f, "%15s %d %d %d %f %f %f", type, &i, &j, &k, &r, &g, &b) == 7) {
        Vec3 color(r, g, b);
        Vec3 c = grid.cell_center(i, j, k);
        double cs = grid.cell_size;

        if (strcmp(type, "sphere") == 0) {
            grid.set(i, j, k, new Sphere(c, cs * 0.45, color));
        } else if (strcmp(type, "box") == 0) {
            Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
            grid.set(i, j, k, new Box(c - half, c + half, color));
        } else if (strcmp(type, "cylinder") == 0) {
            grid.set(i, j, k, new Cylinder(c, cs * 0.38, cs * 0.8, color));
        } else if (strcmp(type, "cone") == 0) {
            grid.set(i, j, k, new Cone(c, cs * 0.38, cs * 0.8, color));
        }
    }

    fclose(f);
}
