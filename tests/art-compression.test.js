'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
global.module = undefined;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['data.js', 'art.js']) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

(async function run() {
  const samples = [
    new Uint8Array(0),
    new Uint8Array([1]),
    new Uint8Array(257),
    Uint8Array.from({ length: 4096 }, (_, index) =>
      (index * 37 + (index >>> 3)) & 0xFF),
    Uint8Array.from({ length: 8192 }, (_, index) =>
      index % 97 < 60 ? 0 : index & 0xFF),
  ];

  let timerFired = false;
  setTimeout(() => { timerFired = true; }, 0);
  let progressUpdates = 0;
  for (const sample of samples) {
    const synchronous = OB64.art.bootLzCompress(sample);
    const cooperative = await OB64.art.bootLzCompressAsync(sample, () => {
      progressUpdates++;
    });
    assert.deepStrictEqual(Array.from(cooperative), Array.from(synchronous),
      'cooperative compression must preserve the exact existing byte stream');
  }
  assert(timerFired,
    'cooperative compression must yield to queued browser work');
  assert(progressUpdates > 0,
    'cooperative compression must expose progress updates');
  console.log('PASS cooperative boot-LZ is byte-identical and yields between work slices');
})().catch(error => {
  console.error(error && error.stack ? error.stack : error);
  process.exitCode = 1;
});
