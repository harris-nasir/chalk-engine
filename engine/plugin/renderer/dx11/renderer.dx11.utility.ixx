module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl.h>

#include <d3d11.h>

export module engine.renderer.dx11:utility;

import engine.core;
import engine.renderer.types;

namespace engine
{
  struct BufferRecord
  {
    Microsoft::WRL::ComPtr<ID3D11Buffer> handle;
    u64 size;
  };

  struct TextureRecord
  {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> handle;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;     // set iff ColorTarget usage
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_stencil_view;     // set iff DepthTarget usage
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view; // set iff Sampled usage
    u32 width;
    u32 height;
    PixelFormat format;
  };

  struct ShaderRecord
  {
    ShaderStage stage{};
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader; // set iff stage == Vertex
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;   // set iff stage == Fragment
    // Kept alive past shader creation: CreateInputLayout needs the vertex
    // shader's compiled bytecode at pipeline-creation time, not just the
    // shader object.
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
  };

  struct PipelineRecord
  {
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_state;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_state;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_state;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    u32 vertex_stride;
  };

  // push_uniforms has no create/destroy handle pair of its own (unlike
  // buffers): D3D11 needs a bound constant buffer per (stage, slot), so
  // DX11Renderer owns and sizes these on demand.
  struct ConstantBufferSlot
  {
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    u64 capacity = 0;
  };

  [[nodiscard]] auto to_dxgi_format(PixelFormat format) -> DXGI_FORMAT
  {
    switch (format)
    {
      case PixelFormat::RGBA8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
      case PixelFormat::BGRA8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
      case PixelFormat::Depth24Stencil8:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    return DXGI_FORMAT_UNKNOWN;
  }

  [[nodiscard]] auto to_dxgi_vertex_format(VertexFormat format) -> DXGI_FORMAT
  {
    switch (format)
    {
      case VertexFormat::F32:
        return DXGI_FORMAT_R32_FLOAT;
      case VertexFormat::F32x2:
        return DXGI_FORMAT_R32G32_FLOAT;
      case VertexFormat::F32x3:
        return DXGI_FORMAT_R32G32B32_FLOAT;
      case VertexFormat::F32x4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
      case VertexFormat::U8x4Norm:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    return DXGI_FORMAT_UNKNOWN;
  }

  [[nodiscard]] auto to_d3d11_primitive_topology(Topology topology) -> D3D11_PRIMITIVE_TOPOLOGY
  {
    switch (topology)
    {
      case Topology::TriangleList:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      case Topology::LineList:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
      case Topology::PointList:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    }
    return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  }

  [[nodiscard]] auto to_hlsl_target_profile(D3D_FEATURE_LEVEL feature_level, ShaderStage stage) -> const char*
  {
    const bool is_vertex = stage == ShaderStage::Vertex;
    switch (feature_level)
    {
      case D3D_FEATURE_LEVEL_11_1:
      case D3D_FEATURE_LEVEL_11_0:
        return is_vertex ? "vs_5_0" : "ps_5_0";
      case D3D_FEATURE_LEVEL_10_1:
        return is_vertex ? "vs_4_1" : "ps_4_1";
      case D3D_FEATURE_LEVEL_10_0:
        return is_vertex ? "vs_4_0" : "ps_4_0";
      default:
        return is_vertex ? "vs_4_0_level_9_3" : "ps_4_0_level_9_3";
    }
  }
} // namespace engine
