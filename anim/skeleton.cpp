#include "skeleton.hpp"
#include "hittable.hpp"
#include <cmath>
#include <vector>

// ---- shared skeletal math ----
// Used by every skinned object (Mesh::apply_bones, SculptMesh::apply_bones)
// and by bind refresh; single source of truth for chain transform composition.

void bone_chain_transforms(const Skeleton& sk,
                           std::vector<Vec3>& wt,
                           std::vector<double>& R) {
    int nb = (int)sk.bones.size();
    wt.assign((size_t)nb, Vec3(0, 0, 0));
    R.assign((size_t)nb * 9, 0.0);
    for (int i = 0; i < nb; i++) {
        const Skeleton::Bone& b = sk.bones[(size_t)i];
        if (b.parent >= 0) {
            const double* P = &R[(size_t)b.parent * 9];
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) {
                    double s = 0.0;
                    for (int k = 0; k < 3; k++) s += P[r * 3 + k] * b.m[k][c];
                    R[(size_t)i * 9 + r * 3 + c] = s;
                }
            Vec3 op = wt[(size_t)b.parent];
            Vec3 offb = anim_mat_mul((const double(*)[3])&R[(size_t)b.parent * 9],
                                     b.offset + b.off);
            wt[(size_t)i] = op + offb;
        } else {
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++)
                    R[(size_t)i * 9 + r * 3 + c] = b.m[r][c];
            wt[(size_t)i] = b.offset + b.off;
        }
    }
}

void bone_rest_joints(const Skeleton& sk, std::vector<Vec3>& joints) {
    joints.resize(sk.bones.size());
    for (int i = 0; i < (int)sk.bones.size(); i++)
        joints[(size_t)i] = sk.joint(i);
}

Vec3 bone_euler_rad(const double m[3][3]) {
    double sy = -m[2][0];
    if (sy > 1.0) sy = 1.0;
    else if (sy < -1.0) sy = -1.0;
    double y = std::asin(sy);
    double x, z;
    if (std::fabs(std::cos(y)) > 1e-9) {
        x = std::atan2(m[2][1], m[2][2]);
        z = std::atan2(m[1][0], m[0][0]);
    } else {
        // gimbal lock (|y| = pi/2): z is degenerate, pin it to 0 and solve x
        // from the remaining non-degenerate entries
        z = 0.0;
        x = std::atan2(-m[1][2], m[1][1]);
    }
    return Vec3(x, y, z);
}

double bone_blend_weight(double d0, int b0, double d1, int b1,
                         const Skeleton& sk) {
    auto bone_of = [&](int b) -> const Skeleton::Bone* {
        return (b >= 0 && b < (int)sk.bones.size())
                   ? &sk.bones[(size_t)b] : nullptr;
    };
    const Skeleton::Bone* B0 = bone_of(b0);
    const Skeleton::Bone* B1 = bone_of(b1);
    double r0 = B0 ? B0->influence : 0.0;
    double r1 = B1 ? B1->influence : 0.0;
    if (r0 <= 1e-6 && r1 <= 1e-6) {
        // legacy: inverse-distance blend w0 = (1/d0)/(1/d0 + 1/d1) = d1/(d0+d1)
        if (d1 <= 1e-12) return 1.0;
        return d1 / (d0 + d1);
    }
    auto falloff = [&](double d, double r, const Skeleton::Bone* B) {
        if (r <= 1e-6) return 1.0; // unlimited reach
        double t = 1.0 - d / r;
        if (t <= 0.0) return 0.0;
        double p = B ? B->sharp : 2.0;
        return std::pow(t, p);
    };
    double f0 = falloff(d0, r0, B0);
    double f1 = falloff(d1, r1, B1);
    if (f0 + f1 <= 1e-9) return 1.0; // beyond every radius: nearest wins
    return f0 / (f0 + f1);
}
