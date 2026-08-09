module;

#include <dxgi1_2.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl.h>

#include <array>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <span>
#include <utility>
#include <variant>
#include <vector>

export module engine.renderer.dx11:renderer;

import engine.core;
import engine.renderer.types;
import engine.renderer.handle_table;
import engine.renderer.token_guard;
import :utility;

export namespace engine
{

  class DX11Renderer
  {
  public:
    DX11Renderer(
        App& app, Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context, Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain,
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> swapchain_render_target_view, D3D_FEATURE_LEVEL feature_level,
        u32 width, u32 height
    )
        : app_(app),
          device_(std::move(device)),
          immediate_context_(std::move(immediate_context)),
          deferred_context_(std::move(deferred_context)),
          swapchain_(std::move(swapchain)),
          swapchain_render_target_view_(std::move(swapchain_render_target_view)),
          feature_level_(feature_level),
          width_(width),
          height_(height)
    {
      // TODO: make configurable with SamplerDescription
      D3D11_SAMPLER_DESC sampler_info{
          .Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT,
          .AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP,
          .AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP,
          .AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP,
          .MipLODBias     = 0.0F,
          .MaxAnisotropy  = 1,
          .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
          .BorderColor    = {0.0F, 0.0F, 0.0F, 0.0F},
          .MinLOD         = 0.0F,
          .MaxLOD         = D3D11_FLOAT32_MAX,
      };
      if (auto result = device_->CreateSamplerState(&sampler_info, &sampler_); FAILED(result))
      {
        app_.report(Severity::Fatal, "CreateSamplerState failed: {}", result);
      }
    }

    auto create_buffer(BufferDescription description) -> BufferHandle;
    auto destroy_buffer(BufferHandle handle) -> void;
    auto create_texture(TextureDescription description) -> TextureHandle;
    auto destroy_texture(TextureHandle handle) -> void;
    auto create_shader(ShaderDescription description) -> ShaderHandle;
    auto destroy_shader(ShaderHandle handle) -> void;
    auto create_pipeline(PipelineDescription description) -> PipelineHandle;
    auto destroy_pipeline(PipelineHandle handle) -> void;

    auto resize(u32 width, u32 height) -> void;

    auto begin_frame() -> FrameID;
    auto swapchain_texture(FrameID frame) -> TextureHandle;
    auto begin_copy_pass(FrameID frame) -> CopyPassID;
    auto upload_buffer(CopyPassID pass, BufferHandle buffer, std::span<const std::byte> bytes) -> void;
    auto upload_texture(CopyPassID pass, TextureHandle texture, std::span<const std::byte> bytes, u32 bytes_per_row)
        -> void;
    auto end_copy_pass(CopyPassID pass) -> void;

    auto begin_pass(FrameID frame, RenderPassDescription description) -> PassID;
    auto bind_pipeline(PassID pass, PipelineHandle pipeline) -> void;
    auto bind_buffer(PassID pass, BufferHandle buffer, u32 slot) -> void;
    auto bind_texture(PassID pass, ShaderStage stage, u32 slot, TextureHandle texture) -> void;
    auto push_uniforms(PassID pass, ShaderStage stage, u32 slot, std::span<const std::byte> bytes) -> void;
    auto draw(PassID pass, u32 vertex_count, u32 instance_count, u32 first_vertex) -> void;
    auto draw_indexed(PassID pass, u32 index_count, u32 instance_count, u32 first_index) -> void;
    auto end_pass(PassID pass) -> void;
    auto submit(FrameID frame) -> void;

  private:
    [[nodiscard]] auto resolve_color_target(TextureHandle handle) -> ID3D11RenderTargetView*;
    [[nodiscard]] auto resolve_depth_target(TextureHandle handle) -> ID3D11DepthStencilView*;
    [[nodiscard]] auto resolve_shader_resource(TextureHandle handle) -> ID3D11ShaderResourceView*;

