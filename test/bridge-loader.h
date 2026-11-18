#ifndef HOSTFRAME_DAWN_TEST_BRIDGE_LOADER_H
#define HOSTFRAME_DAWN_TEST_BRIDGE_LOADER_H

#include "hostframe_dawn/bridge.h"

#include <dlfcn.h>
#include <napi.h>

#include <memory>

namespace fixture {
using Library = std::unique_ptr<void, decltype(&dlclose)>;

inline Library OpenAddon(Napi::Env env, Napi::Value path_arg) {
  const auto path = path_arg.As<Napi::String>().Utf8Value();
  Library library(dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD), dlclose);
  if (!library) {
    throw Napi::Error::New(env, "Dawn addon must be loaded before ABI negotiation");
  }
  return library;
}

inline const HostframeDawnApi* GetApi(Napi::Env env, const Library& library, uint32_t version) {
  const auto get_api = reinterpret_cast<decltype(&hostframe_dawn_get_api)>(
      dlsym(library.get(), "hostframe_dawn_get_api"));
  if (!get_api) {
    throw Napi::Error::New(env, "Dawn addon does not export hostframe_dawn_get_api");
  }
  return get_api(version);
}
}

#endif
