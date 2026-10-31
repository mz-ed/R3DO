#ifndef RENDER_H
#define RENDER_H

#include "v3.hpp"
#include "ray.hpp"
#include "hittable.hpp"
#include "grid.hpp"
#include "camera.hpp"
#include "display.hpp"
#include <vector>

inline constexpr double VIEWPORT_HEIGHT = 2.5;
inline constexpr double FOCAL_LENGTH = 2.0;

Vec3 ray_color(const Ray& r, const Grid& grid, const Vec3& light);

// Rects the raytracer must not touch (UI panels live on the framebuffer
// underneath them and would otherwise be erased every frame -> flicker).
struct RenderSkip { int x, y, w, h; };

// Set the view state the raytracer honours: `skip` rects keep UI pixels
// alive; `block` > 1 renders a cheap chunky preview (one ray per block)
// for fast motion frames. `render_scene` uses whatever state was set last.
void set_render_view(const std::vector<RenderSkip>* skip, int block);

void render_scene(const Grid& grid, const Camera& cam, DisplayWin& display,
                  int image_width, int image_height, int samples,
                  const Vec3& light_dir);
bool hit_center(const Grid& grid, const Camera& cam,
                int image_width, int image_height, HitRecord& rec);
bool pick_at_mouse(const Grid& grid, const Camera& cam,
                   int image_width, int image_height,
                   int mx, int my, HitRecord& rec);
Ray mouse_ray(const Camera& cam, int image_width, int image_height, int mx, int my);
bool project_point(const Camera& cam, int image_width, int image_height,
                   const Vec3& p, int& sx, int& sy);

#endif
