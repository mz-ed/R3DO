#include "mesh.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

#ifdef __SSE2__
#include <xmmintrin.h>
#include <emmintrin.h>
#endif

// --- scalar ray-triangle (fallback / standalone) ---
bool ray_tri_intersect(const Ray& r, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                       double t_min, double t_max, HitRecord& rec) {
    const double EPS = 1e-10;
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    Vec3 pvec = cross(r.direction(), e2);
    double det = dot(e1, pvec);
    if (std::abs(det) < EPS) return false;
    double inv = 1.0 / det;
    Vec3 tvec = r.origin() - v0;
    double u = dot(tvec, pvec) * inv;
    if (u < 0 || u > 1) return false;
    Vec3 qvec = cross(tvec, e1);
    double v = dot(r.direction(), qvec) * inv;
    if (v < 0 || u + v > 1) return false;
    double t = dot(e2, qvec) * inv;
    if (t < t_min || t > t_max) return false;
    rec.t = t;
    rec.p = r.at(t);
    rec.set_face_normal(r, cross(e1, e2));
    return true;
}

// --- AABB helper ---
static inline void expand_point(float bmin[3], float bmax[3], const Vec3& p) {
    for (int a = 0; a < 3; a++) {
        float v = (float)p[a];
        if (v < bmin[a]) bmin[a] = v;
        if (v > bmax[a]) bmax[a] = v;
    }
}

static inline void expand_aabb(float bmin[3], float bmax[3],
                               const float obmin[3], const float obmax[3]) {
    for (int a = 0; a < 3; a++) {
        if (obmin[a] < bmin[a]) bmin[a] = obmin[a];
        if (obmax[a] > bmax[a]) bmax[a] = obmax[a];
    }
}

static inline float surface_area(const float bmin[3], const float bmax[3]) {
    float dx = bmax[0] - bmin[0], dy = bmax[1] - bmin[1], dz = bmax[2] - bmin[2];
    return dx * dy + dy * dz + dz * dx;
}

static inline float centroid_axis(const Triangle& t, int a) {
    return (float)((t.v0[a] + t.v1[a] + t.v2[a]) * (1.0 / 3.0));
}

// --- SAH binned BVH build (recursive, depth ≈ log2(N) ≈ 16) ---
static constexpr int SAH_BINS = 16;
static constexpr int SAH_LEAF = 4;

struct BinInfo {
    float bmin[3], bmax[3];
    int count;
};

