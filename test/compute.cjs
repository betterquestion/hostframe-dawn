const assert = require("node:assert/strict");

async function requestDevice(create) {
  const gpu = create(["backend=vulkan"]);
  const adapter = await gpu.requestAdapter();
  assert.ok(adapter, "Dawn did not find a Vulkan adapter");
  return adapter.requestDevice();
}

async function smokeTest() {
  const { create, globals } = require(process.argv[2]);
  const { GPUBufferUsage, GPUMapMode } = globals;
  const device = await requestDevice(create);
  await new Promise(resolve => setImmediate(resolve));
  // Device operations must keep working when the originating GPU is collected.
  globalThis.gc();

  try {
    const result = device.createBuffer({
      size: 4,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
    });
    const readback = device.createBuffer({
      size: 4,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ
    });
    const module = device.createShaderModule({
      code: `
        @group(0) @binding(0) var<storage, read_write> result: u32;
        @compute @workgroup_size(1)
        fn main() {
          result = 42u;
        }
      `
    });
    const pipeline = device.createComputePipeline({
      layout: "auto",
      compute: { module, entryPoint: "main" }
    });
    const bindGroup = device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [{ binding: 0, resource: { buffer: result } }]
    });

    const encoder = device.createCommandEncoder();
    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatchWorkgroups(1);
    pass.end();
    encoder.copyBufferToBuffer(result, 0, readback, 0, 4);
    device.queue.submit([encoder.finish()]);

    await readback.mapAsync(GPUMapMode.READ);
    assert.equal(new Uint32Array(readback.getMappedRange())[0], 42);
    readback.unmap();
  } finally {
    device.destroy();
  }
  console.log("Dawn compute readback: 42");
}

const timeout = setTimeout(() => {
  console.error("Timed out waiting for Dawn compute readback");
  process.exit(1);
}, 30000);

smokeTest().catch(error => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => clearTimeout(timeout));
