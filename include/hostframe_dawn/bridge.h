#ifndef HOSTFRAME_DAWN_BRIDGE_H
#define HOSTFRAME_DAWN_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTFRAME_DAWN_ABI_V1 1u

typedef struct HostframeDawnApi {
  uint32_t abi_version;
  uint32_t struct_size;
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
