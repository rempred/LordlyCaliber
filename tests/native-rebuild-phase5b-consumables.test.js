'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const MASTER_NAME = 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64';
const MASTER = [
  process.env.OB64_MASTER_ROM,
  path.resolve(EDITOR, '..', MASTER_NAME),
  path.resolve(EDITOR, '..', '..', 'OgreBattlel64', MASTER_NAME),
].filter(Boolean).find(candidate => fs.existsSync(candidate));
const DECOMP = [
  process.env.OB64_NATIVE_DECOMP,
  path.resolve(EDITOR, '..', 'lordly-native-rebuild-decomp'),
].filter(Boolean).find(candidate => fs.existsSync(candidate));

if (!MASTER) throw new Error('Set OB64_MASTER_ROM to the read-only US Rev 0 master ROM.');
if (!DECOMP) throw new Error('Set OB64_NATIVE_DECOMP to the accepted Phase 5A decomp worktree.');

global.window = global;
global.module = undefined;
global.localStorage = { getItem() { return null; }, setItem() {}, removeItem() {} };
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of [
  'data.js', 'rom-names-data.js', 'portraits.js', 'parsers.js', 'repack.js',
  'description-codec.js', 'consumable-effects.js', 'patch.js', 'tools-data.js',
  'tools.js', 'native-rebuild-data.js', 'native-rebuild.js',
]) {
  const fullPath = path.join(EDITOR, filename);
  vm.runInThisContext(fs.readFileSync(fullPath, 'utf8'), { filename: fullPath });
}

const masterContainer = new Uint8Array(fs.readFileSync(MASTER));
const retailZ64 = OB64.loadROM(masterContainer.slice().buffer).z64.slice();
const data = OB64.nativeRebuild.verifyData();
const effect = data.consumables.effect;
const descriptions = data.consumables.descriptions;
const metadata = data.consumables.metadata;
const phase5Root = path.join(DECOMP, 'build', 'lordly-native-rebuild',
  'phase5a-consumables-r1');
let checks = 0;

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex').toUpperCase();
}

function check(name, condition, detail) {
  assert.ok(condition, name + (detail ? ': ' + detail : ''));
  console.log('PASS  ' + name);
  checks++;
}

function expectThrow(name, action, pattern) {
  let found = null;
  try { action(); } catch (error) { found = error; }
  check(name, !!found && pattern.test(found.message), found && found.message);
}

function equalBytes(left, right) {
  return Buffer.from(left).equals(Buffer.from(right));
}

function identityFor(rom) {
  return OB64.consumableEffects.evaluateSourceIdentity({
    filename: MASTER_NAME,
    size: rom.z64.length,
    sha256: sha256(rom.z64),
    byteOrder: rom.byteOrder,
    imageName: 'OgreBattle64',
    gameId: 'NOBE',
    country: rom.z64[0x3E],
    version: rom.z64[0x3F],
    crc1: OB64.readU32BE(rom.z64, 0x10),
    crc2: OB64.readU32BE(rom.z64, 0x14),
  });
}

function freshRom(input) {
  const bytes = input ? input.slice() : masterContainer.slice();
  const rom = OB64.loadROM(bytes.buffer.slice(bytes.byteOffset,
    bytes.byteOffset + bytes.byteLength));
  OB64.nativeRebuild.initializeSource(rom);
  OB64.consumableEffects.initializeSession(rom, identityFor(rom), {
    filename: MASTER_NAME,
  });
  OB64.patch.snapshotOriginal(rom);
  OB64.tools.initState(rom);
  OB64.nativeRebuild.initializeToolState(rom);
  return rom;
}

function cloneModels(models) {
  return JSON.parse(JSON.stringify(models));
}

function setModels(rom, models) {
  rom.consumableEffects.models = OB64.consumableEffects.validateAllModels(
    cloneModels(models));
  OB64.consumableEffects.refreshPending(rom.consumableEffects);
}

