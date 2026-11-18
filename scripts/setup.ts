import { $ } from "bun";
import { mkdir, mkdtemp, rename, rm } from "node:fs/promises";
import { dirname, join } from "node:path";
import lock from "../dawn.lock.json";
import { bridgeWrappersPatch, build, dawnInputs, root } from "./dawn-inputs";

const downloads = join(build, "downloads");
const inputs = [...lock.archives, ...lock.git];
const generated = [
  "src/dawn/node/interop/WebGPU.h",
  "src/dawn/node/interop/WebGPU.cpp",
  "src/dawn/utils/ComboLimits.h",
  "src/dawn/utils/ComboLimits.cpp"
];

async function checkTools() {
  if(process.platform !== "linux" || process.arch !== "x64") {
    throw new Error("Setup currently supports Linux x86-64.");
  }
  const tools = ["cmake", "ninja", "go", "python3", "node", "git", "curl", "tar", "clang++"];
  const missing = tools.filter(tool => !Bun.which(tool));
  if(missing.length) throw new Error(`Missing tools: ${missing.join(", ")}. See README.md for Fedora installation commands.`);
  const version = (await $`go version`.text()).match(/\bgo(\d+)\.(\d+)/);
  if(!version || Number(version[1]) < 1 || (Number(version[1]) === 1 && Number(version[2]) < 26)) {
    throw new Error("Dawn's binding generator requires Go 1.26 or newer.");
  }
}

async function checksum(path: string) {
  const hash = new Bun.CryptoHasher("sha256");
  for await(const chunk of Bun.file(path).stream()) hash.update(chunk);
  return hash.digest("hex");
}

async function download(archive: (typeof lock.archives)[number]) {
  const path = join(downloads, archive.name);
  if(await Bun.file(path).exists() && await checksum(path) === archive.sha256) return path;

  const temporary = await mkdtemp(join(downloads, ".download-"));
  const partial = join(temporary, archive.name);
  try {
    console.log(`Downloading ${archive.name}`);
    await $`curl --fail --location --silent --show-error --retry 2 --connect-timeout 20 --max-time 180 --output ${partial} ${archive.url}`;
    if(await checksum(partial) !== archive.sha256) {
      throw new Error(`SHA-256 mismatch for ${archive.name}.`);
    }
    await rename(partial, path);
    return path;
  } finally {
    await rm(temporary, { recursive: true, force: true });
  }
}

async function checkout(directory: string, dependency: (typeof lock.git)[number]) {
  await $`git init --quiet ${directory}`;
  await $`git fetch --quiet --depth=1 ${dependency.url} ${dependency.revision}`.cwd(directory);
  await $`git checkout --quiet --detach FETCH_HEAD`.cwd(directory);
  const revision = (await $`git rev-parse HEAD`.cwd(directory).text()).trim();
  if(revision !== dependency.revision) throw new Error(`Unexpected revision for ${dependency.name}: ${revision}`);
}

async function generate(staging: string) {
  const source = join(staging, "source");
  const output = join(staging, "generated");
  const interop = "src/dawn/node/interop";
  const idls = [join(interop, "Browser.idl"), "third_party/gpuweb/webgpu.idl", join(interop, "DawnExtensions.idl")];
  const env = {
    ...process.env,
    GOTOOLCHAIN: "local",
    GOFLAGS: "-mod=readonly",
    GOCACHE: join(build, "go-build"),
    GOMODCACHE: join(build, "go-mod")
  };

  console.log("Generating Dawn Node binding sources");
  for(const extension of ["h", "cpp"]) {
    const template = join(interop, `WebGPU.${extension}.tmpl`);
    const path = `../generated/${interop}/WebGPU.${extension}`;
    await mkdir(dirname(join(source, path)), { recursive: true });
    await $`go run ./tools/src/cmd/idlgen/main.go --template ${template} --output ${path} ${idls}`.cwd(source).env(env);
  }

  const args = [
    "generator/dawn_json_generator.py",
    "--dawn-json", "src/dawn/dawn.json",
    "--targets", "dawn_utils",
    "--template-dir", "generator/templates",
    "--root-dir", source,
    "--output-dir", output,
    "--jinja2-path", join(source, "third_party/jinja2"),
    "--markupsafe-path", join(source, "third_party/markupsafe")
  ];
  await $`python3 ${args}`.cwd(source).env({ ...process.env, PYTHONDONTWRITEBYTECODE: "1" });
}

async function ready(directory: string) {
  const files = [
    ...inputs.map(input => join(input.path, input.sentinel)),
    ...generated.map(path => join("generated", path))
  ];
  return (await Promise.all(files.map(path => Bun.file(join(directory, path)).exists()))).every(Boolean);
}

async function prepare() {
  const destination = await dawnInputs();
  await mkdir(downloads, { recursive: true });
  if(await ready(destination)) {
    console.log(`Dawn inputs already prepared at ${destination}`);
    return;
  }

  const staging = await mkdtemp(join(build, ".setup-"));
  try {
    for(const archive of lock.archives) {
      const path = await download(archive);
      const directory = join(staging, archive.path);
      await mkdir(directory, { recursive: true });
      await $`tar -xzf ${path} --strip-components=${archive.strip} -C ${directory}`;
    }
    for(const dependency of lock.git) {
      await checkout(join(staging, dependency.path), dependency);
    }
    await $`git apply --no-index ${bridgeWrappersPatch}`.cwd(join(staging, "source"));
    await generate(staging);
    if(!await ready(staging)) throw new Error("Setup did not produce every required Dawn input.");
    try {
      await rename(staging, destination);
    } catch (error) {
      if(!await ready(destination)) {
        throw new Error(`Could not publish Dawn inputs to ${destination}. Move aside an incomplete directory and retry.`, { cause: error });
      }
    }
    console.log(`Dawn inputs ready at ${destination}`);
  } finally {
    await rm(staging, { recursive: true, force: true });
  }
}

await checkTools();
await $`bun install --frozen-lockfile`.cwd(root);
await prepare();
