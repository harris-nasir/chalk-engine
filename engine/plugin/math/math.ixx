module;

#include <array>
#include <cmath>
#include <cstddef>

export module engine.math;

import engine.core;

export namespace engine
{

  struct Vector2
  {
    f32 x = 0.0F;
    f32 y = 0.0F;

    friend auto operator+(const Vector2& a, const Vector2& b) -> Vector2 { return {.x = a.x + b.x, .y = a.y + b.y}; }
    friend auto operator-(const Vector2& a, const Vector2& b) -> Vector2 { return {.x = a.x - b.x, .y = a.y - b.y}; }
    friend auto operator-(const Vector2& a) -> Vector2 { return {.x = -a.x, .y = -a.y}; }
    friend auto operator*(const Vector2& a, f32 scalar) -> Vector2 { return {.x = a.x * scalar, .y = a.y * scalar}; }
    friend auto operator*(f32 scalar, const Vector2& a) -> Vector2 { return a * scalar; }
    friend auto operator/(const Vector2& a, f32 scalar) -> Vector2 { return {.x = a.x / scalar, .y = a.y / scalar}; }

    auto operator+=(const Vector2& other) -> Vector2&
    {
      x += other.x;
      y += other.y;
      return *this;
    }

    auto operator-=(const Vector2& other) -> Vector2&
    {
      x -= other.x;
      y -= other.y;
      return *this;
    }

    auto operator*=(f32 scalar) -> Vector2&
    {
      x *= scalar;
      y *= scalar;
      return *this;
    }

    auto operator/=(f32 scalar) -> Vector2&
    {
      x /= scalar;
      y /= scalar;
      return *this;
    }
  };

  [[nodiscard]] inline auto dot(const Vector2& a, const Vector2& b) -> f32 { return (a.x * b.x) + (a.y * b.y); }
  [[nodiscard]] inline auto length(const Vector2& v) -> f32 { return std::sqrt(dot(v, v)); }
  [[nodiscard]] inline auto normalize(const Vector2& v) -> Vector2 { return v / length(v); }

  struct Vector3
  {
    f32 x = 0.0F;
    f32 y = 0.0F;
    f32 z = 0.0F;

    friend auto operator+(const Vector3& a, const Vector3& b) -> Vector3
    {
      return {.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
    }

    friend auto operator-(const Vector3& a, const Vector3& b) -> Vector3
    {
      return {.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
    }

    friend auto operator-(const Vector3& a) -> Vector3 { return {.x = -a.x, .y = -a.y, .z = -a.z}; }

    friend auto operator*(const Vector3& a, f32 scalar) -> Vector3
    {
      return {.x = a.x * scalar, .y = a.y * scalar, .z = a.z * scalar};
    }

    friend auto operator*(f32 scalar, const Vector3& a) -> Vector3 { return a * scalar; }

    friend auto operator/(const Vector3& a, f32 scalar) -> Vector3
    {
      return {.x = a.x / scalar, .y = a.y / scalar, .z = a.z / scalar};
    }

    auto operator+=(const Vector3& other) -> Vector3&
    {
      x += other.x;
      y += other.y;
      z += other.z;
      return *this;
    }

    auto operator-=(const Vector3& other) -> Vector3&
    {
      x -= other.x;
      y -= other.y;
      z -= other.z;
      return *this;
    }

    auto operator*=(f32 scalar) -> Vector3&
    {
      x *= scalar;
      y *= scalar;
      z *= scalar;
      return *this;
    }

    auto operator/=(f32 scalar) -> Vector3&
    {
      x /= scalar;
      y /= scalar;
      z /= scalar;
      return *this;
    }
  };

  [[nodiscard]] inline auto dot(const Vector3& a, const Vector3& b) -> f32
  {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
  }

  [[nodiscard]] inline auto length(const Vector3& v) -> f32 { return std::sqrt(dot(v, v)); }
  [[nodiscard]] inline auto normalize(const Vector3& v) -> Vector3 { return v / length(v); }

  [[nodiscard]] inline auto cross(const Vector3& a, const Vector3& b) -> Vector3
  {
    return {
        .x = (a.y * b.z) - (a.z * b.y),
        .y = (a.z * b.x) - (a.x * b.z),
        .z = (a.x * b.y) - (a.y * b.x),
    };
  }

  // Row-major: element (row, col) lives at values[row * 4 + col]. Matches the
  // row_major cbuffer + mul(transform, float4(position,1)) shader convention,
  // so composing translation * rotation * scale applies S, then R, then T.
  struct Matrix4
  {
    std::array<f32, 16> values{
        1.0F,
        0.0F,
        0.0F,
        0.0F, //
        0.0F,
        1.0F,
        0.0F,
        0.0F, //
        0.0F,
        0.0F,
        1.0F,
        0.0F, //
        0.0F,
        0.0F,
        0.0F,
        1.0F, //
    };

    [[nodiscard]] static auto identity() -> Matrix4 { return {}; }

    [[nodiscard]] static auto translation(const Vector3& t) -> Matrix4
    {
      Matrix4 result{};
      result.values[3]  = t.x;
      result.values[7]  = t.y;
      result.values[11] = t.z;
      return result;
    }

    [[nodiscard]] static auto rotation_z(f32 radians) -> Matrix4
    {
      f32 cos_a = std::cos(radians);
      f32 sin_a = std::sin(radians);

      Matrix4 result{};
      result.values[0] = cos_a;
      result.values[1] = -sin_a;
      result.values[4] = sin_a;
      result.values[5] = cos_a;
      return result;
    }

    [[nodiscard]] static auto scale(const Vector2& s) -> Matrix4
    {
      Matrix4 result{};
      result.values[0] = s.x;
      result.values[5] = s.y;
      return result;
    }

    friend auto operator*(const Matrix4& a, const Matrix4& b) -> Matrix4
    {
      Matrix4 result{};
      for (std::size_t row = 0; row < 4; ++row)
      {
        for (std::size_t col = 0; col < 4; ++col)
        {
          f32 sum = 0.0F;
          for (std::size_t k = 0; k < 4; ++k)
          {
            sum += a.values[(row * 4) + k] * b.values[(k * 4) + col];
          }
          result.values[(row * 4) + col] = sum;
        }
      }
      return result;
    }
  };

} // namespace engine
