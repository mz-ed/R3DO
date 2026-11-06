#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include "heightfield.hpp"
#include "sculpt_mesh.hpp"
#include "skeleton.hpp"
#include "animation.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

static const char* MAGIC = "R3DO1";

static bool deserialize_stream(FILE* f, Grid& grid, Camera* cam,
                               int* render_mode, int* palette_idx,
                               Animator* anim);

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

// --- Animation clip lines --------------------------------------------------
//   anim <ttype> <i> <j> <k> <free_idx> <bone> <bpx> <bpy> <bpz> <byaw>
//        <bpitch> <loop> <speed> <weight> "<clip name>"
//   key <prop> <time> <x> <y> <z> <interp>
//   proc <ptype> <amp> <freq> <speed> <ax> <ay> <az> <enabled>
// key/proc lines always attach to the most recent anim line. The <bone>
// token is new (FREE_BONE targets); the reader falls back to the old
// 13-token layout for older saves, where type 3 meant CAMERA.

static void serialize_anim(std::string& out, const Animator& anim) {
    char buf[512];
    for (const auto& ci : anim.clips) {
        const AnimTarget& tg = ci.target;
        if (tg.type == AnimTarget::NONE) continue;
        snprintf(buf, sizeof(buf),
                 "anim %d %d %d %d %d %d %.4f %.4f %.4f %.4f %.4f %d %.4f %.4f %s\n",
                 (int)tg.type, tg.i, tg.j, tg.k, tg.free_idx, tg.bone,
                 ci.base_pos.x, ci.base_pos.y, ci.base_pos.z,
                 ci.base_yaw, ci.base_pitch,
                 ci.clip.loop ? 1 : 0, ci.speed, ci.weight,
                 quote_path(ci.clip.name).c_str());
        out += buf;
        for (const auto& t : ci.clip.tracks) {
            for (const auto& kf : t.keyframes) {
                snprintf(buf, sizeof(buf), "key %d %.4f %.4f %.4f %.4f %d\n",
                         (int)t.property, kf.time,
                         kf.value.x, kf.value.y, kf.value.z, (int)kf.interp);
                out += buf;
            }
        }
        for (const auto& pm : ci.procs) {
            snprintf(buf, sizeof(buf),
                     "proc %d %.4f %.4f %.4f %.4f %.4f %.4f %d\n",
                     (int)pm.type, pm.amplitude, pm.frequency, pm.speed,
                     pm.axis.x, pm.axis.y, pm.axis.z, pm.enabled ? 1 : 0);
            out += buf;
        }
    }
}

