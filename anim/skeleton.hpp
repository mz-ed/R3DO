#ifndef SKELETON_H
#define SKELETON_H

#include "v3.hpp"
#include <string>
#include <vector>

// Per-object bone chain. Offsets are joints in the object's local (bind)
// space; a vertex bound to a bone moves with the chain that leads to it.
//
// Lives with the animation stack (see animation.hpp in this folder): clips key
// bones through FREE_BONE targets and the pose code here feeds every skinned
// object (Mesh / SculptMesh) through bone_chain_transforms().

struct Skeleton {
    struct Bone {
        int parent = -1;
        std::string name;
        Vec3 offset;                      // bind-space joint position
        Vec3 off = Vec3(0, 0, 0);         // live extra translation from keys
        double m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};  // live euler rot

        void clear_pose() {
            off = Vec3(0, 0, 0);
            m[0][0] = 1; m[0][1] = 0; m[0][2] = 0;
            m[1][0] = 0; m[1][1] = 1; m[1][2] = 0;
            m[2][0] = 0; m[2][1] = 0; m[2][2] = 1;
        }
    };

    std::vector<Bone> bones;

    // bumped by every pose write (set_bone_pose / reset / pose drags); bound
    // objects compare it against their last applied version so pose changes
    // from any source reach the render in one place (UI::ensure_bones style
    // sweep in main.cpp)
    int pose_version = 0;

    void touch_pose() { pose_version++; }
    bool bones_posed() const {
        for (const auto& b : bones) {
            if (b.off.length() > 1e-9) return true;
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) {
                    double want = r == c ? 1.0 : 0.0;
                    if (b.m[r][c] != want) return true;
                }
        }
        return false;
    }

    // Bind-space joint position of a bone (sum of the chain's offsets).
    Vec3 joint(int b) const {
        Vec3 p(0, 0, 0);
        for (int i = b; i >= 0; i = bones[(size_t)i].parent)
            p = p + bones[(size_t)i].offset;
        return p;
    }

    // Parents-first guarantee matters: apply iterates in order and chains
    // through parent transforms already computed this frame.
    void add(int parent, const std::string& name, const Vec3& offset) {
        Bone b;
        b.parent = parent;
        b.name = name;
        b.offset = offset;
        bones.push_back(b);
    }
};

// ---- shared chain math (anim/skeleton.cpp) ----
// Builds per-bone world chain transforms for the current pose: wt[i] is the
// posed joint position and R is a 9-float row-major rotation per bone.
// Parents-first: later bones reuse earlier results.
void bone_chain_transforms(const Skeleton& sk,
                           std::vector<Vec3>& wt,
                           std::vector<double>& R);

// Bind-space (rest) joint positions for every bone.
void bone_rest_joints(const Skeleton& sk, std::vector<Vec3>& joints);

#endif
