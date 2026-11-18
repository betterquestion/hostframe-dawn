import { join } from "node:path";
import { build, root } from "./dawn-inputs";

const addon = join(build, "native/dawn.node");
if(!await Bun.file(addon).exists()) throw new Error("Missing dawn.node. Run bun run build first.");

for(const [name, executable] of [["Node", "node"], ["Bun", process.execPath]]) {
  console.log(`Testing Dawn compute in ${name}`);
  const child = Bun.spawn([executable, "--expose-gc", join(root, "test/compute.cjs"), addon], {
    stdout: "inherit",
    stderr: "inherit"
  });
  const timeout = setTimeout(() => child.kill(9), 35000);
  try {
    const status = await child.exited;
    if(status !== 0) throw new Error(`${name} compute test failed (exit ${status}).`);
  } finally {
    clearTimeout(timeout);
  }
}