    App& app_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context_; // records commands, played on immediate_context_
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> swapchain_render_target_view_;
    D3D_FEATURE_LEVEL feature_level_;
    u32 width_; // current swapchain size, tracked so resize() is a no-op when the size hasn't actually changed
    u32 height_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_; // one default sampler, see the constructor

    HandleTable<BufferRecord> buffers_;
    HandleTable<TextureRecord> textures_;
    HandleTable<ShaderRecord> shaders_;
    HandleTable<PipelineRecord> pipelines_;

    // The deferred context just accumulates state between begin_frame and
    // submit. These guards exist purely as call-order / stale-ID checks for
    // the contract, not to gate a real GPU resource.
    TokenGuard<FrameID> frame_guard_;
    TokenGuard<CopyPassID> copy_pass_guard_;
    TokenGuard<PassID> pass_guard_;
    u32 bound_vertex_stride_ = 0; // from bind_pipeline; IASetVertexBuffers needs a stride at bind_buffer time

    static constexpr u32 CONSTANT_BUFFER_SLOT_COUNT = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    std::array<ConstantBufferSlot, CONSTANT_BUFFER_SLOT_COUNT> vertex_constant_buffers_{};
    std::array<ConstantBufferSlot, CONSTANT_BUFFER_SLOT_COUNT> fragment_constant_buffers_{};
  };

  static_assert(RendererBackend<DX11Renderer>);

} // namespace engine

namespace engine
{
  namespace
  {
    // Never a real HandleTable index (those start at 1 and grow one at a
    // time), so it can never collide with a created texture's handle.
    constexpr u64 SWAPCHAIN_TEXTURE_HANDLE = ~u64{0};

    [[nodiscard]] auto make_buffer_handle(BufferUsage type, u64 id) -> BufferHandle
    {
      switch (type)
      {
        case BufferUsage::Vertex:
          return static_cast<VertexBufferHandle>(id);
        case BufferUsage::Index:
          return static_cast<IndexBufferHandle>(id);
        case BufferUsage::Uniform:
          return static_cast<UniformBufferHandle>(id);
      }
      return BufferHandle{};
    }
  } // namespace

