#include "sculpt_mesh.hpp"
#include "mesh.hpp" // for ray_tri_intersect
#include <chrono>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <vector>
#include <fstream>
#include <limits>
#include <string>

static inline Vec3 tri_normalv(const Vec3& v0, const Vec3& v1, const Vec3& v2) {
    Vec3 e1 = v1 - v0;
    Vec3 e2 = v2 - v0;
    Vec3 n = cross(e1, e2);
    double len = n.length();
    return len < 1e-12 ? Vec3(0,0,0) : n / len;
}

SculptMesh::SculptMesh() : color_(Vec3(0.8,0.7,0.5)) {}

static SculptMesh* make_mesh() { return new SculptMesh(); }

SculptMesh* SculptMesh::create_sphere(const Vec3& center, double radius, int segments, int rings, const Vec3& color) {
    auto* m = new SculptMesh();
    m->color_ = color;
    segments = std::max(4, segments); rings = std::max(3, rings);
    for (int r = 0; r <= rings; r++) {
        double theta = M_PI*r/rings;
        double st = sin(theta), ct = cos(theta);
        for (int s = 0; s < segments; s++) {
            double phi = 2.0*M_PI*s/segments;
            double sp = sin(phi), cp = cos(phi);
            Vec3 p = Vec3(cp*st, ct, sp*st)*radius + center;
            m->verts_.push_back({p, Vec3(cp*st,ct,sp*st)});
        }
    }
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            int a = r*segments+s, b = r*segments+(s+1)%segments;
            int c = (r+1)*segments+(s+1)%segments, d = (r+1)*segments+s;
            m->faces_.push_back({a,b,c});
            if (r < rings-1) m->faces_.push_back({a,c,d});
        }
    }
    m->rebuild_normals(); m->rebuild_topology(true); m->rebuild_bvh();
    return m;
}

SculptMesh* SculptMesh::create_cube(const Vec3& center, double size, const Vec3& color) {
    auto* m = new SculptMesh(); m->color_=color; double hs=size*0.5;
    Vec3 vlist[8] = {{-hs,-hs,-hs},{hs,-hs,-hs},{hs,hs,-hs},{-hs,hs,-hs},{-hs,-hs,hs},{hs,-hs,hs},{hs,hs,hs},{-hs,hs,hs}};
    for (int i=0;i<8;i++) m->verts_.push_back({vlist[i]+center, Vec3(0,0,0)});
    int f[12][3] = {{0,1,2},{0,2,3},{1,5,6},{1,6,2},{5,4,7},{5,7,6},{4,0,3},{4,3,7},{3,2,6},{3,6,7},{4,5,1},{4,1,0}};
    for (int i=0;i<12;i++) m->faces_.push_back({f[i][0],f[i][1],f[i][2]});
    m->rebuild_normals(); m->rebuild_topology(true); m->rebuild_bvh();
    return m;
}

SculptMesh* SculptMesh::create_plane(const Vec3& center, double size, int div, const Vec3& color) {
    auto* m=new SculptMesh(); m->color_=color; div=std::max(1,div); int d=div; double hs=size*0.5, step=size/d;
    for (int y=0;y<=d;y++) for (int x=0;x<=d;x++) m->verts_.push_back({center+Vec3(x*step-hs,0,y*step-hs), Vec3(0,1,0)});
    for (int y=0;y<d;y++) for (int x=0;x<d;x++) {
        int a=y*(d+1)+x,b=a+1,c=(y+1)*(d+1)+1+x,dd=(y+1)*(d+1)+x;
        m->faces_.push_back({a,b,c}); m->faces_.push_back({a,c,dd});
    }
    m->rebuild_normals(); m->rebuild_topology(true); m->rebuild_bvh();
    return m;
}

