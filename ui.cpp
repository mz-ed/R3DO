#include "ui.hpp"
#include "saver.hpp"
#include "sphere.hpp"
#include "box.hpp"
#include "cylinder.hpp"
#include "cone.hpp"
#include "obj_loader.hpp"
#include "sculpt_mesh.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

double UI::now_s() const {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

UI::UI(Grid& g, Camera& c, DisplayWin& dsp, SculptEngine* sculpt)
    : grid(g), cam(c), display(dsp), sculpt_(sculpt) {
    scan_meshes();
    update_labels();
}

void UI::update_labels() {
    // Keep the Nuklear combo selection in sync with the active render mode
    // (the sidebar label is looked up via mode_label_idx_).
    if (render_mode_ < 0) render_mode_ = 0;
    if (render_mode_ > 4) render_mode_ = 4;
    mode_label_idx_ = render_mode_;
}

void UI::scan_meshes() {
    mesh_files_.clear();
    std::error_code ec;
    if (!fs::exists("models")) {
        fs::create_directory("models", ec);
        return;
    }
    for (const auto& entry : fs::directory_iterator("models")) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() == ".obj")
            mesh_files_.push_back(p.string());
    }
    std::sort(mesh_files_.begin(), mesh_files_.end());
    if (mesh_idx_ >= (int)mesh_files_.size()) mesh_idx_ = 0;
}

void UI::cycle_mesh(int dir) {
    if (mesh_files_.empty()) return;
    mesh_idx_ = (mesh_idx_ + dir + (int)mesh_files_.size()) % (int)mesh_files_.size();
}

void UI::set_brush_radius(float r) {
    if (r < 0.05f) r = 0.05f;
    if (r > 2.0f) r = 2.0f;
    brush_radius_ = r;
    char buf[64]; snprintf(buf, sizeof(buf), "Radius: %.2f", brush_radius_);
    show_msg(buf);
}

void UI::set_brush_strength(float s) {
    if (s < 0.05f) s = 0.05f;
    if (s > 2.0f) s = 2.0f;
    brush_strength_ = s;
    char buf[64]; snprintf(buf, sizeof(buf), "Strength: %.2f", brush_strength_);
    show_msg(buf);
}

void UI::set_sculpt_res(int segs, int rings) {
    if (segs < 8) segs = 8;
    if (segs > 64) segs = 64;
    if (rings < 6) rings = 6;
    if (rings > 48) rings = 48;
    sculpt_segs_ = segs;
    sculpt_rings_ = rings;
    char buf[64]; snprintf(buf, sizeof(buf), "Res: %dx%d", sculpt_segs_, sculpt_rings_);
    show_msg(buf);
}

Vec3 UI::palette_color(int index) const {
    static const Vec3 palette[] = {
        Vec3(1, 0.2, 0.2), Vec3(0.2, 1, 0.3), Vec3(1, 1, 0.2),
        Vec3(1, 0.5, 0), Vec3(0.2, 0.4, 1), Vec3(0.9, 0.2, 0.9),
        Vec3(0.2, 0.9, 0.9), Vec3(0.8, 0.6, 0.2), Vec3(0.5, 0.3, 1),
        Vec3(1, 0.6, 0.6)
    };
    return palette[index % 10];
}

void UI::push_undo() {
    undo_stack_.push_back(serialize_scene(grid, cam, render_mode_, palette_idx_));
    if (undo_stack_.size() > 50)
        undo_stack_.erase(undo_stack_.begin());
}

bool UI::undo() {
    if (undo_stack_.empty()) return false;
    std::string snapshot = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    deserialize_scene(snapshot, grid, &cam, &render_mode_, &palette_idx_);
    update_labels();
    return true;
}

void UI::clear_scene() {
    push_undo();
    grid.clear();
    show_msg("Scene cleared");
}

void UI::add_sculpt_mesh() {
    // Spawn where the crosshair points so the mesh is immediately visible
    // and targetable with the brush.
    Vec3 target = cam.pos + cam.forward() * 3.0;
    SculptMesh* sm = SculptMesh::create_sphere(target, 0.4,
        sculpt_segs_, sculpt_rings_, Vec3(0.8,0.7,0.5));
    if (sm) {
        grid.add_free(sm);
        if (sculpt_) sculpt_->set_active(sm);
        show_msg("Sculpt mesh added");
    }
}

void UI::do_remesh() {
    if (sculpt_ && sculpt_->active()) {
        sculpt_->active()->remesh_subdivide();
        char buf[64];
        snprintf(buf, sizeof(buf), "Remeshed: %d verts", (int)sculpt_->active()->vertices().size());
        show_msg(buf);
    } else {
        show_msg("No active mesh");
    }
}

