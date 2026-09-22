'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const root = path.resolve(__dirname, '../..');
global.window = global;
global.OB64 = {};
for (const file of ['data.js', 'art.js', 'cutscene-model.js', 'cutscene-codec.js',
  'cutscene-runtime.js', 'cutscene-preview.js', 'cutscene-renderer.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(root, 'editor', file), 'utf8'), { filename: file });
}
const raw = fs.readFileSync(path.join(root, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const rom = new Uint8Array(raw.length);
for (let i = 0; i < raw.length; i += 2) { rom[i] = raw[i + 1]; rom[i + 1] = raw[i]; }
const api = OB64.cutsceneRuntime.pathRibbon;
const words = [0x49, 0, 3, 0, 90, -1, 3, 0, 0, 1];
const ribbon = api.create(rom, words);
assert.deepStrictEqual(ribbon.points.map(p => [p.x, p.y]), [[40, 47], [-18, -49]]);
assert.equal(ribbon.active, 1);
let updates = 0;
while (ribbon.active && ++updates < 100) assert(api.advance(ribbon));
assert(updates > 1 && updates < 100, 'ROM path must finish without an external query answer');
const completed = ribbon.revealed;
api.setEndpoint(ribbon, 10, -1);
api.advance(ribbon);
assert.equal(ribbon.revealed, completed, 'lowering the endpoint does not rewind');
api.setEndpoint(ribbon, 100, -1);
api.advance(ribbon);
assert(ribbon.revealed > completed);
assert.equal(ribbon.active, 0, 'retargeting does not reactivate the native activity flag');
api.setEndpoint(ribbon, 0, 1);
api.setEndpoint(ribbon, 1, -1);
for (let i = 0; i < 100; i++) api.advance(ribbon);
assert.equal(ribbon.revealed, ribbon.length, 'stored milestone still overrides percentage writes');
assert.throws(() => api.setEndpoint(ribbon, 0, 2), e => e.code === 'path-ribbon-endpoint');
assert.throws(() => api.create(rom, [0x49, 20, 3, 0, 90, -1, 3, 0, 0, 1]), e => e.code === 'path-ribbon-selector');
assert.throws(() => api.create(rom, [0x49, 0, 3, 0, 101, -1, 3, 0, 0, 1]), e => e.code === 'path-ribbon-endpoint');
ribbon.fadeAge = 0;
for (let i = 0; i < 29; i++) assert(api.advance(ribbon));
assert.equal(api.advance(ribbon), false);

const document = OB64.cutsceneModel.createSceneDocument({ identity: {
  sceneId: 'scene:ribbon-test', technicalName: 'Ribbon test', engine: 'director',
  sourceRevision: 'us-rev0', directorKey: '00000001', aliases: [], triggerStatus: 'fixture'
} });
const preview = OB64.cutscenePreview.evaluateAtFrame(document, 0);
const baseline = OB64.cutsceneRenderer.renderFrame(document, preview);
const visible = api.create(rom, words);
visible.revealed = visible.length;
preview.pathRibbons = [visible];
const image = OB64.cutsceneRenderer.renderFrame(document, preview);
assert.notDeepStrictEqual(image.rgba, baseline.rgba, 'the ROM path must be visible');
const at = (y, x) => (y * 320 + x) * 4;
assert.notDeepStrictEqual(image.rgba.slice(at(167, 200), at(167, 200) + 4),
  baseline.rgba.slice(at(167, 200), at(167, 200) + 4), 'source point uses centered map coordinates');
assert.deepStrictEqual(image.rgba.slice(0, 4), baseline.rgba.slice(0, 4));
visible.fadeAge = 30;
assert.deepStrictEqual(OB64.cutsceneRenderer.renderFrame(document, preview).rgba, baseline.rgba);
console.log('PASS ROM path decoding, reveal/retarget/fade lifecycle, bounds, and visible map placement.');