int Mesh::build_sah(std::vector<Triangle>& tris, int start, int count) {
    // Compute centroid bounds
    float cbmin[3] = {1e30f, 1e30f, 1e30f};
    float cbmax[3] = {-1e30f, -1e30f, -1e30f};
    for (int i = start; i < start + count; i++) {
        for (int a = 0; a < 3; a++) {
            float c = centroid_axis(tris[i], a);
            if (c < cbmin[a]) cbmin[a] = c;
            if (c > cbmax[a]) cbmax[a] = c;
        }
    }

    // Leaf: small node or degenerate centroid range
    if (count <= SAH_LEAF) {
        int idx = (int)nodes_.size();
        FlatNode n;
        n.bmin[0] = n.bmin[1] = n.bmin[2] = 1e30f;
        n.bmax[0] = n.bmax[1] = n.bmax[2] = -1e30f;
        for (int i = start; i < start + count; i++) {
            expand_point(n.bmin, n.bmax, tris[i].v0);
            expand_point(n.bmin, n.bmax, tris[i].v1);
            expand_point(n.bmin, n.bmax, tris[i].v2);
        }
        n.leftFirst = start;
        n.count = count;
        n.axis = 0;
        nodes_.push_back(n);
        return idx;
    }

    // Choose split axis (largest centroid extent)
    int axis = 0;
    float maxext = cbmax[0] - cbmin[0];
    if (cbmax[1] - cbmin[1] > maxext) { axis = 1; maxext = cbmax[1] - cbmin[1]; }
    if (cbmax[2] - cbmin[2] > maxext) { axis = 2; maxext = cbmax[2] - cbmin[2]; }

    if (maxext < 1e-8f) {
        int idx = (int)nodes_.size();
        FlatNode n;
        n.bmin[0] = cbmin[0]; n.bmin[1] = cbmin[1]; n.bmin[2] = cbmin[2];
        n.bmax[0] = cbmax[0]; n.bmax[1] = cbmax[1]; n.bmax[2] = cbmax[2];
        n.leftFirst = start;
        n.count = count;
        n.axis = 0;
        nodes_.push_back(n);
        return idx;
    }

    // Bin triangles
    BinInfo bins[SAH_BINS];
    for (int b = 0; b < SAH_BINS; b++) {
        bins[b].bmin[0] = bins[b].bmin[1] = bins[b].bmin[2] = 1e30f;
        bins[b].bmax[0] = bins[b].bmax[1] = bins[b].bmax[2] = -1e30f;
        bins[b].count = 0;
    }

    float scale = (float)SAH_BINS / (cbmax[axis] - cbmin[axis]);
    for (int i = start; i < start + count; i++) {
        float c = centroid_axis(tris[i], axis);
        int b = std::min(SAH_BINS - 1, (int)((c - cbmin[axis]) * scale));
        bins[b].count++;
        expand_point(bins[b].bmin, bins[b].bmax, tris[i].v0);
        expand_point(bins[b].bmin, bins[b].bmax, tris[i].v1);
        expand_point(bins[b].bmin, bins[b].bmax, tris[i].v2);
    }

    // Sweep to find best split
    float leaf_cost = (float)count * surface_area(cbmin, cbmax);
    float best_cost = 1e30f;
    int best_split = -1;

    float lmin[3] = {1e30f, 1e30f, 1e30f};
    float lmax[3] = {-1e30f, -1e30f, -1e30f};
    int lcount = 0;

    float rmin[3] = {1e30f, 1e30f, 1e30f};
    float rmax[3] = {-1e30f, -1e30f, -1e30f};
    int rcount = 0;
    for (int b = 0; b < SAH_BINS; b++) {
        if (bins[b].count > 0) {
            expand_aabb(rmin, rmax, bins[b].bmin, bins[b].bmax);
            rcount += bins[b].count;
        }
    }

    for (int b = 0; b < SAH_BINS - 1; b++) {
        if (bins[b].count > 0) {
            expand_aabb(lmin, lmax, bins[b].bmin, bins[b].bmax);
            lcount += bins[b].count;
        }
        rcount -= bins[b].count;
        if (lcount == 0 || rcount == 0) continue;

        float la = surface_area(lmin, lmax);
        float ra = surface_area(rmin, rmax);
        float cost = (float)lcount * la + (float)rcount * ra;
        if (cost < best_cost) {
            best_cost = cost;
            best_split = b;
        }
    }

    if (best_cost >= leaf_cost || best_split < 0) {
        int idx = (int)nodes_.size();
        FlatNode n;
        n.bmin[0] = cbmin[0]; n.bmin[1] = cbmin[1]; n.bmin[2] = cbmin[2];
        n.bmax[0] = cbmax[0]; n.bmax[1] = cbmax[1]; n.bmax[2] = cbmax[2];
        n.leftFirst = start;
        n.count = count;
        n.axis = 0;
        nodes_.push_back(n);
        return idx;
    }

    // Partition triangles around split plane
    float split_pos = cbmin[axis] + (best_split + 1) * (cbmax[axis] - cbmin[axis]) / (float)SAH_BINS;
    auto it = std::partition(tris.begin() + start, tris.begin() + start + count,
        [&](const Triangle& t) { return centroid_axis(t, axis) < split_pos; });
    int mid = (int)(it - tris.begin());
    if (mid == start || mid == start + count) {
        mid = start + count / 2; // fallback to median
    }

    // Reserve this node's slot (right child will be left+1)
    int idx = (int)nodes_.size();
    nodes_.emplace_back(); // placeholder

    int left = build_sah(tris, start, mid - start);
    int right = build_sah(tris, mid, start + count - mid);

    // Compute combined bounds
    nodes_[idx].bmin[0] = std::min(nodes_[left].bmin[0], nodes_[right].bmin[0]);
    nodes_[idx].bmin[1] = std::min(nodes_[left].bmin[1], nodes_[right].bmin[1]);
    nodes_[idx].bmin[2] = std::min(nodes_[left].bmin[2], nodes_[right].bmin[2]);
    nodes_[idx].bmax[0] = std::max(nodes_[left].bmax[0], nodes_[right].bmax[0]);
    nodes_[idx].bmax[1] = std::max(nodes_[left].bmax[1], nodes_[right].bmax[1]);
    nodes_[idx].bmax[2] = std::max(nodes_[left].bmax[2], nodes_[right].bmax[2]);
    nodes_[idx].leftFirst = left;
    nodes_[idx].count = 0; // internal
    nodes_[idx].axis = (unsigned char)axis;
    return idx;
}

