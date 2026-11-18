#include "hostframe_dawn/bridge.h"

namespace {
constexpr HostframeDawnApi api_v1 = {
    HOSTFRAME_DAWN_ABI_V1,
    sizeof(HostframeDawnApi),
};
}

[[gnu::visibility("default")]]
const HostframeDawnApi* hostframe_dawn_get_api(uint32_t abi_version) {
  return abi_version == HOSTFRAME_DAWN_ABI_V1 ? &api_v1 : nullptr;
}