SculptMesh* SculptMesh::create_torus(const Vec3& center, double major,double minor,int ma,int mi,const Vec3& color) {
    auto* m=new SculptMesh(); m->color_=color; ma=std::max(4,ma); mi=std::max(4,mi);
    for (int j=0;j<mi;j++) { double phi=2*M_PI*j/mi; for (int i=0;i<ma;i++) { double th=2*M_PI*i/ma; double cx=major*cos(th), cy=major*sin(th); Vec3 p=Vec3(cx+minor*cos(phi)*cos(th), cy+minor*cos(phi)*sin(th), minor*sin(phi)); m->verts_.push_back({p+center, Vec3(0,0,0)}); }}
    for (int i=0;i<ma;i++) for (int j=0;j<mi;j++) {
        int a=i+j*ma,b=(i+1)%ma+j*ma,c=(i+1)%ma+((j+1)%mi)*ma,d=i+((j+1)%mi)*ma;
        m->faces_.push_back({a,b,c}); m->faces_.push_back({a,c,d});
    }
    m->rebuild_normals(); m->rebuild_topology(true); m->rebuild_bvh();
    return m;
}

void SculptMesh::rebuild_normals() {
    for (auto& v: verts_) v.normal = Vec3(0,0,0);
    for (const auto& f: faces_) {
        if (f.v0<0||f.v1<0||f.v2<0||f.v0>=int(verts_.size())||f.v1>=int(verts_.size())||f.v2>=int(verts_.size())) continue;
        Vec3 n = tri_normalv(verts_[f.v0].pos, verts_[f.v1].pos, verts_[f.v2].pos);
        verts_[f.v0].normal += n; verts_[f.v1].normal += n; verts_[f.v2].normal += n;
    }
    for (auto& v: verts_) { double len=v.normal.length(); v.normal = len>1e-12 ? v.normal/len : Vec3(0,1,0); }
}

void SculptMesh::rebuild_topology(bool) {
    adj_faces_.assign(verts_.size(), std::vector<int>());
    for (size_t i=0;i<faces_.size();i++) {
        auto& f=faces_[i];
        if (f.v0>=0&&f.v0<int(adj_faces_.size())) adj_faces_[f.v0].push_back(int(i));
        if (f.v1>=0&&f.v1<int(adj_faces_.size())) adj_faces_[f.v1].push_back(int(i));
        if (f.v2>=0&&f.v2<int(adj_faces_.size())) adj_faces_[f.v2].push_back(int(i));
    }
}

Vec3 SculptMesh::get_center() const {
    if (verts_.empty()) return anim_offset();
    Vec3 s(0,0,0); for (const auto& v: verts_) s+=v.pos;
    Vec3 c = s/(double)verts_.size();
    return c + anim_offset();
}

void SculptMesh::remesh_subdivide() {
    if (faces_.empty()) return;
    
    std::vector<SculptVertex> new_verts = verts_;
    std::vector<SculptTriangle> new_faces;
    new_faces.reserve(faces_.size() * 4);
    
    // For each triangle, create 3 midpoints and split into 4 triangles
    for (const auto& f : faces_) {
        int i0 = f.v0, i1 = f.v1, i2 = f.v2;
        if (i0 < 0 || i1 < 0 || i2 < 0 || 
            i0 >= (int)verts_.size() || i1 >= (int)verts_.size() || i2 >= (int)verts_.size())
            continue;
        
        // Create midpoints
        Vec3 p01 = (verts_[i0].pos + verts_[i1].pos) * 0.5;
        Vec3 p12 = (verts_[i1].pos + verts_[i2].pos) * 0.5;
        Vec3 p20 = (verts_[i2].pos + verts_[i0].pos) * 0.5;
        
        Vec3 n01 = unit_vector(verts_[i0].normal + verts_[i1].normal);
        Vec3 n12 = unit_vector(verts_[i1].normal + verts_[i2].normal);
        Vec3 n20 = unit_vector(verts_[i2].normal + verts_[i0].normal);
        
        int m01 = (int)new_verts.size(); new_verts.push_back({p01, n01});
        int m12 = (int)new_verts.size(); new_verts.push_back({p12, n12});
        int m20 = (int)new_verts.size(); new_verts.push_back({p20, n20});
        
        // 4 sub-triangles
        new_faces.push_back({i0, m01, m20});
        new_faces.push_back({m01, i1, m12});
        new_faces.push_back({m20, m12, i2});
        new_faces.push_back({m01, m12, m20});
    }
    
    verts_ = std::move(new_verts);
    faces_ = std::move(new_faces);
    
    rebuild_normals();
    rebuild_topology(true);
    rebuild_bvh();
}

