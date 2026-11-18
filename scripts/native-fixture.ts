import { $ } from "bun";
import { join } from "node:path";
import { build } from "./dawn-inputs";

export async function prepareFixture() {
  const native = join(build, "native");
  if(!await Bun.file(join(native, "CMakeCache.txt")).exists()) {
    throw new Error("Native build directory is missing. Run bun run build first.");
  }
  await $`cmake --build ${native} --target dawn_abi_fixture`;
  return join(native, "dawn_abi_fixture.node");
}
