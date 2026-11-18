#include "hostframe_dawn/bridge.h"

#include "src/dawn/node/binding/GPU.h"
#include "src/dawn/node/binding/GPUDevice.h"

namespace {
template <typename Interface, typename Binding>
Binding* Unwrap(napi_env env, napi_value value) {
  napi_valuetype type;
  if (!env || !value || napi_typeof(env, value, &type) != napi_ok || type != napi_object) {
    return nullptr;
  }
  return dynamic_cast<Binding*>(Interface::Unwrap(Napi::Object(env, value)));
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

constexpr HostframeDawnApi api_v1 = {
    HOSTFRAME_DAWN_ABI_V1,
    sizeof(HostframeDawnApi),
    AcquireInstance,
    AcquireDevice,
    wgpuInstanceRelease,
    wgpuDeviceRelease,
};
}

[[gnu::visibility("default")]]
const HostframeDawnApi* hostframe_dawn_get_api(uint32_t abi_version) {
  return abi_version == HOSTFRAME_DAWN_ABI_V1 ? &api_v1 : nullptr;
}