void SculptMesh::translate(const Vec3& d){
    for (auto& v: verts_) v.pos += d;
    // keep the bind pose + stored joints aligned with the moved verts
    for (auto& v : bind_verts_) v.pos += d;
    if (skeleton_) for (auto& b : skeleton_->bones) b.offset = b.offset + d;
    // Keep the flat BVH in sync without a rebuild: a pure translation shifts
    // every node box by d and preserves the tree structure, so leaf boxes stay
    // valid and the mesh stays visible at its new location.
    for (auto& n : fn2_) {
        n.bmin[0] += (float)d.x; n.bmin[1] += (float)d.y; n.bmin[2] += (float)d.z;
        n.bmax[0] += (float)d.x; n.bmax[1] += (float)d.y; n.bmax[2] += (float)d.z;
    }
}

static inline void fe(float bmin[3],float bmax[3],const Vec3& p){ float v[3]={float(p.x),float(p.y),float(p.z)}; for(int a=0;a<3;a++){ if(v[a]<bmin[a])bmin[a]=v[a]; if(v[a]>bmax[a])bmax[a]=v[a];}}
static inline float sa(const float bmin[3],float bmax[3]){ float dx=bmax[0]-bmin[0],dy=bmax[1]-bmin[1],dz=bmax[2]-bmin[2]; return dx*dy+dy*dz+dz*dx;}
static inline float ca_c(const Vec3& v0,const Vec3& v1,const Vec3& v2,int a){ return float((v0[a]+v1[a]+v2[a])/3.0);}

void SculptMesh::rebuild_bvh() {
    bnodes_.clear(); fn2_.clear(); tri_list_.clear();
    if (faces_.empty()) return;
    tri_list_.resize(faces_.size()); for (size_t i=0;i<faces_.size();i++) tri_list_[i]=(int)i;
    std::function<int(int,int)> rec = [&](int s,int c)->int {
        if (c<=0) return -1;
        BN bn; for(int a=0;a<3;a++){ bn.bmin[a]=1e30f; bn.bmax[a]=-1e30f; }
        for(int i=s;i<s+c;i++){ int t=tri_list_[i]; const auto& f=faces_[t]; fe(bn.bmin,bn.bmax,verts_[f.v0].pos); fe(bn.bmin,bn.bmax,verts_[f.v1].pos); fe(bn.bmin,bn.bmax,verts_[f.v2].pos); }
        if (c<=4){ bn.leaf=true; bn.start=s; bn.count=c; bn.left=bn.right=-1; bn.axis=0; int id=int(bnodes_.size()); bnodes_.push_back(bn); return id; }
        float cbmin[3]={1e30f,1e30f,1e30f}, cbmax[3]={-1e30f,-1e30f,-1e30f};
        for(int i=s;i<s+c;i++){ int t=tri_list_[i]; const auto& f=faces_[t]; for(int a=0;a<3;a++){                 float cc=ca_c(verts_[f.v0].pos,verts_[f.v1].pos,verts_[f.v2].pos,a); if(cc<cbmin[a])cbmin[a]=cc; if(cc>cbmax[a])cbmax[a]=cc;}}
        int axis=0; float maxe=cbmax[0]-cbmin[0]; if(cbmax[1]-cbmin[1]>maxe){axis=1;maxe=cbmax[1]-cbmin[1];} if(cbmax[2]-cbmin[2]>maxe){axis=2;maxe=cbmax[2]-cbmin[2];}
        float sp=cbmin[axis]+0.5f*(cbmax[axis]-cbmin[axis]);
            std::partition(tri_list_.begin()+s, tri_list_.begin()+s+c, [&](int t){ const auto& f=faces_[t]; return ca_c(verts_[f.v0].pos,verts_[f.v1].pos,verts_[f.v2].pos,axis)<sp; });
        int mid=s+c/2;
        bn.leaf=false; bn.axis=axis; bn.start=bn.count=0;
        int id=int(bnodes_.size()); bnodes_.push_back(bn);
        int L=rec(s,mid-s), R=rec(mid,s+c-mid);
        bnodes_[id].left=L; bnodes_[id].right=R; return id;
    };
    rec(0,int(tri_list_.size()));
    std::function<int(int)> flat=[&](int bi)->int{
        if (bi<0) return -1;
        const auto& bn=bnodes_[bi];
        int fi=int(fn2_.size());
        if (bn.leaf){ fn2_.push_back({{bn.bmin[0],bn.bmin[1],bn.bmin[2]},{bn.bmax[0],bn.bmax[1],bn.bmax[2]},-1,-1,bn.count,bn.start,(unsigned char)bn.axis,true}); return fi; }
        else { fn2_.push_back({{bn.bmin[0],bn.bmin[1],bn.bmin[2]},{bn.bmax[0],bn.bmax[1],bn.bmax[2]},-1,-1,0,0,(unsigned char)bn.axis,false}); int Lf=flat(bn.left), Rf=flat(bn.right); fn2_[fi].leftFirst=Lf; fn2_[fi].rightFirst=Rf; return fi; }
    };
    if (!bnodes_.empty()) flat(0);
}