// --- Build SoA float data for SSE ---
void Mesh::build_soa() {
    int n = (int)triangles_.size();
    int packets = (n + 3) / 4;
    soa_.resize(packets * 36, 0.0f);
    for (int i = 0; i < n; i++) {
        int p = i / 4;
        int lane = i % 4;
        const Triangle& t = triangles_[i];
        float* base = soa_.data() + p * 36;
        // v0
        base[0 * 4 + lane] = (float)t.v0.x;
        base[1 * 4 + lane] = (float)t.v0.y;
        base[2 * 4 + lane] = (float)t.v0.z;
        // e1 = v1 - v0
        base[3 * 4 + lane] = (float)(t.v1.x - t.v0.x);
        base[4 * 4 + lane] = (float)(t.v1.y - t.v0.y);
        base[5 * 4 + lane] = (float)(t.v1.z - t.v0.z);
        // e2 = v2 - v0
        base[6 * 4 + lane] = (float)(t.v2.x - t.v0.x);
        base[7 * 4 + lane] = (float)(t.v2.y - t.v0.y);
        base[8 * 4 + lane] = (float)(t.v2.z - t.v0.z);
    }
}

// --- Mesh constructor ---
Mesh::Mesh(std::vector<Triangle> tris, const Vec3& col)
    : color(col), triangles_(std::move(tris)) {
    nodes_.reserve(triangles_.size() * 2);
    build_sah(triangles_, 0, (int)triangles_.size());
    build_soa();
}

Vec3 Mesh::get_center() const {
    if (nodes_.empty()) return anim_offset();
    const FlatNode& r = nodes_[0];
    Vec3 c((r.bmin[0] + r.bmax[0]) * 0.5,
           (r.bmin[1] + r.bmax[1]) * 0.5,
           (r.bmin[2] + r.bmax[2]) * 0.5);
    return c + anim_offset();
}

