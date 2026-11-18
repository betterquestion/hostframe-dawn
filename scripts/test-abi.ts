import { join } from "node:path";
import { root, testAddon } from "./dawn-inputs";
import { prepareFixture } from "./native-fixture";
import { runInRuntimes } from "./run-in-runtimes";

const addon = await testAddon(process.argv[2]);
const fixture = await prepareFixture();
await runInRuntimes("Dawn bridge ABI", [join(root, "test/abi.cjs"), addon, fixture]);