void SculptMesh::refresh_bvh_bounds() {
    if (fn2_.empty() || faces_.empty()) return;
    for (int i = (int)fn2_.size() - 1; i >= 0; i--) {
        FN2& n = fn2_[(size_t)i];
        if (n.leaf) {
            for (int a = 0; a < 3; a++) { n.bmin[a] = 1e30f; n.bmax[a] = -1e30f; }
            for (int k = 0; k < n.count; k++) {
                int t = tri_list_[(size_t)n.start + (size_t)k];
                const auto& f = faces_[(size_t)t];
                fe(n.bmin, n.bmax, verts_[(size_t)f.v0].pos);
                fe(n.bmin, n.bmax, verts_[(size_t)f.v1].pos);
                fe(n.bmin, n.bmax, verts_[(size_t)f.v2].pos);
            }
        } else {
            const FN2& L = fn2_[(size_t)n.leftFirst];
            const FN2& R = fn2_[(size_t)n.rightFirst];
            for (int a = 0; a < 3; a++) {
                n.bmin[a] = std::min(L.bmin[a], R.bmin[a]);
                n.bmax[a] = std::max(L.bmax[a], R.bmax[a]);
            }
        }
    }
}

bool SculptMesh::hit(const Ray& r_in,double t_min,double t_max,HitRecord& rec) const {
    bool xf = anim_xform_active();
    Vec3 pivot;
    if (xf) {
        if (!fn2_.empty())
            pivot = Vec3((fn2_[0].bmin[0] + fn2_[0].bmax[0]) * 0.5,
                         (fn2_[0].bmin[1] + fn2_[0].bmax[1]) * 0.5,
                         (fn2_[0].bmin[2] + fn2_[0].bmax[2]) * 0.5);
        else
            pivot = get_center() - anim_offset();
    }
    Ray r(r_in.origin() - anim_offset_, r_in.direction());
    if (xf) r = anim_local_ray(r_in, pivot);
    if (fn2_.empty()||faces_.empty()){
        bool any=false; double cl=t_max; HitRecord tmp;
        for(const auto& f:faces_){ if(ray_tri_intersect(r,verts_[f.v0].pos,verts_[f.v1].pos,verts_[f.v2].pos,t_min,cl,tmp)){ any=true; cl=tmp.t; rec=tmp;}}
        if(any){
            if (xf) {
                rec.p = anim_world_point(rec.p, pivot);
                rec.normal = anim_world_normal(rec.normal);
                rec.set_face_normal(r_in, rec.normal);
            } else {
                rec.p = rec.p + anim_offset_;
            }
            rec.color=color_;
            anim_color(rec);
            return true;
        }
        return false;
    }
    struct St{ int n; double t0,t1;};
    int stack[256]; int sp=0; stack[sp++]=0;
    bool any=false; double cl=t_max; HitRecord tmp;
    while(sp>0){
        int ni=stack[--sp]; const auto& n=fn2_[ni];
        double nt0=t_min, nt1=cl;
        bool box=true;
        for(int a=0;a<3;a++){
            double invD=1.0/r.direction()[a];
            double t0v=(n.bmin[a]-r.origin()[a])*invD, t1v=(n.bmax[a]-r.origin()[a])*invD;
            if(invD<0) std::swap(t0v,t1v);
            nt0=std::max(t0v,nt0); nt1=std::min(t1v,nt1);
            if(nt1<=nt0){ box=false; break; }
        }
        if(!box) continue;
        if(n.leaf&&n.count>0){
            for(int i=0;i<n.count;i++){
                int t=tri_list_[n.start + i];
                const auto& f=faces_[t];
                if(ray_tri_intersect(r,verts_[f.v0].pos,verts_[f.v1].pos,verts_[f.v2].pos,t_min,cl,tmp)){ any=true; cl=tmp.t; rec=tmp;}
            }
        }else{
            bool neg=r.direction()[n.axis]<0;
            if(neg){ stack[sp++]=n.leftFirst; stack[sp++]=n.rightFirst; }
            else { stack[sp++]=n.rightFirst; stack[sp++]=n.leftFirst; }
        }
    }
    if(any){
        if (xf) {
            rec.p = anim_world_point(rec.p, pivot);
            rec.normal = anim_world_normal(rec.normal);
            rec.set_face_normal(r_in, rec.normal);
        } else {
            rec.p = rec.p + anim_offset_;
        }
        rec.color=color_;
        anim_color(rec);
        return true;
    }
    return false;
}

