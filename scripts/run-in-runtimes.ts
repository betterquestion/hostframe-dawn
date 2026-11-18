export async function runInRuntimes(label: string, args: string[]) {
  for(const [name, executable] of [["Node", "node"], ["Bun", process.execPath]]) {
    console.log(`Testing ${label} in ${name}`);
    const child = Bun.spawn([executable, ...args], {
      stdout: "inherit",
      stderr: "inherit"
    });
    const timeout = setTimeout(() => child.kill(9), 35000);
    try {
      const status = await child.exited;
      if(status !== 0) throw new Error(`${name} ${label} test failed (exit ${status}).`);
    } finally {
      clearTimeout(timeout);
    }
  }
}
