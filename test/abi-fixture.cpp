#include "hostframe_dawn/bridge.h"

#include <dlfcn.h>
#include <napi.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>

namespace {
using Library = std::unique_ptr<void, decltype(&dlclose)>;

Library OpenAddon(Napi::Env env, Napi::Value path_arg) {
  const auto path = path_arg.As<Napi::String>().Utf8Value();
  Library library(dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD), dlclose);
  if (!library) {
    throw Napi::Error::New(env, "Dawn addon must be loaded before ABI negotiation");
  }
  return library;
}

const HostframeDawnApi* GetApi(Napi::Env env, const Library& library, uint32_t version) {
  const auto get_api = reinterpret_cast<decltype(&hostframe_dawn_get_api)>(
      dlsym(library.get(), "hostframe_dawn_get_api"));
  if (!get_api) {
    throw Napi::Error::New(env, "Dawn addon does not export hostframe_dawn_get_api");
  }
  return get_api(version);
}

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
  exports.Set("foreignWrapper", Napi::Function::New(env, ForeignWrapper));
  return exports;
}
}

NODE_API_MODULE(dawn_abi_fixture, Initialize)
