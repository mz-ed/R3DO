#include "sculpt_mesh.hpp"
#include "mesh.hpp" // for ray_tri_intersect
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
    if (verts_.empty()) return Vec3(0,0,0);
    Vec3 s(0,0,0); for (const auto& v: verts_) s+=v.pos; return s/(double)verts_.size();
}
void SculptMesh::translate(const Vec3& d){ for (auto& v: verts_) v.pos+=d; }

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

bool SculptMesh::hit(const Ray& r,double t_min,double t_max,HitRecord& rec) const {
    if (fn2_.empty()||faces_.empty()){
        bool any=false; double cl=t_max; HitRecord tmp;
        for(const auto& f:faces_){ if(ray_tri_intersect(r,verts_[f.v0].pos,verts_[f.v1].pos,verts_[f.v2].pos,t_min,cl,tmp)){ any=true; cl=tmp.t; rec=tmp;}}
        if(any){ rec.color=color_; return true;} return false;
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
    if(any){ rec.color=color_; return true;} return false;
}

bool SculptMesh::export_obj(const std::string& fn) const {
    std::ofstream out(fn); if(!out) return false;
    for(const auto& v: verts_) out<<"v "<<v.pos.x<<" "<<v.pos.y<<" "<<v.pos.z<<"\n";
    for(const auto& v: verts_) out<<"vn "<<v.normal.x<<" "<<v.normal.y<<" "<<v.normal.z<<"\n";
    for(const auto& f: faces_) out<<"f "<<(f.v0+1)<<"//"<<(f.v0+1)<<" "<<(f.v1+1)<<"//"<<(f.v1+1)<<" "<<(f.v2+1)<<"//"<<(f.v2+1)<<"\n";
    return true;
}