bool SculptMesh::export_obj(const std::string& fn) const {
    std::ofstream out(fn); if(!out) return false;
    for(const auto& v: verts_) out<<"v "<<v.pos.x<<" "<<v.pos.y<<" "<<v.pos.z<<"\n";
    for(const auto& v: verts_) out<<"vn "<<v.normal.x<<" "<<v.normal.y<<" "<<v.normal.z<<"\n";
    for(const auto& f: faces_) out<<"f "<<(f.v0+1)<<"//"<<(f.v0+1)<<" "<<(f.v1+1)<<"//"<<(f.v1+1)<<" "<<(f.v2+1)<<"//"<<(f.v2+1)<<"\n";
    return true;
}
bool SculptMesh::export_blob(const std::string& path) const {
    std::ofstream out(path);
    if (!out) return false;
    out << "R3SC1\n";
    out << verts_.size() << " " << faces_.size() << "\n";
    out.precision(9);
    for (const auto& v : verts_)
        out << v.pos.x << " " << v.pos.y << " " << v.pos.z << " "
            << v.normal.x << " " << v.normal.y << " " << v.normal.z << "\n";
    for (const auto& f : faces_)
        out << f.v0 << " " << f.v1 << " " << f.v2 << "\n";
    return (bool)out;
}

SculptMesh* SculptMesh::from_blob(const std::string& path, const Vec3& color) {
    std::ifstream in(path);
    if (!in) return nullptr;
    std::string magic;
    size_t nv = 0, nf = 0;
    if (!(in >> magic) || magic != "R3SC1") return nullptr;
    if (!(in >> nv >> nf)) return nullptr;
    auto* m = new SculptMesh();
    m->color_ = color;
    m->verts_.resize(nv);
    for (auto& v : m->verts_)
        if (!(in >> v.pos.x >> v.pos.y >> v.pos.z >>
                  v.normal.x >> v.normal.y >> v.normal.z)) {
            delete m; return nullptr;
        }
    m->faces_.resize(nf);
    for (auto& f : m->faces_)
        if (!(in >> f.v0 >> f.v1 >> f.v2)) {
            delete m; return nullptr;
        }
    m->rebuild_normals();
    m->rebuild_topology(true);
    m->rebuild_bvh();
    m->set_path(path);
    return m;
}