void UI::load_terrain() {
    if (mesh_files_.empty()) {
        show_msg("No meshes in models/");
        return;
    }
    const std::string& path = mesh_files_[mesh_idx_];
    Mesh* m = load_obj(path.c_str(), Vec3(0.3, 0.5, 0.25), Vec3(0, 0, 0), 1.0);
    if (m) {
        grid.set_terrain(m, path);
        show_msg("Terrain loaded");
    } else {
        show_msg("Mesh load failed");
    }
}

const std::vector<std::string>* UI::object_list() {
    obj_refs_.clear();
    obj_names_.clear();

    // cells
    for (int i = 0; i < grid.nx; i++) {
        for (int j = 0; j < grid.ny; j++) {
            for (int k = 0; k < grid.nz; k++) {
                Hittable* obj = grid.get(i, j, k);
                if (!obj) continue;
                ObjRef r;
                r.type = ObjRef::CELL;
                r.i = i; r.j = j; r.k = k;
                r.base_center = obj->get_center() - obj->anim_offset();
                obj_refs_.push_back(r);
                char buf[64];
                snprintf(buf, sizeof(buf), "cell %d,%d,%d %s", i, j, k, obj->type_name());
                obj_names_.push_back(buf);
            }
        }
    }
    // free
    const auto& fobjs = grid.free_objects();
    for (int fi = 0; fi < (int)fobjs.size(); fi++) {
        Hittable* obj = fobjs[(size_t)fi].get();
        if (!obj) continue;
        ObjRef r;
        r.type = ObjRef::FREE;
        r.free_idx = fi;
        r.base_center = obj->get_center() - obj->anim_offset();
        obj_refs_.push_back(r);
        char buf[64];
        snprintf(buf, sizeof(buf), "free#%d %s", fi, obj->type_name());
        obj_names_.push_back(buf);
    }
    if (selected_obj_ < 0 && !obj_names_.empty()) selected_obj_ = 0;
    if (selected_obj_ >= (int)obj_names_.size()) selected_obj_ = -1;
    return &obj_names_;
}

void UI::select_object(int i) {
    if (i < 0) { selected_obj_ = -1; return; }
    if (i >= (int)obj_names_.size()) return;
    selected_obj_ = i;
}

// Move a cell-based object: take (preserve geometry/color), translate, and
// re-home into the cell containing newc. If newc falls outside the grid, the
// object is converted to a free object so raytracing still finds it.
void UI::relocate_cell(int sel, const Vec3& newc) {
    if (sel < 0 || sel >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)sel];
    if (r.type != ObjRef::CELL) return;
    Hittable* obj = grid.take_cell(r.i, r.j, r.k);
    if (!obj) return;
    Vec3 oldc = obj->get_center() - obj->anim_offset();
    obj->set_anim_offset(Vec3(0, 0, 0));
    obj->translate(newc - oldc);
    int ni, nj, nk;
    if (grid.world_to_cell(newc, ni, nj, nk)) {
        grid.set(ni, nj, nk, obj);
        r.i = ni; r.j = nj; r.k = nk;
        r.base_center = obj->get_center();
        char buf[64];
        snprintf(buf, sizeof(buf), "cell %d,%d,%d %s", ni, nj, nk, obj->type_name());
        obj_names_[(size_t)sel] = buf;
    } else {
        grid.add_free(obj);
        r.type = ObjRef::FREE;
        r.free_idx = (int)grid.free_objects().size() - 1;
        r.i = r.j = r.k = -1;
        r.base_center = obj->get_center();
        char buf[64];
        snprintf(buf, sizeof(buf), "free#%d %s", r.free_idx, obj->type_name());
        obj_names_[(size_t)sel] = buf;
    }
}

void UI::move_selected(const Vec3& d) {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (obj) {
            push_undo();
            obj->translate(d);
            r.base_center = obj->get_center() - obj->anim_offset();
            show_msg("Moved object");
        }
        return;
    }
    if (r.type == ObjRef::CELL) {
        Hittable* obj = grid.get(r.i, r.j, r.k);
        if (!obj) return;
        push_undo();
        Vec3 oldc = obj->get_center() - obj->anim_offset();
        relocate_cell(selected_obj_, oldc + d);
        show_msg("Moved object");
    }
}

