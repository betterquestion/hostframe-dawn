#ifndef HOSTFRAME_DAWN_TEST_X11_SURFACE_H
#define HOSTFRAME_DAWN_TEST_X11_SURFACE_H

#include <napi.h>

namespace fixture {
void ExportX11Surface(Napi::Env env, Napi::Object exports);
}

#endif
