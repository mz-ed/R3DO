#include "obj_loader.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>

static int parse_vert_idx(const char* tok) {
    if (!tok || !*tok) return 0;
    // token is "i", "i/j", "i//j", or "i/j/k"
    return atoi(tok);
}

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
            // Tokenize face line: skip "f", parse each vertex token
            char* p = line + 2;
            int idx[4], nidx = 0;
            while (*p && nidx < 4) {
                while (*p == ' ' || *p == '\t') p++;
                if (!*p || *p == '\n') break;
                int v = parse_vert_idx(p);
                if (v > 0 && v <= (int)verts.size()) {
                    idx[nidx++] = v;
                }
                // skip to next whitespace
                while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            }
            if (nidx == 3) {
                Vec3 v0 = verts[idx[0] - 1] + center;
                Vec3 v1 = verts[idx[1] - 1] + center;
                Vec3 v2 = verts[idx[2] - 1] + center;
                tris.push_back({v0, v1, v2});
            } else if (nidx == 4) {
                // Fan triangulate quad
                Vec3 v0 = verts[idx[0] - 1] + center;
                Vec3 v1 = verts[idx[1] - 1] + center;
                Vec3 v2 = verts[idx[2] - 1] + center;
                Vec3 v3 = verts[idx[3] - 1] + center;
                tris.push_back({v0, v1, v2});
                tris.push_back({v0, v2, v3});
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