// --- SSE4-wide ray-triangle intersection ---
#ifdef __SSE2__
static inline bool sse_test_packet(
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    const float* pk, float tmin, float& closest_t, int& hit_lane)
{
    __m128 v0x = _mm_loadu_ps(pk + 0);
    __m128 v0y = _mm_loadu_ps(pk + 4);
    __m128 v0z = _mm_loadu_ps(pk + 8);
    __m128 e1x = _mm_loadu_ps(pk + 12);
    __m128 e1y = _mm_loadu_ps(pk + 16);
    __m128 e1z = _mm_loadu_ps(pk + 20);
    __m128 e2x = _mm_loadu_ps(pk + 24);
    __m128 e2y = _mm_loadu_ps(pk + 28);
    __m128 e2z = _mm_loadu_ps(pk + 32);

    __m128 OX = _mm_set1_ps(ox), OY = _mm_set1_ps(oy), OZ = _mm_set1_ps(oz);
    __m128 DX = _mm_set1_ps(dx), DY = _mm_set1_ps(dy), DZ = _mm_set1_ps(dz);
    __m128 ZERO = _mm_setzero_ps();
    __m128 ONE  = _mm_set1_ps(1.0f);
    __m128 EPS  = _mm_set1_ps(1e-7f);
    __m128 ABS  = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    __m128 TMIN = _mm_set1_ps(tmin);
    __m128 TMAX = _mm_set1_ps(closest_t);

    // pvec = cross(D, e2)
    __m128 pvx = _mm_sub_ps(_mm_mul_ps(DY, e2z), _mm_mul_ps(DZ, e2y));
    __m128 pvy = _mm_sub_ps(_mm_mul_ps(DZ, e2x), _mm_mul_ps(DX, e2z));
    __m128 pvz = _mm_sub_ps(_mm_mul_ps(DX, e2y), _mm_mul_ps(DY, e2x));

    // det = dot(e1, pvec)
    __m128 det = _mm_add_ps(_mm_add_ps(_mm_mul_ps(e1x, pvx), _mm_mul_ps(e1y, pvy)),
                            _mm_mul_ps(e1z, pvz));
    __m128 det_ok = _mm_cmpge_ps(_mm_and_ps(det, ABS), EPS);

    __m128 inv_det = _mm_div_ps(_mm_set1_ps(1.0f), det);

    // tvec = O - v0
    __m128 tvx = _mm_sub_ps(OX, v0x);
    __m128 tvy = _mm_sub_ps(OY, v0y);
    __m128 tvz = _mm_sub_ps(OZ, v0z);

    // u = dot(tvec, pvec) * inv_det
    __m128 u = _mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(tvx, pvx),
                                                  _mm_mul_ps(tvy, pvy)),
                                      _mm_mul_ps(tvz, pvz)), inv_det);
    __m128 u_ok = _mm_and_ps(_mm_cmpge_ps(u, ZERO), _mm_cmple_ps(u, ONE));

    // qvec = cross(tvec, e1)
    __m128 qvx = _mm_sub_ps(_mm_mul_ps(tvy, e1z), _mm_mul_ps(tvz, e1y));
    __m128 qvy = _mm_sub_ps(_mm_mul_ps(tvz, e1x), _mm_mul_ps(tvx, e1z));
    __m128 qvz = _mm_sub_ps(_mm_mul_ps(tvx, e1y), _mm_mul_ps(tvy, e1x));

    // v = dot(D, qvec) * inv_det
    __m128 v = _mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(DX, qvx),
                                                  _mm_mul_ps(DY, qvy)),
                                      _mm_mul_ps(DZ, qvz)), inv_det);
    __m128 v_ok = _mm_and_ps(_mm_cmpge_ps(v, ZERO), _mm_cmple_ps(_mm_add_ps(u, v), ONE));

    // t = dot(e2, qvec) * inv_det
    __m128 t = _mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(e2x, qvx),
                                                  _mm_mul_ps(e2y, qvy)),
                                      _mm_mul_ps(e2z, qvz)), inv_det);
    __m128 t_ok = _mm_and_ps(_mm_cmpgt_ps(t, TMIN), _mm_cmplt_ps(t, TMAX));

    __m128 mask = _mm_and_ps(_mm_and_ps(det_ok, u_ok), _mm_and_ps(v_ok, t_ok));
    int m = _mm_movemask_ps(mask);
    if (m == 0) return false;

    // Find closest hit lane
    float tvals[4];
    _mm_storeu_ps(tvals, t);
    int best = -1;
    for (int i = 0; i < 4; i++) {
        if ((m & (1 << i)) && tvals[i] < closest_t) {
            closest_t = tvals[i];
            best = i;
        }
    }
    if (best < 0) return false;
    hit_lane = best;
    return true;
}
#endif

// --- BVH traversal ---
bool Mesh::hit(const Ray& r_in, double t_min, double t_max, HitRecord& rec) const {
    if (nodes_.empty()) return false;
    bool xf = anim_xform_active();
    Vec3 pivot((nodes_[0].bmin[0] + nodes_[0].bmax[0]) * 0.5,
               (nodes_[0].bmin[1] + nodes_[0].bmax[1]) * 0.5,
               (nodes_[0].bmin[2] + nodes_[0].bmax[2]) * 0.5);
    Ray r(r_in.origin() - anim_offset_, r_in.direction());
    if (xf) r = anim_local_ray(r_in, pivot);

    float closest_t = (float)t_max;
    bool hit_any = false;

#ifdef __SSE2__
    float rox = (float)r.origin().x, roy = (float)r.origin().y, roz = (float)r.origin().z;
    float rdx = (float)r.direction().x, rdy = (float)r.direction().y, rdz = (float)r.direction().z;
#endif

    // Stack-based traversal
    int stack[128];
    int sp = 0;
    stack[sp++] = 0; // root

    while (sp > 0) {
        int idx = stack[--sp];
        const FlatNode& node = nodes_[idx];

        // AABB slab test
        float tmin_f = (float)t_min;
        bool hit_box = true;
        for (int a = 0; a < 3; a++) {
            float invD = 1.0f / r.direction()[a];
            float t0 = (node.bmin[a] - r.origin()[a]) * invD;
            float t1 = (node.bmax[a] - r.origin()[a]) * invD;
            if (invD < 0) std::swap(t0, t1);
            tmin_f = std::max(t0, tmin_f);
            closest_t = std::min(t1, closest_t);
            if (closest_t <= tmin_f) { hit_box = false; break; }
        }
        if (!hit_box) continue;

        if (node.count > 0) {
            // Leaf: intersect triangles
            int first = node.leftFirst;
            int last = first + node.count;

            int i = first;
#ifdef __SSE2__
            // Process 4 triangles at a time with SSE
            for (; i + 4 <= last; i += 4) {
                int lane;
                float ct = closest_t;
                if (sse_test_packet(rox, roy, roz, rdx, rdy, rdz,
                                    soa_.data() + (i / 4) * 36,
                                    (float)t_min, ct, lane)) {
                    closest_t = ct;
                    hit_any = true;
                    int tri_idx = i + lane;
                    const Triangle& tri = triangles_[tri_idx];
                    HitRecord temp;
                    if (ray_tri_intersect(r, tri.v0, tri.v1, tri.v2, t_min, closest_t, temp)) {
                        closest_t = (float)temp.t;
                        rec = temp;
                    }
                }
            }
#endif
            // Scalar remainder for leftover triangles
            for (; i < last; i++) {
                const Triangle& tri = triangles_[i];
                HitRecord temp;
                if (ray_tri_intersect(r, tri.v0, tri.v1, tri.v2, t_min, closest_t, temp)) {
                    closest_t = (float)temp.t;
                    hit_any = true;
                    rec = temp;
                }
            }
        } else {
            // Internal node: push children (near-first order)
            int left = node.leftFirst;
            int right = left + 1;
            bool neg = r.direction()[node.axis] < 0;
            int near = neg ? right : left;
            int far  = neg ? left : right;
            stack[sp++] = far;
            stack[sp++] = near;
        }
    }

    if (hit_any) {
        if (xf) {
            rec.p = anim_world_point(rec.p, pivot);
            rec.normal = anim_world_normal(rec.normal);
            rec.set_face_normal(r_in, rec.normal);
        } else {
            rec.p = rec.p + anim_offset_;
        }
        rec.color = color;
        anim_color(rec);
        return true;
    }
    return false;
}

