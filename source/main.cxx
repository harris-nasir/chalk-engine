#include <array>
#include <span>

import engine;
using namespace engine;

struct Vertex
{
  f32 x, y, z;
  f32 r, g, b;
};

struct State
{
  PipelineHandle pipeline;
  BufferHandle vbo;
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

          auto vertex_code   = read_file_as_bytes("shaders/position.vertex.spv");
          auto fragment_code = read_file_as_bytes("shaders/gradient.fragment.spv");

          ShaderDescription description{};
          description.format  = ShaderFormat::SPIRV;
          description.code    = vertex_code;
          description.stage   = ShaderStage::Vertex;
          ShaderHandle vertex = renderer.create_shader(description);

          description.code      = fragment_code;
          description.stage     = ShaderStage::Fragment;
          ShaderHandle fragment = renderer.create_shader(description);

          std::array attributes{
              VertexAttribute{.location = 0, .offset = 0 * sizeof(f32), .format = VertexFormat::F32x3},
              VertexAttribute{.location = 1, .offset = 3 * sizeof(f32), .format = VertexFormat::F32x3},
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

          constexpr std::array<Vertex, 3> triangle{{
              {.x = -0.5F, .y = -0.5F, .z = 0.0F, .r = 1.0F, .g = 0.0F, .b = 0.0F},
              {.x = 0.5F, .y = -0.5F, .z = 0.0F, .r = 0.0F, .g = 1.0F, .b = 0.0F},
              {.x = 0.0F, .y = 0.5F, .z = 0.0F, .r = 0.0F, .g = 0.0F, .b = 1.0F},
          }};

          BufferHandle vbo = renderer.create_buffer(
              BufferDescription{
                  .size  = sizeof(triangle),
                  .usage = BufferUsage::Vertex,
              }
          );

          FrameID upload_frame = renderer.begin_frame();
          CopyPassID copy      = renderer.begin_copy_pass(upload_frame);

          renderer.upload_buffer(copy, vbo, std::as_bytes(std::span{triangle}));

          renderer.end_copy_pass(copy);
          renderer.submit(upload_frame);

          app.insert_resource<State>(State{.pipeline = pipeline, .vbo = vbo});
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

            auto& demo = app.require_resource<State>();
            renderer.bind_pipeline(pass, demo.pipeline);
            renderer.bind_buffer(pass, demo.vbo, 0);
            renderer.draw(pass, 3, 1, 0);

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
          auto& demo     = app.require_resource<State>();
          renderer.destroy_pipeline(demo.pipeline);
          renderer.destroy_buffer(demo.vbo);
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