// ---- skeleton support ----
static double sculpt_bone_now_s() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static double avg_edge_len(const std::vector<SculptVertex>& vs,
                           const std::vector<SculptTriangle>& fs) {
    if (fs.empty()) return 0.0;
    auto seg = [&](int a, int b) -> double {
        if (a < 0 || b < 0 || a >= (int)vs.size() || b >= (int)vs.size()) return 0.0;
        return (vs[(size_t)a].pos - vs[(size_t)b].pos).length();
    };
    double sum = 0.0;
    for (const auto& f : fs) {
        sum += seg(f.v0, f.v1);
        sum += seg(f.v1, f.v2);
        sum += seg(f.v2, f.v0);
    }
    return sum / (double)(fs.size() * 3);
}

void SculptMesh::bind_skeleton(Skeleton* sk) {
    // reset() would delete the old pointer UNCONDITIONALLY - re-binding the
    // skeleton we already own would free it out from under the caller (UAF).
    if (skeleton_.get() != sk) skeleton_.reset(sk);
    if (!sk) { bind_verts_.clear(); bind_bone_.clear(); bind_bone2_.clear(); bind_wt_.clear(); return; }
    bind_verts_ = verts_;
    std::vector<Vec3> joints(sk->bones.size());
    for (int i = 0; i < (int)sk->bones.size(); i++) joints[i] = sk->joint(i);
    bind_bone_.assign(verts_.size(), 0);
    bind_bone2_.assign(verts_.size(), -1);
    bind_wt_.assign(verts_.size(), 255);
    for (size_t i = 0; i < verts_.size(); i++) {
        double best = 0.0, second = 0.0; int bi = 0, bj = -1; bool first = true;
        for (int j = 0; j < (int)joints.size(); j++) {
            double d = (verts_[i].pos - joints[j]).length();
            if (first || d < best) {
                if (!first) { second = best; bj = bi; }
                best = d; bi = j; first = false;
            } else if (bj < 0 || d < second) {
                second = d; bj = j;
            }
        }
        bind_bone_[i] = bi;
        if (bj >= 0 && second > 1e-12) {
            // two-joint blend softens the crease: legacy inverse-distance
            // by default, per-bone influence falloff when radii are set
            double w0 = bone_blend_weight(best, bi, second, bj, *sk);
            bind_bone2_[i] = bj;
            bind_wt_[i] = (unsigned char)(w0 * 255.0 + 0.5);
        }
    }
    bones_applied_ = skeleton_->pose_version; // bind pose == current pose
    stretch_cap_ = avg_edge_len(bind_verts_, faces_) * Hittable::kStretchEdgeFactor;
}

// Stretch-balloon handling for the POSE drag's REMESH mode: subdivision only
// makes sense while a pose is actually applied. Measures how far the current
// edges have stretched past the bind snapshot; past max_ratio the bind mesh
// is subdivided (so the rest shape and straighten stay intact), the skin is
// re-bound at the new density, and the live pose is re-derived.
bool SculptMesh::remesh_if_stretched(double max_ratio, size_t max_tris) {
    if (!skeleton_ || bind_verts_.empty()) return false;
    if (!skeleton_->bones_posed()) return false;
    if (faces_.empty() || bind_verts_.size() != verts_.size()) return false;
    double eb = avg_edge_len(bind_verts_, faces_);
    if (eb <= 1e-9) return false;
    double ec = avg_edge_len(verts_, faces_);
    if (ec < eb * max_ratio) return false;
    if (faces_.size() * 4 > max_tris) return false;
    verts_ = bind_verts_;
    remesh_subdivide();
    bind_skeleton(skeleton_.get());
    bvh_dirty_ = true;
    apply_bones();
    return true;
}

