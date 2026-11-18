import { $ } from "bun";
import { join } from "node:path";
import { build, root } from "./dawn-inputs";
import { runInRuntimes } from "./run-in-runtimes";

const native = join(build, "native");
if(!await Bun.file(join(native, "CMakeCache.txt")).exists()) {
  throw new Error("Native build directory is missing. Run bun run build first.");
}
await $`cmake --build ${native} --target dawn dawn_abi_fixture`;
await runInRuntimes("Dawn bridge ABI", [
  join(root, "test/abi.cjs"), join(native, "dawn.node"), join(native, "dawn_abi_fixture.node")
]);
