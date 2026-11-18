import { join } from "node:path";
import { build, root } from "./dawn-inputs";
import { runInRuntimes } from "./run-in-runtimes";

const addon = join(build, "native/dawn.node");
if(!await Bun.file(addon).exists()) throw new Error("Missing dawn.node. Run bun run build first.");

await runInRuntimes("Dawn compute", ["--expose-gc", join(root, "test/compute.cjs"), addon]);
