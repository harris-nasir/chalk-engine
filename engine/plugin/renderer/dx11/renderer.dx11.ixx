module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl.h>

#include <algorithm>
#include <array>
#include <d3d11.h>

export module engine.renderer.dx11;

import engine.core;
import engine.platform;
import engine.renderer;

export namespace engine
{

  class RendererDX11Plugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

namespace
{
  struct Swapchain
  {
    Microsoft::WRL::ComPtr<IDXGISwapChain> handle;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
  };

  struct DeviceContext
  {
    Microsoft::WRL::ComPtr<ID3D11Device> device; // high level rendering device
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context; // records commands, played on immediate context
  };

  struct State
  {
    UINT create_device_flags{};
    D3D_FEATURE_LEVEL feature_level{};
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;   // device bound to a gpu
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter; // represents a gpu
    Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory; // manages adapter & creates swapchain
    Swapchain swapchain;
    DeviceContext device_context;
  };

  // Finishes the command list recorded on the deferred context this frame,
  // then plays it back on the immediate context (only the immediate context
  // can execute a command list against the GPU).
  void execute_command_list(engine::App& app, const State& state)
  {
    Microsoft::WRL::ComPtr<ID3D11CommandList> command_list;
    if (auto result = state.device_context.deferred_context->FinishCommandList(false, &command_list); FAILED(result))
    {
      app.report(engine::Severity::Fatal, "Failed to finish command list: {}", result);
    }

    state.device_context.immediate_context->ExecuteCommandList(command_list.Get(), false);
  }

} // namespace

namespace engine
{

  void RendererDX11Plugin::build(App& app)
  {
    app.add_system(
        Schedule::Startup,
        [](App& app) -> void
        {
          auto& window{app.require_resource<Window>()};
          auto& native_window{app.require_resource<NativeWindowHandle>()};

          State state{};

#ifdef _DEBUG
          state.create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

          if (auto result = D3D11CreateDevice(
                  nullptr,
                  D3D_DRIVER_TYPE_HARDWARE,
                  nullptr,
                  state.create_device_flags,
                  nullptr,
                  0,
                  D3D11_SDK_VERSION,
                  &state.device_context.device,
                  &state.feature_level,
                  &state.device_context.immediate_context
              );
              FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to create graphics context: {}", result);
          }

          if (auto result = state.device_context.device->QueryInterface(IID_PPV_ARGS(&state.dxgi_device));
              FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to retreive dxgi device: {}", result);
          }

          if (auto result = state.dxgi_device->GetParent(IID_PPV_ARGS(&state.dxgi_adapter)); FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to retreive dxgi adapter: {}", result);
          }

          if (auto result = state.dxgi_adapter->GetParent(IID_PPV_ARGS(&state.dxgi_factory)); FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to retreive dxgi factory: {}", result);
          }

          const auto swapchain_width{window.width};   // TODO: update when resizing the window
          const auto swapchain_height{window.height}; // TODO: update when resizing the window
          DXGI_SWAP_CHAIN_DESC description{
              .BufferDesc{
                  .Width            = std::max(1U, swapchain_width),
                  .Height           = std::max(1U, swapchain_height),
                  .RefreshRate      = DXGI_RATIONAL{.Numerator = 60, .Denominator = 1}, // TODO: take from description
                  .Format           = DXGI_FORMAT_R8G8B8A8_UNORM,
                  .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                  .Scaling          = DXGI_MODE_SCALING_UNSPECIFIED,
              },
              .SampleDesc{
                  .Count   = 1,
                  .Quality = 0,
              },
              .BufferCount  = 2, // double buffering
              .OutputWindow = static_cast<HWND>(native_window.handle),
              .SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD,
              .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
              .Windowed     = true, // TODO: take from window desc
              .Flags        = 0
          };

          if (auto result = state.dxgi_factory->CreateSwapChain(
                  state.device_context.device.Get(), &description, &state.swapchain.handle
              );
              FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to create Swapchain: {}", result);
          }

          Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer{};
          if (auto result = state.swapchain.handle->GetBuffer(0, IID_PPV_ARGS(&buffer)); FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to get buffer from Swapchain: {}", result);
          }

          if (auto result = state.device_context.device->CreateRenderTargetView(
                  buffer.Get(), nullptr, &state.swapchain.render_target_view
              );
              FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to create Render Target View: {}", result);
          }

          if (auto result
              = state.device_context.device->CreateDeferredContext(0, &state.device_context.deferred_context);
              FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to create Deferred Context: {}", result);
          }

          app.insert_resource<State>(state); // TODO: split this struct into smaller resourecs
          app.report(Severity::Info, "Renderer ready");
        }
    );

    app.add_system(
        Schedule::Render,
        [](App& app) -> void
        {
          auto& state{app.require_resource<State>()};

          std::array<FLOAT, 4> color{1, 0, 0, 1};
          state.device_context.deferred_context->ClearRenderTargetView(
              state.swapchain.render_target_view.Get(), color.data()
          );

          auto* rtv{state.swapchain.render_target_view.Get()};
          state.device_context.deferred_context->OMSetRenderTargets(1, &rtv, nullptr);

          execute_command_list(app, state);

          bool is_vsync_enabled{false}; // TODO: put somewhere better
          if (auto result = state.swapchain.handle->Present(is_vsync_enabled, 0); FAILED(result))
          {
            app.report(Severity::Fatal, "Failed to present swapchain: {}", result);
          }
        }
    );
  }

} // namespace engine
