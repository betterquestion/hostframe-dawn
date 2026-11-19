#include "bridge-loader.h"

#ifdef HOSTFRAME_DAWN_X11
#include "x11-surface.h"
#endif

#include <cmath>
#include <cstddef>
#include <limits>

namespace {
using fixture::GetApi;
using fixture::OpenAddon;

Napi::Value Negotiate(const Napi::CallbackInfo& info) {
  const auto env = info.Env();
  if (info.Length() != 2 || !info[0].IsString() || !info[1].IsNumber()) {
    throw Napi::TypeError::New(env, "Expected a module path and an ABI version");
  }
  const double version = info[1].As<Napi::Number>().DoubleValue();
  if (!std::isfinite(version) || version != std::floor(version) ||
      version < 0 || version > std::numeric_limits<uint32_t>::max()) {
    throw Napi::RangeError::New(env, "ABI version must be a uint32");
  }

  const auto library = OpenAddon(env, info[0]);
  const auto* api = GetApi(env, library, static_cast<uint32_t>(version));
  if (!api) {
    return env.Null();
  }

  auto result = Napi::Object::New(env);
  result.Set("version", Napi::Number::New(env, api->abi_version));
  result.Set("size", Napi::Number::New(env, api->struct_size));
  return result;
}

template <auto Acquire, auto Release>
Napi::Value CompareHandles(const Napi::CallbackInfo& info) {
  const auto env = info.Env();
  if (info.Length() != 3 || !info[0].IsString()) {
    throw Napi::TypeError::New(env, "Expected a module path and two JavaScript values");
  }
  const auto library = OpenAddon(env, info[0]);
  const auto* api = GetApi(env, library, HOSTFRAME_DAWN_ABI_V1);
  constexpr auto required_size = offsetof(HostframeDawnApi, release_device) +
                                 sizeof(HostframeDawnApi::release_device);
  if (!api || api->struct_size < required_size) {
    throw Napi::Error::New(env, "Dawn bridge lacks the handle accessors");
  }

  const auto acquire = api->*Acquire;
  const auto release = api->*Release;
  const auto first = acquire(env, info[1]);
  const auto second = acquire(env, info[2]);
  const bool valid = first && second;
  const bool same = first == second;
  if (first) {
    release(first);
  }
  if (second) {
    release(second);
  }
  if (!valid) {
    return env.Null();
  }
  return Napi::Boolean::New(env, same);
}

Napi::Value CheckDevice(const Napi::CallbackInfo& info) {
  const auto env = info.Env();
  if (info.Length() < 3 || info.Length() > 4 || !info[0].IsString() ||
      (info.Length() == 4 && !info[3].IsFunction())) {
    throw Napi::TypeError::New(env, "Expected a module path, GPU, device, and optional callback");
  }

  const auto library = OpenAddon(env, info[0]);
  const auto* api = GetApi(env, library, HOSTFRAME_DAWN_ABI_V1);
  constexpr auto required_size = offsetof(HostframeDawnApi, check_device) +
                                 sizeof(HostframeDawnApi::check_device);
  if (!api || api->struct_size < required_size) {
    throw Napi::Error::New(env, "Dawn bridge lacks check_device");
  }

  const std::unique_ptr<WGPUInstanceImpl, decltype(api->release_instance)> instance(
      api->acquire_instance(env, info[1]), api->release_instance);
  const std::unique_ptr<WGPUDeviceImpl, decltype(api->release_device)> device(
      api->acquire_device(env, info[2]), api->release_device);
  if (info.Length() == 4) {
    if (!instance || !device) {
      throw Napi::Error::New(env, "Could not retain the Dawn handles before the callback");
    }
    info[3].As<Napi::Function>().Call({});
  }
  return Napi::Number::New(env, api->check_device(instance.get(), device.get()));
}

Napi::Value ForeignWrapper(const Napi::CallbackInfo& info) {
  static int payload = 0;
  const auto env = info.Env();
  auto object = Napi::Object::New(env);
  if (napi_wrap(env, object, &payload, nullptr, nullptr, nullptr) != napi_ok) {
    throw Napi::Error::New(env, "Could not create a foreign native wrapper");
  }
  return object;
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports) {
  exports.Set("negotiate", Napi::Function::New(env, Negotiate));
  exports.Set("compareInstances", Napi::Function::New(
      env, CompareHandles<&HostframeDawnApi::acquire_instance, &HostframeDawnApi::release_instance>));
  exports.Set("compareDevices", Napi::Function::New(
      env, CompareHandles<&HostframeDawnApi::acquire_device, &HostframeDawnApi::release_device>));
  exports.Set("checkDevice", Napi::Function::New(env, CheckDevice));
  exports.Set("foreignWrapper", Napi::Function::New(env, ForeignWrapper));
#ifdef HOSTFRAME_DAWN_X11
  fixture::ExportX11Surface(env, exports);
#endif
  return exports;
}
}

NODE_API_MODULE(dawn_abi_fixture, Initialize)