double SculptMesh::stretch_cap() const { return stretch_cap_; }

// Keep the bind snapshot in step with sculpting: without this, every pose
// apply (slider / pose drag / clip) would re-derive verts from the bind-time
// snapshot and erase anything sculpted after Add Skeleton / Re-link.
//  - chain at rest: fresh snapshot + nearest-joint re-pick (a bulge crossing
//    a bone boundary re-attaches to the closer joint automatically)
//  - pose active: keep assignments, invert the primary bone's transform per
//    vert so the sculpt lives in bind space and posing keeps working
void SculptMesh::refresh_bind_verts() {
    if (!skeleton_ || skeleton_->bones.empty()) return;
    if (verts_.empty() || bind_verts_.size() != verts_.size()) return;

    if (!skeleton_->bones_posed()) {
        bind_verts_ = verts_;
        std::vector<Vec3> joints;
        bone_rest_joints(*skeleton_, joints);
        for (size_t i = 0; i < verts_.size(); i++) {
            double best = 0.0, second = 0.0;
            int bi = 0, bj = -1; bool first = true;
            for (int j = 0; j < (int)joints.size(); j++) {
                double d = (verts_[i].pos - joints[j]).length();
                if (first || d < best) {
                    if (!first) { second = best; bj = bi; }
                    best = d; bi = j; first = false;
                } else if (bj < 0 || d < second) {
                    second = d; bj = j;
                }
            }
            bind_bone_[i] = bi;
            if (bj >= 0 && second > 1e-12) {
                double w0 = bone_blend_weight(best, bi, second, bj,
                                              *skeleton_);
                bind_bone2_[i] = bj;
                bind_wt_[i] = (unsigned char)(w0 * 255.0 + 0.5);
            } else {
                bind_bone2_[i] = -1;
                bind_wt_[i] = 255;
            }
        }
        return;
    }

    // posed: exact inverse through the primary bone per vert (blended verts
    // recover approximately through their dominant joint)
    int nb = (int)skeleton_->bones.size();
    std::vector<Vec3> wt((size_t)nb);
    std::vector<double> R((size_t)nb * 9);
    bone_chain_transforms(*skeleton_, wt, R);
    std::vector<Vec3> Jb((size_t)nb);
    bone_rest_joints(*skeleton_, Jb);
    for (size_t i = 0; i < verts_.size(); i++) {
        int bi = bind_bone_[i];
        const double* M = &R[(size_t)bi * 9];
        double Mt[9];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) Mt[r * 3 + c] = M[c * 3 + r];
        Vec3 rel = verts_[i].pos - wt[(size_t)bi];
        bind_verts_[i].pos = Jb[(size_t)bi] +
            anim_mat_mul((const double(*)[3])Mt, rel);
    }
}

void SculptMesh::set_bone_pose(int bone, const Vec3& rot, const Vec3& off) {
    if (!skeleton_ || bone < 0 || bone >= (int)skeleton_->bones.size()) return;
    Skeleton::Bone& b = skeleton_->bones[(size_t)bone];
    b.off = off;
    double mt[3][3];
    anim_euler_matrix(rot, b.m, mt);
    skeleton_->touch_pose();
    bvh_dirty_ = true;
}

