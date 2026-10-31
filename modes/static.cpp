#include "render_modes.hpp"

// Debug/no-op mode: each pixel gets a fixed color, no ray casting.
void static_mode(Grid&, Camera&, DisplayWin& display, const Vec3&, int) {
    int w = display.width();
    int h = display.height();
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            unsigned int c = ((unsigned)(i * 255 / w) << 16) |
                             ((unsigned)(j * 255 / h) << 8) |
                             (unsigned)(128);
            display.set_pixel(i, j, c);
        }
    }
    display.update();
}
