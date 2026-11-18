import { $ } from "bun";
import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { root } from "./dawn-inputs";
import { archiveChecksum, archiveName, bridgeHeader, packageDirectory, packageMetadata, packageName } from "./linux-package";

const archive = join(packageDirectory, archiveName);
const checksum = await Bun.file(`${archive}.sha256`).text();
assert.equal(checksum, `${await archiveChecksum(archive)}  ${archiveName}\n`, "Package checksum mismatch");

const staging = await mkdtemp(join(tmpdir(), "hostframe-dawn-package-"));
try {
  await $`tar -xzf ${archive} -C ${staging}`;
  const payload = join(staging, packageName);
  assert.deepEqual(await Bun.file(join(payload, "metadata.json")).json(), await packageMetadata());
  const include = join(payload, "include");
  await $`clang -x c -std=c11 -pedantic-errors -fsyntax-only -I ${include} ${join(payload, bridgeHeader)}`;
  const addon = join(payload, "dawn.node");
  console.log(`Testing unpacked addon at ${addon}`);
  for(const check of ["test:abi", "test:compute", "test:surface"]) {
    await $`bun run ${check} ${addon}`.cwd(root);
  }
} finally {
  await rm(staging, { recursive: true, force: true });
}