function executeNative(rom, shopsDirty) {
  OB64.buildModes.set(rom, 'rebuild');
  const plan = OB64.nativeRebuild.prepareExport(rom, { shopsDirty: !!shopsDirty });
  plan.compositionAuthorization = OB64.nativeRebuild.authorizeComposition(
    plan.owners.concat(plan.restorationOwners));
  const candidate = Object.assign({}, rom, { z64: plan.baseZ64.slice() });
  const result = OB64.nativeRebuild.applyPlan(plan, candidate, null);
  if (result.crc) OB64.recalcN64CRC(candidate.z64);
  const validation = OB64.nativeRebuild.validateApplied(candidate, plan, null);
  return { candidate, plan, result, validation };
}

function executeLegacyEffects(rom) {
  OB64.buildModes.set(rom, 'patch');
  const candidate = Object.assign({}, rom, { z64: rom.z64.slice() });
  const transaction = OB64.consumableEffects.prepareTransaction(
    rom.consumableEffects, candidate.z64, []);
  const writes = transaction ? OB64.consumableEffects.applyTransaction(
    transaction, candidate.z64, rom.consumableEffects) : [];
  if (writes.length) OB64.recalcN64CRC(candidate.z64);
  return { candidate, transaction, writes };
}

function buildBoth(models) {
  const legacyRom = freshRom();
  setModels(legacyRom, models);
  const legacy = executeLegacyEffects(legacyRom);
  const nativeRom = freshRom();
  setModels(nativeRom, models);
  const native = executeNative(nativeRom, false);
  return { legacy, native };
}

function modelVariant(key, field, value) {
  const models = OB64.consumableEffects.vanillaModels();
  models[key][field] = value;
  return models;
}

function assertSharedAfterImage(label, models) {
  const built = buildBoth(models);
  check(label + ' matches the accepted legacy after-image',
    equalBytes(built.native.candidate.z64, built.legacy.candidate.z64));
  check(label + ' writes one complete effect owner',
    built.native.plan.writes.filter(write => write.owner === effect.owner).length === 1 &&
    built.native.plan.owners.find(owner => owner.id === effect.owner).regions
      .some(region => region.kind === 'rom' && region.start === effect.romStart &&
        region.size === effect.bytes));
  return built;
}

check('Patch ROM retains the frozen legacy Consumables implementation',
  sha256(Buffer.from(fs.readFileSync(path.join(EDITOR, 'consumable-effects.js'), 'utf8')
    .replace(/\r\n/g, '\n'), 'utf8')) ===
    '40D627214239F3C2664C4B2151A94F7FE5A2C77D0558176474172B5D1B78277D');
const appSource = fs.readFileSync(path.join(EDITOR, 'app.js'), 'utf8')
  .replace(/\r\n/g, '\n');
check('Rebuild ROM skips the legacy effect transaction',
  appSource.includes("if (!nativePlan) {\n        effectTransaction = OB64.consumableEffects.prepareTransaction("));
check('Rebuild ROM skips the separate manual description writer',
  appSource.includes('if (dirty.consumableDescriptions && !nativePlan) {'));
check('the authenticated native owners retain retail placement',
  effect.romStart === 0x41098 && effect.ramStart === 0x8016B198 &&
  effect.bytes === 1380 && descriptions.romStart === 0x237744A &&
  descriptions.bytes === 2064 && descriptions.streamCapacity === 2055);
check('Phase 5A adds no relocation, tail code, or RAM allocation',
  data.consumables.acceptedSource.relocationLedgerSha256 ===
    '3209437ED671D853AC3A1A4D3656019EB750E95D9A7CD813A4D1BCE6702B9EB1');
check('Shop capacities remain 50 equipment and 30 Expendables in Rebuild ROM',
  OB64.nativeRebuild.capacities('rebuild').equipment === 50 &&
  OB64.nativeRebuild.capacities('rebuild').expendables === 30);
check('locked and hidden Consumable records remain unchanged',
  OB64.consumableEffects.lockedIds().filter(id => id <= 31).join(',') ===
    '6,7,8,9,20,21,22,23,24,25,26,27,28,29,30,31' &&
  OB64.consumableEffects.lockedIds().filter(id => id >= 32).join(',') ===
    '32,33,34,35,36,37,38,39,40,41,42,43,44' &&
  OB64.consumableEffects.CATALOG_MAX_ID === 31);

