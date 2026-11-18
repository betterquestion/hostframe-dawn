import { $ } from "bun";
import { cp, mkdir, mkdtemp, rename, rm } from "node:fs/promises";
import { dirname, join } from "node:path";
import { build, dawnInputs, root } from "./dawn-inputs";
import { archiveChecksum, archiveName, bridgeHeader, packageDirectory, packageMetadata, packageName } from "./linux-package";

if(process.platform !== "linux" || process.arch !== "x64") {
  throw new Error("Packaging currently supports Linux x86-64.");
}

await $`bun run build`.cwd(root);
const inputs = await dawnInputs();
const source = join(inputs, "source");
const thirdParty = join(source, "third_party");
await mkdir(packageDirectory, { recursive: true });
const staging = await mkdtemp(join(packageDirectory, ".package-"));
try {
  const payload = join(staging, packageName);
  const files = [
    [join(build, "native/dawn.node"), "dawn.node"],
    [join(root, bridgeHeader), bridgeHeader],
    [join(inputs, "headers/include/webgpu/webgpu.h"), "include/webgpu/webgpu.h"],
    [join(inputs, "headers/include/dawn/webgpu.h"), "include/dawn/webgpu.h"],
    [join(inputs, "headers/include/dawn/dawn_version.h"), "include/dawn/dawn_version.h"],
    [join(source, "LICENSE"), "licenses/dawn.txt"],
    [join(root, "licenses/abseil-cpp.txt"), "licenses/abseil-cpp.txt"],
    [join(thirdParty, "node-addon-api/LICENSE.md"), "licenses/node-addon-api.md"],
    [join(thirdParty, "node-api-headers/LICENSE"), "licenses/node-api-headers.txt"],
    [join(thirdParty, "gpuweb/LICENSE.md"), "licenses/gpuweb.md"],
    [join(thirdParty, "webgpu-headers/LICENSE"), "licenses/webgpu-headers.txt"],
    [join(thirdParty, "spirv-headers/LICENSE"), "licenses/spirv-headers.txt"],
    [join(thirdParty, "spirv-tools/LICENSE"), "licenses/spirv-tools.txt"],
    [join(thirdParty, "vulkan-headers/LICENSE.txt"), "licenses/vulkan-headers.txt"],
    [join(thirdParty, "EGL-Registry/LICENSE"), "licenses/egl-registry.txt"],
    [join(thirdParty, "OpenGL-Registry/LICENSE"), "licenses/opengl-registry.txt"]
  ];
  await cp(join(thirdParty, "node-api-headers/include"), join(payload, "include"), { recursive: true });
  for(const [from, path] of files) {
    const target = join(payload, path);
    await mkdir(dirname(target), { recursive: true });
    await cp(from, target);
  }
  await Bun.write(join(payload, "metadata.json"), `${JSON.stringify(await packageMetadata(), null, 2)}\n`);

  const archive = join(staging, archiveName);
  await $`tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -czf ${archive} -C ${staging} ${packageName}`;
  const checksum = `${archive}.sha256`;
  await Bun.write(checksum, `${await archiveChecksum(archive)}  ${archiveName}\n`);
  await rename(archive, join(packageDirectory, archiveName));
  await rename(checksum, join(packageDirectory, `${archiveName}.sha256`));
  console.log(`Package and SHA-256 checksum ready in ${packageDirectory}`);
} finally {
  await rm(staging, { recursive: true, force: true });
}
