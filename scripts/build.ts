import { $ } from "bun";
import { join } from "node:path";
import { build, dawnInputs, root } from "./dawn-inputs";
import lock from "../dawn.lock.json";

const inputs = await dawnInputs();
const required = [
  ...[...lock.archives, ...lock.git].map(input => join(input.path, input.sentinel)),
  "generated/src/dawn/node/interop/WebGPU.h",
  "generated/src/dawn/node/interop/WebGPU.cpp",
  "generated/src/dawn/utils/ComboLimits.h",
  "generated/src/dawn/utils/ComboLimits.cpp"
];
for(const path of required) {
  if(!await Bun.file(join(inputs, path)).exists()) {
    throw new Error(`Missing Dawn input ${path}. Run bun setup first.`);
  }
}

const native = join(build, "native");
await $`cmake -S ${root} -B ${native} -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DDAWN_INPUTS=${inputs}`;
await $`cmake --build ${native} --target dawn`;
console.log(`Addon ready at ${join(native, "dawn.node")}`);
