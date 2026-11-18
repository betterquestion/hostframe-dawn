#include "hostframe_dawn/bridge.h"

#include "src/dawn/node/binding/GPU.h"
#include "src/dawn/node/binding/GPUDevice.h"
#include "src/dawn/node/binding/GPUTexture.h"

namespace {
template <typename Interface, typename Binding>
Binding* Unwrap(napi_env env, napi_value value, napi_env* owner = nullptr) {
  napi_valuetype type;
  if (!env || !value || napi_typeof(env, value, &type) != napi_ok || type != napi_object) {
    return nullptr;
  }
  return dynamic_cast<Binding*>(Interface::Unwrap(Napi::Object(env, value), owner));
}

HostframeDawnInstance AcquireInstance(napi_env env, napi_value value) {
  auto* gpu = Unwrap<wgpu::interop::GPU, wgpu::binding::GPU>(env, value);
  auto instance = gpu ? gpu->GetNativeInstance() : nullptr;
  if (instance) {
    wgpuInstanceAddRef(instance);
  }
  return instance;
}

HostframeDawnDevice AcquireDevice(napi_env env, napi_value value) {
  auto* gpu_device = Unwrap<wgpu::interop::GPUDevice, wgpu::binding::GPUDevice>(env, value);
  auto device = gpu_device ? gpu_device->GetNativeDevice() : nullptr;
  if (device) {
    wgpuDeviceAddRef(device);
  }
  return device;
}

napi_status BindingStatus(napi_env env, napi_status fallback) {
  bool pending = false;
  const auto status = napi_is_exception_pending(env, &pending);
  if (status != napi_ok) {
    return status;
  }
  return pending ? napi_pending_exception : fallback;
}

napi_status FinishBinding(napi_env caller, napi_env owner, napi_value value, napi_value* result) {
  auto status = BindingStatus(owner, value ? napi_ok : napi_generic_failure);
  if (status == napi_pending_exception && caller != owner) {
    napi_value exception = nullptr;
    status = napi_get_and_clear_last_exception(owner, &exception);
    if (status != napi_ok) {
      return status;
    }
    status = napi_throw(caller, exception);
    return status == napi_ok ? napi_pending_exception : status;
  }
  if (status == napi_ok) {
    *result = value;
  }
  return status;
}

napi_status RequestSurfaceAdapter(napi_env env, napi_value gpu_value, WGPUSurface surface,
                                 napi_value options, napi_value* result) {
  if (!result) {
    return napi_invalid_arg;
  }
  *result = nullptr;
  napi_env owner = nullptr;
  auto* gpu = Unwrap<wgpu::interop::GPU, wgpu::binding::GPU>(env, gpu_value, &owner);
  if (!gpu || !surface) {
    return napi_invalid_arg;
  }
  wgpu::interop::GPURequestAdapterOptions descriptor;
  if (options) {
    napi_valuetype type;
    const auto status = napi_typeof(env, options, &type);
    if (status != napi_ok) {
      return status;
    }
    if (type != napi_undefined && type != napi_null) {
      if (type != napi_object && type != napi_function) {
        return napi_invalid_arg;
      }
      if (!wgpu::interop::FromJS(env, Napi::Value(env, options), descriptor)) {
        return BindingStatus(env, napi_invalid_arg);
      }
    }
  }

  // Dawn's generated constructors are registered in its own addon environment.
  const napi_value promise = gpu->RequestAdapterForSurface(owner, std::move(descriptor), surface);
  return FinishBinding(env, owner, promise, result);
}

napi_status WrapSurfaceTexture(napi_env env, napi_value device_value, WGPUTexture handle,
                               napi_value* result) {
  if (!result) {
    return napi_invalid_arg;
  }
  *result = nullptr;
  napi_env owner = nullptr;
  auto* binding = Unwrap<wgpu::interop::GPUDevice, wgpu::binding::GPUDevice>(
      env, device_value, &owner);
  if (!binding || !handle) {
    return napi_invalid_arg;
  }
  wgpu::Texture texture(handle);
  wgpu::TextureDescriptor descriptor;
  descriptor.size = {texture.GetWidth(), texture.GetHeight(), texture.GetDepthOrArrayLayers()};
  descriptor.format = texture.GetFormat();
  descriptor.usage = texture.GetUsage();
  const napi_value wrapped = wgpu::interop::GPUTexture::Create<wgpu::binding::GPUTexture>(
      owner, wgpu::Device(binding->GetNativeDevice()), descriptor, std::move(texture));
  return FinishBinding(env, owner, wrapped, result);
}

void SubmitCleanup(HostframeDawnInstance instance, HostframeDawnDevice handle) {
  const wgpu::Device device(handle);
  if (wgpu::Instance(instance).WaitAny(device.GetLostFuture(), 0) == wgpu::WaitStatus::Success) {
    return;
  }
  auto commands = device.CreateCommandEncoder().Finish();
  device.GetQueue().Submit(1, &commands);
}

uint32_t DeviceTick(HostframeDawnDevice device) {
  return dawn::native::DeviceTick(device);
}

constexpr HostframeDawnApi api_v1 = {
    .abi_version = HOSTFRAME_DAWN_ABI_V1,
    .struct_size = sizeof(HostframeDawnApi),
    .acquire_instance = AcquireInstance,
    .acquire_device = AcquireDevice,
    .release_instance = wgpuInstanceRelease,
    .release_device = wgpuDeviceRelease,
    .request_surface_adapter = RequestSurfaceAdapter,
    .wrap_surface_texture = WrapSurfaceTexture,
    .create_surface = wgpuInstanceCreateSurface,
    .surface_get_capabilities = wgpuSurfaceGetCapabilities,
    .surface_capabilities_free_members = wgpuSurfaceCapabilitiesFreeMembers,
    .device_get_adapter = wgpuDeviceGetAdapter,
    .release_adapter = wgpuAdapterRelease,
    .surface_configure = wgpuSurfaceConfigure,
    .surface_get_current_texture = wgpuSurfaceGetCurrentTexture,
    .surface_present = wgpuSurfacePresent,
    .surface_unconfigure = wgpuSurfaceUnconfigure,
    .release_surface = wgpuSurfaceRelease,
    .release_texture = wgpuTextureRelease,
    .submit_cleanup = SubmitCleanup,
    .device_tick = DeviceTick,
    .device_destroy = wgpuDeviceDestroy,
};
}

[[gnu::visibility("default")]]
const HostframeDawnApi* hostframe_dawn_get_api(uint32_t abi_version) {
  return abi_version == HOSTFRAME_DAWN_ABI_V1 ? &api_v1 : nullptr;
}
