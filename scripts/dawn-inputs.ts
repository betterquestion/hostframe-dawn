import { join, resolve } from "node:path";
import lock from "../dawn.lock.json";

export const root = join(import.meta.dir, "..");
export const build = join(root, "build");
export const bindingPatches = ["bridge-wrappers.patch", "surface-adapter.patch"]
  .map(name => join(root, "patches", name));

export async function testAddon(path = join(build, "native/dawn.node")) {
  const addon = resolve(path);
  if(!await Bun.file(addon).exists()) throw new Error(`Missing addon at ${addon}. Run bun run build or provide an addon path.`);
  return addon;
}

export async function dawnInputs() {
  const hash = new Bun.CryptoHasher("sha256")
    .update(JSON.stringify(lock))
    .update(await Bun.file(join(import.meta.dir, "setup.ts")).text());
  for(const patch of bindingPatches) hash.update(await Bun.file(patch).text());
  const digest = hash.digest("hex").slice(0, 12);
  return join(build, `dawn-${lock.revision.slice(0, 12)}-${digest}`);
}