const noOp = executeNative(freshRom(), false);
check('no-op Rebuild ROM is exact US Rev 0', sha256(noOp.candidate.z64) ===
  '571E83396BC81E70DA4C0A20313D82DBD7DFE685F2C37418C8E27F927E2CC67A');
check('no-op Rebuild ROM records both complete native owners',
  noOp.plan.owners.filter(owner => owner.id === effect.owner ||
    owner.id === descriptions.owner).length === 2 && noOp.plan.writes.length === 2);
check('no-op descriptions require no encoding pass',
  noOp.plan.consumables.descriptionEncodingPasses === 0);

// Exercise legacy admission before an equivalent Phase 5B image enters the
// trusted in-session owner-image registry.
const conversionModels = modelVariant('powerFruit', 'magnitude', 37);
const legacyConversionRom = freshRom();
setModels(legacyConversionRom, conversionModels);
const legacyConversion = executeLegacyEffects(legacyConversionRom);
const recognizedLegacy = freshRom(legacyConversion.candidate.z64);
check('load detection identifies a complete legacy Consumables owner',
  recognizedLegacy.nativeRebuild.sourceClassification.owners.consumables.ownerState ===
    'legacy-complete');
const converted = executeNative(recognizedLegacy, false);
check('recognized legacy ownership converts through the Phase 5A source owner',
  converted.plan.consumables.conversion === 'legacy-to-native' &&
  equalBytes(converted.candidate.z64, legacyConversion.candidate.z64));
const convertedInMemory = OB64.nativeRebuild._test.classifyConsumableOwner(
  converted.candidate.z64);
check('validated candidate receipt is bound to the exact in-memory product',
  convertedInMemory.ownerState === 'native-complete' &&
  convertedInMemory.provenance.source === 'validated-phase5b-candidate' &&
  convertedInMemory.provenance.lifecycle === 'validated-candidate' &&
  convertedInMemory.provenance.romSha256 === sha256(converted.candidate.z64));
const unchangedPatchReload = freshRom(legacyConversion.candidate.z64);
let unchangedPatchError = null;
try {
  OB64.nativeRebuild.patchSourceCheck(unchangedPatchReload);
} catch (error) {
  unchangedPatchError = error;
}
check('an unchanged valid Patch image remains Patch-compatible after Rebuild validation',
  !unchangedPatchError &&
  unchangedPatchReload.nativeRebuild.sourceClassification.owners.consumables.ownerState ===
    'legacy-complete', unchangedPatchError && unchangedPatchError.message);
const convertedReload = freshRom(converted.candidate.z64);
check('a reloaded byte copy does not inherit an in-memory candidate receipt',
  convertedReload.nativeRebuild.sourceClassification.owners.consumables.ownerState ===
    'legacy-complete');

const receiptOutsideOwnerOffset = 0x02000000;
converted.candidate.z64[receiptOutsideOwnerOffset] ^= 0x01;
const changedReceiptState = OB64.nativeRebuild._test.classifyConsumableOwner(
  converted.candidate.z64);
check('a validated receipt rejects a later full-ROM identity change outside its owner ranges',
  changedReceiptState.ownerState === 'partial-native' &&
  changedReceiptState.provenance.mismatch === true &&
  changedReceiptState.provenance.expectedRomSha256 ===
    convertedInMemory.provenance.romSha256);
converted.candidate.z64[receiptOutsideOwnerOffset] ^= 0x01;
check('restoring the exact validated full-ROM identity restores its lifecycle receipt',
  OB64.nativeRebuild._test.classifyConsumableOwner(
    converted.candidate.z64).ownerState === 'native-complete');

const partialLegacy = legacyConversion.candidate.z64.slice();
partialLegacy[effect.romStart] ^= 0x01;
const partialLegacyState = OB64.nativeRebuild._test.classifyConsumableOwner(partialLegacy);
check('an untrusted partial legacy effect image fails closed',
  partialLegacyState.ownerState === 'partial-legacy' &&
  partialLegacyState.state === 'foreign');
converted.candidate.z64[effect.romStart] ^= 0x01;
const partialNativeState = OB64.nativeRebuild._test.classifyConsumableOwner(
  converted.candidate.z64);
check('a trusted partial native effect image fails closed',
  partialNativeState.ownerState === 'partial-native' &&
  partialNativeState.state === 'foreign');

