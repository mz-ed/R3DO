#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "mesh.hpp"

Mesh* load_obj(const char* filename, const Vec3& color,
               const Vec3& center, double scale);

#endif
