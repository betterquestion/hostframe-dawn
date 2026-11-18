import { join } from "node:path";
import lock from "../dawn.lock.json";

export const root = join(import.meta.dir, "..");
export const build = join(root, "build");

export async function dawnInputs() {
  const digest = new Bun.CryptoHasher("sha256")
    .update(JSON.stringify(lock))
    .update(await Bun.file(join(import.meta.dir, "setup.ts")).text())
    .digest("hex").slice(0, 12);
  return join(build, `dawn-${lock.revision.slice(0, 12)}-${digest}`);
}