std::string serialize_scene(const Grid& grid, const Camera& cam,
                            int render_mode, int palette_idx,
                            const Animator* anim,
                            const std::string& filename) {
    // stem of the save file name, used to name per-scene asset folders
    auto filename_stem = [](const std::string& fn) {
        size_t slash = fn.find_last_of("/\\");
        size_t dot = fn.rfind('.');
        return fn.substr((slash == std::string::npos) ? 0 : slash + 1,
                         dot == std::string::npos ? std::string::npos
                                                  : dot - (slash == std::string::npos ? 0 : slash + 1));
    };
    std::string stem = filename_stem(filename);
    if (stem.empty()) stem = "scene";
    const std::string blob_dir = "meshes-" + stem;
    mkdir(blob_dir.c_str(), 0755);
    int sculpt_save_idx = 1;

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
        // Sculpt meshes persist through a native blob (exact verts/faces);
        // regular meshes need a source path. Anything else without a path
        // would come back as a broken `mesh ""` line that shifts animation
        // target indices.
        auto* sm = dynamic_cast<SculptMesh*>(f.get());
        if (sm) {
            std::string blob = blob_dir + "/sculpt_" + std::to_string(sculpt_save_idx) + ".sculpt";
            sculpt_save_idx++;
            if (sm->export_blob(blob)) {
                sm->set_path(blob);
                Vec3 c = f->get_color();
                Vec3 ctr = f->get_center();
                snprintf(buf, sizeof(buf),
                         "sculpt %s %.4f %.4f %.4f %.3f %.3f %.3f\n",
                         quote_path(blob).c_str(), ctr.x, ctr.y, ctr.z, c.x, c.y, c.z);
                out += buf;
            }
            continue;
        }
        if (f->mesh_path().empty()) continue;
        Vec3 c = f->get_color();
        Vec3 ctr = f->get_center();
        // Free objects are meshes; scale is derived from cell size on load.
        snprintf(buf, sizeof(buf), "mesh %s %.4f %.4f %.4f %.3f %.3f %.3f\n",
                 quote_path(f->mesh_path()).c_str(),
                 ctr.x, ctr.y, ctr.z, c.x, c.y, c.z);
        out += buf;
    }

    // Bone chains: after all free-object lines so the loader can resolve
    // free indices, and before terrain/anim lines.
    for (size_t fi = 0; fi < grid.free_objects_.size(); fi++) {
        const Skeleton* sk = grid.free_objects_[fi] ? grid.free_objects_[fi]->skeleton() : nullptr;
        if (!sk || sk->bones.empty()) continue;
        snprintf(buf, sizeof(buf), "bones %zu %zu\n", fi, sk->bones.size());
        out += buf;
        for (size_t bi = 0; bi < sk->bones.size(); bi++) {
            const Skeleton::Bone& b = sk->bones[bi];
            snprintf(buf, sizeof(buf), "bone %zu %d %.4f %.4f %.4f %s\n",
                     bi, b.parent, b.offset.x, b.offset.y, b.offset.z,
                     quote_path(b.name).c_str());
            out += buf;
        }
    }

    if (grid.terrain()) {
        std::string tpath = grid.terrain_path();
        // A scene-blank or paint-only heightfield has no source path; without
        // a path the `terrain ""` line would fail to reload and the map would
        // vanish. Bake the current state out to maps/auto_<save>.png once.
        if (tpath.empty()) {
            auto* hf = dynamic_cast<const HeightField*>(grid.terrain());
            if (hf) {
                std::string png = "maps/auto_" + stem + ".png";
                if (hf->export_png(png)) {
                    tpath = png;
                } else if (hf->export_png("maps/auto_scene.png")) {
                    tpath = "maps/auto_scene.png";
                }
                // if both exports fail, keep tpath empty: the reload loses
                // this blank canvas but at least does not crash.
            }
        }
        snprintf(buf, sizeof(buf), "terrain %s\n", quote_path(tpath).c_str());
        out += buf;
    }

    if (anim) serialize_anim(out, *anim);

    return out;
}

bool deserialize_scene(const std::string& data, Grid& grid, Camera* cam,
                       int* render_mode, int* palette_idx, Animator* anim) {
    FILE* f = fmemopen((void*)data.data(), data.size(), "r");
    if (!f) return false;

    bool result = deserialize_stream(f, grid, cam, render_mode, palette_idx, anim);
    fclose(f);
    return result;
}