  auto DX11Renderer::create_buffer(BufferDescription description) -> BufferHandle
  {
    UINT bind_flags{};
    switch (description.usage)
    {
      case BufferUsage::Vertex:
        bind_flags = D3D11_BIND_VERTEX_BUFFER;
        break;
      case BufferUsage::Index:
        bind_flags = D3D11_BIND_INDEX_BUFFER;
        break;
      case BufferUsage::Uniform:
        bind_flags = D3D11_BIND_CONSTANT_BUFFER;
        break;
    }

    // D3D11 constant buffers must be a multiple of 16 bytes; other usages have no such requirement.
    const u64 byte_width
        = description.usage == BufferUsage::Uniform ? (description.size + 15) & ~u64{15} : description.size;

    D3D11_BUFFER_DESC info{
        .ByteWidth           = static_cast<UINT>(byte_width),
        .Usage               = D3D11_USAGE_DEFAULT,
        .BindFlags           = bind_flags,
        .CPUAccessFlags      = 0,
        .MiscFlags           = 0,
        .StructureByteStride = 0,
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    if (auto result = device_->CreateBuffer(&info, nullptr, &buffer); FAILED(result))
    {
      app_.report(Severity::Error, "CreateBuffer failed: {}", result);
      return make_buffer_handle(description.usage, 0);
    }

    return make_buffer_handle(
        description.usage, buffers_.insert(BufferRecord{.handle = buffer, .size = description.size})
    );
  }

  auto DX11Renderer::destroy_buffer(BufferHandle handle) -> void
  {
    const u64 id = std::visit([](auto buffer) -> auto { return static_cast<u64>(buffer); }, handle);
    if (buffers_.get(id) == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed buffer handle (id: {})", id);
      return;
    }
    buffers_.destroy(id);
  }

  auto DX11Renderer::create_texture(TextureDescription description) -> TextureHandle
  {
    UINT bind_flags{};
    const auto usage_raw = static_cast<u32>(description.usage);
    if ((usage_raw & static_cast<u32>(TextureUsage::Sampled)) != 0U)
    {
      bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if ((usage_raw & static_cast<u32>(TextureUsage::ColorTarget)) != 0U)
    {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
    }
    if ((usage_raw & static_cast<u32>(TextureUsage::DepthTarget)) != 0U)
    {
      bind_flags |= D3D11_BIND_DEPTH_STENCIL;
    }

    D3D11_TEXTURE2D_DESC info{
        .Width          = description.width,
        .Height         = description.height,
        .MipLevels      = 1,
        .ArraySize      = 1,
        .Format         = to_dxgi_format(description.format),
        .SampleDesc     = {.Count = 1, .Quality = 0},
        .Usage          = D3D11_USAGE_DEFAULT,
        .BindFlags      = bind_flags,
        .CPUAccessFlags = 0,
        .MiscFlags      = 0,
    };

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (auto result = device_->CreateTexture2D(&info, nullptr, &texture); FAILED(result))
    {
      app_.report(Severity::Error, "CreateTexture2D failed: {}", result);
      return TextureHandle::Invalid;
    }

    TextureRecord record{
        .handle               = texture,
        .render_target_view   = nullptr,
        .depth_stencil_view   = nullptr,
        .shader_resource_view = nullptr,
        .width                = description.width,
        .height               = description.height,
        .format               = description.format
    };

    if ((bind_flags & D3D11_BIND_RENDER_TARGET) != 0U)
    {
      if (auto result = device_->CreateRenderTargetView(texture.Get(), nullptr, &record.render_target_view);
          FAILED(result))
      {
        app_.report(Severity::Error, "CreateRenderTargetView failed: {}", result);
        return TextureHandle::Invalid;
      }
    }
    if ((bind_flags & D3D11_BIND_DEPTH_STENCIL) != 0U)
    {
      if (auto result = device_->CreateDepthStencilView(texture.Get(), nullptr, &record.depth_stencil_view);
          FAILED(result))
      {
        app_.report(Severity::Error, "CreateDepthStencilView failed: {}", result);
        return TextureHandle::Invalid;
      }
    }
    if ((bind_flags & D3D11_BIND_SHADER_RESOURCE) != 0U)
    {
      if (auto result = device_->CreateShaderResourceView(texture.Get(), nullptr, &record.shader_resource_view);
          FAILED(result))
      {
        app_.report(Severity::Error, "CreateShaderResourceView failed: {}", result);
        return TextureHandle::Invalid;
      }
    }

    return static_cast<TextureHandle>(textures_.insert(std::move(record)));
  }

  auto DX11Renderer::destroy_texture(TextureHandle handle) -> void
  {
    if (textures_.get(static_cast<u64>(handle)) == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed texture handle");
      return;
    }
    textures_.destroy(static_cast<u64>(handle));
  }

  auto DX11Renderer::create_shader(ShaderDescription description) -> ShaderHandle
  {
    if (description.format != ShaderFormat::HLSL)
    {
      app_.report(
          Severity::Error, "dx11 renderer only accepts ShaderFormat::HLSL, got {}", static_cast<u32>(description.format)
      );
      return ShaderHandle::Invalid;
    }

    const char* target_profile = to_hlsl_target_profile(feature_level_, description.stage);

    UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    if (auto result = D3DCompile(
            description.code.data(),
            description.code.size(),
            nullptr,
            nullptr,
            nullptr,
            description.entry_point,
            target_profile,
            compile_flags,
            0,
            &bytecode,
            &error_blob
        );
        FAILED(result))
    {
      const char* message = (error_blob != nullptr) ? static_cast<const char*>(error_blob->GetBufferPointer()) : "";
      app_.report(Severity::Error, "D3DCompile failed: {} ({})", result, message);
      return ShaderHandle::Invalid;
    }

    if (error_blob != nullptr && error_blob->GetBufferSize() > 0)
    {
      app_.report(Severity::Warn, "D3DCompile warning: {}", static_cast<const char*>(error_blob->GetBufferPointer()));
    }

    ShaderRecord record{
        .stage         = description.stage,
        .vertex_shader = nullptr,
        .pixel_shader  = nullptr,
        .bytecode      = bytecode,
    };

    switch (description.stage)
    {
      case ShaderStage::Vertex:
        if (auto result = device_->CreateVertexShader(
                bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &record.vertex_shader
            );
            FAILED(result))
        {
          app_.report(Severity::Error, "CreateVertexShader failed: {}", result);
          return ShaderHandle::Invalid;
        }
        break;
      case ShaderStage::Fragment:
        if (auto result = device_->CreatePixelShader(
                bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &record.pixel_shader
            );
            FAILED(result))
        {
          app_.report(Severity::Error, "CreatePixelShader failed: {}", result);
          return ShaderHandle::Invalid;
        }
        break;
    }

    return static_cast<ShaderHandle>(shaders_.insert(std::move(record)));
  }

  auto DX11Renderer::destroy_shader(ShaderHandle handle) -> void
  {
    if (shaders_.get(static_cast<u64>(handle)) == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed shader handle");
      return;
    }
    shaders_.destroy(static_cast<u64>(handle));
  }

  auto DX11Renderer::create_pipeline(PipelineDescription description) -> PipelineHandle
  {
    auto* vertex_shader   = shaders_.get(static_cast<u64>(description.vertex_shader));
    auto* fragment_shader = shaders_.get(static_cast<u64>(description.fragment_shader));
    if (vertex_shader == nullptr || fragment_shader == nullptr)
    {
      app_.report(Severity::Error, "invalid vertex or fragment shader handle");
      return PipelineHandle::Invalid;
    }

    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
    elements.reserve(description.vertex_layout.size());
    for (const auto& attribute : description.vertex_layout)
    {
      elements.push_back(
          D3D11_INPUT_ELEMENT_DESC{
              // HLSL has no numeric attribute location; every .hlsl source in this
              // repo (assets/shaders/*.hlsl) already uses TEXCOORD<N> where N is
              // the same value as the contract's VertexAttribute::location (the
              // existing DXC/shadercross convention these shaders were written
              // against), so no shader-file change is needed here.
              .SemanticName         = "TEXCOORD",
              .SemanticIndex        = attribute.location,
              .Format               = to_dxgi_vertex_format(attribute.format),
              .InputSlot            = 0,
              .AlignedByteOffset    = attribute.offset,
              .InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA,
              .InstanceDataStepRate = 0,
          }
      );
    }

    PipelineRecord record{
        .vertex_shader    = vertex_shader->vertex_shader,
        .pixel_shader     = fragment_shader->pixel_shader,
        .input_layout     = nullptr,
        .blend_state      = nullptr,
        .depth_state      = nullptr,
        .rasterizer_state = nullptr,
        .topology         = to_d3d11_primitive_topology(description.topology),
        .vertex_stride    = description.vertex_stride,
    };

    if (auto result = device_->CreateInputLayout(
            elements.data(),
            static_cast<UINT>(elements.size()),
            vertex_shader->bytecode->GetBufferPointer(),
            vertex_shader->bytecode->GetBufferSize(),
            &record.input_layout
        );
        FAILED(result))
    {
      app_.report(Severity::Error, "CreateInputLayout failed: {}", result);
      return PipelineHandle::Invalid;
    }

    D3D11_BLEND_DESC blend_info{};
    blend_info.RenderTarget[0] = D3D11_RENDER_TARGET_BLEND_DESC{
        .BlendEnable           = static_cast<BOOL>(description.blend.enabled),
        .SrcBlend              = description.blend.enabled ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_ONE,
        .DestBlend             = description.blend.enabled ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_ZERO,
        .BlendOp               = D3D11_BLEND_OP_ADD,
        .SrcBlendAlpha         = D3D11_BLEND_ONE,
        .DestBlendAlpha        = description.blend.enabled ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_ZERO,
        .BlendOpAlpha          = D3D11_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
    };

    if (auto result = device_->CreateBlendState(&blend_info, &record.blend_state); FAILED(result))
    {
      app_.report(Severity::Error, "CreateBlendState failed: {}", result);
      return PipelineHandle::Invalid;
    }

    D3D11_DEPTH_STENCIL_DESC depth_info{
        .DepthEnable      = static_cast<BOOL>(description.depth.test_enabled),
        .DepthWriteMask   = description.depth.write_enabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc        = D3D11_COMPARISON_LESS,
        .StencilEnable    = {},
        .StencilReadMask  = {},
        .StencilWriteMask = {},
        .FrontFace        = {},
        .BackFace         = {}
    };

    if (auto result = device_->CreateDepthStencilState(&depth_info, &record.depth_state); FAILED(result))
    {
      app_.report(Severity::Error, "CreateDepthStencilState failed: {}", result);
      return PipelineHandle::Invalid;
    }

    // No rasterizer state is bound anywhere, so D3D11 would fall back to its
    // default (D3D11_CULL_BACK) and discard any counter-clockwise-wound
    // triangle. Mirror the SDL3 backend's hardcoded rasterizer state: fill,
    // no culling, no depth bias.
    D3D11_RASTERIZER_DESC rasterizer_info{
        .FillMode              = D3D11_FILL_SOLID,
        .CullMode              = D3D11_CULL_NONE,
        .FrontCounterClockwise = FALSE,
        .DepthBias             = 0,
        .DepthBiasClamp        = 0.0F,
        .SlopeScaledDepthBias  = 0.0F,
        .DepthClipEnable       = TRUE,
        .ScissorEnable         = FALSE,
        .MultisampleEnable     = FALSE,
        .AntialiasedLineEnable = FALSE,
    };

    if (auto result = device_->CreateRasterizerState(&rasterizer_info, &record.rasterizer_state); FAILED(result))
    {
      app_.report(Severity::Error, "CreateRasterizerState failed: {}", result);
      return PipelineHandle::Invalid;
    }

    return static_cast<PipelineHandle>(pipelines_.insert(std::move(record)));
  }

  auto DX11Renderer::destroy_pipeline(PipelineHandle handle) -> void
  {
    if (pipelines_.get(static_cast<u64>(handle)) == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed pipeline handle");
      return;
    }
    pipelines_.destroy(static_cast<u64>(handle));
  }

  auto DX11Renderer::resolve_color_target(TextureHandle handle) -> ID3D11RenderTargetView*
  {
    if (static_cast<u64>(handle) == SWAPCHAIN_TEXTURE_HANDLE)
    {
      return swapchain_render_target_view_.Get();
    }
    auto* record = textures_.get(static_cast<u64>(handle));
    return (record != nullptr) ? record->render_target_view.Get() : nullptr;
  }

  auto DX11Renderer::resolve_depth_target(TextureHandle handle) -> ID3D11DepthStencilView*
  {
    auto* record = textures_.get(static_cast<u64>(handle));
    return (record != nullptr) ? record->depth_stencil_view.Get() : nullptr;
  }

  auto DX11Renderer::resolve_shader_resource(TextureHandle handle) -> ID3D11ShaderResourceView*
  {
    auto* record = textures_.get(static_cast<u64>(handle));
    return (record != nullptr) ? record->shader_resource_view.Get() : nullptr;
  }

  auto DX11Renderer::resize(u32 width, u32 height) -> void
  {
    if (width == 0 || height == 0 || (width == width_ && height == height_))
    {
      // 0x0 happens while the window is minimized so keep the existing buffers until it's restored.
      return;
    }

    swapchain_render_target_view_.Reset(); // every reference to the backbuffer must drop before ResizeBuffers

    if (auto result = swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0); FAILED(result))
    {
      app_.report(Severity::Error, "ResizeBuffers failed: {}", result);
      return;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
    if (auto result = swapchain_->GetBuffer(0, IID_PPV_ARGS(&backbuffer)); FAILED(result))
    {
      app_.report(Severity::Error, "retrieving resized swapchain buffer failed: {}", result);
      return;
    }
    if (auto result = device_->CreateRenderTargetView(backbuffer.Get(), nullptr, &swapchain_render_target_view_);
        FAILED(result))
    {
      app_.report(Severity::Error, "creating resized render target view failed: {}", result);
      return;
    }

    width_  = width;
    height_ = height;
  }

  auto DX11Renderer::begin_frame() -> FrameID { return frame_guard_.begin(); }

  auto DX11Renderer::swapchain_texture(FrameID frame) -> TextureHandle
  {
    if (!frame_guard_.check(frame, app_, "frame"))
    {
      return TextureHandle::Invalid;
    }
    return static_cast<TextureHandle>(SWAPCHAIN_TEXTURE_HANDLE);
  }

  auto DX11Renderer::begin_copy_pass(FrameID frame) -> CopyPassID
  {
    if (!frame_guard_.check(frame, app_, "frame"))
    {
      return CopyPassID::Invalid;
    }
    return copy_pass_guard_.begin();
  }

  auto DX11Renderer::upload_buffer(CopyPassID pass, BufferHandle buffer, std::span<const std::byte> bytes) -> void
  {
    if (!copy_pass_guard_.check(pass, app_, "copy pass"))
    {
      return;
    }
    const u64 id = std::visit([](auto b) -> auto { return static_cast<u64>(b); }, buffer);
    auto* record = buffers_.get(id);
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed buffer handle (id: {})", id);
      return;
    }
    if (bytes.size() > record->size)
    {
      app_.report(Severity::Error, "data size {} exceeds buffer size {} (id: {})", bytes.size(), record->size, id);
      return;
    }
    deferred_context_->UpdateSubresource(record->handle.Get(), 0, nullptr, bytes.data(), 0, 0);
  }

  auto DX11Renderer::upload_texture(
      CopyPassID pass, TextureHandle texture, std::span<const std::byte> bytes, u32 bytes_per_row
  ) -> void
  {
    if (!copy_pass_guard_.check(pass, app_, "copy pass"))
    {
      return;
    }
    if (static_cast<u64>(texture) == SWAPCHAIN_TEXTURE_HANDLE)
    {
      app_.report(Severity::Error, "cannot upload directly into the swapchain texture");
      return;
    }
    auto* record = textures_.get(static_cast<u64>(texture));
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed texture handle");
      return;
    }
    deferred_context_->UpdateSubresource(record->handle.Get(), 0, nullptr, bytes.data(), bytes_per_row, 0);
  }

