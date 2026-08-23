'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['cutscene-model.js', 'cutscene-assets.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, filename), 'utf8'), { filename });
}

const A = OB64.cutsceneAssets;

function writeU16(bytes, offset, value) {
  bytes[offset] = (value >>> 8) & 0xFF;
  bytes[offset + 1] = value & 0xFF;
}

function writeU32(bytes, offset, value) {
  bytes[offset] = (value >>> 24) & 0xFF;
  bytes[offset + 1] = (value >>> 16) & 0xFF;
  bytes[offset + 2] = (value >>> 8) & 0xFF;
  bytes[offset + 3] = value & 0xFF;
}

function image64(type, subtype, width, height, bodyLength) {
  const bytes = new Uint8Array(8 + bodyLength);
  bytes[0] = 0x36;
  bytes[1] = 0x34;
  bytes[2] = type;
  bytes[3] = subtype;
  writeU16(bytes, 4, width);
  writeU16(bytes, 6, height);
  return bytes;
}

const rgba16 = image64(0, 2, 2, 1, 8);
writeU16(rgba16, 8, 0xF801);
writeU16(rgba16, 10, 0x003F);
let parsed = A.parseN64Image(rgba16);
assert.strictEqual(parsed.variant, 'rgba5551');
assert.strictEqual(parsed.capability, 'native');
assert.strictEqual(parsed.renderable, true);
assert.deepStrictEqual(Array.from(parsed.rgba), [
  255, 0, 0, 255,
  0, 0, 255, 255,
]);

const rgba32 = image64(0, 3, 1, 1, 16);
rgba32.set([11, 22, 33, 44], 8);
parsed = A.parseN64Image(rgba32);
assert.strictEqual(parsed.variant, 'rgba32');
assert.deepStrictEqual(Array.from(parsed.rgba), [11, 22, 33, 44]);

const ci8 = image64(2, 1, 2, 1, 8 + 512);
ci8[8] = 0;
ci8[9] = 1;
writeU16(ci8, 16, 0xFFFF);
writeU16(ci8, 18, 0x0001);
parsed = A.parseN64Image(ci8);
assert.strictEqual(parsed.variant, 'ci8');
assert.deepStrictEqual(Array.from(parsed.indices), [0, 1]);
assert.deepStrictEqual(Array.from(parsed.rgba), [
  255, 255, 255, 255,
  0, 0, 0, 255,
]);

const ci4 = image64(2, 0, 2, 1, 1 + 32);
ci4[8] = 0x01;
writeU16(ci4, 9, 0xF801);
writeU16(ci4, 11, 0x07C1);
parsed = A.parseN64Image(ci4);
assert.strictEqual(parsed.renderable, false);
assert.strictEqual(parsed.capability, 'needs-research');
assert.deepStrictEqual(Array.from(parsed.indices), [0, 1]);
assert(parsed.warnings[0].includes('palette location'));
parsed = A.parseN64Image(ci4, { paletteOffset: 9 });
assert.strictEqual(parsed.renderable, true);
assert.deepStrictEqual(Array.from(parsed.rgba), [
  255, 0, 0, 255,
  0, 255, 0, 255,
]);

const runtimePalette = image64(4, 1, 2, 1, 8);
runtimePalette.set([1, 2], 8);
parsed = A.parseN64Image(runtimePalette);
assert.strictEqual(parsed.variant, 'i8');
assert.strictEqual(parsed.renderable, true);
assert.deepStrictEqual(Array.from(parsed.indices), [1, 2]);
assert.deepStrictEqual(Array.from(parsed.rgba), [
  1, 1, 1, 255,
  2, 2, 2, 255,
]);

const unknown = image64(9, 9, 1, 1, 0);
parsed = A.parseN64Image(unknown);
assert.strictEqual(parsed.renderable, false);
assert.strictEqual(parsed.capability, 'needs-research');
assert(parsed.warnings[0].includes('not decoded'));

