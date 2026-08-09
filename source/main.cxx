#include <array>
#include <span>

import engine;
using namespace engine;

import camera;

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
            world.add<Transform>(entity, {.position = Vector3{.x = offset}, .scale = Vector2{.x = 0.8F, .y = 0.8F}});
            world.add<Renderable>(
                entity, {.pipeline = pipeline, .vbo = vbo, .ibo = ibo, .index_count = 6, .texture = texture}
            );
          }
        }
    );

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

            auto& world  = app.require_resource<World>();
            auto& camera = app.require_resource<Camera>();
            auto& window = app.require_resource<Window>();

            f32 aspect_ratio        = static_cast<f32>(window.width) / static_cast<f32>(window.height);
            Matrix4 view_projection = camera.projection(aspect_ratio) * camera.view();

            for (const auto& [id, transform, renderable] : world.view<Transform, Renderable>())
            {
              renderer.bind_pipeline(pass, renderable.pipeline);
              renderer.bind_buffer(pass, renderable.vbo, 0);
              renderer.bind_buffer(pass, renderable.ibo, 0);
              renderer.bind_texture(pass, ShaderStage::Fragment, 0, renderable.texture);

              Matrix4 model = Matrix4::translation(transform.position) * Matrix4::rotation_z(transform.rotation)
                              * Matrix4::scale(transform.scale);
              Matrix4 mvp   = view_projection * model;
              renderer.push_uniforms(pass, ShaderStage::Vertex, 0, std::as_bytes(std::span{mvp.values}));

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
  app.add_plugin<CameraPlugin>();
  app.add_plugin<GamePlugin>();

  app.execute();
}