void Mesh::translate(const Vec3& d) {
    for (auto& t : triangles_) {
        t.v0 += d; t.v1 += d; t.v2 += d;
    }
    // keep the bind pose + stored joints aligned with the moved verts
    if (bind_) for (auto& t : bind_->tris) { t.v0 += d; t.v1 += d; t.v2 += d; }
    if (skeleton_) for (auto& b : skeleton_->bones) b.offset = b.offset + d;
    nodes_.clear();
    if (triangles_.empty()) return;
    auto tris = triangles_;
    // rebuild BVH
    std::vector<int> order(tris.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    build_sah(tris, 0, (int)tris.size());
    // build_sah uses triangles_? it modifies the passed vector in place via reorder logic; easier to rebuild from scratch
    nodes_.clear();
    triangles_ = std::move(tris);
    build_sah(triangles_, 0, (int)triangles_.size());
}

// ---- skeleton support ----
static double bone_now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Mesh::rebuild_bvh() {
    nodes_.clear();
    if (triangles_.empty()) return;
    auto tris = triangles_;
    build_sah(tris, 0, (int)tris.size());
    nodes_.clear();
    triangles_ = std::move(tris);
    build_sah(triangles_, 0, (int)triangles_.size());
}

void Mesh::bind_skeleton(Skeleton* sk) {
    // reset() would delete the old pointer UNCONDITIONALLY - re-binding the
    // skeleton we already own would free it out from under the caller (UAF).
    if (skeleton_.get() != sk) skeleton_.reset(sk);
    if (!sk) { bind_.reset(); return; }
    bind_ = std::make_unique<Bind>();
    bind_->tris = triangles_;
    // nearest bind-space joint per triangle corner (parents-first chain sum)
    std::vector<Vec3> joints(sk->bones.size());
    for (int i = 0; i < (int)sk->bones.size(); i++) joints[i] = sk->joint(i);
    bind_->bone.assign(bind_->tris.size() * 3, 0);
    for (size_t f = 0; f < bind_->tris.size(); f++) {
        const Triangle& t = bind_->tris[f];
        const Vec3* p[3] = {&t.v0, &t.v1, &t.v2};
        for (int c = 0; c < 3; c++) {
            double best = 0.0; int bi = 0; bool first = true;
            for (int i = 0; i < (int)joints.size(); i++) {
                double d = ((*p[c]) - joints[i]).length();
                if (first || d < best) { best = d; bi = i; first = false; }
            }
            bind_->bone[f * 3 + c] = bi;
        }
    }
}

void Mesh::set_bone_pose(int bone, const Vec3& rot, const Vec3& off) {
    if (!skeleton_ || bone < 0 || bone >= (int)skeleton_->bones.size()) return;
    Skeleton::Bone& b = skeleton_->bones[(size_t)bone];
    b.off = off;
    double mt[3][3];
    anim_euler_matrix(rot, b.m, mt);
    bvh_dirty_ = true;
}

void Mesh::apply_bones() {
    if (!skeleton_ || !bind_ || bind_->bone.empty()) return;
    const auto& bones = skeleton_->bones;
    int nb = (int)bones.size();
    // per-frame chain transforms (parents-first)
    std::vector<Vec3> wt((size_t)nb);
    std::vector<double> R((size_t)nb * 9);
    for (int i = 0; i < nb; i++) {
        const Skeleton::Bone& b = bones[(size_t)i];
        if (b.parent >= 0) {
            const double* P = &R[(size_t)b.parent * 9];
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) {
                    double s = 0.0;
                    for (int k = 0; k < 3; k++) s += P[r * 3 + k] * b.m[k][c];
                    R[(size_t)i * 9 + r * 3 + c] = s;
                }
            Vec3 op = wt[(size_t)b.parent];
            Vec3 offb = anim_mat_mul((const double(*)[3])&R[(size_t)b.parent * 9], b.offset + b.off);
            wt[(size_t)i] = op + offb;
        } else {
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) R[(size_t)i * 9 + r * 3 + c] = b.m[r][c];
            wt[(size_t)i] = b.offset + b.off;
        }
    }
    std::vector<Vec3> Jb((size_t)nb);
    for (int i = 0; i < nb; i++) Jb[(size_t)i] = skeleton_->joint(i);

    for (size_t f = 0; f < bind_->tris.size(); f++) {
        const Triangle& bt = bind_->tris[f];
        const int* pb = &bind_->bone[(size_t)f * 3];
        const Vec3* in[3] = {&bt.v0, &bt.v1, &bt.v2};
        Vec3* out[3] = {&triangles_[f].v0, &triangles_[f].v1, &triangles_[f].v2};
        for (int c = 0; c < 3; c++) {
            int bi = pb[c];
            Vec3 loc = (*in[c]) - Jb[(size_t)bi];
            (*out[c]) = wt[(size_t)bi] + anim_mat_mul((const double(*)[3])&R[(size_t)bi * 9], loc);
        }
    }
    if (bvh_dirty_) {
        double t = bone_now_s();
        if (t - last_bvh_rebuild_ > 0.25 || last_bvh_rebuild_ == 0.0) {
            rebuild_bvh();
            last_bvh_rebuild_ = t;
            bvh_dirty_ = false;
        }
    }
}

