#include <array>
#include <cmath>
#include <span>

import engine;
using namespace engine;

struct Vertex
{
  f32 x, y, z;
  f32 u, v;
};

struct SharedAssets
{
  PipelineHandle pipeline;
  BufferHandle vbo;
  BufferHandle ibo;
  TextureHandle texture;
};

auto key_name(Key key) -> char const*
{
  switch (key)
  {
    case Key::W:
      return "W";
    case Key::A:
      return "A";
    case Key::S:
      return "S";
    case Key::D:
      return "D";
    case Key::Space:
      return "Space";
    case Key::Escape:
      return "Escape";
    case Key::Up:
      return "Up";
    case Key::Down:
      return "Down";
    case Key::Left:
      return "Left";
    case Key::Right:
      return "Right";
    default:
      return "?";
  }
}

auto add_input_test(App& app) -> void
{
  app.add_system(
      Schedule::PreUpdate,
      [last_summary = 0.0](App& app) mutable -> void
      {
        if (!app.has_resource<InputState>())
        {
          return;
        }

        constexpr std::array test_keys{
            Key::W,
            Key::A,
            Key::S,
            Key::D,
            Key::Space,
            Key::Escape,
            Key::Up,
            Key::Down,
            Key::Left,
            Key::Right,
        };

        auto& input = app.require_resource<InputState>();

        for (Key key : test_keys)
        {
          if (input.just_pressed(key))
          {
            app.report(Severity::Info, "input: {} just pressed", key_name(key));
          }
          if (input.just_released(key))
          {
            app.report(Severity::Info, "input: {} just released", key_name(key));
          }
        }

        if (input.just_pressed(MouseButton::Left))
        {
          app.report(Severity::Info, "input: left click at ({}, {})", input.mouse_x(), input.mouse_y());
        }
        if (input.just_pressed(MouseButton::Right))
        {
          app.report(Severity::Info, "input: right click at ({}, {})", input.mouse_x(), input.mouse_y());
        }
        if (input.just_pressed(MouseButton::Middle))
        {
          app.report(Severity::Info, "input: middle click at ({}, {})", input.mouse_x(), input.mouse_y());
        }

        const auto& time = app.require_resource<Time>();
        if (time.elapsed_seconds - last_summary >= 1.0)
        {
          last_summary = time.elapsed_seconds;

          for (Key key : test_keys)
          {
            if (input.is_pressed(key))
            {
              app.report(Severity::Info, "input: {} held", key_name(key));
            }
          }

          app.report(
              Severity::Info,
              "input: mouse at ({}, {}) delta ({}, {})",
              input.mouse_x(),
              input.mouse_y(),
              input.mouse_delta_x(),
              input.mouse_delta_y()
          );
        }
      }
  );
}

class GamePlugin
{
public:
  void build(App& app)
  {
    app.add_system(
        Schedule::Startup,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<Renderer>();

          Shader vertex_source   = load_shader("textured.vertex");
          Shader fragment_source = load_shader("textured.fragment");

          ShaderDescription vertex_description{
              .code      = vertex_source.code,
              .stage     = ShaderStage::Vertex,
              .format    = vertex_source.format,
              .resources = ShaderResourceCounts{.uniform_buffers = 1},
          };
          ShaderHandle vertex = renderer.create_shader(vertex_description);

          ShaderDescription fragment_description{
              .code      = fragment_source.code,
              .stage     = ShaderStage::Fragment,
              .format    = fragment_source.format,
              .resources = ShaderResourceCounts{.samplers = 1},
          };
          ShaderHandle fragment = renderer.create_shader(fragment_description);

          std::array attributes{
              VertexAttribute{.location = 0, .offset = 0 * sizeof(f32), .format = VertexFormat::F32x3},
              VertexAttribute{.location = 1, .offset = 3 * sizeof(f32), .format = VertexFormat::F32x2},
          };

          PipelineHandle pipeline = renderer.create_pipeline(
              PipelineDescription{
                  .vertex_shader   = vertex,
                  .fragment_shader = fragment,
                  .vertex_layout   = attributes,
                  .vertex_stride   = sizeof(Vertex),
                  .topology        = Topology::TriangleList,
                  .blend           = BlendState{.enabled = false},
                  .depth           = DepthState{.test_enabled = false, .write_enabled = false},
              }
          );

          renderer.destroy_shader(vertex);
          renderer.destroy_shader(fragment);

          constexpr std::array<Vertex, 4> quad{{
              {.x = -0.5F, .y = -0.5F, .z = 0.0F, .u = 0.0F, .v = 1.0F},
              {.x = 0.5F, .y = -0.5F, .z = 0.0F, .u = 1.0F, .v = 1.0F},
              {.x = 0.5F, .y = 0.5F, .z = 0.0F, .u = 1.0F, .v = 0.0F},
              {.x = -0.5F, .y = 0.5F, .z = 0.0F, .u = 0.0F, .v = 0.0F},
          }};
          constexpr std::array<u32, 6> indices{0, 1, 2, 2, 3, 0};

          BufferHandle vbo
              = renderer.create_buffer(BufferDescription{.size = sizeof(quad), .usage = BufferUsage::Vertex});
          BufferHandle ibo
              = renderer.create_buffer(BufferDescription{.size = sizeof(indices), .usage = BufferUsage::Index});

          // Tiny checkerboard pattern
          constexpr u32 checker_size = 4;
          std::array<u8, checker_size * checker_size * 4> pixels{};
          for (u32 y = 0; y < checker_size; ++y)
          {
            for (u32 x = 0; x < checker_size; ++x)
            {
              const bool is_light = ((x + y) % 2) == 0;
              const u8 value      = is_light ? 255 : 32;
              const u32 offset    = (y * checker_size + x) * 4;
              pixels[offset + 0]  = value;
              pixels[offset + 1]  = value;
              pixels[offset + 2]  = value;
              pixels[offset + 3]  = 255;
            }
          }

          TextureHandle texture = renderer.create_texture(
              TextureDescription{
                  .width  = checker_size,
                  .height = checker_size,
                  .format = PixelFormat::RGBA8,
                  .usage  = TextureUsage::Sampled,
              }
          );

          FrameID upload_frame = renderer.begin_frame();
          CopyPassID copy      = renderer.begin_copy_pass(upload_frame);

          renderer.upload_buffer(copy, vbo, std::as_bytes(std::span{quad}));
          renderer.upload_buffer(copy, ibo, std::as_bytes(std::span{indices}));
          renderer.upload_texture(copy, texture, std::as_bytes(std::span{pixels}), checker_size * 4);

          renderer.end_copy_pass(copy);
          renderer.submit(upload_frame);

          app.insert_resource<SharedAssets>(
              SharedAssets{.pipeline = pipeline, .vbo = vbo, .ibo = ibo, .texture = texture}
          );

          auto& world = app.require_resource<World>();

          constexpr std::array<f32, 3> offsets{-0.6F, 0.0F, 0.6F};
          for (f32 offset : offsets)
          {
            EntityID entity = world.spawn();
            world.add<Transform>(entity, {.x = offset, .scale_x = 0.4F, .scale_y = 0.4F});
            world.add<Renderable>(
                entity, {.pipeline = pipeline, .vbo = vbo, .ibo = ibo, .index_count = 6, .texture = texture}
            );
          }
        }
    );

