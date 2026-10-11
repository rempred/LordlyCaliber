'use strict';

// Dialogue portrait identity 0 (class None) has an empty class route. The native
// loader falls back to a zero-filled texture and palette, so the portrait draws
// fully transparent instead of failing. This keeps retail face-less windows
// previewable and lets authored boxes choose "no face" with identity 0.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'art.js', 'cutscene-dialogue-draw.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}
const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
const draw = new OB64.cutsceneDialogueDraw(z64);

const view = new DataView(z64.buffer);
assert.strictEqual(view.getUint16(0x64be0), 0, 'class 0 has an empty portrait route');
const empty = draw.portrait(0, 0);
assert.strictEqual(empty.empty, true);
assert.deepStrictEqual([empty.width, empty.height, empty.indices.length, empty.palette.length], [40, 48, 1920, 80]);
assert(empty.indices.every(index => index === 0));
assert(empty.palette.every(color => color[3] === 0), 'every palette entry is transparent');

const face = draw.portrait(83, 0);
assert.notStrictEqual(face.empty, true);
assert(face.indices.some(index => index !== 0), 'a real class route yields portrait texels');
assert(face.palette.some(color => color[3] === 255), 'a real portrait has opaque colors');
console.log('portrait empty route: ok');
