const assert = require("node:assert/strict");
const { setTimeout: delay } = require("node:timers/promises");

const [, , addon, fixture] = process.argv;
const { create, globals } = require(addon);
const { X11Surface } = require(fixture);
assert.ok(X11Surface, "The fixture requires X11 development headers at build time");

function clearFrame(device, texture) {
  const encoder = device.createCommandEncoder();
  const pass = encoder.beginRenderPass({
    colorAttachments: [{
      view: texture.createView(),
      clearValue: { r: 1, g: 0, b: 1, a: 1 },
      loadOp: "clear",
      storeOp: "store"
    }]
  });
  pass.end();
  device.queue.submit([encoder.finish()]);
}

async function checkPixels(surface) {
  const expected = [255, 0, 255];
  const deadline = Date.now() + 5000;
  let pixels;
  do {
    pixels = surface.readPixels();
    assert.equal(pixels.length, 64 * 64 * expected.length);
    if(pixels.every((value, index) => value === expected[index % expected.length])) return;
    await delay(10);
  } while(Date.now() < deadline);

  assert.fail(`Expected a magenta X11 window, got first pixel [${pixels.slice(0, 3)}]`);
}

async function checkSurface() {
  const gpu = create(["backend=vulkan"]);
  const surface = new X11Surface(addon, gpu);
  let device;
  try {
    const exception = new Error("adapter options getter");
    assert.throws(() => surface.requestAdapter(gpu, {
      get powerPreference() { throw exception; }
    }), error => error === exception);
    assert.equal(await surface.requestAdapter(gpu, { featureLevel: "invalid" }), null);

    const adapter = await surface.requestAdapter(gpu, { powerPreference: "low-power" });
    assert.ok(adapter instanceof globals.GPUAdapter);
    device = await adapter.requestDevice();
    device.pushErrorScope("validation");
    const frame = surface.acquire(device);
    assert.ok(frame instanceof globals.GPUTexture);
    assert.equal(frame.width, 64);
    assert.equal(frame.height, 64);
    clearFrame(device, frame);
    surface.present();
    assert.equal(await device.popErrorScope(), null);
    await checkPixels(surface);
  } finally {
    try {
      surface.close();
    } finally {
      device?.destroy();
    }
  }
  console.log("Presented and verified a 64×64 magenta frame, then closed the X11 display");
}

checkSurface().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
