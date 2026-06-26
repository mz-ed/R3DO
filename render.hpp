#ifndef RENDER_H
#define RENDER_H

#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"

Vec3 ray_color(const Ray& r, const Grid& grid, const Vec3& light_dir);
void render_scene(const Grid& grid, const Camera& cam, DisplayWin& display,
                  int image_width, int image_height, int samples,
                  const Vec3& light_dir);
bool hit_center(const Grid& grid, const Camera& cam,
                int image_width, int image_height, HitRecord& rec);

#endif
