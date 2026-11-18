import { join } from "node:path";
import { root } from "./dawn-inputs";
import { prepareFixture } from "./native-fixture";
import { runInRuntimes } from "./run-in-runtimes";

const addons = await prepareFixture();
const xvfb = Bun.which("Xvfb");
if(!xvfb) throw new Error("Xvfb is required. See README.md for install instructions.");

const server = Bun.spawn([xvfb, "-displayfd", "1", "-screen", "0", "640x480x24", "-nolisten", "tcp", "-noreset", "-ac"], {
  stdout: "pipe",
  stderr: "inherit"
});
try {
  const reader = server.stdout.getReader();
  const timeout = setTimeout(() => server.kill(9), 5000);
  let display;
  try {
    const { value } = await reader.read();
    display = value && new TextDecoder().decode(value).trim();
  } finally {
    clearTimeout(timeout);
    reader.releaseLock();
  }
  if(!display || !/^\d+$/.test(display)) throw new Error("Xvfb did not start a display.");

  await runInRuntimes("Dawn X11 presentation", [
    join(root, "test/surface.cjs"), ...addons
  ], { ...process.env, DISPLAY: `:${display}` });
} finally {
  server.kill();
  await server.exited;
}