void SculptMesh::apply_bones() {
    if (!skeleton_ || bind_bone_.empty() || bind_verts_.empty()) return;
    int nb = (int)skeleton_->bones.size();
    std::vector<Vec3> wt((size_t)nb);
    std::vector<double> R((size_t)nb * 9);
    bone_chain_transforms(*skeleton_, wt, R);
    std::vector<Vec3> Jb((size_t)nb);
    bone_rest_joints(*skeleton_, Jb);

    for (size_t i = 0; i < verts_.size(); i++) {
        int bi = bind_bone_[i];
        Vec3 loc = bind_verts_[i].pos - Jb[(size_t)bi];
        Vec3 offb = anim_mat_mul((const double(*)[3])&R[(size_t)bi * 9], loc);
        Vec3 np = wt[(size_t)bi] + offb;
        // two-joint blend: soften creases where fold regions meet
        int bj = bind_bone2_[i];
        if (bj >= 0 && bj != bi) {
            unsigned char wq = bind_wt_[i];
            if (wq < 255) {
                Vec3 loc2 = bind_verts_[i].pos - Jb[(size_t)bj];
                Vec3 off2 = anim_mat_mul((const double(*)[3])&R[(size_t)bj * 9], loc2);
                Vec3 np2 = wt[(size_t)bj] + off2;
                np = np + (np2 - np) * (1.0 - (double)wq / 255.0);
            }
        }
        verts_[i].pos = np;
    }
    rebuild_normals();
    if (bvh_dirty_) {
        double t = sculpt_bone_now_s();
        if (t - last_bvh_rebuild_ > 0.25 || last_bvh_rebuild_ == 0.0) {
            rebuild_bvh();
            last_bvh_rebuild_ = sculpt_bone_now_s();
        } else {
            refresh_bvh_bounds();
        }
        bvh_dirty_ = false;
    }
}

void SculptMesh::remap_bone_binds(int removed, int replacement) {
    auto fix = [&](int& e) {
        if (e == removed) e = replacement < 0 ? 0 : replacement;
        else if (e > removed) e--;
    };
    for (auto& e : bind_bone_) fix(e);
    for (auto& e : bind_bone2_) {
        if (e == removed) { e = -1; }  // dropped secondary: single-joint skin
        else if (e > removed) e--;
    }
}

Vec3 SculptMesh::bone_anchor() const {
    if (verts_.empty()) return get_center();
    double lx = 1e30, ly = 1e30, lz = 1e30, hx = -1e30, hy = -1e30, hz = -1e30;
    for (const auto& v : verts_) {
        double x = v.pos.x, y = v.pos.y, z = v.pos.z;
        if (x < lx) lx = x;
        if (x > hx) hx = x;
        if (y < ly) ly = y;
        if (y > hy) hy = y;
        if (z < lz) lz = z;
        if (z > hz) hz = z;
    }
    return Vec3((lx + hx) * 0.5, (ly + hy) * 0.5, (lz + hz) * 0.5);
}

double SculptMesh::bone_span() const {
    if (verts_.empty()) return 1.0;
    double lo = 1e30, hi = -1e30;
    for (const auto& v : verts_) {
        if (v.pos.y < lo) lo = v.pos.y;
        if (v.pos.y > hi) hi = v.pos.y;
    }
    double s = hi - lo;
    return s > 0.05 ? s : 1.0;
}

void SculptMesh::reset_bones() {
    if (skeleton_) {
        for (auto& b : skeleton_->bones) b.clear_pose();
        skeleton_->touch_pose();
    }
    if (!bind_verts_.empty()) {
        verts_ = bind_verts_;
        rebuild_normals();
        rebuild_topology(true);
        rebuild_bvh();
        bvh_dirty_ = false;
    }
}

int SculptMesh::bone_bound_count(int bone) const {
    int n = 0;
    for (size_t i = 0; i < bind_bone_.size(); i++)
        if (bind_bone_[i] == bone) n++;
    return n;
}

bool SculptMesh::bone_bound_vert(int bone, int idx, Vec3& out) const {
    int seen = 0;
    for (size_t i = 0; i < bind_bone_.size(); i++) {
        if (bind_bone_[i] != bone) continue;
        if (seen++ == idx) { out = verts_[i].pos; return true; }
    }
    return false;
}
