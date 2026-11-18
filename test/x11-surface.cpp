#include "x11-surface.h"

#include "bridge-loader.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>

namespace fixture {
namespace {
constexpr uint32_t kSize = 64;

Napi::Value BindingResult(Napi::Env env, napi_status status, napi_value value,
                          const char* message) {
  if (status == napi_pending_exception) {
    return {};
  }
  if (status != napi_ok || !value) {
    throw Napi::Error::New(env, message);
  }
  return Napi::Value(env, value);
}

struct SurfaceState {
  Library library{nullptr, dlclose};
  const HostframeDawnApi* api = nullptr;
  Display* display = nullptr;
  Window window = 0;
  WGPUInstance instance = nullptr;
  WGPUSurface surface = nullptr;
  WGPUDevice device = nullptr;
  WGPUTexture frame = nullptr;
  bool configured = false;

  ~SurfaceState() { Close(); }

  bool Close() {
    using namespace std::chrono_literals;
    bool drained = true;
    if (frame) {
      api->release_texture(frame);
      frame = nullptr;
    }
    if (surface) {
      if (configured) {
        api->surface_unconfigure(surface);
      }
      api->release_surface(surface);
      surface = nullptr;
    }
    if (device) {
      if (configured) {
        api->submit_cleanup(instance, device);
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (api->device_tick(device)) {
          if (std::chrono::steady_clock::now() >= deadline) {
            api->device_destroy(device);
            drained = false;
            break;
          }
          std::this_thread::sleep_for(1ms);
        }
      }
      api->release_device(device);
      device = nullptr;
    }
    if (instance) {
      api->release_instance(instance);
      instance = nullptr;
    }
    if (display) {
      if (window) {
        XDestroyWindow(display, window);
      }
      XCloseDisplay(display);
      display = nullptr;
    }
    return drained;
  }
};

class X11Surface final : public Napi::ObjectWrap<X11Surface> {
 public:
  explicit X11Surface(const Napi::CallbackInfo& info) : ObjectWrap(info) {
    const auto env = info.Env();
    if (info.Length() != 2 || !info[0].IsString()) {
      throw Napi::TypeError::New(env, "Expected a Dawn addon path and GPU");
    }
    state_.library = OpenAddon(env, info[0]);
    state_.api = GetApi(env, state_.library, HOSTFRAME_DAWN_ABI_V1);
    constexpr auto size = offsetof(HostframeDawnApi, device_destroy) +
                          sizeof(HostframeDawnApi::device_destroy);
    if (!state_.api || state_.api->struct_size < size) {
      throw Napi::Error::New(env, "Dawn bridge lacks the surface operations");
    }
    state_.instance = state_.api->acquire_instance(env, info[1]);
    if (!state_.instance) {
      throw Napi::TypeError::New(env, "Expected a Dawn GPU");
    }

    state_.display = XOpenDisplay(nullptr);
    if (!state_.display) {
      throw Napi::Error::New(env, "Could not open the test X11 display");
    }
    const auto screen = DefaultScreen(state_.display);
    state_.window = XCreateSimpleWindow(state_.display, RootWindow(state_.display, screen),
                                        0, 0, kSize, kSize, 0, 0, BlackPixel(state_.display, screen));
    XMapWindow(state_.display, state_.window);
    XSync(state_.display, False);

    WGPUSurfaceSourceXlibWindow source = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
    source.display = state_.display;
    source.window = state_.window;
    WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
    descriptor.nextInChain = &source.chain;
    state_.surface = state_.api->create_surface(state_.instance, &descriptor);
    if (!state_.surface) {
      throw Napi::Error::New(env, "Could not create the Dawn surface");
    }
  }

  static Napi::Function Define(Napi::Env env) {
    return DefineClass(env, "X11Surface", {
        InstanceMethod("requestAdapter", &X11Surface::RequestAdapter),
        InstanceMethod("acquire", &X11Surface::Acquire),
        InstanceMethod("present", &X11Surface::Present),
        InstanceMethod("readPixels", &X11Surface::ReadPixels),
        InstanceMethod("close", &X11Surface::Close),
    });
  }

 private:
  void RequireOpen(Napi::Env env) const {
    if (!state_.surface) {
      throw Napi::Error::New(env, "Test surface is closed");
    }
  }

  Napi::Value RequestAdapter(const Napi::CallbackInfo& info) {
    const auto env = info.Env();
    RequireOpen(env);
    // Check that the GPU passed from JavaScript owns this fixture's surface.
    const auto instance = state_.api->acquire_instance(env, info[0]);
    if (instance) {
      state_.api->release_instance(instance);
    }
    if (instance != state_.instance) {
      throw Napi::TypeError::New(env, "GPU must belong to the surface instance");
    }
    napi_value result = nullptr;
    const auto status = state_.api->request_surface_adapter(
        env, info[0], state_.surface, info[1], &result);
    return BindingResult(env, status, result, "Could not request a surface adapter");
  }

