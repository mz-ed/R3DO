#ifndef SKELETON_H
#define SKELETON_H

#include "v3.hpp"
#include <string>
#include <vector>

// Per-object bone chain. Offsets are joints in the object's local (bind)
// space; a vertex bound to a bone moves with the chain that leads to it.
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

#endif