  auto DX11Renderer::end_copy_pass(CopyPassID pass) -> void
  {
    if (!copy_pass_guard_.check(pass, app_, "copy pass"))
    {
      return;
    }
    copy_pass_guard_.end();
  }

  auto DX11Renderer::begin_pass(FrameID frame, RenderPassDescription description) -> PassID
  {
    if (!frame_guard_.check(frame, app_, "frame"))
    {
      return PassID::Invalid;
    }

    std::vector<ID3D11RenderTargetView*> targets;
    targets.reserve(description.color_attachments.size());
    for (const auto& attachment : description.color_attachments)
    {
      auto* target = resolve_color_target(attachment.target);
      if (target == nullptr)
      {
        app_.report(Severity::Error, "color attachment has an invalid texture handle");
        return PassID::Invalid;
      }
      if (attachment.load == LoadOp::Clear)
      {
        const std::array<FLOAT, 4> clear_color{
            attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a
        };
        deferred_context_->ClearRenderTargetView(target, clear_color.data());
      }
      targets.push_back(target);
    }

    ID3D11DepthStencilView* depth_target = nullptr;
    if (description.depth_attachment)
    {
      depth_target = resolve_depth_target(description.depth_attachment->target);
      if (depth_target == nullptr)
      {
        app_.report(Severity::Error, "depth attachment has an invalid texture handle");
        return PassID::Invalid;
      }
      if (description.depth_attachment->load == LoadOp::Clear)
      {
        deferred_context_->ClearDepthStencilView(
            depth_target, D3D11_CLEAR_DEPTH, description.depth_attachment->clear_depth, 0
        );
      }
    }

    deferred_context_->OMSetRenderTargets(static_cast<UINT>(targets.size()), targets.data(), depth_target);

    // D3D11 contexts start with no viewport bound, so without an explicit viewport
    // the rasterizer discards every triangle. Size it from the first color target
    // to track the swapchain (and any future offscreen target) automatically.
    if (!targets.empty())
    {
      Microsoft::WRL::ComPtr<ID3D11Resource> resource;
      targets.front()->GetResource(&resource);
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
      if (SUCCEEDED(resource.As(&texture)))
      {
        D3D11_TEXTURE2D_DESC texture_description{};
        texture->GetDesc(&texture_description);
        const D3D11_VIEWPORT viewport{
            .TopLeftX = 0.0F,
            .TopLeftY = 0.0F,
            .Width    = static_cast<FLOAT>(texture_description.Width),
            .Height   = static_cast<FLOAT>(texture_description.Height),
            .MinDepth = 0.0F,
            .MaxDepth = 1.0F,
        };
        deferred_context_->RSSetViewports(1, &viewport);
      }
    }

    return pass_guard_.begin();
  }

