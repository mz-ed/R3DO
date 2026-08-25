#include "render.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>
#include <thread>
#include <vector>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

// --- Minimal thread pool (file-local) ---
namespace {
class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv_jobs;
    std::condition_variable cv_done;
    int active_ = 0;
    bool stop = false;
public:
    explicit ThreadPool(size_t n) {
        for (size_t i = 0; i < n; i++)
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock lk(mtx);
                        cv_jobs.wait(lk, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                        active_++;
                    }
                    task();
                    {
                        std::lock_guard lk(mtx);
                        active_--;
                    }
                    cv_done.notify_all();
                }
            });
    }
    ~ThreadPool() {
        { std::lock_guard lk(mtx); stop = true; }
        cv_jobs.notify_all();
        for (auto& w : workers) w.join();
    }
    void submit(std::function<void()> f) {
        { std::lock_guard lk(mtx); tasks.push(std::move(f)); }
        cv_jobs.notify_one();
    }
    void wait_idle() {
        std::unique_lock lk(mtx);
        cv_done.wait(lk, [this] { return tasks.empty() && active_ == 0; });
    }
    size_t size() const { return workers.size(); }
};

ThreadPool& pool() {
    static ThreadPool p(std::max<size_t>(1, std::thread::hardware_concurrency()));
    return p;
}
} // anon

// --- Ray color (light must be pre-normalized) ---
Vec3 ray_color(const Ray& r, const Grid& grid, const Vec3& light) {
    HitRecord rec;
    Vec3 bg(0.05, 0.05, 0.15);

    if (grid.hit(r, 0.001, 1000.0, rec)) {
        double diff = std::max(0.0, dot(rec.normal, light));
        double intensity = 0.3 + 0.7 * diff;
        return rec.color * intensity;
    }

    Vec3 gmin, gmax;
    grid.grid_bounds(gmin, gmax);
    double ground_y = gmin.y;
    const Vec3& ro = r.origin();
    const Vec3& rd = r.direction();
    if (rd.y < -1e-10) {
        double t = (ground_y - ro.y) / rd.y;
        if (t > 0.001 && t < 1000.0) {
            Vec3 p = r.at(t);
            int cx = (int)std::floor(p.x / grid.cell_size);
            int cz = (int)std::floor(p.z / grid.cell_size);
            bool light_sq = ((cx + cz) & 1) == 0;
            Vec3 n(0, 1, 0);
            double diff = std::max(0.0, dot(light, n));
            double intensity = 0.3 + 0.7 * diff;
            Vec3 col = light_sq ? Vec3(0.25, 0.25, 0.3) : Vec3(0.15, 0.15, 0.2);
            return col * intensity;
        }
    }

    return bg;
}

// --- Projection basis ---
struct ViewBasis {
    Vec3 horizontal, vertical, lower_left;
};

static ViewBasis make_basis(const Camera& cam, int w, int h) {
    double vw = VIEWPORT_HEIGHT * w / h;
    Vec3 fwd = cam.forward();
    Vec3 rgt = cam.right();
    Vec3 up = cross(rgt, fwd);
    ViewBasis b;
    b.horizontal = vw * rgt;
    b.vertical = VIEWPORT_HEIGHT * up;
    b.lower_left = cam.pos - b.horizontal / 2 - b.vertical / 2 + fwd * FOCAL_LENGTH;
    return b;
}

static unsigned int to_rgb(const Vec3& color) {
    int ir = int(255.999 * std::sqrt(color.x));
    int ig = int(255.999 * std::sqrt(color.y));
    int ib = int(255.999 * std::sqrt(color.z));
    return ((unsigned)ir << 16) | ((unsigned)ig << 8) | (unsigned)ib;
}

// --- Render one tile (called from thread pool) ---
static void render_tile(const Grid& grid, const ViewBasis& b, DisplayWin& display,
                        const Vec3& origin, const Vec3& light, int samples,
                        int image_width, int image_height,
                        int tx, int ty, int tile_size) {
    int x0 = tx * tile_size;
    int y0 = ty * tile_size;
    int x1 = std::min(x0 + tile_size, image_width);
    int y1 = std::min(y0 + tile_size, image_height);
    for (int j = y0; j < y1; j++) {
        double v = 1.0 - (double)j / (image_height - 1);
        for (int i = x0; i < x1; i++) {
            Vec3 color;
            for (int s = 0; s < samples; s++) {
                double u_off = (s % 2) * 0.5 + 0.25;
                double v_off = (s / 2) * 0.5 + 0.25;
                double u = (i + u_off) / (image_width - 1);
                double vv = v - (v_off - 0.5) / image_height;
                Ray r(origin, b.lower_left + u * b.horizontal + vv * b.vertical - origin);
                color += ray_color(r, grid, light);
            }
            display.set_pixel(i, j, to_rgb(color / samples));
        }
    }
}

// --- Main render entry point ---
void render_scene(const Grid& grid, const Camera& cam, DisplayWin& display,
                  int image_width, int image_height, int samples,
                  const Vec3& light_dir) {
    if (samples < 1) samples = 1;
    ViewBasis b = make_basis(cam, image_width, image_height);
    Vec3 light = unit_vector(light_dir);
    Vec3 origin = cam.pos;

    // Phase 1: coarse preview (1 sample, stride ~120 pixels across)
    int stride = std::max(1, std::min(image_width, image_height) / 120);
    for (int j = 0; j < image_height; j += stride) {
        double v = 1.0 - (double)j / (image_height - 1);
        for (int i = 0; i < image_width; i += stride) {
            double u = (double)i / (image_width - 1);
            Ray r(origin, b.lower_left + u * b.horizontal + v * b.vertical - origin);
            unsigned int c = to_rgb(ray_color(r, grid, light));
            for (int dy = 0; dy < stride && j + dy < image_height; dy++)
                for (int dx = 0; dx < stride && i + dx < image_width; dx++)
                    display.set_pixel(i + dx, j + dy, c);
        }
    }
    display.update();
    display.process_events();

    // Phase 2: full-quality tiled render via thread pool
    const int TILE = 64;
    int tiles_x = (image_width + TILE - 1) / TILE;
    int tiles_y = (image_height + TILE - 1) / TILE;
    int total_tiles = tiles_x * tiles_y;
    std::atomic<int> tiles_done{0};

    for (int ty = 0; ty < tiles_y; ty++) {
        for (int tx = 0; tx < tiles_x; tx++) {
            pool().submit([&, tx, ty] {
                render_tile(grid, b, display, origin, light, samples,
                           image_width, image_height, tx, ty, TILE);
                tiles_done++;
            });
        }
    }

    // Main thread: pump X11 events while tiles render
    auto last_update = std::chrono::steady_clock::now();
    while (tiles_done < total_tiles) {
        display.process_events();
        if (display.is_closed()) break;

        auto now = std::chrono::steady_clock::now();
        if (now - last_update >= std::chrono::milliseconds(32)) {
            display.update();
            int pct = 100 * (int)tiles_done / total_tiles;
            std::cout << "\rRaytracing: " << pct << "% " << std::flush;
            last_update = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    pool().wait_idle();
    display.update();
    std::cout << "\r              \r" << std::flush;
}

bool hit_center(const Grid& grid, const Camera& cam,
                int image_width, int image_height, HitRecord& rec) {
    ViewBasis b = make_basis(cam, image_width, image_height);
    Ray r(cam.pos, b.lower_left + 0.5 * b.horizontal + 0.5 * b.vertical - cam.pos);
    return grid.hit(r, 0.001, 1000.0, rec);
}
