const assert = require("node:assert/strict");

const [, , addon, fixture] = process.argv;
const { negotiate, compareInstances, compareDevices, checkDevice, foreignWrapper } = require(fixture);

const usable = 0;
const wrongInstance = 1;
const lostOrDestroyed = 2;
const queryFailed = 3;

assert.throws(() => negotiate(addon, 1), /must be loaded/);
assert.throws(() => negotiate(fixture, 1), /does not export/);
assert.throws(() => negotiate(addon, -1), RangeError);

const dawn = require(addon);
assert.equal(typeof dawn.create, "function");
const api = negotiate(addon, 1);
assert.equal(api.version, 1);
assert.ok(api.size >= 8);
assert.equal(negotiate(addon, 0), null);
assert.equal(negotiate(addon, 0xffffffff), null);
assert.deepEqual(negotiate(addon, 1), api);

console.log(`Negotiated Dawn bridge ABI v${api.version} (${api.size} bytes)`);

function checkWrongValues(compare, valid, wrongType) {
  const prototype = Object.getPrototypeOf(valid);
  const foreign = Object.setPrototypeOf(foreignWrapper(), prototype);
  const proxy = new Proxy(valid, {
    getPrototypeOf() {
      throw new Error("Bridge accessors must avoid prototype lookup");
    }
  });
  for(const value of [undefined, null, 42, "GPU", {}, wrongType, Object.create(prototype), foreign, proxy]) {
    assert.equal(compare(addon, valid, value), null);
    assert.equal(compare(addon, value, valid), null);
  }
}

async function checkHandles() {
  const gpu = dawn.create(["backend=vulkan"]);
  const otherGpu = dawn.create(["backend=vulkan"]);
  assert.equal(compareInstances(addon, gpu, gpu), true);
  assert.equal(compareInstances(addon, gpu, otherGpu), false);

  const adapter = await gpu.requestAdapter();
  assert.ok(adapter, "Dawn did not find a Vulkan adapter");
  const otherAdapter = await otherGpu.requestAdapter();
  assert.ok(otherAdapter, "Dawn did not find a second Vulkan adapter");
  const device = await adapter.requestDevice();
  try {
    const otherDevice = await otherAdapter.requestDevice();
    try {
      assert.equal(compareDevices(addon, device, device), true);
      assert.equal(compareDevices(addon, device, otherDevice), false);
      checkWrongValues(compareInstances, gpu, device);
      checkWrongValues(compareDevices, device, gpu);

      const buffer = device.createBuffer({ size: 4, usage: dawn.globals.GPUBufferUsage.COPY_DST });
      assert.equal(buffer.size, 4);
      buffer.destroy();

      assert.equal(checkDevice(addon, gpu, device), usable);
      assert.equal(checkDevice(addon, otherGpu, otherDevice), usable);
      assert.equal(checkDevice(addon, gpu, otherDevice), wrongInstance);
      assert.equal(checkDevice(addon, null, device), queryFailed);
      assert.equal(checkDevice(addon, gpu, null), queryFailed);

      // The fixture retains both native handles before calling back into JavaScript.
      assert.equal(checkDevice(addon, gpu, device, () => device.destroy()), lostOrDestroyed);
      assert.equal(checkDevice(addon, otherGpu, device), wrongInstance);
    } finally {
      otherDevice.destroy();
    }
  } finally {
    device.destroy();
  }
  console.log("Dawn instance and device handles, ownership, and loss checks validated");
}

const timeout = setTimeout(() => {
  console.error("Timed out waiting for Dawn handle validation");
  process.exit(1);
}, 30000);

checkHandles().catch(error => {
  console.error(error);
  process.exitCode = 1;
}).finally(() => clearTimeout(timeout));
