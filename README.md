# hostframe-dawn

This project will build and distribute a patched Dawn Node addon for Hostframe.
[Hostframe discussion #46](https://github.com/jacl38/hostframe/discussions/46)
explains why the project is being split out. Hostframe needs access to Dawn's
JavaScript binding internals for window presentation. Shipping that access in a
separate binary lets Hostframe own windowing and application lifecycle while
this project owns the C++ binding build and release.

The “incision” is a small patch set at Dawn's Node binding boundary. It exposes
a versioned C interface for getting the native instance and device behind Dawn's
JavaScript objects, selecting a surface-compatible adapter, and wrapping an
acquired surface texture as a Dawn `GPUTexture`. The interface also exposes the
Dawn operations an embedder needs to drive deferred cleanup. Hostframe owns
when to run that cleanup and when to close the window.

A checked-in manifest will pin the upstream Dawn revision and the URLs and
checksums for its source archive and matching prebuilt engine artifacts. The
build will verify the downloads, apply the patches to an extracted copy, and
compile the patched Node bindings and bridge against the matching engine. This
keeps the build focused on the addon and uses Dawn's prebuilt engine as shipped.
A revision and platform are eligible when a matching engine artifact is
available and exports the functions the bridge needs.

## Development setup

The bootstrap currently targets Linux x86-64. Install Bun 1.3.14 or newer
and a current Node.js release. On Fedora 44, install the host build tools with:

```sh
sudo dnf install clang cmake ninja-build golang python3 git curl tar
```

Use Go 1.26 or newer. From this repository, run:

```sh
bun setup
bun run lint
```

Setup installs the locked development dependencies, downloads and verifies the
Dawn source, headers, Linux engine archive, and binding dependencies, then runs
Dawn's Go and Python generators. Jinja2 and MarkupSafe are fetched at Dawn's
pinned revisions. Setup prints the directory containing `source/`, `headers/`,
`engine/`, and `generated/`. The engine archive is at
`engine/lib64/libwebgpu_dawn.a`. Rerunning setup reuses completed inputs for the
same manifest. Downloads and generated files live under the ignored `build/`
directory.

Build the addon and run the windowless compute check with:

```sh
bun run build
bun run test:compute
```

The build applies `patches/dawn-node.patch` to a build-local copy and writes
`build/native/dawn.node`. The compute check loads that same binary in fresh
Node and Bun processes and asserts the GPU readback in both.
It requires a Vulkan driver. On Fedora, `sudo dnf install mesa-vulkan-drivers`
provides Mesa's Vulkan drivers, including a software driver.

Run `bun run test:abi` after building to check the C entry point in
`include/hostframe_dawn/bridge.h`. The native fixture resolves
`hostframe_dawn_get_api` from the `dawn.node` JavaScript loaded and checks
version negotiation and handle access in Node and Bun using Vulkan.
The v1 table can acquire the native instance and device from genuine Dawn
`GPU` and `GPUDevice` objects. Each successful acquisition transfers one
reference to the caller, which releases it through the matching table entry.
The optional v1 `check_device` entry checks whether a retained device belongs to
the expected instance and whether it has been lost or destroyed. The ABI fixture
checks ordinary JavaScript-created devices, a device from a second instance,
and a device destroyed immediately before the check while the native handles
remain retained. See the public header for the required table size check.

For the Linux/X11 presentation fixture, also install the X11 development
headers and Xvfb:

```sh
sudo dnf install libX11-devel xorg-x11-server-Xvfb vulkan-tools
```

Run `bun run test:surface` after setup and build. The runner starts its own
Xvfb display. The native fixture creates the X11 window, requests a compatible
adapter through the bridge, and hands JavaScript an acquired Dawn
`GPUTexture`. The test clears it to magenta, presents it, and checks every
window pixel. The fixture releases the surface and drains deferred Dawn
cleanup before closing the display. Both Node and Bun must exit successfully.

## Linux package

```sh
bun setup
bun run package
bun run test:package
```

The archive and its SHA-256 checksum are written to `build/package/`. The
archive contains `dawn.node`, the C bridge and its matching WebGPU and Node-API
headers, dependency licenses, and metadata with the Dawn revision and ABI
version. The package test verifies the checksum, extracts the archive, checks
that the headers compile as C, and runs ABI, compute, and X11 presentation
tests against the extracted binary.

The Linux packaging workflow targets Ubuntu 24.04 x86-64 and uses Mesa's
software Vulkan driver. Run the same job locally with Act and Docker:

```sh
act pull_request -j linux-package \
  -P ubuntu-24.04=catthehacker/ubuntu:act-24.04 \
  --container-daemon-socket -
```

After a successful build and package test on `main`, GitHub Actions publishes
the archive and its checksum on the [Releases page](https://github.com/betterquestion/hostframe-dawn/releases).
Each release uses the tag `linux-x64-<full commit SHA>` so consumers can pin a
specific build. The [latest release](https://github.com/betterquestion/hostframe-dawn/releases/latest)
also provides stable download links for the
[archive](https://github.com/betterquestion/hostframe-dawn/releases/latest/download/hostframe-dawn-linux-x64.tar.gz)
and its [SHA-256 checksum](https://github.com/betterquestion/hostframe-dawn/releases/latest/download/hostframe-dawn-linux-x64.tar.gz.sha256).

## Validate before integrating with Hostframe

Use a small native test addon as the first consumer of the public C interface.
It loads the entry point from the exact `dawn.node` already loaded by
JavaScript. The fixture links only its own test code and platform dependencies.
Run it against the same binary in fresh Node and Bun processes.

1. **Check the bridge without a window.** Load the addon, check ABI version
   negotiation, and pass real JavaScript GPU and GPUDevice objects across the
   boundary. Verify that malformed objects fail cleanly. Submit a small compute
   operation and assert the readback so the test also checks that the patched
   addon still performs ordinary WebGPU work.
2. **Prove the surface handoff.** On Linux/X11 with Vulkan, let the fixture
   create a minimal test window and supply its native surface descriptor.
   Request a surface-compatible adapter through the bridge, configure the
   surface with a JavaScript-created device, and pass an acquired texture back
   to JavaScript. Assert that it is a Dawn `GPUTexture`, clear it to a known
   color through WebGPU, present it through the fixture, and check the window
   output. Release the surface, drive deferred cleanup with the display still
   alive, close the window, and assert clean process exit.

The fixture owns only enough window lifetime to exercise the binary interface.
These checks establish that the fork can supply the capabilities Hostframe
needs. Application-level resize, frame scheduling, and lifecycle behavior
belong in Hostframe's tests once its new integration exists.
