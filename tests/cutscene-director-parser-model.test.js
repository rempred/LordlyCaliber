'use strict';

// The Director grammar the editor authors against (from the 153-command corpus
// product) must agree with the parser model generated from the accepted
// byte-exact C dispatcher (OB64 Decomp src/lib/func_00284288.c). This checks
// that the checked-in model regenerates from the C on disk, that the catalog
// cross-check verifies every opcode's static width and query class, that the
// dispatcher's director-mode guards reach retail nodes, and that a disagreeing
// grammar is refused.

const assert = require('assert');
const child_process = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const editor = path.resolve(process.env.OB64_EDITOR_ROOT || path.join(__dirname, '..'));
const { PARENT, MASTER_NAME } = require('./project-paths.cjs');
const generator = path.join(editor, 'tools', 'generate-cutscene-director-parser-model.js');
const acceptedSource = process.env.OB64_DISPATCHER_C ||
  path.join(PARENT, 'OB64 Decomp', 'src', 'lib', 'func_00284288.c');
assert(fs.existsSync(acceptedSource), 'Accepted dispatcher C is missing: ' + acceptedSource +
  ' (set OB64_DISPATCHER_C).');

// 1. The checked-in model is exactly what the accepted C produces today.
const check = child_process.spawnSync(process.execPath, [generator, '--check', '--source', acceptedSource],
  { encoding: 'utf8' });
assert.strictEqual(check.status, 0, 'parser model is stale:\n' + check.stdout + check.stderr);

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['parsers.js', 'repack.js', 'cutscene-data.js', 'cutscene-director-parser-model.js',
  'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js', 'cutscene-codec.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}

const model = OB64.cutsceneDirectorParserModel;
const sourceSha = crypto.createHash('sha256').update(fs.readFileSync(acceptedSource)).digest('hex').toUpperCase();
assert.strictEqual(model.source.sha256, sourceSha, 'the model pins the accepted C on disk');
assert.deepStrictEqual(model.counts, {
  entries: 154, ordinary: 143, extended: 11, queries: 33, directorModeGuarded: 11
});
const entry = opcode => model.entries.find(candidate => candidate.opcodeU32 === opcode);

// 2. Parser facts read straight from the C.
assert.strictEqual(entry(0x5E).widthKind, 'fixed');
assert.strictEqual(entry(0x5E).staticWordSpan, 4, '0x5E is a four-word command the reference table omits');
assert.deepStrictEqual(entry(0x5E).callees, ['func_002B0E8C']);
assert.strictEqual(entry(0xB5).queryRecordKind, 'Q3');
assert.strictEqual(entry(0xB5).fallsThroughTo, 0x0E, '0xB5 falls into the registered-counter query body');
assert.strictEqual(entry(0x95).fallsThroughTo, 0x8E, '0x95 shares the two-word tail of 0x8E');
assert.strictEqual(entry(0x59).widthKind, 'cursor-replacing');
assert.strictEqual(entry(0x59).staticWordSpan, 2);
assert.strictEqual(entry(0x80000001).widthKind, 'terminal-with-trailer');
assert.strictEqual(entry(0x80000003).widthKind, 'returning');
assert.strictEqual(entry(0x80000003).staticWordSpan, 2);
assert.strictEqual(entry(0x2A).staticWordSpan, 7, 'both mode branches of 0x2A add six operand words');
assert.strictEqual(entry(0x2A).directorModeRequirement, 2);
assert.strictEqual(entry(0x0D).directorModeRequirement, 0, 'an early break inverts the guard');
assert.strictEqual(entry(0x80000006).directorModeRequirement, null, 'a branch-selecting guard is not a requirement');
assert.strictEqual(entry(0x80000006).guards[0].scope, 'branch-select');
assert.strictEqual(entry(0x47).directorModeRequirement, null, 'a guard over part of the effect is not a requirement');
assert.strictEqual(entry(0x47).guards[0].scope, 'partial');
assert.strictEqual(entry(0x56).entersResyncMode, true);
assert(model.entries.every(candidate => candidate.widthKind !== 'conditional'),
  'every command adds the same operand words on every path');

// 3. The catalog cross-check verifies all 154 grammar definitions and attaches guards.
const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
assert.strictEqual(catalog.directorParserModel.status, 'verified');
assert.strictEqual(catalog.directorParserModel.matched, 154);
assert.strictEqual(catalog.directorParserModel.source.sha256, sourceSha);
assert.deepStrictEqual(catalog.directorParserModel.directorModeGuarded,
  [0x08, 0x0D, 0x1C, 0x2A, 0x2B, 0x2C, 0x2D, 0x92, 0x94, 0x95, 0x96]);
const grammar = opcode => catalog.data.directorGrammar.find(candidate => candidate.opcodeU32 === opcode);
assert.strictEqual(grammar(0x96).parser.directorModeRequirement, 2);
assert.strictEqual(grammar(0x1C).parser.directorModeRequirement, 0);
assert.strictEqual(grammar(0x07).parser.directorModeRequirement, null);
assert.deepStrictEqual(grammar(0x96).parser.callees, ['func_002ACF08']);

// 4. A grammar that disagrees with the C parser is refused, and so is a missing opcode.
const tampered = JSON.parse(JSON.stringify(OB64.cutsceneData.directorGrammar));
tampered.find(candidate => candidate.opcodeU32 === 0x1C).sourceWordSpan = 4;
assert.throws(() => OB64.cutsceneCatalog.crossCheckDirectorGrammar(tampered, model),
  /disagrees with the accepted C parser.*0x1C/);
const missing = JSON.parse(JSON.stringify(OB64.cutsceneData.directorGrammar))
  .filter(candidate => candidate.opcodeU32 !== 0x5E);
assert.throws(() => OB64.cutsceneCatalog.crossCheckDirectorGrammar(missing, model), /0x0000005E/);
const withoutModel = OB64.cutsceneCatalog.crossCheckDirectorGrammar(
  JSON.parse(JSON.stringify(OB64.cutsceneData.directorGrammar)), null);
assert.strictEqual(withoutModel.status, 'not-loaded');

// 5. Program primitives carry the guard, and a scene's launch mode marks inert commands.
const master = [process.env.OB64_MASTER_ROM, path.join(PARENT, MASTER_NAME)]
  .filter(Boolean).find(file => fs.existsSync(file));
assert(master, 'Set OB64_MASTER_ROM to the US Rev 0 ROM.');
const raw = fs.readFileSync(master);
const retail = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);

