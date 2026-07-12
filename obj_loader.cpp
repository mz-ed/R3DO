#include "obj_loader.hpp"
#include <cstdio>
#include <vector>
#include <cmath>

Mesh* load_obj(const char* filename, const Vec3& color,
               const Vec3& center, double scale) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open OBJ: %s\n", filename);
        return nullptr;
    }

    std::vector<Vec3> verts;
    std::vector<Triangle> tris;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t')) {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3)
                verts.push_back(Vec3(x * scale, y * scale, z * scale));
        } else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t')) {
            int i0, i1, i2;
            int n = sscanf(line + 2, "%d %d %d", &i0, &i1, &i2);
            if (n == 3) {
                if (i0 > 0 && i0 <= (int)verts.size() &&
                    i1 > 0 && i1 <= (int)verts.size() &&
                    i2 > 0 && i2 <= (int)verts.size()) {
                    Vec3 v0 = verts[i0 - 1] + center;
                    Vec3 v1 = verts[i1 - 1] + center;
                    Vec3 v2 = verts[i2 - 1] + center;
                    tris.push_back({v0, v1, v2});
                }
            } else {
                // f i/j/k i/j/k i/j/k format
                int t0, t1, t2;
                n = sscanf(line + 2, "%d/%*d/%*d %d/%*d/%*d %d/%*d/%*d", &i0, &i1, &i2);
                n = sscanf(line + 2, "%d/%*d %d/%*d %d/%*d", &i0, &i1, &i2);
                if (n == 3) {
                    if (i0 > 0 && i0 <= (int)verts.size() &&
                        i1 > 0 && i1 <= (int)verts.size() &&
                        i2 > 0 && i2 <= (int)verts.size()) {
                        Vec3 v0 = verts[i0 - 1] + center;
                        Vec3 v1 = verts[i1 - 1] + center;
                        Vec3 v2 = verts[i2 - 1] + center;
                        tris.push_back({v0, v1, v2});
                    }
                }
            }
        }
    }

    fclose(f);

    if (tris.empty()) {
        fprintf(stderr, "OBJ loaded no triangles: %s\n", filename);
        return nullptr;
    }

    fprintf(stderr, "OBJ loaded %zu triangles from %s\n", tris.size(), filename);
    return new Mesh(tris, color);
}