void UI::center_selected_above() {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    ObjRef& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (obj) {
            push_undo();
            Vec3 c = obj->get_center() - obj->anim_offset();
            double gh = grid.get_ground_height(c.x, c.z);
            Vec3 target(c.x, gh + 1.2, c.z);
            obj->translate(target - c);
            r.base_center = obj->get_center() - obj->anim_offset();
            show_msg("Centered above");
        }
        return;
    }
    if (r.type == ObjRef::CELL) {
        Hittable* obj = grid.get(r.i, r.j, r.k);
        if (!obj) return;
        push_undo();
        Vec3 oldc = obj->get_center() - obj->anim_offset();
        double gh = grid.get_ground_height(oldc.x, oldc.z);
        relocate_cell(selected_obj_, Vec3(oldc.x, gh + 0.6, oldc.z));
        show_msg("Centered above");
    }
}

void UI::reset_selected() {
    if (selected_obj_ < 0 || selected_obj_ >= (int)obj_refs_.size()) return;
    const auto& r = obj_refs_[(size_t)selected_obj_];
    if (r.type == ObjRef::FREE) {
        if (r.free_idx < 0 || r.free_idx >= (int)grid.free_objects().size()) return;
        Hittable* obj = grid.free_objects()[(size_t)r.free_idx].get();
        if (obj) {
            push_undo();
            Vec3 c = obj->get_center() - obj->anim_offset();
            obj->translate(r.base_center - c);
            show_msg("Reset to original");
        }
    }
}

void UI::show_msg(const char* msg) {
    snprintf(save_msg_, sizeof(save_msg_), "%s", msg);
    save_msg_until_ = now_s() + 3.0;
}

bool UI::try_place(ShapeType type) {
    Vec3 target = cam.pos + cam.forward() * 3.0;
    int i, j, k;

    if (ground_mode_) {
        Vec3 ro = cam.pos;
        Vec3 rd = cam.forward();
        if (grid.has_terrain()) {
            Ray fwd(ro, rd);
            HitRecord trec;
            if (grid.terrain()->hit(fwd, 0.001, 100.0, trec))
                target = trec.p;
            else
                target = ro + rd * 5.0;
        } else {
            if (rd.y > -1e-10) return false;
            Vec3 gmin, gmax;
            grid.grid_bounds(gmin, gmax);
            double gy = gmin.y;
            double t = (gy - ro.y) / rd.y;
            if (t < 0) return false;
            target = ro + rd * t;
        }
        if (!grid.world_to_cell(target, i, j, k)) return false;
        j = 0;
    } else if (!grid.world_to_cell(target, i, j, k)) {
        return false;
    }

    if (grid.get(i, j, k)) return false;

    push_undo();

    double cs = grid.cell_size;
    Vec3 c = grid.cell_center(i, j, k);
    Vec3 col = palette_color(palette_idx_);

    switch (type) {
        case ShapeType::SPHERE:
            grid.set(i, j, k, new Sphere(c, cs * 0.45, col));
            break;
        case ShapeType::BOX: {
            Vec3 half(cs * 0.38, cs * 0.38, cs * 0.38);
            grid.set(i, j, k, new Box(c - half, c + half, col));
            break;
        }
        case ShapeType::CYLINDER:
            grid.set(i, j, k, new Cylinder(c, cs * 0.38, cs * 0.8, col));
            break;
        case ShapeType::CONE:
            grid.set(i, j, k, new Cone(c, cs * 0.38, cs * 0.8, col));
            break;
        case ShapeType::MESH: {
            if (mesh_files_.empty()) return false;
            const std::string& path = mesh_files_[mesh_idx_];
            Mesh* m = load_obj(path.c_str(), col, c, cs * 0.4);
            if (!m) return false;
            grid.add_free(m);
            break;
        }
    }

    last_shape_ = type;
    palette_idx_++;
    return true;
}

void UI::toggle_ground_mode() {
    ground_mode_ = !ground_mode_;
    update_labels();
}

void UI::set_ground_mode(bool v) {
    if (ground_mode_ == v) return;
    toggle_ground_mode();
}

void UI::set_save_name(const char* s) {
    snprintf(save_name_, sizeof(save_name_), "%s", s ? s : "");
}

void UI::confirm_save() {
    std::string name(save_name_);
    // Strip anything that could escape saves/ or break the filesystem.
    std::string clean;
    for (char c : name) {
        unsigned char u = (unsigned char)c;
        bool ok = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
                  (u >= '0' && u <= '9') || c == '-' || c == '_' || c == ' ';
        if (ok) clean += c;
    }
    while (!clean.empty() && clean.back() == ' ') clean.pop_back();
    if (!clean.empty() && (clean[0] == '.' || clean == "saves")) {
        clean.clear();
    }
    if (!clean.empty()) {
        std::string path = "saves/" + clean + ".r3do";
        save_scene(grid, cam, path, render_mode_, palette_idx_);
        show_msg(("Saved: " + clean).c_str());
    }
    save_dialog_active_ = false;
}
