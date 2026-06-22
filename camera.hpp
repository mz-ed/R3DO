#ifndef CAMERA_H
#define CAMERA_H

#include "v3.hpp"
#include <cmath>

struct Camera {
    Vec3 pos;
    double yaw;
    double pitch;

    Camera(Vec3 p = Vec3(0, 0, 0), double y = 0, double pi = 0)
        : pos(p), yaw(y), pitch(pi) {}

    Vec3 forward() const {
        return Vec3(-sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch));
    }

    Vec3 right() const {
        return Vec3(cos(yaw), 0, -sin(yaw));
    }

    void move_fwd(double d) { pos = pos + forward() * d; }
    void move_right(double d) { pos = pos + right() * d; }
    void move_up(double d) { pos.y += d; }

    void rotate(double dyaw, double dpitch) {
        yaw += dyaw;
        pitch += dpitch;
        if (pitch > 1.5) pitch = 1.5;
        if (pitch < -1.5) pitch = -1.5;
    }
};

#endif