void Mesh::remap_bone_binds(int removed, int replacement) {
    if (!bind_) return;
    auto fix = [&](int& e) {
        if (e == removed) e = replacement < 0 ? 0 : replacement;
        else if (e > removed) e--;
    };
    for (auto& e : bind_->bone) fix(e);
}

Vec3 Mesh::bone_anchor() const {
    if (triangles_.empty()) return get_center();
    double lx = 1e30, ly = 1e30, lz = 1e30, hx = -1e30, hy = -1e30, hz = -1e30;
    for (const auto& t : triangles_) {
        const Vec3* p[3] = {&t.v0, &t.v1, &t.v2};
        for (int c = 0; c < 3; c++) {
            double x = p[c]->x, y = p[c]->y, z = p[c]->z;
            if (x < lx) lx = x;
            if (x > hx) hx = x;
            if (y < ly) ly = y;
            if (y > hy) hy = y;
            if (z < lz) lz = z;
            if (z > hz) hz = z;
        }
    }
    return Vec3((lx + hx) * 0.5, (ly + hy) * 0.5, (lz + hz) * 0.5);
}

double Mesh::bone_span() const {
    if (triangles_.empty()) return 1.0;
    double lo = 1e30, hi = -1e30;
    for (const auto& t : triangles_) {
        for (int c = 0; c < 3; c++) {
            double y = c == 0 ? t.v0.y : (c == 1 ? t.v1.y : t.v2.y);
            if (y < lo) lo = y;
            if (y > hi) hi = y;
        }
    }
    double s = hi - lo;
    return s > 0.05 ? s : 1.0;
}

void Mesh::reset_bones() {
    if (skeleton_) for (auto& b : skeleton_->bones) b.clear_pose();
    if (bind_) {
        triangles_ = bind_->tris;
        rebuild_bvh();
        bvh_dirty_ = false;
    }
}
