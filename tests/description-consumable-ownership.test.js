'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const MASTER = path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');

global.window = global;
global.localStorage = { getItem() { return null; }, setItem() {}, removeItem() {} };
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js', 'parsers.js', 'repack.js', 'description-codec.js',
  'consumable-effects.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

let failures = 0;
function check(name, condition, detail) {
  console.log((condition ? 'PASS' : 'FAIL') + '  ' + name +
    (condition || !detail ? '' : ' - ' + detail));
  if (!condition) failures++;
}

function equalBytes(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let index = 0; index < a.length; index++) if (a[index] !== b[index]) return false;
  return true;
}

(async function main() {
  const sourceBytes = new Uint8Array(fs.readFileSync(MASTER));
  const identity = await OB64.consumableEffects.inspectSourceIdentity(
    sourceBytes,
    path.basename(MASTER)
  );
  const rom = OB64.loadROM(sourceBytes.slice().buffer);
  OB64.consumableEffects.initializeSession(rom, identity, {
    filename: path.basename(MASTER),
  });
  rom.original = {
    consumables: rom.consumables.map(record => ({
      flagHi: record.flagHi,
      price: record.price,
      flagLo: record.flagLo.slice(),
    })),
  };
  rom.consumableDescriptions = OB64.descriptionCodec.withText(
    rom.consumableDescriptions,
    20,
    'Edited ordinary consumable text.'
  );

  const pendingDirty = {
    consumableDescriptions: true,
    consumableEffects: true,
  };
  OB64.consumableEffects.setItemMagnitude(
    rom.consumableEffects,
    4,
    rom.consumableEffects.models[OB64.consumableEffects.ITEM_TO_MODEL[4]].magnitude + 1
  );
  const pendingOwners = OB64.consumableEffects.standardPatchOwners(rom, pendingDirty);
  let simultaneousError = null;
  try {
    OB64.consumableEffects.prepareTransaction(
      rom.consumableEffects,
      rom.z64,
      pendingOwners
    );
  } catch (error) {
    simultaneousError = error;
  }
  check('pending consumable text and effect writes collide before export',
    simultaneousError && /Consumable Descriptions/.test(simultaneousError.message),
    simultaneousError && simultaneousError.message);

  OB64.consumableEffects.resetItem(rom.consumableEffects, 4);
  const candidate = Object.assign({}, rom, { z64: rom.z64.slice() });
  OB64.serializeConsumableDescriptions(
    rom.consumableDescriptions,
    candidate.z64
  );
  const descriptionOnlyDirty = { consumableDescriptions: true };
  const descriptionOwners = OB64.consumableEffects.standardPatchOwners(
    rom,
    descriptionOnlyDirty
  );
  const rawCandidate = OB64.consumableEffects.serializeCandidate(candidate);
  const prepared = await OB64.consumableEffects.prepareOrdinaryExport(
    candidate,
    rom.consumableEffects,
    candidate.z64,
    rawCandidate,
    'ob64_modified.v64',
    descriptionOwners
  );
  OB64.consumableEffects.commitOrdinaryExport(
    rom.consumableEffects,
    prepared,
    candidate.z64
  );
  const manifest = rom.consumableEffects.revisionManifest.description;
  check('ordinary description export advances the complete-slot ledger',
    equalBytes(
      rom.consumableEffects.ledger.currentDescriptionSlot,
      candidate.z64.subarray(manifest.start, manifest.end)
    ));
  check('ordinary description export records adopted owner identity',
    rom.consumableEffects.ledger.priorOwnerRegions.some(owner =>
      owner.id === 'consumable-descriptions-adopted'));

  rom.z64 = candidate.z64;
  OB64.consumableEffects.setItemMagnitude(
    rom.consumableEffects,
    4,
    rom.consumableEffects.models[OB64.consumableEffects.ITEM_TO_MODEL[4]].magnitude + 1
  );
  const laterOwners = OB64.consumableEffects.standardPatchOwners(rom, {
    consumableEffects: true,
  });
  let laterTransaction = null;
  let laterError = null;
  try {
    laterTransaction = OB64.consumableEffects.prepareTransaction(
      rom.consumableEffects,
      rom.z64,
      laterOwners
    );
  } catch (error) {
    laterError = error;
  }
  check('later non-healing effect edit accepts adopted text ownership',
    !laterError && laterTransaction && laterTransaction.writes.length > 0,
    laterError && laterError.message);

  if (failures) {
    console.error('\n' + failures + ' consumable ownership check(s) failed.');
    process.exit(1);
  }
  console.log('\nAll consumable description ownership checks passed.');
})().catch(error => {
  console.error(error && error.stack || error);
  process.exit(1);
});
