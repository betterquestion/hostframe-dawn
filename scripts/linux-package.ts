import { join } from "node:path";
import { build, root } from "./dawn-inputs";
import lock from "../dawn.lock.json";

export const packageName = "hostframe-dawn-linux-x64";
export const packageDirectory = join(build, "package");
export const archiveName = `${packageName}.tar.gz`;
export const bridgeHeader = "include/hostframe_dawn/bridge.h";

export async function packageMetadata() {
  const header = await Bun.file(join(root, bridgeHeader)).text();
  const abi = header.match(/^#define HOSTFRAME_DAWN_ABI_V(\d+) \1u$/m);
  if(!abi) throw new Error("Could not read the ABI version from the public bridge header.");
  return {
    platform: "linux",
    arch: "x64",
    dawnRevision: lock.revision,
    dawnRelease: lock.release,
    bridgeAbiVersion: Number(abi[1])
  };
}

export async function archiveChecksum(path: string) {
  const hash = new Bun.CryptoHasher("sha256");
  for await(const chunk of Bun.file(path).stream()) hash.update(chunk);
  return hash.digest("hex");
}
