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
if (!window.crypto || !window.crypto.subtle) {
  Object.defineProperty(window, 'crypto', { value: crypto.webcrypto });
}
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
global.OB64 = { className: id => `Class ${id}` };

for (const filename of ['art.js', 'army-sprites.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(EDITOR, filename), 'utf8'), {
    filename,
  });
}

function normalizeV64(bytes) {
  const output = new Uint8Array(bytes);
  for (let offset = 0; offset < output.length; offset += 2) {
    const first = output[offset];
    output[offset] = output[offset + 1];
    output[offset + 1] = first;
  }
  return output;
}

(async function run() {
  const z64 = normalizeV64(fs.readFileSync(ROM));
  const rom = { z64, layout: { id: 'us-rev0' } };
  await OB64.art.initialize(rom);

  const army = rom.art.armySprites;
  assert.strictEqual(rom.art.supported, true);
  assert.strictEqual(army.supported, true, army.unavailableReason);
  assert.strictEqual(army.atlases.length, 6);
  assert.strictEqual(army.models.length, 240);
  assert.strictEqual(army.retailModels.length, 225);
  assert.strictEqual(army.sourceModels.length, 225);
  assert.deepStrictEqual(army.atlases.map(atlas => atlas.modelCount),
    [56, 56, 38, 38, 26, 26]);
  assert.deepStrictEqual(army.atlases.map(atlas => atlas.retailModelCount),
    [56, 56, 38, 24, 26, 25]);
  assert.strictEqual(army.classRoutes.length, 0xA5);
  const missingPlayerRoutes = army.classRoutes.filter(
    route => route.playerMissingInRetail);
  assert.strictEqual(missingPlayerRoutes.length, 24);
  assert.strictEqual(new Set(missingPlayerRoutes.map(
    route => route.playerModel.key)).size, 15);
  const gatekeeperRoute = army.byClassId[0x71];
  assert.strictEqual(gatekeeperRoute.modelId, 0x20);
  assert.strictEqual(gatekeeperRoute.lane, 'special');
  assert.strictEqual(gatekeeperRoute.playerMissingInRetail, true);
  assert.strictEqual(gatekeeperRoute.enemyMissingInRetail, false);
  assert.deepStrictEqual(
    army.byKey['player-back-special'].models[0x1D].classIds,
    [0x69, 0x6A, 0x6B]
  );
  assert.deepStrictEqual(
    army.byKey['player-back-large'].transparentIndices,
    [0, 177, 178, 179, 243, 244, 245, 246, 247, 248, 249, 250,
      251, 252, 253, 254, 255]
  );

  const atlas = army.byKey['player-back-ordinary'];
  const fighterModel = atlas.models[3];
  const fighterRgba = new Uint8Array(fighterModel.originalIndices.length * 4);
  fighterModel.originalIndices.forEach((paletteIndex, pixel) => {
    const word = atlas.palettes[1][paletteIndex];
    const expand = value => (value << 3) | (value >>> 2);
    fighterRgba.set([
      expand((word >>> 11) & 31),
      expand((word >>> 6) & 31),
      expand((word >>> 1) & 31),
      word & 1 ? 255 : 0,
    ], pixel * 4);
  });
  assert.strictEqual(
    crypto.createHash('sha256').update(fighterRgba).digest('hex').toUpperCase(),
    '920A2438B41E02E83644B03DE0FDD71CD4CAF459CF61C27D1110B86CDF9D3215'
  );

  const replacement = new Uint8Array(atlas.width * atlas.height)
    .fill(atlas.transparentIndices[0]);
  assert.strictEqual(OB64.armySprites.setEdit(
    army, atlas.key, fighterModel.modelId, replacement), true);
  assert.strictEqual(OB64.armySprites.editCount(army), 1);
  assert.strictEqual(OB64.armySprites.undo(
    army, atlas.key, fighterModel.modelId), true);
  assert.deepStrictEqual(
    OB64.armySprites.currentIndices(army, atlas.key, fighterModel.modelId),
    fighterModel.originalIndices
  );
  assert.strictEqual(OB64.armySprites.redo(
    army, atlas.key, fighterModel.modelId), true);

  const project = OB64.art.collectProjectPayload(rom);
  assert.strictEqual(project.schemaVersion, 5);
  assert.deepStrictEqual(Object.keys(project.armySprites),
    ['player-back-ordinary:03']);
  assert.strictEqual(Buffer.from(
    project.armySprites['player-back-ordinary:03']
      .paletteRgba5551BeBase64, 'base64').length, 0x400);
  assert.strictEqual(
    project.armySprites['player-back-ordinary:03'].retailPlane, true);
  const freshArmy = OB64.armySprites.initialize(z64);
  const prepared = OB64.armySprites.prepareProject(
    freshArmy, project.armySprites);
  assert.strictEqual(prepared.count, 1);
  assert.strictEqual(OB64.armySprites.applyPrepared(freshArmy, prepared), 1);
  assert.deepStrictEqual(
    OB64.armySprites.currentIndices(freshArmy, atlas.key, fighterModel.modelId),
    replacement
  );
  const wrongPaletteProject = JSON.parse(JSON.stringify(project.armySprites));
  wrongPaletteProject['player-back-ordinary:03']
    .paletteRgba5551BeBase64 = btoa('\0'.repeat(0x400));
  assert.throws(() => OB64.armySprites.prepareProject(
    freshArmy, wrongPaletteProject), /fixed palettes/);

  const originalEnvelope = z64.slice(atlas.sizeWord,
    atlas.sizeWord + 4 + atlas.originalCapacity);
  const candidate = { z64: z64.slice() };
  const plan = OB64.art.prepareExport(rom, candidate);
  assert(plan);
  assert.strictEqual(plan.armySpriteAtlases.length, 1);
  assert.strictEqual(plan.armySpriteAtlases[0].placement, 'in-place');
  assert(plan.armySpriteAtlases[0].built.stored.length <=
    plan.armySpriteAtlases[0].originalCapacity);
  assert.deepStrictEqual(
    Array.from(plan.armySpriteAtlases[0].built.decoded.slice(0, 0x400)),
    Array.from(atlas.resource.decoded.slice(0, 0x400))
  );
  assert.deepStrictEqual(
    Array.from(plan.armySpriteAtlases[0].built.decoded.slice(-0x200)),
    Array.from(atlas.resource.decoded.slice(-0x200))
  );

  const conflictCandidate = { z64: z64.slice() };
  conflictCandidate.z64[atlas.sizeWord + 4] ^= 1;
  assert.throws(() => OB64.art.applyExport(
    rom, conflictCandidate, plan), /preimage differs/);

  const result = OB64.art.applyExport(rom, candidate, plan);
  assert.strictEqual(result.editedArmySpriteCount, 1);
  const exported = OB64.art.readCompressedResource(
    candidate.z64, atlas.resourceKey).decoded;
  assert.deepStrictEqual(
    Array.from(exported.slice(fighterModel.decodedOffset,
      fighterModel.decodedOffset + replacement.length)),
    Array.from(replacement)
  );
  assert.notDeepStrictEqual(
    Array.from(candidate.z64.slice(atlas.sizeWord,
      atlas.sizeWord + 4 + atlas.originalCapacity)),
    Array.from(originalEnvelope)
  );
  assert.deepStrictEqual(
    Array.from(z64.slice(atlas.sizeWord,
      atlas.sizeWord + 4 + atlas.originalCapacity)),
    Array.from(originalEnvelope)
  );
  const reopenedArmy = OB64.armySprites.initialize(candidate.z64);
  assert.strictEqual(reopenedArmy.supported, true,
    reopenedArmy.unavailableReason);
  assert.deepStrictEqual(
    OB64.armySprites.currentIndices(reopenedArmy,
      atlas.key, fighterModel.modelId),
    replacement
  );

  const importSource = {
    name: 'army-test.png', format: 'PNG', width: 2, height: 1,
    rgba: new Uint8ClampedArray([
      255, 255, 255, 255,
      0, 0, 0, 0,
    ]),
  };
  const imported = OB64.armySprites.prepareImageImport(
    importSource, atlas, 0, {
      resizeMode: 'nearest', panX: 0.5, panY: 0.5, dither: true,
    });
  assert.strictEqual(imported.indices.length, atlas.width * atlas.height);
  assert(imported.indices.some(index => index === atlas.transparentIndices[0]));
  assert(imported.indices.some(index => index !== atlas.transparentIndices[0]));
  assert.strictEqual(imported.dithered, true);

  const customRom = { z64: z64.slice(), layout: { id: 'us-rev0' } };
  await OB64.art.initialize(customRom);
  const customArmy = customRom.art.armySprites;
  const specialAtlas = customArmy.byKey['player-back-special'];
  const gatekeeperModel = specialAtlas.models[0x20];
  assert.strictEqual(gatekeeperModel.retailPresent, false);
  assert.strictEqual(gatekeeperModel.sourcePresent, false);
  assert.strictEqual(OB64.armySprites.hasCurrentPlane(
    customArmy, specialAtlas.key, 0x20), false);
  assert.strictEqual(OB64.armySprites.createBlankPlane(
    customArmy, specialAtlas.key, 0x20), true);
  const gatekeeperIndices = OB64.armySprites.currentIndices(
    customArmy, specialAtlas.key, 0x20).slice();
  const opaqueIndex = specialAtlas.palettes[0].findIndex(word => word & 1);
  gatekeeperIndices[0] = opaqueIndex;
  assert.strictEqual(OB64.armySprites.setEdit(
    customArmy, specialAtlas.key, 0x20, gatekeeperIndices), true);

  const customProject = OB64.art.collectProjectPayload(customRom);
  assert.strictEqual(customProject.schemaVersion, 5);
  assert.strictEqual(
    customProject.armySprites['player-back-special:20'].retailPlane, false);
  const customProjectTarget = OB64.armySprites.initialize(z64);
  assert.throws(() => OB64.armySprites.prepareProject(
    customProjectTarget, customProject.armySprites, 4), /schemaVersion 5/);
  const customPrepared = OB64.armySprites.prepareProject(
    customProjectTarget, customProject.armySprites, 5);
  assert.strictEqual(OB64.armySprites.applyPrepared(
    customProjectTarget, customPrepared), 1);
  assert.strictEqual(OB64.armySprites.hasCurrentPlane(
    customProjectTarget, specialAtlas.key, 0x20), true);

  const expandedCandidate = { z64: z64.slice() };
  const expandedPlan = OB64.art.prepareExport(customRom, expandedCandidate);
  assert(expandedPlan);
  assert.strictEqual(expandedPlan.armySpriteAtlases.length, 1);
  const expandedRow = expandedPlan.armySpriteAtlases[0];
  assert.strictEqual(expandedRow.placement, 'relocated');
  assert.strictEqual(expandedRow.built.decoded.length, 0x3D00);
  assert(expandedRow.allocation);
  const gapPlane = expandedRow.built.decoded.slice(
    0x400 + 0x18 * 0x180, 0x400 + 0x19 * 0x180);
  assert(gapPlane.every(index =>
    index === specialAtlas.transparentIndices[0]));
  const expandedResult = OB64.art.applyExport(
    customRom, expandedCandidate, expandedPlan);
  assert.strictEqual(expandedResult.editedArmySpriteCount, 1);
  assert.strictEqual(OB64.art.resolveSplitKey(
    expandedCandidate.z64, specialAtlas.ownerSites,
    'player special test owner'), expandedRow.allocation.key);
  const expandedDecoded = OB64.art.readCompressedResource(
    expandedCandidate.z64, expandedRow.allocation.key).decoded;
  assert.deepStrictEqual(Array.from(expandedDecoded.slice(
    gatekeeperModel.decodedOffset,
    gatekeeperModel.decodedOffset + gatekeeperIndices.length)),
  Array.from(gatekeeperIndices));
  const reopenedExpanded = OB64.armySprites.initialize(expandedCandidate.z64);
  assert.strictEqual(reopenedExpanded.supported, true,
    reopenedExpanded.unavailableReason);
  assert.strictEqual(
    reopenedExpanded.byKey['player-back-special'].sourceModelCount, 38);
  assert.strictEqual(reopenedExpanded.byKey['player-back-special']
    .models[0x20].sourcePresent, true);

  const largeArmy = OB64.armySprites.initialize(z64);
  const largeAtlas = largeArmy.byKey['player-back-large'];
  assert.strictEqual(OB64.armySprites.createBlankPlane(
    largeArmy, largeAtlas.key, 0x19), true);
  const largeRows = OB64.armySprites.buildResources(largeArmy);
  assert.strictEqual(largeRows.length, 1);
  assert.strictEqual(largeRows[0].placement, 'relocated');
  assert.strictEqual(largeRows[0].built.decoded.length, 0x5F00);
  assert.strictEqual(OB64.armySprites.resetModel(
    largeArmy, largeAtlas.key, 0x19), true);
  assert.strictEqual(OB64.armySprites.hasCurrentPlane(
    largeArmy, largeAtlas.key, 0x19), false);

  const oversizedArmy = OB64.armySprites.initialize(z64);
  const oversizedAtlas = oversizedArmy.byKey['player-back-ordinary'];
  let random = 0x12345678;
  oversizedAtlas.models.forEach(model => {
    const noise = new Uint8Array(oversizedAtlas.width * oversizedAtlas.height);
    for (let index = 0; index < noise.length; index++) {
      random ^= random << 13;
      random ^= random >>> 17;
      random ^= random << 5;
      noise[index] = random & 0xFF;
    }
    OB64.armySprites.setEdit(oversizedArmy, oversizedAtlas.key,
      model.modelId, noise, { history: false });
  });
  assert.throws(() => OB64.armySprites.buildResources(oversizedArmy),
    /in-place compressed capacity/);

  console.log('PASS Army sprite class routes, missing player planes, editing, Project data, and export');
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
