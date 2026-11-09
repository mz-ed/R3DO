#include "skeleton.hpp"
#include "hittable.hpp"
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
