'use strict';
// Run the preserved browser asset reader and transforms without a browser runtime.
const fs = require('node:fs'), vm = require('node:vm'), path = require('node:path');
const context = { Uint8ClampedArray, Uint8Array, ArrayBuffer, console,
  btoa: x => Buffer.from(x, 'binary').toString('base64'),
  atob: x => Buffer.from(x, 'base64').toString('binary') };
context.window = context; context.OB64 = {};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.resolve(__dirname, '../../sprite-library.js'), 'utf8'), context);
const api = context.OB64.spriteLibrary;
const inputs = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
const output = inputs.map(file => {
  const asset = api.prepareAssetFile(file);
  const state = api.createState(); state.assets = [asset]; api.indexState(state);
  const composite = Array.from(asset.frames, (_, i) => Buffer.from(api.compositeFrame(asset, i)).toString('base64'));
  api.rotateAsset(state, asset.id, true);
  const rotated = api.assetFilePayload(api.assetFor(state, asset.id));
  state.assets = [api.prepareAssetFile(file)]; api.indexState(state);
  api.resizeAsset(state, asset.id, 13, 9);
  const scaled = api.assetFilePayload(api.assetFor(state, asset.id));
  return {composite, rotated, scaled};
});
fs.writeFileSync(process.argv[3], JSON.stringify(output));
console.log(`Browser reader, ${output.length} layered composites/rotations/scales completed`);