(async () => {
  let inert = null;
  let active = null;
  let inspected = 0;
  const programOf = async scene => {
    const source = await OB64.cutsceneCodec.loadSceneSource(retail, scene, { allowModified: true });
    return OB64.cutsceneCodec.createIr(scene, source.decodedBytes, source.nodeDefinitions).program;
  };
  for (const scene of catalog.directorScenes) {
    const launch = scene.launchProfile && scene.launchProfile.directorMode;
    if (!launch || !Number.isInteger(launch.value)) continue;
    const program = await programOf(scene);
    inspected += 1;
    const guarded = program.primitives.filter(node => Number.isInteger(node.directorModeRequirement));
    guarded.forEach(node => {
      assert.strictEqual(node.inertInLaunchMode, node.directorModeRequirement !== launch.value);
      assert.strictEqual(/No effect in this scene/.test(node.summary), node.inertInLaunchMode);
    });
    if (!inert) inert = guarded.find(node => node.inertInLaunchMode) ? scene : null;
    if (!active) active = guarded.find(node => !node.inertInLaunchMode) ? scene : null;
    if (inert && active) break;
    if (inspected >= 600) break;
  }
  assert(active, 'no scene exercised a mode-guarded command in its own launch mode');
  const activeProgram = await programOf(active);
  const activeNode = activeProgram.primitives.find(node => Number.isInteger(node.directorModeRequirement) &&
    !node.inertInLaunchMode);
  assert.strictEqual(activeNode.directorModeRequirement, active.launchProfile.directorMode.value);
  const unguarded = activeProgram.primitives.find(node => node.opcode === 0x00);
  assert.strictEqual(unguarded.directorModeRequirement, null);
  assert.strictEqual(unguarded.inertInLaunchMode, false);
  // Persisted node definitions stay lean: the mode facts live on the program only.
  const activeSource = await OB64.cutsceneCodec.loadSceneSource(retail, active, { allowModified: true });
  assert(activeSource.nodeDefinitions.every(node => !('inertInLaunchMode' in node)));
  console.log('cutscene director parser model: ok (154 opcodes verified against ' +
    path.basename(acceptedSource) + '; ' + inspected + ' scenes inspected; inert example ' +
    (inert ? inert.assetId : 'none') + ', active example ' + active.assetId + ')');
})().catch(error => { console.error(error); process.exit(1); });
