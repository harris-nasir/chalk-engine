module;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl.h>

#include <algorithm>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgitype.h>
#include <utility>

export module engine.renderer.dx11;

import engine.core;
import engine.platform;
import engine.renderer.types;

export import :renderer;

export namespace engine
{

  class RendererDX11Plugin
  {
  public:
    void build(App& app);
  };

} // namespace engine

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
          if (native_window.kind != NativeWindowKind::Win32)
          {
            app.report(Severity::Fatal, "unsupported native window provided, native window must be a win32 window");
          }

          UINT create_device_flags{};

#ifdef _DEBUG
          create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

          Microsoft::WRL::ComPtr<ID3D11Device> device;
          Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
          D3D_FEATURE_LEVEL feature_level{};
          constexpr D3D_FEATURE_LEVEL device_feature_level = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_1;

          if (auto result = D3D11CreateDevice(
                  nullptr,
                  D3D_DRIVER_TYPE_HARDWARE,
                  nullptr,
                  create_device_flags,
                  &device_feature_level,
                  1,
                  D3D11_SDK_VERSION,
                  &device,
                  &feature_level,
                  &immediate_context
              );
              FAILED(result))
          {
            app.report(Severity::Fatal, "creating graphics context failed: {}", result);
          }

          Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
          if (auto result = device->QueryInterface(IID_PPV_ARGS(&dxgi_device)); FAILED(result))
          {
            app.report(Severity::Fatal, "retrieving dxgi device failed: {}", result);
          }

          Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
          if (auto result = dxgi_device->GetParent(IID_PPV_ARGS(&dxgi_adapter)); FAILED(result))
          {
            app.report(Severity::Fatal, "retrieving dxgi adapter failed: {}", result);
          }

          Microsoft::WRL::ComPtr<IDXGIFactory> dxgi_factory;
          if (auto result = dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)); FAILED(result))
          {
            app.report(Severity::Fatal, "retrieving dxgi factory failed: {}", result);
          }

          const auto swapchain_width{window.width};
          const auto swapchain_height{window.height};
          DXGI_SWAP_CHAIN_DESC swapchain_description{
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
              .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
              .BufferCount  = 2, // double buffering
              .OutputWindow = static_cast<HWND>(native_window.handle),
              .Windowed     = true,                          // TODO: take from window desc
              .SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD, // discard back buffer after swap
              .Flags        = 0
          };

          Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain;
          if (auto result = dxgi_factory->CreateSwapChain(device.Get(), &swapchain_description, &swapchain);
              FAILED(result))
          {
            app.report(Severity::Fatal, "creating swapchain failed: {}", result);
          }

          Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer{};
          if (auto result = swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)); FAILED(result))
          {
            app.report(Severity::Fatal, "retrieving swapchain buffer failed: {}", result);
          }

          Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
          if (auto result = device->CreateRenderTargetView(backbuffer.Get(), nullptr, &render_target_view);
              FAILED(result))
          {
            app.report(Severity::Fatal, "creating render target view failed: {}", result);
          }

          Microsoft::WRL::ComPtr<ID3D11DeviceContext> deferred_context;
          if (auto result = device->CreateDeferredContext(0, &deferred_context); FAILED(result))
          {
            app.report(Severity::Fatal, "creating deferred context failed: {}", result);
          }

          app.insert_resource<DX11Renderer>(DX11Renderer{
              app,
              std::move(device),
              std::move(immediate_context),
              std::move(deferred_context),
              std::move(swapchain),
              std::move(render_target_view),
              feature_level
          });
          app.report(Severity::Info, "dx11 renderer ready");
        }
    );

    app.add_system(
        Schedule::Shutdown,
        [](App& app) -> void
        {
          // ComPtr members release automatically
          app.remove_resource<DX11Renderer>();
          app.report(Severity::Info, "dx11 renderer shut down");
        }
    );
  }

} // namespace engine
