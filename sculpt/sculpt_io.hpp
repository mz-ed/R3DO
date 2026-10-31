#ifndef SCULPT_IO_H
#define SCULPT_IO_H

#include "sculpt_mesh.hpp"
#include <string>

class SculptIO {
public:
    static bool save_sculpt(const SculptMesh& m, const std::string& filename);
    static SculptMesh* load_sculpt(const std::string& filename);
};

#endif
