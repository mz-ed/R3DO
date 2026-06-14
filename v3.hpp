#ifndef VEC3_H
#define VEC3_H

#include <cmath>
#include <iostream>


class Vec3 {

public:
    
    double x, y, z;
    
    Vec3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}    
        
    Vec3 operator -() const { return Vec3(-x, -y, -z); }
    
    Vec3& operator += (const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    
    Vec3& operator *= (double t) { x *= t; y *= t; z *= t; return *this; }
    
    double length() const { return std::sqrt(x*x + y*y + z*z); }

};

// Utility functions for vector math
inline Vec3 operator + (const Vec3& u, const Vec3& v) { return Vec3(u.x + v.x, u.y + v.y, u.z + v.z); }

inline Vec3 operator - (const Vec3& u, const Vec3& v) { return Vec3(u.x - v.x, u.y - v.y, u.z - v.z); }

inline Vec3 operator * (const Vec3& u, const Vec3& v) { return Vec3(u.x * v.x, u.y * v.y, u.z * v.z); }

inline Vec3 operator * (double t, const Vec3& v) { return Vec3(t*v.x, t*v.y, t*v.z); }

inline Vec3 operator / (const Vec3& v, double t) { return (1/t) * v; }

inline double dot (const Vec3& u, const Vec3& v) { return u.x * v.x + u.y * v.y + u.z * v.z; }

inline Vec3 cross (const Vec3& u, const Vec3& v) {
    return Vec3(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x);
}

inline Vec3 unit_vector (const Vec3& v) { return v / v.length(); }

#endif
