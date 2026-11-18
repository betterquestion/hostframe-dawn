#ifndef HOSTFRAME_DAWN_BRIDGE_H
#define HOSTFRAME_DAWN_BRIDGE_H

#include <stdint.h>
#include <node_api.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTFRAME_DAWN_ABI_V1 1u

typedef struct WGPUInstanceImpl* HostframeDawnInstance;
typedef struct WGPUDeviceImpl* HostframeDawnDevice;

typedef struct HostframeDawnApi {
  uint32_t abi_version;
  uint32_t struct_size;

  /*
   * Call on the JavaScript thread with a live env and value from that env.
   * Each success returns one owned reference to the wrapped Dawn handle.
   * Release each returned reference once using the matching function below.
   * A value without a matching wrapper from this addon returns NULL.
   * Owning a device reference does not prevent explicit destruction or loss.
   */
  HostframeDawnInstance (*acquire_instance)(napi_env env, napi_value gpu);
  HostframeDawnDevice (*acquire_device)(napi_env env, napi_value device);
  void (*release_instance)(HostframeDawnInstance instance);
  void (*release_device)(HostframeDawnDevice device);
} HostframeDawnApi;

/*
 * Request an exact ABI version. An unsupported version returns NULL.
 * The table belongs to dawn.node and remains valid while that library is loaded.
 * New fields may be appended. Check struct_size before reading them.
 */
const HostframeDawnApi* hostframe_dawn_get_api(uint32_t abi_version);

#ifdef __cplusplus
}
#endif

#endif