const mixed = retailZ64.slice();
const mixedModels = modelVariant('healLeaf', 'magnitude', 111);
mixed.set(OB64.nativeRebuild._test.buildConsumableEffectSection(mixedModels).bytes,
  effect.romStart);
const mixedState = OB64.nativeRebuild._test.classifyConsumableOwner(mixed);
check('mixed effect and healing-description ownership fails closed',
  mixedState.ownerState === 'mixed' && mixedState.state === 'mixed');
expectThrow('Rebuild ROM rejects mixed Consumables ownership before writing', () => {
  const rom = freshRom(mixed);
  OB64.buildModes.set(rom, 'rebuild');
  OB64.nativeRebuild.prepareExport(rom, {});
}, /partial, mixed, or unknown migrated ownership/);
const malformed = retailZ64.slice();
OB64.writeU32BE(malformed, 0x410F8, 0xFFFFFFFF);
check('malformed source opcodes fail closed',
  OB64.nativeRebuild._test.classifyConsumableOwner(malformed).ownerState ===
    'partial-legacy');
expectThrow('Patch ROM rejects partial Consumables ownership', () => {
  const rom = freshRom(malformed);
  OB64.nativeRebuild.patchSourceCheck(rom);
}, /cannot export from a Rebuild ROM or mixed native source/);

const descriptionPayloadSize = OB64.readU32BE(retailZ64, descriptions.romStart);
for (const malformedPayload of [
  { label: 'zero-length', payloadSize: 4 },
  { label: 'short', payloadSize: descriptionPayloadSize - 1 },
]) {
  const malformedDescription = retailZ64.slice();
  OB64.writeU32BE(malformedDescription, descriptions.romStart,
    malformedPayload.payloadSize);
  expectThrow(malformedPayload.label + ' declared description payload is rejected by the codec',
    () => OB64.descriptionCodec.parseBlock('consumables', malformedDescription),
    /header declares only/);
  const malformedDescriptionState =
    OB64.nativeRebuild._test.classifyConsumableOwner(malformedDescription);
  check(malformedPayload.label + ' declared description payload fails owner detection closed',
    malformedDescriptionState.state === 'foreign' &&
    malformedDescriptionState.ownerState === 'partial-legacy' &&
    /header declares only/.test(malformedDescriptionState.reason));
  expectThrow(malformedPayload.label + ' declared description payload cannot enter Rebuild planning',
    () => {
      const malformedDescriptionRom = freshRom(malformedDescription);
      OB64.buildModes.set(malformedDescriptionRom, 'rebuild');
      OB64.nativeRebuild.prepareExport(malformedDescriptionRom, {});
    }, /partial, mixed, or unknown migrated ownership/);
}

for (const key of OB64.consumableEffects.MODEL_ORDER) {
  const def = OB64.consumableEffects.MODEL_DEFS[key];
  const models = OB64.consumableEffects.vanillaModels();
  if (def.kind === 'magnitude') {
    models[key].magnitude = def.vanillaMagnitude === def.domainMax
      ? def.domainMin : def.vanillaMagnitude + 1;
  } else {
    models[key].minimum = def.domainMin;
    models[key].maximum = Math.min(def.domainMax, def.domainMin + 2);
  }
  assertSharedAfterImage('single model ' + key, models);
}

const allModels = {
  healLeaf: { magnitude: 111 }, healSeed: { magnitude: 222 },
  healPack: { magnitude: 333 }, powerFruit: { magnitude: 21 },
  angelFruit: { magnitude: 51 }, cupOfLife: { minimum: 7, maximum: 12 },
  sharedStatBoosters11To16: { minimum: 3, maximum: 5 },
  scrollOfDiscipline: { minimum: 7, maximum: 9 },
  urnOfChaos: { minimum: -8, maximum: -4 },
  gobletOfDestiny: { minimum: -5, maximum: 5 },
};
const allBuilt = assertSharedAfterImage('all ten models', allModels);
check('Heal Pack keeps both compiled words equal',
  (OB64.readU32BE(allBuilt.native.candidate.z64, 0x41108) & 0xFFFF) === 333 &&
  (OB64.readU32BE(allBuilt.native.candidate.z64, 0x4110C) & 0xFFFF) === 333);
