#ifndef HOSTFRAME_DAWN_BRIDGE_H
#define HOSTFRAME_DAWN_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <node_api.h>
#include <webgpu/webgpu.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTFRAME_DAWN_ABI_V1 1u

typedef struct WGPUInstanceImpl* HostframeDawnInstance;
typedef struct WGPUDeviceImpl* HostframeDawnDevice;

typedef uint32_t HostframeDawnDeviceStatus;
enum {
  HOSTFRAME_DAWN_DEVICE_USABLE = 0,
  HOSTFRAME_DAWN_DEVICE_WRONG_INSTANCE = 1,
  HOSTFRAME_DAWN_DEVICE_LOST_OR_DESTROYED = 2,
  HOSTFRAME_DAWN_DEVICE_QUERY_FAILED = 3,
};

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

  /*
   * JavaScript-thread operations. All raw Dawn handles must belong to this
   * loaded addon; the GPU and surface must share an instance. Adapter options
   * may be NULL. A successful request returns a Promise for a compatible
   * GPUAdapter or null. An exception from JavaScript remains pending.
   * The wrapped texture must be an acquired frame from the given device.
   * The wrapper retains its own reference; the caller still owns the input.
   * The surface controls frame validity, including after presentation.
   */
  napi_status (*request_surface_adapter)(napi_env env, napi_value gpu, WGPUSurface surface,
                                         napi_value options, napi_value* result);
  napi_status (*wrap_surface_texture)(napi_env env, napi_value device, WGPUTexture texture,
                                     napi_value* result);

  /* Native presentation operations with the pinned WebGPU C signatures. */
  WGPUProcInstanceCreateSurface create_surface;
  WGPUProcSurfaceGetCapabilities surface_get_capabilities;
  WGPUProcSurfaceCapabilitiesFreeMembers surface_capabilities_free_members;
  WGPUProcDeviceGetAdapter device_get_adapter;
  WGPUProcAdapterRelease release_adapter;
  WGPUProcSurfaceConfigure surface_configure;
  WGPUProcSurfaceGetCurrentTexture surface_get_current_texture;
  WGPUProcSurfacePresent surface_present;
  WGPUProcSurfaceUnconfigure surface_unconfigure;
  WGPUProcSurfaceRelease release_surface;
  WGPUProcTextureRelease release_texture;

  /*
   * After unconfiguring and releasing the surface, submit cleanup work while
   * the display is still alive. This skips a lost device. Tick returns nonzero
   * if more ticks are needed. The caller owns the deadline and fallback.
   */
  void (*submit_cleanup)(HostframeDawnInstance instance, HostframeDawnDevice device);
  uint32_t (*device_tick)(HostframeDawnDevice device);
  WGPUProcDeviceDestroy device_destroy;

  /*
   * On the JavaScript thread, borrow two retained handles from this loaded
   * addon for the duration of the call. Check ownership first, then poll the
   * device's lost future with zero timeout. This is synchronous and submits
   * no queue work. NULL handles report QUERY_FAILED. USABLE covers ownership
   * and loss only. Query surface capabilities separately for presentation.
   * Before reading this field from a v1 table, require struct_size >=
   * offsetof(HostframeDawnApi, check_device) + sizeof(api->check_device),
   * where api is the table pointer.
   */
  HostframeDawnDeviceStatus (*check_device)(HostframeDawnInstance expected_instance,
                                           HostframeDawnDevice device);
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
