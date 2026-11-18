#include "hostframe_dawn/bridge.h"

#include <dlfcn.h>
#include <napi.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace {
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

  const std::string path = info[0].As<Napi::String>().Utf8Value();
  std::unique_ptr<void, decltype(&dlclose)> library(
      dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD), dlclose);
  if (!library) {
    throw Napi::Error::New(env, "Dawn addon must be loaded before ABI negotiation");
  }

  const auto get_api = reinterpret_cast<decltype(&hostframe_dawn_get_api)>(
      dlsym(library.get(), "hostframe_dawn_get_api"));
  if (!get_api) {
    throw Napi::Error::New(env, "Dawn addon does not export hostframe_dawn_get_api");
  }
  const auto* api = get_api(static_cast<uint32_t>(version));
  if (!api) {
    return env.Null();
  }

  auto result = Napi::Object::New(env);
  result.Set("version", Napi::Number::New(env, api->abi_version));
  result.Set("size", Napi::Number::New(env, api->struct_size));
  return result;
}

Napi::Object Initialize(Napi::Env env, Napi::Object exports) {
  exports.Set("negotiate", Napi::Function::New(env, Negotiate));
  return exports;
}
}

NODE_API_MODULE(dawn_abi_fixture, Initialize)