check('records 11 through 16 keep one shared range model',
  (OB64.readU32BE(allBuilt.native.candidate.z64, 0x41444) & 0xFFFF) === 3 &&
  (OB64.readU32BE(allBuilt.native.candidate.z64, 0x41480) & 0xFFFF) === 3);

for (const key of OB64.consumableEffects.MODEL_ORDER) {
  const def = OB64.consumableEffects.MODEL_DEFS[key];
  for (const boundary of ['minimum', 'maximum']) {
    const models = OB64.consumableEffects.vanillaModels();
    if (def.kind === 'magnitude') {
      models[key].magnitude = boundary === 'minimum' ? def.domainMin : def.domainMax;
    } else {
      const value = boundary === 'minimum' ? def.domainMin : def.domainMax;
      models[key].minimum = value;
      models[key].maximum = value;
    }
    assertSharedAfterImage(key + ' ' + boundary, models);
  }
}

const manualOnlyRom = freshRom();
manualOnlyRom.consumableDescriptions = OB64.descriptionCodec.withText(
  manualOnlyRom.consumableDescriptions, 30, 'Phase 5B manual description only.');
const manualLegacy = Object.assign({}, manualOnlyRom, { z64: manualOnlyRom.z64.slice() });
OB64.serializeConsumableDescriptions(manualOnlyRom.consumableDescriptions,
  manualLegacy.z64);
const manualNative = executeNative(manualOnlyRom, false);
check('manual-only descriptions match the accepted Patch writer',
  equalBytes(manualNative.candidate.z64, manualLegacy.z64));
check('manual-only descriptions encode the complete slot once',
  manualNative.plan.consumables.descriptionEncodingPasses === 1 &&
  manualNative.plan.consumables.descriptionMode === 'unified-manual-and-healing');

const healingOnly = assertSharedAfterImage('healing-only descriptions',
  modelVariant('healSeed', 'magnitude', 444));
check('healing-only descriptions use one legacy-compatible complete-slot encode',
  healingOnly.native.plan.consumables.descriptionEncodingPasses === 1 &&
  healingOnly.native.plan.consumables.descriptionMode ===
    'legacy-compatible-unified-healing');

const arbitraryTextRom = freshRom();
arbitraryTextRom.consumableDescriptions = OB64.descriptionCodec.withText(
  arbitraryTextRom.consumableDescriptions, 1,
  'Manual Patch description with no numeric healing token.');
const arbitraryPatchBytes = arbitraryTextRom.z64.slice();
OB64.serializeConsumableDescriptions(arbitraryTextRom.consumableDescriptions,
  arbitraryPatchBytes);
const arbitraryPatchReload = freshRom(arbitraryPatchBytes);
let arbitraryPatchError = null;
try {
  OB64.nativeRebuild.patchSourceCheck(arbitraryPatchReload);
} catch (error) {
  arbitraryPatchError = error;
}
check('arbitrary manual healing text remains valid for Patch ROM load detection',
  !arbitraryPatchError &&
  arbitraryPatchReload.nativeRebuild.sourceClassification.owners.consumables.ownerState ===
    'legacy-complete' &&
  arbitraryPatchReload.nativeRebuild.sourceClassification.owners.consumables
    .patchCompatible === true,
  arbitraryPatchError && arbitraryPatchError.message);
expectThrow('Rebuild ROM rejects manual healing text without a complete numeric HP token',
  () => {
    OB64.buildModes.set(arbitraryPatchReload, 'rebuild');
    OB64.nativeRebuild.prepareExport(arbitraryPatchReload, {});
  }, /complete numeric value before HP/);

const fourDigitRom = freshRom();
const fourDigitText = fourDigitRom.consumableDescriptions.records[1].editableText
  .replace(/[0-9]+(?=HP)/, '1000')
  .replace('Leaf of a curative herb', 'Edited four-digit curative leaf');
fourDigitRom.consumableDescriptions = OB64.descriptionCodec.withText(
  fourDigitRom.consumableDescriptions, 1, fourDigitText);
const fourDigitPatchBytes = fourDigitRom.z64.slice();
OB64.serializeConsumableDescriptions(fourDigitRom.consumableDescriptions,
  fourDigitPatchBytes);
