'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const ROOT = path.resolve(EDITOR, '..');
const ROMS = {
  'us-rev0': path.join(ROOT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'),
  'us-rev1': path.join(ROOT, 'Rev 1.1',
    'Ogre Battle 64 - Person of Lordly Caliber (USA) (Rev 1).z64'),
};

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = window.OB64 || {};');
for (const filename of ['parsers.js', 'description-codec.js']) {
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
  for (let index = 0; index < a.length; index++) {
    if (a[index] !== b[index]) return false;
  }
  return true;
}

function loadNormalized(layoutId) {
  const source = new Uint8Array(fs.readFileSync(ROMS[layoutId]));
  const normalized = OB64.normalizeRomImage(
    source.buffer.slice(source.byteOffset, source.byteOffset + source.byteLength)
  );
  const layout = OB64.detectRomLayout(normalized.z64);
  if (!layout || layout.id !== layoutId) {
    throw new Error('Expected ' + layoutId + ', found ' + (layout && layout.id));
  }
  OB64.applyRomLayout(layout);
  return { z64: normalized.z64, layout };
}

function outsideOwnerUnchanged(before, after, region) {
  return equalBytes(before.subarray(0, region.start), after.subarray(0, region.start)) &&
    equalBytes(before.subarray(region.start + region.size),
      after.subarray(region.start + region.size));
}

const expectedCounts = { items: 278, consumables: 50, classes: 225, actions: 159 };
const expectedDecodedSizes = {
  items: 20427, consumables: 3234, classes: 15317, actions: 8536,
};
const expectedCompressedCapacities = {
  items: 9702, consumables: 2055, classes: 6399, actions: 3145,
};

for (const layoutId of Object.keys(ROMS)) {
  const { z64 } = loadNormalized(layoutId);
  const blocks = OB64.descriptionCodec.parseAll(z64);
  for (const kind of Object.keys(expectedCounts)) {
    const block = blocks[kind];
    check(layoutId + ' ' + kind + ' record count',
      block.records.length === expectedCounts[kind], block.records.length);
    check(layoutId + ' ' + kind + ' decoded size',
      block.meta.decompSize === expectedDecodedSizes[kind], block.meta.decompSize);
    check(layoutId + ' ' + kind + ' fixed compressed capacity',
      block.meta.compressedCapacity === expectedCompressedCapacities[kind],
      block.meta.compressedCapacity);
    check(layoutId + ' ' + kind + ' computed header location',
      block.meta.headerOffset === OB64.LZSS_GAP_START +
        OB64.descriptionCodec.specs[kind].gapOffset,
      block.meta.headerOffset.toString(16));

    const decoded = OB64.lzssDecode(
      z64,
      block.meta.headerOffset + 8,
      block.meta.decompSize
    ).output;
    check(layoutId + ' ' + kind + ' lossless record parse/serialize',
      equalBytes(decoded, OB64.descriptionCodec.serializeRecords(block)));
  }

  if (layoutId === 'us-rev1') {
    check('Rev 1 consumable description header',
      blocks.consumables.meta.headerOffset === 0x237709E,
      blocks.consumables.meta.headerOffset.toString(16));
  }

  const classWithHelp = blocks.classes.records.find(record =>
    record.id > 0 && record.id <= 0xA4 &&
    record.tokens.some(token => token.type === 'reference' && token.marker === 'HELP'));
  const physicalAction = blocks.actions.records.find(record =>
    record.id > 0 && record.tokens.some(token =>
      token.type === 'reference' && token.value === '10'));
  const cases = [
    { kind: 'items', id: 1, text: 'Edited item description.' },
    { kind: 'consumables', id: 1, text: 'Edited consumable description.' },
    { kind: 'classes', id: classWithHelp.id, text: 'Edited class description.' },
    { kind: 'actions', id: physicalAction.id, text: 'Type: Physical.' },
  ];

  for (const testCase of cases) {
    const originalBlock = blocks[testCase.kind];
    const originalHelp = testCase.kind === 'classes'
      ? originalBlock.records[testCase.id].helpReference : null;
    const edited = OB64.descriptionCodec.withText(
      originalBlock, testCase.id, testCase.text
    );
    const measurement = OB64.descriptionCodec.measureText(
      originalBlock, testCase.id, testCase.text
    );
    const candidate = z64.slice();
    OB64.descriptionCodec.serializeBlock(edited, candidate);
    const reparsed = OB64.descriptionCodec.parseBlock(testCase.kind, candidate);
    const region = OB64.descriptionCodec.ownerRegion(edited);
    check(layoutId + ' ' + testCase.kind + ' edited text reads back',
      reparsed.records[testCase.id].editableText === testCase.text,
      reparsed.records[testCase.id].editableText);
    check(layoutId + ' ' + testCase.kind + ' stays inside its physical slot',
      outsideOwnerUnchanged(z64, candidate, region));
    check(layoutId + ' ' + testCase.kind + ' compressed stream fits',
      edited.meta.compressedSize <= edited.meta.compressedCapacity,
      edited.meta.compressedSize + ' / ' + edited.meta.compressedCapacity);
    check(layoutId + ' ' + testCase.kind + ' live size matches export size',
      measurement.compressedSize === edited.meta.compressedSize,
      measurement.compressedSize + ' / ' + edited.meta.compressedSize);
    check(layoutId + ' ' + testCase.kind + ' live size reports remaining capacity',
      measurement.fits && measurement.remaining ===
        measurement.compressedCapacity - measurement.compressedSize,
      measurement.remaining);
    if (testCase.kind === 'classes') {
      check(layoutId + ' class help reference is preserved',
        reparsed.records[testCase.id].helpReference === originalHelp,
        reparsed.records[testCase.id].helpReference + ' / ' + originalHelp);
    }
    if (testCase.kind === 'actions') {
      check(layoutId + ' action Physical reference is preserved structurally',
        reparsed.records[testCase.id].tokens.some(token =>
          token.type === 'reference' && token.value === '10'));
    }
  }

  const prepared = OB64.descriptionCodec.prepareProjectChanges({
    itemDescriptions: blocks.items,
    consumableDescriptions: blocks.consumables,
    classDescriptions: blocks.classes,
    actionDescriptions: blocks.actions,
  }, {
    items: { 2: 'Project item text.' },
    consumables: { 2: 'Project consumable text.' },
    classes: { [classWithHelp.id]: 'Project class text.' },
    actions: { [physicalAction.id]: 'Type: Physical.' },
  });
  check(layoutId + ' Project descriptions prevalidate all four blocks',
    prepared.counts.items === 1 && prepared.counts.consumables === 1 &&
    prepared.counts.classes === 1 && prepared.counts.actions === 1);
}

if (failures) {
  console.error('\n' + failures + ' description codec check(s) failed.');
  process.exit(1);
}
console.log('\nAll description codec checks passed.');