assert.throws(() => A.parseN64Image(new Uint8Array(8)), /does not use the 64 container/);
assert.throws(() => A.parseN64Image(image64(0, 2, 0, 1, 0)), /dimensions/);
assert.throws(() => A.parseN64Image(image64(0, 2, 2, 2, 1)), /outside the image container/);

function bg2(format, width, height, pixelBytes) {
  const bytes = new Uint8Array(24 + pixelBytes);
  bytes[0] = 0x42;
  bytes[1] = 0x35;
  bytes[2] = format;
  bytes[3] = 1;
  writeU16(bytes, 4, 0);
  writeU16(bytes, 6, 0);
  writeU16(bytes, 8, 0);
  writeU16(bytes, 10, 0);
  writeU16(bytes, 12, width);
  writeU16(bytes, 14, height);
  writeU32(bytes, 16, pixelBytes);
  writeU32(bytes, 20, 0);
  return bytes;
}

const directBg2 = bg2(1, 2, 1, 8);
writeU16(directBg2, 24, 0xF801);
writeU16(directBg2, 26, 0x003F);
parsed = A.parseBg2(directBg2);
assert.strictEqual(parsed.variant, undefined);
assert.strictEqual(parsed.records[0].dataOffset, 24);
assert.strictEqual(parsed.recordCount, 1);
assert.strictEqual(parsed.capability, 'native');
assert.deepStrictEqual(Array.from(parsed.rgba), [
  255, 0, 0, 255,
  0, 0, 255, 255,
]);

const maskedBg2 = bg2(0, 2, 1, 16);
writeU16(maskedBg2, 24, 0xF801);
writeU16(maskedBg2, 26, 0x07C1);
maskedBg2[32] = 64;
maskedBg2[33] = 255;
parsed = A.parseBg2(maskedBg2);
assert.strictEqual(parsed.renderable, true);
assert.strictEqual(parsed.capability, 'preview-only');
assert.strictEqual(parsed.rgba[3], 64);
assert.strictEqual(parsed.rgba[7], 255);
assert(parsed.warnings[0].includes('alpha'));

assert.throws(() => A.parseBg2(bg2(2, 1, 1, 0)), /unsupported format/);
assert.throws(() => A.parseBg2(new Uint8Array(24)), /does not use the B5/);

const composite = A.compositeLayers([
  {
    width: 2, height: 1, x: 0, y: 0, depth: 0, visible: true,
    rgba: new Uint8ClampedArray([255, 0, 0, 255, 255, 0, 0, 255]),
  },
  {
    width: 1, height: 1, x: 1, y: 0, depth: 1, visible: true,
    rgba: new Uint8ClampedArray([0, 0, 255, 255]),
  },
], 2, 1);
assert.deepStrictEqual(Array.from(composite), [
  255, 0, 0, 255,
  0, 0, 255, 255,
]);

const soukoPath = path.join(ROOT, 'ob64_all', 'e100', 'souko.bg2');
if (fs.existsSync(soukoPath)) {
  const souko = A.parseBg2(new Uint8Array(fs.readFileSync(soukoPath)));
  assert.strictEqual(souko.width, 415);
  assert.strictEqual(souko.height, 303);
  assert.strictEqual(souko.format, 1);
  assert.strictEqual(souko.recordCount, 4);
  assert.strictEqual(souko.originX, -202);
  assert.strictEqual(souko.originY, -152);
  assert.strictEqual(souko.rgba.length, 415 * 303 * 4);
  assert.strictEqual(
    crypto.createHash('sha256').update(souko.rgba).digest('hex').toUpperCase(),
    'A89AD777A02E875C65551A81B20274AC3253F2D17337E0BAA044F6A38B6B650F',
  );
}

console.log('PASS Cutscene Studio 64 images, RGBA bg2 bases, partial-format warnings, and layer composition');