const fourDigitPatchReload = freshRom(fourDigitPatchBytes);
let fourDigitPatchError = null;
try {
  OB64.nativeRebuild.patchSourceCheck(fourDigitPatchReload);
} catch (error) {
  fourDigitPatchError = error;
}
check('a complete four-digit manual token remains Patch-compatible on reload',
  !fourDigitPatchError &&
  fourDigitPatchReload.nativeRebuild.sourceClassification.owners.consumables
    .patchCompatible === true,
  fourDigitPatchError && fourDigitPatchError.message);
setModels(fourDigitRom, modelVariant('healLeaf', 'magnitude', 555));
const fourDigitBuild = executeNative(fourDigitRom, false);
const fourDigitBlock = OB64.descriptionCodec.parseBlock(
  'consumables', fourDigitBuild.candidate.z64);
const fourDigitOutput = fourDigitBlock.records[1].editableText;
check('healing synchronization replaces the complete numeric token before HP',
  fourDigitOutput.includes('Edited four-digit curative leaf') &&
  (fourDigitOutput.match(/[0-9]+HP/) || [null])[0] === '555HP' &&
  !fourDigitOutput.includes('1555HP'));
check('serialized visible healing text equals the effect model value',
  Number(((fourDigitOutput.match(/[0-9]+(?=HP)/) || [NaN])[0])) ===
    fourDigitBuild.plan.consumables.models.healLeaf.magnitude &&
  fourDigitBuild.validation.details.consumableOwnerState === 'native-complete');

const combinedRom = freshRom();
combinedRom.consumableDescriptions = OB64.descriptionCodec.withText(
  combinedRom.consumableDescriptions, 1,
  combinedRom.consumableDescriptions.records[1].editableText.replace(
    'Leaf of a curative herb', 'Edited curative leaf'));
setModels(combinedRom, modelVariant('healLeaf', 'magnitude', 555));
let serializerCalls = 0;
const originalSerializer = OB64.descriptionCodec.serializeBlock;
OB64.descriptionCodec.serializeBlock = function(block, bytes) {
  serializerCalls++;
  return originalSerializer(block, bytes);
};
const combined = executeNative(combinedRom, false);
OB64.descriptionCodec.serializeBlock = originalSerializer;
const combinedBlock = OB64.descriptionCodec.parseBlock('consumables', combined.candidate.z64);
check('manual text is composed before healing-number synchronization',
  combinedBlock.records[1].editableText.includes('Edited curative leaf') &&
  combinedBlock.records[1].editableText.includes('555HP'));
check('manual plus healing composition calls the complete-slot serializer once',
  serializerCalls === 1 && combined.plan.consumables.descriptionEncodingPasses === 1);
check('all fifty description records survive unified composition',
  combinedBlock.records.length === 50 &&
  combinedBlock.records.slice(4).every((record, index) =>
    record.editableText === combinedRom.consumableDescriptions.records[index + 4].editableText));

const overflowRom = freshRom();
let seed = 0x13579BDF;
let overflowText = '';
for (let index = 0; index < 12000; index++) {
  seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
  overflowText += String.fromCharCode(0x20 + (seed % 0x5F));
}
overflowRom.consumableDescriptions = OB64.descriptionCodec.replaceText(
  overflowRom.consumableDescriptions, 30, overflowText);
const overflowSourceHash = sha256(overflowRom.z64);
expectThrow('unified descriptions reject streams above 2,055 bytes', () =>
  OB64.nativeRebuild.prepareExport(overflowRom, {}), /holds 2055|exceeds its accepted slot/);
check('description overflow rejection leaves the source ROM unchanged',
  sha256(overflowRom.z64) === overflowSourceHash);

const resetRom = freshRom();
OB64.consumableEffects.setItemMagnitude(resetRom.consumableEffects, 4, 99);
OB64.consumableEffects.resetItem(resetRom.consumableEffects, 4);
const resetBuild = executeNative(resetRom, false);
check('reset removes the native consumable delta',
  equalBytes(resetBuild.candidate.z64, retailZ64));
