import { join } from "node:path";
import { root } from "./dawn-inputs";
import { prepareFixture } from "./native-fixture";
import { runInRuntimes } from "./run-in-runtimes";

const addons = await prepareFixture();
await runInRuntimes("Dawn bridge ABI", [join(root, "test/abi.cjs"), ...addons]);