    add_input_test(app);

    app.add_system(
        Schedule::Update,
        [](App& app) -> void
        {
          const auto& time = app.require_resource<Time>();
          auto& world      = app.require_resource<World>();
          for (auto& [id, transform] : world.view<Transform>())
          {
            transform.rotation += static_cast<f32>(time.delta_seconds) * 1.5F;
          }
        }
    );

    app.add_system(
        Schedule::Render,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<Renderer>();

          FrameID frame        = renderer.begin_frame();
          TextureHandle target = renderer.swapchain_texture(frame);

          if (target != TextureHandle::Invalid)
          {
            std::array<ColorAttachment, 1> color_attachments{ColorAttachment{
                .target = target,
                .load   = LoadOp::Clear,
                .store  = StoreOp::Store,
                .clear  = Color{.r = 17.0F / 255.0F, .g = 17.0F / 255.0F, .b = 17.0F / 255.0F, .a = 1.0F},
            }};

            PassID pass = renderer.begin_pass(frame, RenderPassDescription{.color_attachments = color_attachments});

            auto& world = app.require_resource<World>();

            for (const auto& [id, transform, renderable] : world.view<Transform, Renderable>())
            {
              renderer.bind_pipeline(pass, renderable.pipeline);
              renderer.bind_buffer(pass, renderable.vbo, 0);
              renderer.bind_buffer(pass, renderable.ibo, 0);
              renderer.bind_texture(pass, ShaderStage::Fragment, 0, renderable.texture);

              const f32 cos_a = std::cos(transform.rotation);
              const f32 sin_a = std::sin(transform.rotation);

              // row_major. each row here is one row of the matrix.
              // mul(transform, vec4) treats the vector as a column, so
              // translation lives in the last column of each row, not
              // the last row.
              const std::array<f32, 16> matrix{
                  cos_a * transform.scale_x,
                  -sin_a * transform.scale_y,
                  0.0F,
                  transform.x, //
                  sin_a * transform.scale_x,
                  cos_a * transform.scale_y,
                  0.0F,
                  transform.y, //
                  0.0F,
                  0.0F,
                  1.0F,
                  transform.z, //
                  0.0F,
                  0.0F,
                  0.0F,
                  1.0F, //
              };
              renderer.push_uniforms(pass, ShaderStage::Vertex, 0, std::as_bytes(std::span{matrix}));

              renderer.draw_indexed(pass, renderable.index_count, 1, 0);
            }

            renderer.end_pass(pass);
          }

          renderer.submit(frame);
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [](App& app) -> void
        {
          auto& renderer = app.require_resource<Renderer>();
          auto& assets   = app.require_resource<SharedAssets>();
          renderer.destroy_pipeline(assets.pipeline);
          renderer.destroy_buffer(assets.vbo);
          renderer.destroy_buffer(assets.ibo);
          renderer.destroy_texture(assets.texture);
        }
    );
  }
};

auto main() -> int
{
  App app;
  app.add_plugin<DefaultPlugin>(WindowDescription{.title = "Engine"});
  app.add_plugin<GamePlugin>();

  app.execute();
}