  void Configure(Napi::Env env, WGPUDevice device) {
    const auto adapter = state_.api->device_get_adapter(device);
    if (!adapter) {
      throw Napi::Error::New(env, "Could not get the device adapter");
    }
    WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
    const auto status = state_.api->surface_get_capabilities(state_.surface, adapter, &capabilities);
    state_.api->release_adapter(adapter);
    if (status != WGPUStatus_Success) {
      throw Napi::Error::New(env, "Could not get surface capabilities");
    }
    const auto format = capabilities.formatCount ? capabilities.formats[0] : WGPUTextureFormat_Undefined;
    const bool can_render = (capabilities.usages & WGPUTextureUsage_RenderAttachment) != 0;
    state_.api->surface_capabilities_free_members(capabilities);
    if (format == WGPUTextureFormat_Undefined || !can_render) {
      throw Napi::Error::New(env, "Surface cannot be used as a render attachment");
    }

    WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
    config.device = device;
    config.format = format;
    config.width = kSize;
    config.height = kSize;
    state_.api->surface_configure(state_.surface, &config);
    state_.configured = true;
  }

  Napi::Value Acquire(const Napi::CallbackInfo& info) {
    const auto env = info.Env();
    RequireOpen(env);
    if (state_.frame) {
      throw Napi::Error::New(env, "A frame is already acquired");
    }
    const auto device = state_.api->acquire_device(env, info[0]);
    if (!device) {
      throw Napi::TypeError::New(env, "Expected a Dawn GPUDevice");
    }
    std::unique_ptr<WGPUDeviceImpl, WGPUProcDeviceRelease> acquired(device, state_.api->release_device);
    if (state_.device && state_.device != device) {
      throw Napi::Error::New(env, "The fixture uses one device per surface");
    }
    if (!state_.device) {
      Configure(env, device);
      state_.device = acquired.release();
    }

    WGPUSurfaceTexture texture = WGPU_SURFACE_TEXTURE_INIT;
    state_.api->surface_get_current_texture(state_.surface, &texture);
    if (!texture.texture || (texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
                             texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)) {
      if (texture.texture) {
        state_.api->release_texture(texture.texture);
      }
      throw Napi::Error::New(env, "Dawn could not acquire a surface texture");
    }
    state_.frame = texture.texture;
    napi_value result = nullptr;
    const auto status = state_.api->wrap_surface_texture(env, info[0], state_.frame, &result);
    return BindingResult(env, status, result, "Could not wrap the surface texture");
  }

  void Present(const Napi::CallbackInfo& info) {
    if (!state_.frame) {
      throw Napi::Error::New(info.Env(), "Acquire a frame before presenting");
    }
    if (state_.api->surface_present(state_.surface) != WGPUStatus_Success) {
      throw Napi::Error::New(info.Env(), "Dawn could not present the surface texture");
    }
    state_.api->release_texture(state_.frame);
    state_.frame = nullptr;
  }

  Napi::Value ReadPixels(const Napi::CallbackInfo& info) {
    const auto env = info.Env();
    RequireOpen(env);
    auto destroy = [](XImage* image) { XDestroyImage(image); };
    std::unique_ptr<XImage, decltype(destroy)> image(
        XGetImage(state_.display, state_.window, 0, 0, kSize, kSize, AllPlanes, ZPixmap),
        destroy);
    if (!image) {
      throw Napi::Error::New(env, "Could not read the test window pixels");
    }
    auto pixels = Napi::Uint8Array::New(env, kSize * kSize * 3);
    auto* bytes = pixels.Data();
    for (uint32_t y = 0; y < kSize; ++y) {
      for (uint32_t x = 0; x < kSize; ++x) {
        const auto pixel = XGetPixel(image.get(), x, y);
        for (const auto mask : {image->red_mask, image->green_mask, image->blue_mask}) {
          const auto low_bit = mask & -mask;
          *bytes++ = low_bit ? (pixel & mask) / low_bit * 255 / (mask / low_bit) : 0;
        }
      }
    }
    return pixels;
  }

  void Close(const Napi::CallbackInfo& info) {
    if (!state_.Close()) {
      throw Napi::Error::New(info.Env(), "Dawn cleanup timed out before closing the display");
    }
  }

  SurfaceState state_;
};
}

void ExportX11Surface(Napi::Env env, Napi::Object exports) {
  exports.Set("X11Surface", X11Surface::Define(env));
}
}