static bool deserialize_stream(FILE* f, Grid& grid, Camera* cam,
                        int* render_mode, int* palette_idx,
                        Animator* anim) {
    grid.clear();
    if (anim) anim->clear();

    // Most recent "anim" line's clip; key/proc lines attach to it.
    int cur_bones_idx = -1;
    Skeleton* cur_sk = nullptr;
    Animator::ClipInstance* cur = nullptr;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        // Trim trailing newline for cleaner parsing
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;

        // A completed bone chain binds as soon as the next non-"bone" line
        // shows up; vertices picked their bones the moment joints existed,
        // but attaching here keeps bind() seeing the full chain.
        if (cur_bones_idx >= 0 && strncmp(line, "bone ", 5) != 0) {
            if (cur_sk && cur_bones_idx >= 0 &&
                cur_bones_idx < (int)grid.free_objects_.size() &&
                grid.free_objects_[(size_t)cur_bones_idx])
                grid.free_objects_[(size_t)cur_bones_idx]->bind_skeleton(cur_sk);
            else
                delete cur_sk;
            cur_sk = nullptr;
            cur_bones_idx = -1;
        }

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
        if (strncmp(line, "anim ", 5) == 0) {
            cur = nullptr;
            if (!anim) continue;
            int ttype, i, j, k, fidx, loop;
            float bpx, bpy, bpz, byaw, bpitch, speed, weight;
            int off = 0, bone = 0;
            int n = sscanf(line,
                           "anim %d %d %d %d %d %d %f %f %f %f %f %d %f %f %n",
                           &ttype, &i, &j, &k, &fidx, &bone, &bpx, &bpy, &bpz,
                           &byaw, &bpitch, &loop, &speed, &weight, &off);
            if (n < 13) continue;
            if (n < 14) {
                // legacy layout: no bone token, type 3 = CAMERA
                off = 0;
                if (sscanf(line,
                           "anim %d %d %d %d %d %f %f %f %f %f %d %f %f %n",
                           &ttype, &i, &j, &k, &fidx, &bpx, &bpy, &bpz, &byaw,
                           &bpitch, &loop, &speed, &weight, &off) < 13)
                    continue;
                if (ttype == 3) ttype = (int)AnimTarget::CAMERA;
                bone = 0;
            }
            if (ttype < 1 || ttype > 4) continue;
            std::string name;
            const char* p = line + off;
            if (!parse_quoted(&p, name)) name.clear();
            Animator::ClipInstance ci;
            ci.target.type = (AnimTarget::Type)ttype;
            ci.target.i = i; ci.target.j = j; ci.target.k = k;
            ci.target.free_idx = fidx;
            ci.target.bone = (ttype == (int)AnimTarget::FREE_BONE) ? bone : -1;
            ci.base_pos = Vec3(bpx, bpy, bpz);
            ci.base_yaw = byaw; ci.base_pitch = bpitch;
            ci.base_snapped = (ttype == AnimTarget::CAMERA);
            ci.clip.loop = loop != 0;
            ci.speed = speed; ci.weight = weight;
            ci.clip.name = name;
            anim->clips.push_back(ci);
            cur = &anim->clips.back();
            continue;
        }
        if (strncmp(line, "key ", 4) == 0) {
            int prop, interp;
            float time, x, y, z;
            if (cur && sscanf(line, "key %d %f %f %f %f %d",
                              &prop, &time, &x, &y, &z, &interp) == 6 &&
                prop >= 0 && prop <= 3 && interp >= 0 && interp <= 4 && time >= 0)
                cur->clip.add_keyframe((AnimProperty)prop, time,
                                       Vec3(x, y, z), (InterpolationType)interp);
            continue;
        }
        if (strncmp(line, "proc ", 5) == 0) {
            int ptype, en;
            float amp, freq, spd, ax, ay, az;
            if (cur && sscanf(line, "proc %d %f %f %f %f %f %f %d",
                              &ptype, &amp, &freq, &spd, &ax, &ay, &az, &en) == 8 &&
                ptype >= 0 && ptype <= 3)
                cur->procs.push_back({(ProceduralType)ptype, amp, freq,
                                      spd, Vec3(ax, ay, az), en != 0});
            continue;
        }
        if (strncmp(line, "bones ", 6) == 0) {
            cur_bones_idx = -1;
            cur_sk = nullptr;
            size_t fi, count;
            if (sscanf(line + 6, "%zu %zu", &fi, &count) == 2 &&
                fi < grid.free_objects_.size() && grid.free_objects_[fi] &&
                count > 0 && count <= 64) {
                cur_bones_idx = (int)fi;
                cur_sk = new Skeleton();
                cur_sk->bones.reserve(count);
            }
            continue;
        }
        if (strncmp(line, "bone ", 5) == 0) {
            if (!cur_sk || cur_bones_idx < 0) continue;
            int idx, parent;
            float ox, oy, oz;
            int off = 0;
            if (sscanf(line + 5, "%d %d %f %f %f %n",
                       &idx, &parent, &ox, &oy, &oz, &off) < 5)
                continue;
            if (idx != (int)cur_sk->bones.size()) continue;
            std::string name;
            const char* bp = line + 5 + off;
            if (!parse_quoted(&bp, name)) name.clear();
            Skeleton::Bone b;
            b.parent = parent;
            b.name = name;
            b.offset = Vec3(ox, oy, oz);
            cur_sk->bones.push_back(b);
            continue;
        }
        if (strncmp(line, "terrain", 7) == 0 &&
            (line[7] == ' ' || line[7] == '\t')) {
            const char* p = line + 7;
            std::string path;
            if (parse_quoted(&p, path)) {
                size_t dot = path.rfind('.');
                std::string ext = (dot == std::string::npos) ? "" : path.substr(dot);
                if (ext == ".png") {
                    // heightmap-backed terrain; footprint follows the grid
                    Vec3 gmin, gmax;
                    grid.grid_bounds(gmin, gmax);
                    double side = std::max(gmax.x - gmin.x, gmax.z - gmin.z) + 1.6;
                    auto* hf = HeightField::from_png(path,
                                                     gmin.x - (side - (gmax.x - gmin.x)) / 2,
                                                     gmin.z - (side - (gmax.z - gmin.z)) / 2,
                                                     side, 0.0, 3.0,
                                                     Vec3(0.3, 0.5, 0.25)).release();
                    if (hf) grid.set_terrain(hf, path);
                } else {
                    Mesh* m = load_mesh_centered(path.c_str(), Vec3(0.3, 0.5, 0.25),
                                                 Vec3(0, 0, 0), 1.0);
                    if (m) grid.set_terrain(m, path);
                }
            }
            continue;
        }
        if (strncmp(line, "sculpt ", 7) == 0) {
            const char* p = line + 7;
            std::string path;
            if (!parse_quoted(&p, path)) continue;
            float cx, cy, cz, r, g, b;
            if (sscanf(p, "%f %f %f %f %f %f", &cx, &cy, &cz, &r, &g, &b) != 6) continue;
            SculptMesh* sm = SculptMesh::from_blob(path, Vec3(r, g, b));
            if (sm) {
                Vec3 want(cx, cy, cz);
                sm->translate(want - sm->get_center());
                grid.add_free(sm);
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
            if (m) {
                m->set_path(path);
                grid.add_free(m);
            }
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

    // trailing bone chain with no following line
    if (cur_bones_idx >= 0) {
        if (cur_sk && cur_bones_idx < (int)grid.free_objects_.size() &&
            grid.free_objects_[(size_t)cur_bones_idx])
            grid.free_objects_[(size_t)cur_bones_idx]->bind_skeleton(cur_sk);
        else
            delete cur_sk;
        cur_sk = nullptr;
        cur_bones_idx = -1;
    }

    if (anim) anim->update_max_time();
    return true;
}

void save_scene(const Grid& grid, const Camera& cam, const std::string& filename,
                int render_mode, int palette_idx, const Animator* anim) {
    mkdir("saves", 0755);

    std::string data = serialize_scene(grid, cam, render_mode, palette_idx, anim, filename);

    FILE* f = fopen(filename.c_str(), "w");
    if (!f) return;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
}

bool load_scene(const std::string& filename, Grid& grid, Camera* cam,
                int* render_mode, int* palette_idx, Animator* anim) {
    FILE* f = fopen(filename.c_str(), "r");
    if (!f) return false;
    bool ok = deserialize_stream(f, grid, cam, render_mode, palette_idx, anim);
    fclose(f);
    return ok;
}
