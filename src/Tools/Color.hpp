#pragma once

#include <algorithm>
#include <cassert>

#include "Common.hpp"

struct color_t {
    union {
        struct {
            word_t r, g, b, a;
        };
        word_t comp[4];
    };

    color_t() = default;
    color_t(const color_t&) = default;
    color_t(color_t&&) = default;
    color_t& operator=(const color_t&) = default;
    color_t& operator=(color_t&&) = default;

    explicit color_t(word_t w) : r{w}, g{w}, b{w}, a{255} {}

    inline bool operator==(const color_t& v) { return r == v.r && g == v.g && b == v.b && a == v.a; }

    inline color_t& operator=(word_t val) {
        r = g = b = a = val;
        return *this;
    }

    inline bool operator==(word_t val) const { return r == g && g == b && b == a && a == val; }

    inline word_t& operator[](word_t val) {
        assert(val < 4);
        return comp[val];
    }

    inline word_t operator[](word_t val) const {
        assert(val < 4);
        return comp[val];
    }
};

template<typename T>
inline uint8_t saturate(T n) {
    return n > 255 ? 255 : n;
}

template<typename T>
inline uint8_t pos(T n) {
    return n < 0 ? 0 : n;
}

inline color_t operator+(const color_t& c0, const color_t& c1) {
    color_t r;
    r.r = saturate(c0.r + c1.r);
    r.g = saturate(c0.g + c1.g);
    r.b = saturate(c0.b + c1.b);
    r.a = saturate(c0.a + c1.a);
    return r;
}

inline color_t operator-(const color_t& c0, const color_t& c1) {
    color_t r;
    r.r = pos(c0.r - c1.r);
    r.g = pos(c0.g - c1.g);
    r.b = pos(c0.b - c1.b);
    r.a = pos(c0.a - c1.a);
    return r;
}

inline color_t operator*(float val, const color_t& c) {
    color_t r;
    r.r = saturate(c.r * val);
    r.g = saturate(c.g * val);
    r.b = saturate(c.b * val);
    r.a = saturate(c.a * val);
    return r;
}

inline color_t operator*(const color_t& c, float val) {
    return val * c;
}