const projectBefore = JSON.stringify({
  models: resetRom.consumableEffects.models,
  descriptions: resetRom.consumableDescriptions.records.map(row => row.editableText),
});
OB64.buildModes.set(resetRom, 'patch');
OB64.buildModes.set(resetRom, 'rebuild');
check('switching or uninstalling a build path does not fork the project model',
  JSON.stringify({
    models: resetRom.consumableEffects.models,
    descriptions: resetRom.consumableDescriptions.records.map(row => row.editableText),
  }) === projectBefore);

const focusedProduct = new Uint8Array(fs.readFileSync(path.join(phase5Root,
  'products', 'native-consumables', 'native-consumables.us_rev0.z64')));
const canonicalProduct = new Uint8Array(fs.readFileSync(path.join(phase5Root,
  'products', 'canonical-native-consumables',
  'canonical-native-consumables.us_rev0.z64')));
const focusedRom = freshRom();
setModels(focusedRom, allModels);
focusedRom.consumableDescriptions = OB64.descriptionCodec.withText(
  focusedRom.consumableDescriptions, 30, 'Phase 5A native consumable composition.');
const focusedBuild = executeNative(focusedRom, false);
check('Editor Rebuild reproduces the accepted focused Phase 5A product',
  equalBytes(focusedBuild.candidate.z64, focusedProduct),
  sha256(focusedBuild.candidate.z64));

const phase4CanonicalPath = path.join(DECOMP, 'build', 'lordly-native-rebuild',
  'phase4-native-shops-r1', 'products', 'canonical-native-shops',
  'canonical-native-shops.us_rev0.z64');
const canonicalBase = new Uint8Array(fs.readFileSync(phase4CanonicalPath));
const canonicalRom = freshRom(canonicalBase);
setModels(canonicalRom, allModels);
canonicalRom.consumableDescriptions = OB64.descriptionCodec.withText(
  canonicalRom.consumableDescriptions, 30, 'Phase 5A native consumable composition.');
const canonicalBuild = executeNative(canonicalRom, false);
check('Editor Rebuild preserves every Phase 1 through Phase 4 owner',
  equalBytes(canonicalBuild.candidate.z64, canonicalProduct),
  sha256(canonicalBuild.candidate.z64));
check('consumable metadata remains a byte-exact passthrough owner',
  equalBytes(canonicalBuild.candidate.z64.slice(metadata.romStart,
    metadata.romStart + metadata.bytes),
  canonicalBase.slice(metadata.romStart, metadata.romStart + metadata.bytes)));

const acceptedNativeLoad = freshRom(focusedProduct);
const focusedAcceptedProvenance =
  acceptedNativeLoad.nativeRebuild.sourceClassification.owners.consumables.provenance;
check('accepted Phase 5A product provenance identifies native-complete ownership',
  acceptedNativeLoad.nativeRebuild.sourceClassification.owners.consumables.ownerState ===
    'native-complete' && acceptedNativeLoad.buildMode === 'rebuild' &&
  focusedAcceptedProvenance.product === 'native-consumables' &&
  focusedAcceptedProvenance.romSha256 === sha256(focusedProduct));
const acceptedCanonicalLoad = freshRom(canonicalProduct);
const canonicalAcceptedProvenance =
  acceptedCanonicalLoad.nativeRebuild.sourceClassification.owners.consumables.provenance;
check('canonical Phase 5A provenance requires its exact full-ROM identity',
  canonicalAcceptedProvenance.product === 'canonical-native-consumables' &&
  canonicalAcceptedProvenance.romSha256 === sha256(canonicalProduct));
expectThrow('Patch ROM rejects authenticated native Consumables ownership', () =>
  OB64.nativeRebuild.patchSourceCheck(acceptedNativeLoad),
  /cannot export from a Rebuild ROM/);
expectThrow('retired healing-text ownership cannot enter Rebuild composition', () =>
  OB64.nativeRebuild.authorizeComposition([{
    id: 'consumable-healing-descriptions',
    name: 'Retired healing descriptions',
    category: 'consumableEffects',
    regions: [{ kind: 'rom', start: descriptions.romStart,
      size: descriptions.bytes, label: 'retired complete slot' }],
  }]), /retired physical owner/);

console.log('\nAll ' + checks + ' Phase 5B Consumables checks passed.');