  auto DX11Renderer::bind_pipeline(PassID pass, PipelineHandle pipeline) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    auto* record = pipelines_.get(static_cast<u64>(pipeline));
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed pipeline handle");
      return;
    }
    deferred_context_->IASetInputLayout(record->input_layout.Get());
    deferred_context_->IASetPrimitiveTopology(record->topology);
    deferred_context_->VSSetShader(record->vertex_shader.Get(), nullptr, 0);
    deferred_context_->PSSetShader(record->pixel_shader.Get(), nullptr, 0);
    const std::array<FLOAT, 4> blend_factor{0, 0, 0, 0};
    deferred_context_->OMSetBlendState(record->blend_state.Get(), blend_factor.data(), 0xFFFFFFFF);
    deferred_context_->OMSetDepthStencilState(record->depth_state.Get(), 0);
    deferred_context_->RSSetState(record->rasterizer_state.Get());
    bound_vertex_stride_ = record->vertex_stride;
  }

  auto DX11Renderer::bind_buffer(PassID pass, BufferHandle buffer, u32 slot) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    const u64 id = std::visit([](auto b) -> auto { return static_cast<u64>(b); }, buffer);
    auto* record = buffers_.get(id);
    if (record == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed buffer handle (id: {})", id);
      return;
    }

    if (std::holds_alternative<VertexBufferHandle>(buffer))
    {
      ID3D11Buffer* raw = record->handle.Get();
      const UINT stride = bound_vertex_stride_;
      const UINT offset = 0;
      deferred_context_->IASetVertexBuffers(slot, 1, &raw, &stride, &offset);
    }
    else if (std::holds_alternative<IndexBufferHandle>(buffer))
    {
      deferred_context_->IASetIndexBuffer(record->handle.Get(), DXGI_FORMAT_R32_UINT, 0);
    }
    else
    {
      app_.report(Severity::Error, "uniform buffers cannot be bound to a render pass");
    }
  }

  auto DX11Renderer::bind_texture(PassID pass, ShaderStage stage, u32 slot, TextureHandle texture) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    auto* srv = resolve_shader_resource(texture);
    if (srv == nullptr)
    {
      app_.report(Severity::Error, "invalid or already-destroyed texture handle");
      return;
    }

    ID3D11ShaderResourceView* raw_srv = srv;
    ID3D11SamplerState* raw_sampler   = sampler_.Get();
    if (stage == ShaderStage::Vertex)
    {
      deferred_context_->VSSetShaderResources(slot, 1, &raw_srv);
      deferred_context_->VSSetSamplers(slot, 1, &raw_sampler);
    }
    else
    {
      deferred_context_->PSSetShaderResources(slot, 1, &raw_srv);
      deferred_context_->PSSetSamplers(slot, 1, &raw_sampler);
    }
  }

  auto DX11Renderer::push_uniforms(PassID pass, ShaderStage stage, u32 slot, std::span<const std::byte> bytes) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    if (slot >= CONSTANT_BUFFER_SLOT_COUNT)
    {
      app_.report(
          Severity::Error, "constant buffer slot {} exceeds the {} slot limit", slot, CONSTANT_BUFFER_SLOT_COUNT
      );
      return;
    }

    auto& slots          = (stage == ShaderStage::Vertex) ? vertex_constant_buffers_ : fragment_constant_buffers_;
    auto& entry          = slots.at(slot);
    const u64 byte_width = (bytes.size() + 15) & ~u64{15}; // D3D11 constant buffers must be a multiple of 16 bytes

    if ((entry.buffer == nullptr) || entry.capacity < byte_width)
    {
      D3D11_BUFFER_DESC info{
          .ByteWidth           = static_cast<UINT>(byte_width),
          .Usage               = D3D11_USAGE_DYNAMIC,
          .BindFlags           = D3D11_BIND_CONSTANT_BUFFER,
          .CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE,
          .MiscFlags           = 0,
          .StructureByteStride = 0,
      };
      if (auto result = device_->CreateBuffer(&info, nullptr, &entry.buffer); FAILED(result))
      {
        app_.report(Severity::Error, "CreateBuffer (constant buffer) failed: {}", result);
        return;
      }
      entry.capacity = byte_width;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (auto result = deferred_context_->Map(entry.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        FAILED(result))
    {
      app_.report(Severity::Error, "Map (constant buffer) failed: {}", result);
      return;
    }
    std::memcpy(mapped.pData, bytes.data(), bytes.size());
    deferred_context_->Unmap(entry.buffer.Get(), 0);

    ID3D11Buffer* raw = entry.buffer.Get();
    if (stage == ShaderStage::Vertex)
    {
      deferred_context_->VSSetConstantBuffers(slot, 1, &raw);
    }
    else
    {
      deferred_context_->PSSetConstantBuffers(slot, 1, &raw);
    }
  }

  auto DX11Renderer::draw(PassID pass, u32 vertex_count, u32 instance_count, u32 first_vertex) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    deferred_context_->DrawInstanced(vertex_count, instance_count, first_vertex, 0);
  }

  auto DX11Renderer::draw_indexed(PassID pass, u32 index_count, u32 instance_count, u32 first_index) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    deferred_context_->DrawIndexedInstanced(index_count, instance_count, first_index, 0, 0);
  }

  auto DX11Renderer::end_pass(PassID pass) -> void
  {
    if (!pass_guard_.check(pass, app_, "pass"))
    {
      return;
    }
    pass_guard_.end();
  }

  auto DX11Renderer::submit(FrameID frame) -> void
  {
    if (!frame_guard_.check(frame, app_, "frame"))
    {
      return;
    }
    if (pass_guard_.is_open() || copy_pass_guard_.is_open())
    {
      app_.report(Severity::Error, "called with a pass still open; call end_pass/end_copy_pass first");
      pass_guard_.end();
      copy_pass_guard_.end();
    }

    Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;
    if (auto result = deferred_context_->FinishCommandList(0, &command_list); FAILED(result))
    {
      app_.report(Severity::Error, "FinishCommandList failed: {}", result);
      frame_guard_.end();
      return;
    }
    immediate_context_->ExecuteCommandList(command_list.Get(), 0);

    // synchronize with presentation,
    // 0 means: no synchronization (unlimited FPS),
    // 1 means: sync every v-blank (regular v-sync),
    // 2 means: sync every other v-blank and so on, up to 4
    constexpr bool is_vsync_enabled = false; // TODO: put somewhere better
    if (auto result = swapchain_->Present(static_cast<UINT>(is_vsync_enabled), 0); FAILED(result))
    {
      app_.report(Severity::Error, "Present failed: {}", result);
    }

    frame_guard_.end();
  }

} // namespace engine
