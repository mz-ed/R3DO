#include "obj_loader.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

// Parse "i", "i/j", "i//j", or "i/j/k" into a 0-based vertex index.
// Handles negative (relative) indices per the OBJ spec.
static int parse_vert_idx(const char* tok, size_t vert_count) {
    if (!tok || !*tok) return -1;
    long v = strtol(tok, nullptr, 10);
    if (v < 0) v += (long)vert_count + 1;   // -1 = last vertex
    else v -= 1;                            // 1-based -> 0-based
    if (v < 0 || v >= (long)vert_count) return -1;
    return (int)v;
}

Mesh* load_obj(const char* filename, const Vec3& color,
               const Vec3& center, double scale) {
    std::ifstream in(filename);
    if (!in) {
        fprintf(stderr, "Failed to open OBJ: %s\n", filename);
        return nullptr;
    }

    std::vector<Vec3> verts;
    std::vector<Triangle> tris;
    std::string line;

    while (std::getline(in, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t')) {
            float x, y, z;
            if (sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3)
                verts.push_back(Vec3(x * scale, y * scale, z * scale));
        } else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t')) {
            // Collect all vertex indices on the face line
            int face_idx[64];
            int nidx = 0;
            const char* p = line.c_str() + 2;
            while (*p && nidx < 64) {
                while (*p == ' ' || *p == '\t') p++;
                if (!*p || *p == '\n' || *p == '\r') break;
                int v = parse_vert_idx(p, verts.size());
                if (v >= 0) face_idx[nidx++] = v;
                while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
            }
            // Fan-triangulate polygons of any vertex count
            for (int i = 2; i < nidx; i++) {
                Vec3 v0 = verts[face_idx[0]] + center;
                Vec3 v1 = verts[face_idx[i - 1]] + center;
                Vec3 v2 = verts[face_idx[i]] + center;
                tris.push_back({v0, v1, v2});
            }
        }
    }

    if (tris.empty()) {
        fprintf(stderr, "OBJ loaded no triangles: %s\n", filename);
        return nullptr;
    }

    fprintf(stderr, "OBJ loaded %zu triangles from %s\n", tris.size(), filename);
    Mesh* m = new Mesh(std::move(tris), color);
    m->set_path(filename);
    return m;
}
