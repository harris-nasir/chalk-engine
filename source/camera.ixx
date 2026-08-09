module;

#include <cmath>

export module camera;

import engine;
using namespace engine;

export struct Camera
{
  Vector3 position{0.0F, 0.0F, 5.0F};
  f32 yaw   = 0.0F; // radians around world Y
  f32 pitch = 0.0F; // radians around local right

  f32 fov_y      = 1.0472F; // ~60 degrees
  f32 near_plane = 0.1F;
  f32 far_plane  = 100.0F;

  f32 look_sensitivity = 0.002F; // radians per mouse pixel

  // Right-handed: yaw = 0, pitch = 0 looks down -Z.
  [[nodiscard]] auto forward() const -> Vector3
  {
    return {
        .x = std::cos(pitch) * std::sin(yaw),
        .y = std::sin(pitch),
        .z = -std::cos(pitch) * std::cos(yaw),
    };
  }

  [[nodiscard]] auto view() const -> Matrix4
  {
    Vector3 fwd = forward();
    Vector3 world_up{.x = 0.0F, .y = 1.0F, .z = 0.0F};
    Vector3 right = normalize(cross(fwd, world_up));
    Vector3 up    = cross(right, fwd);

    return Matrix4{
        .values = {
            right.x,
            right.y,
            right.z,
            -dot(right, position), //
            up.x,
            up.y,
            up.z,
            -dot(up, position), //
            -fwd.x,
            -fwd.y,
            -fwd.z,
            dot(fwd, position), //
            0.0F,
            0.0F,
            0.0F,
            1.0F, //
        }
    };
  }

  // D3D-style [0, 1] depth range, row-major mul(M, v) convention.
  [[nodiscard]] auto projection(f32 aspect_ratio) const -> Matrix4
  {
    f32 focal_length = 1.0F / std::tan(fov_y * 0.5F);
    f32 depth_range  = far_plane / (near_plane - far_plane);

    return Matrix4{
        .values = {
            focal_length / aspect_ratio,
            0.0F,
            0.0F,
            0.0F, //
            0.0F,
            focal_length,
            0.0F,
            0.0F, //
            0.0F,
            0.0F,
            depth_range,
            near_plane * depth_range, //
            0.0F,
            0.0F,
            -1.0F,
            0.0F, //
        }
    };
  }
};

export class CameraPlugin
{
public:
  void build(App& app);
};

void CameraPlugin::build(App& app)
{
  app.insert_resource<Camera>({});

  // Fly control: W/S move along forward, A/D strafe along right, mouse looks.
  app.add_system(
      Schedule::PreUpdate,
      [](App& app) -> void
      {
        auto& input  = app.require_resource<InputState>();
        auto& time   = app.require_resource<Time>();
        auto& camera = app.require_resource<Camera>();

        camera.yaw += input.mouse_delta_x() * camera.look_sensitivity;
        camera.pitch -= input.mouse_delta_y() * camera.look_sensitivity;

        // Keep pitch away from the poles to avoid flipping.
        constexpr f32 max_pitch = 1.55F;
        if (camera.pitch > max_pitch)
        {
          camera.pitch = max_pitch;
        }
        if (camera.pitch < -max_pitch)
        {
          camera.pitch = -max_pitch;
        }

        constexpr f32 speed = 5.0F;
        Vector3 right       = normalize(cross(camera.forward(), Vector3{.x = 0.0F, .y = 1.0F, .z = 0.0F}));

        Vector3 move{.x = 0.0F, .y = 0.0F, .z = 0.0F};
        if (input.is_pressed(Key::W))
        {
          move += camera.forward();
        }
        if (input.is_pressed(Key::S))
        {
          move -= camera.forward();
        }
        if (input.is_pressed(Key::A))
        {
          move -= right;
        }
        if (input.is_pressed(Key::D))
        {
          move += right;
        }

        if (length(move) > 0.0F)
        {
          camera.position += normalize(move) * speed * static_cast<f32>(time.delta_seconds);
        }
      }
  );
}
