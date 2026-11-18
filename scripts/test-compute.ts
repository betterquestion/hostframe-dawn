import { join } from "node:path";
import { root, testAddon } from "./dawn-inputs";
import { runInRuntimes } from "./run-in-runtimes";

const addon = await testAddon(process.argv[2]);

await runInRuntimes("Dawn compute", ["--expose-gc", join(root, "test/compute.cjs"), addon]);
