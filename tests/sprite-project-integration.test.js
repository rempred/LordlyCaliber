'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROM = path.join(ROOT,
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.module = undefined;
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');

for (const filename of [
  'data.js', 'parsers.js', 'repack.js', 'art.js', 'army-sprites.js',
  'sprite-library.js',
  'description-codec.js', 'patch.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

function freshRom(bytes) {
  const array = new Uint8Array(bytes);
  return OB64.loadROM(array.buffer.slice(
    array.byteOffset, array.byteOffset + array.byteLength
  ));
}

function dirtyFlags() {
  return {
    shops: false, items: false, itemDescriptions: false,
    classDefs: false, classDescriptions: false, actionDescriptions: false,
    art: false, cutscenes: false, encounters: false, creatureDrops: false,
    neutralRuntime: false, consumables: false, consumableDescriptions: false,
    consumableEffects: false, combatAnimationOverrides: false,
    statGates: false, tools: false, squadOverrides: false, scenario: false,
  };
}

(async function run() {
  const bytes = fs.readFileSync(ROM);
  const source = freshRom(bytes);
  await OB64.art.initialize(source);
  OB64.spriteLibrary.initialize(source);
  OB64.patch.snapshotOriginal(source);

  const rgba = new Uint8ClampedArray(4 * 4 * 4);
  rgba.set([248, 80, 40, 255], 0);
  const asset = OB64.spriteLibrary.assetFromRgba(source.spriteLibrary, {
    name: 'Project Sprite', kind: 'sprite', width: 4, height: 4,
    rgba, provenance: { source: 'test', label: 'Project integration fixture' },
  });
  OB64.spriteLibrary.addAsset(source.spriteLibrary, asset);

  const project = OB64.patch.collectPatch(source);
  assert.strictEqual(project.version, 34);
  assert.strictEqual(project.summary.sprite_library_assets, 1);
  assert.strictEqual(project.patches.spriteLibrary.schemaVersion, 1);
  assert.strictEqual(project.patches.spriteLibrary.assets.length, 1);

  const target = freshRom(bytes);
  await OB64.art.initialize(target);
  OB64.spriteLibrary.initialize(target);
  OB64.patch.snapshotOriginal(target);
  const dirty = dirtyFlags();
  const result = OB64.patch.applyPatch(target, project, dirty);
  assert.strictEqual(result.applied.spriteLibrary, 1);
  assert.strictEqual(target.spriteLibrary.assets.length, 1);
  assert.strictEqual(target.spriteLibrary.assets[0].name, 'Project Sprite');
  assert.deepStrictEqual(
    [...target.spriteLibrary.assets[0].frames[0].layers[0].pixels], [...rgba]
  );
  assert.strictEqual(dirty.art, false,
    'Project-only Sprite Library records must not schedule a ROM art write');

  const armySource = freshRom(bytes);
  await OB64.art.initialize(armySource);
  OB64.spriteLibrary.initialize(armySource);
  OB64.patch.snapshotOriginal(armySource);
  const atlas = armySource.art.armySprites.byKey['enemy-front-ordinary'];
  const model = atlas.models[0];
  const replacement = new Uint8Array(atlas.width * atlas.height)
    .fill(atlas.transparentIndices[0]);
  assert.strictEqual(OB64.armySprites.setEdit(
    armySource.art.armySprites, atlas.key, model.modelId, replacement), true);
  const armyProject = OB64.patch.collectPatch(armySource);
  assert.strictEqual(armyProject.version, 34);
  assert.strictEqual(armyProject.summary.army_sprite_art_modified, 1);
  assert.strictEqual(armyProject.patches.art.schemaVersion, 5);
  assert.deepStrictEqual(Object.keys(armyProject.patches.art.armySprites),
    ['enemy-front-ordinary:00']);

  const armyTarget = freshRom(bytes);
  await OB64.art.initialize(armyTarget);
  OB64.spriteLibrary.initialize(armyTarget);
  OB64.patch.snapshotOriginal(armyTarget);
  const armyDirty = dirtyFlags();
  const armyResult = OB64.patch.applyPatch(
    armyTarget, armyProject, armyDirty);
  assert.strictEqual(armyResult.applied.art, 1);
  assert.strictEqual(armyDirty.art, true);
  assert.deepStrictEqual(
    OB64.armySprites.currentIndices(
      armyTarget.art.armySprites, atlas.key, model.modelId),
    replacement
  );

  const customSource = freshRom(bytes);
  await OB64.art.initialize(customSource);
  OB64.spriteLibrary.initialize(customSource);
  OB64.patch.snapshotOriginal(customSource);
  const specialAtlas = customSource.art.armySprites
    .byKey['player-back-special'];
  assert.strictEqual(OB64.armySprites.createBlankPlane(
    customSource.art.armySprites, specialAtlas.key, 0x20), true);
  const customProject = OB64.patch.collectPatch(customSource);
  assert.strictEqual(customProject.version, 34);
  assert.strictEqual(customProject.patches.art.armySprites
    ['player-back-special:20'].retailPlane, false);

  const customTarget = freshRom(bytes);
  await OB64.art.initialize(customTarget);
  OB64.spriteLibrary.initialize(customTarget);
  OB64.patch.snapshotOriginal(customTarget);
  const legacyCustomProject = JSON.parse(JSON.stringify(customProject));
  legacyCustomProject.version = 32;
  assert.throws(() => OB64.patch.applyPatch(
    customTarget, legacyCustomProject, dirtyFlags()), /version 33/);
  const customDirty = dirtyFlags();
  const customResult = OB64.patch.applyPatch(
    customTarget, customProject, customDirty);
  assert.strictEqual(customResult.applied.art, 1);
  assert.strictEqual(OB64.armySprites.hasCurrentPlane(
    customTarget.art.armySprites, specialAtlas.key, 0x20), true);

  console.log('PASS Sprite Library and Army sprite Project v34 collection and application');
})().catch(error => {
  console.error(error.stack || error);
  process.exitCode = 1;
});
