const assert = require("node:assert/strict");

const [, , addon, fixture] = process.argv;
const { negotiate } = require(fixture);

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
