#pragma once

#include <cmath>
#include <raylib.h>

namespace sim {

struct Vec2d {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2d() = default;
    constexpr Vec2d(double x_, double y_) : x(x_), y(y_) {}

    // Unary negation
    constexpr Vec2d operator-() const { return Vec2d(-x, -y); }

    // Binary vector operators
    constexpr Vec2d operator+(const Vec2d& rhs) const { return Vec2d(x + rhs.x, y + rhs.y); }
    constexpr Vec2d operator-(const Vec2d& rhs) const { return Vec2d(x - rhs.x, y - rhs.y); }
    constexpr Vec2d operator*(const Vec2d& rhs) const { return Vec2d(x * rhs.x, y * rhs.y); }
    constexpr Vec2d operator/(const Vec2d& rhs) const { return Vec2d(x / rhs.x, y / rhs.y); }

    // Binary scalar operators
    constexpr Vec2d operator*(double scalar) const { return Vec2d(x * scalar, y * scalar); }
    constexpr Vec2d operator/(double scalar) const { return Vec2d(x / scalar, y / scalar); }

    // Compound assignment operators
    constexpr Vec2d& operator+=(const Vec2d& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr Vec2d& operator-=(const Vec2d& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    constexpr Vec2d& operator*=(double scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr Vec2d& operator*=(const Vec2d& rhs) {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    constexpr Vec2d& operator/=(double scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    constexpr Vec2d& operator/=(const Vec2d& rhs) {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    // Vector math operations
    double lengthSq() const { return x * x + y * y; }
    double length() const { return std::sqrt(lengthSq()); }

    Vec2d normalized() const {
        double len = length();
        if (len > 1e-12) {
            return Vec2d(x / len, y / len);
        }
        return Vec2d(0.0, 0.0);
    }

    constexpr double dot(const Vec2d& other) const { return x * other.x + y * other.y; }
    constexpr double cross(const Vec2d& other) const { return x * other.y - y * other.x; }
    constexpr Vec2d perpendicular() const { return Vec2d(-y, x); }

    Vector2 toRaylib() const {
        return Vector2{ static_cast<float>(x), static_cast<float>(y) };
    }
};

inline constexpr Vec2d operator*(double scalar, const Vec2d& v) {
    return Vec2d(v.x * scalar, v.y * scalar);
}

} // namespace sim
