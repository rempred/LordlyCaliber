'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');

global.window = global;
global.OB64 = {
  statGateRelocation: {
    parse(bytes, layout) {
      assert.strictEqual(bytes[0], 9);
      assert.strictEqual(layout.id, 'rev0');
      return {
        raw: Uint8Array.of(7, 8),
        meta: { mode: 'relocated' },
        byClass: {
          1: { str: 42, vit: 43 },
          2: { str: 52, vit: 53 },
        },
      };
    },
  },
};
window.OB64 = global.OB64;
vm.runInThisContext(
  fs.readFileSync(path.join(EDITOR, 'rom-export-candidate.js'), 'utf8'),
  { filename: path.join(EDITOR, 'rom-export-candidate.js') }
);

function sourceRom() {
  return {
    z64: Uint8Array.of(1, 2, 3, 4),
    layout: { id: 'rev0' },
    sharedParserState: { retained: true },
    scenarioEditor: {
      archiveOriginalSlots: { 1: Uint8Array.of(10, 11) },
      slotOwnedArchives: { 1: true },
      relocationOwnedWindows: [{ start: 100, end: 120 }],
      authoringState: { retained: true },
    },
    scenarioRelocations: [{ key: 1, start: 100 }],
    statGates: {
      raw: Uint8Array.of(1),
      meta: { mode: 'retail' },
      byClass: { 1: { str: 10, vit: 11, retained: true } },
    },
  };
}

const source = sourceRom();
const candidate = OB64.romExportCandidate.create(source);
assert.notStrictEqual(candidate, source);
assert.notStrictEqual(candidate.z64, source.z64);
assert.deepStrictEqual(Array.from(candidate.z64), Array.from(source.z64));
assert.strictEqual(candidate.layout, source.layout,
  'read-only parsed state keeps the existing shallow-copy contract');
assert.strictEqual(candidate.sharedParserState, source.sharedParserState);
assert.notStrictEqual(candidate.scenarioEditor, source.scenarioEditor);
assert.notStrictEqual(candidate.scenarioEditor.archiveOriginalSlots,
  source.scenarioEditor.archiveOriginalSlots);
assert.notStrictEqual(candidate.scenarioEditor.slotOwnedArchives,
  source.scenarioEditor.slotOwnedArchives);
assert.notStrictEqual(candidate.scenarioEditor.relocationOwnedWindows,
  source.scenarioEditor.relocationOwnedWindows);
assert.notStrictEqual(candidate.scenarioEditor.relocationOwnedWindows[0],
  source.scenarioEditor.relocationOwnedWindows[0]);
assert.strictEqual(candidate.scenarioEditor.authoringState,
  source.scenarioEditor.authoringState);
assert.notStrictEqual(candidate.scenarioRelocations, source.scenarioRelocations);
assert.notStrictEqual(candidate.scenarioRelocations[0], source.scenarioRelocations[0]);

candidate.z64[0] = 9;
candidate.scenarioEditor.slotOwnedArchives[2] = true;
candidate.scenarioEditor.relocationOwnedWindows[0].start = 101;
candidate.scenarioRelocations[0].start = 101;
assert.strictEqual(source.z64[0], 1,
  'candidate ROM writes must not mutate the loaded source bytes');
assert.strictEqual(source.scenarioEditor.slotOwnedArchives[2], undefined);
assert.strictEqual(source.scenarioEditor.relocationOwnedWindows[0].start, 100);
assert.strictEqual(source.scenarioRelocations[0].start, 100);

candidate.statGatePlan = { changed: true };
const originalClassOne = source.statGates.byClass[1];
const adopted = OB64.romExportCandidate.adopt(source, candidate);
assert.strictEqual(adopted, source);
assert.strictEqual(source.z64, candidate.z64,
  'ordinary adoption installs the validated candidate byte buffer');
assert.strictEqual(source.scenarioEditor.archiveOriginalSlots,
  candidate.scenarioEditor.archiveOriginalSlots);
assert.strictEqual(source.scenarioEditor.slotOwnedArchives,
  candidate.scenarioEditor.slotOwnedArchives);
assert.strictEqual(source.scenarioEditor.relocationOwnedWindows,
  candidate.scenarioEditor.relocationOwnedWindows);
assert.strictEqual(source.scenarioRelocations, candidate.scenarioRelocations);
assert.strictEqual(source.statGates.byClass[1], originalClassOne,
  'existing stat-gate row identity remains stable during readback');
assert.deepStrictEqual(source.statGates.byClass[1],
  { str: 42, vit: 43, retained: true });
assert.deepStrictEqual(source.statGates.byClass[2], { str: 52, vit: 53 });
assert.deepStrictEqual(Array.from(source.statGates.raw), [7, 8]);
assert.deepStrictEqual(source.statGates.meta, { mode: 'relocated' });

const verifiedTarget = sourceRom();
const verifiedCandidate = OB64.romExportCandidate.create(verifiedTarget);
verifiedCandidate.z64[0] = 9;
verifiedCandidate.scenarioEditor.slotOwnedArchives[3] = true;
const alreadyAdoptedBytes = Uint8Array.of(9, 2, 3, 4);
verifiedTarget.z64 = alreadyAdoptedBytes;
OB64.romExportCandidate.adopt(verifiedTarget, verifiedCandidate, {
  romBytesAlreadyAdopted: true,
});
assert.strictEqual(verifiedTarget.z64, alreadyAdoptedBytes,
  'verified Consumable Effects adoption retains its private installed bytes');
assert.strictEqual(verifiedTarget.scenarioEditor.slotOwnedArchives,
  verifiedCandidate.scenarioEditor.slotOwnedArchives,
  'verified adoption still advances Scenario ownership metadata');

assert.throws(
  () => OB64.romExportCandidate.create(null),
  /Source ROM must provide a normalized z64 byte buffer/
);
assert.throws(
  () => OB64.romExportCandidate.create({ z64: {} }),
  /Source ROM must provide a normalized z64 byte buffer/
);
assert.throws(
  () => OB64.romExportCandidate.adopt(source, source),
  /Candidate ROM must be detached from the target ROM/
);

const indexSource = fs.readFileSync(path.join(EDITOR, 'index.html'), 'utf8');
assert(indexSource.indexOf('rom-export-candidate.js') >= 0);
assert(indexSource.indexOf('rom-export-candidate.js') < indexSource.indexOf('app.js'));

console.log('PASS detached ROM export candidate creation, rejection, and adoption');
