'use strict';

// Short-site stubs are safe for every retail short launch site. Ported from
// Astra's read-only audit (runs/cutscene-editor-readiness-sapphireibis-20260927/
// event-trampoline-audit.cjs) onto the product rewrite, OB64.cutsceneEvents.boundBytes.
//
// It reuses the generator's static event VM analyzer (declarations only, with
// one enqueue hook that records control-flow edges) and checks, for all 402
// short sites:
//   1. no analyzed edge enters the site's second instruction, and no sequence
//      starts at the launch, so replacing both instructions is safe;
//   2. the route that resumes after the site never reads register 0 before
//      overwriting it (the stub leaves its own value there);
//   3. with every short site redirected through a stub that keeps its retail
//      selector, every modeled launch context keeps exact parity, and the
//      rewritten rows analyze without caps or unresolved long jumps;
//   4. redirecting every short site to selector 319 changes nothing except
//      the known abstract refinement of the preceding selector to 319.

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const { createRequire } = require('module');
const editor = path.resolve(__dirname, '..');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
for (const file of ['cutscene-model.js', 'cutscene-codec.js', 'cutscene-event-rows-data.js', 'cutscene-events.js']) {
  vm.runInThisContext(fs.readFileSync(path.join(editor, file), 'utf8'), { filename: file });
}

// Generator declarations with an edge-recording hook, as in the audit.
const generatorPath = path.join(editor, 'tools/generate-cutscene-data.js');
let code = fs.readFileSync(generatorPath, 'utf8').replace(/\r\n/g, '\n');
const mainAt = code.lastIndexOf('\ntry {\n  main();');
assert(mainAt > 0, 'the generator declarations can be isolated');
code = code.slice(0, mainAt);
function edit(needle, replacement) {
  assert.strictEqual(code.split(needle).length, 2, 'instrumentation anchor is unique: ' + needle);
  code = code.replace(needle, replacement);
}
edit('  let unknownLongJumps = 0;\n  function enqueue(state) {',
  '  let unknownLongJumps = 0;\n  let auditSource = -1; const auditEdges = [];\n  function enqueue(state) {\n' +
  '    if (auditSource >= 0) auditEdges.push({ from: auditSource, to: state.pc });');
edit('    const state = queue.shift();\n    const opcode = decoded[state.pc];',
  '    const state = queue.shift();\n    auditSource = state.pc;\n    const opcode = decoded[state.pc];');
edit('  return {\n    contextsByOffset,', '  return {\n    auditEdges,\n    contextsByOffset,');
code += '\nglobalThis.auditApi = { readEventSequenceInventory, analyzeEventSequenceLaunches, decodeCustomLz, eventConditionalBranchResult };';
const context = { require: createRequire(generatorPath), __dirname: path.dirname(generatorPath), Buffer,
  process: { argv: [], cwd: () => editor }, console };
vm.createContext(context);
vm.runInContext(code, context, { filename: generatorPath });
const api = context.auditApi;

const raw = fs.readFileSync(path.join(require('./project-paths.cjs').PARENT, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const z64 = Uint8Array.from(raw, (_, index) => raw[index ^ 1]);
const pinned = OB64.cutsceneEventRows;
const E = OB64.cutsceneEvents;

// The resume route must not read register 0 before overwriting it. Rules are
// the audit's, from the event interpreter (z64 46EFC..47174, 473F4..474B0).
function resumeRisks(decoded, edges, resume) {
  const visited = new Set(), queue = [resume], risks = [];
  while (queue.length) {
    const cursor = queue.shift();
    if (visited.has(cursor)) continue;
    visited.add(cursor);
    const op = decoded[cursor], arg = decoded[cursor + 1];
    if (op === 0x68) continue; // loads register 0 without reading it
    const getter = (op >= 0x60 && op <= 0x67) || (op >= 0x78 && op <= 0x7f);
    const safeGetter = getter && (arg >= 0xe5 || (arg & 7) !== 0);
    if (safeGetter && (op & 7) === 0) continue;
    if (op === 0x12 || op === 0x2f) continue;
    const condition = op >= 0xa1 ? api.eventConditionalBranchResult(op, Array(8).fill(null)) : undefined;
    const low = op & 15;
    const conditionDoesNotRead0 = condition !== undefined && ((low >= 12 && (op & 3) !== 0) ||
      ([1, 2, 3, 6, 7, 11].includes(low) && (op & 3) !== 0 && ((op & 12) >> 2) !== 0));
    const arithmeticDoesNotRead0 = op >= 0x90 && op <= 0x9f && (op & 7) !== 0 && (op >= 0x98 || (arg & 7) !== 0);
    if (op === 0x01 || op === 0x13 || op === 0x1b || safeGetter || conditionDoesNotRead0 || arithmeticDoesNotRead0 ||
        (op >= 0x69 && op <= 0x6f)) {
      const targets = [...new Set(edges.filter(edge => edge.from === cursor).map(edge => edge.to))];
      if (!targets.length) risks.push({ cursor, op, reason: 'no analyzed successor' });
      queue.push(...targets);
      continue;
    }
    risks.push({ cursor, op, reason: 'requires native register-use proof' });
  }
  return risks;
}

let shortSites = 0, parityContexts = 0, refinedContexts = 0;
const failures = [];
for (let row = 0; row < pinned.rowCount; row++) {
  const entry = pinned.rows[row];
  if (!entry || !entry.shortSites.length) continue;
  const prefix = pinned.resourceBaseZ64 + entry.key;
  const decoded = api.decodeCustomLz(Buffer.from(z64.slice(prefix + 4, prefix + 4 + entry.storedLength)), 'row ' + row).bytes;
  const inventory = api.readEventSequenceInventory(decoded, row, 'row ' + row);
  // All launch sites of the row, long and short, are analysis targets.
  const allSites = [];
  for (let offset = 4; offset + 1 < decoded.length; offset += 2) {
    if (decoded[offset] === 0x10 && decoded[offset + 1] === 0x00 &&
        (decoded[offset - 2] === 0x68 || (decoded[offset - 4] === 0x68 && decoded[offset - 2] === 0x88))) allSites.push(offset);
  }
  const edges = [], originals = [];
  for (const sequence of inventory.sequences) {
    const analysis = api.analyzeEventSequenceLaunches(decoded, sequence.cursor, new Set(allSites), row);
    edges.push(...analysis.auditEdges);
    originals.push({ sequence, analysis });
  }
  const bindingsFor = selectorOf => entry.shortSites.map(at => ({
    launchSiteId: 'event-director:' + row + ':b' + at.toString(16).toUpperCase().padStart(4, '0'),
    offset: at, form: 'short', retailSelector: decoded[at - 1], selector: selectorOf(decoded[at - 1])
  }));
  for (const at of entry.shortSites) {
    shortSites++;
    const incoming = edges.filter(edge => edge.to === at && edge.from !== at - 2);
    if (incoming.length) failures.push({ row, at, reason: 'edge into the second instruction' });
    if (inventory.sequences.some(sequence => sequence.cursor === at)) failures.push({ row, at, reason: 'sequence starts at the launch' });
    const risks = resumeRisks(decoded, edges, at + 2);
    if (risks.length) failures.push({ row, at, reason: 'resume route', risks });
  }
  // Parity through the product rewrite. Stub i sits at retail length + 10 i;
  // its launch byte is stub + 4 and its return cursor is stub + 6.
  function parity(selectorOf, allowRefinement) {
    const bindings = bindingsFor(selectorOf);
    const patched = Buffer.from(E.boundBytes(Uint8Array.from(decoded), bindings));
    const ordered = bindings.slice().sort((left, right) => left.launchSiteId < right.launchSiteId ? -1 : 1);
    const launchOldToNew = new Map(), launchNewToOld = new Map(), cursorNewToOld = new Map();
    ordered.forEach((binding, index) => {
      const stub = decoded.length + index * E.stubBytes;
      launchOldToNew.set(binding.offset, stub + 4);
      launchNewToOld.set(stub + 4, binding.offset);
      cursorNewToOld.set(stub + 6, binding.offset + 2);
    });
    const targets = new Set(allSites.map(at => launchOldToNew.get(at) ?? at));
    const normalize = (contexts, launchOffset, isPatched) => Array.from(contexts, item => {
      const copy = JSON.parse(JSON.stringify(item));
      if (isPatched) {
        for (const key of ['eventInvocationCursor', 'precedingDirectorInvocationCursor']) copy[key] = cursorNewToOld.get(copy[key]) ?? copy[key];
        copy.precedingDirectorLaunchOffset = launchNewToOld.get(copy.precedingDirectorLaunchOffset) ?? copy.precedingDirectorLaunchOffset;
        copy.eventInvocationOffset = launchOffset - copy.eventInvocationCursor;
      }
      if (allowRefinement) delete copy.precedingDirectorSelector;
      return JSON.stringify(copy);
    }).sort();
    for (const { sequence, analysis } of originals) {
      const next = api.analyzeEventSequenceLaunches(patched, sequence.cursor, targets, row);
      if (next.capped || next.unknownLongJumps) failures.push({ row, entry: sequence.cursor, reason: 'rewritten row does not analyze cleanly' });
      if (next.contextsByOffset.size !== analysis.contextsByOffset.size) failures.push({ row, entry: sequence.cursor, reason: 'launch reachability changed' });
      for (const [at, contexts] of analysis.contextsByOffset) {
        const got = next.contextsByOffset.get(launchOldToNew.get(at) ?? at) || [];
        const before = normalize(contexts, at, false), after = normalize(got, at, true);
        if (allowRefinement) refinedContexts += contexts.length; else parityContexts += contexts.length;
        if (JSON.stringify(before) !== JSON.stringify(after)) {
          failures.push({ row, entry: sequence.cursor, launch: at, reason: allowRefinement ? 'more than the selector refinement' : 'context parity' });
        }
      }
    }
  }
  parity(selector => selector, false);
  parity(() => 319, true);
}

assert.strictEqual(shortSites, 402, 'every retail short site is audited');
assert.deepStrictEqual(failures, [], JSON.stringify(failures.slice(0, 5)));
assert(parityContexts >= 978, 'the modeled launch contexts are all compared: ' + parityContexts);
console.log('PASS short-site stub audit: ' + shortSites + ' sites, no entry into a rewritten instruction, no register-0 read on any resume route, ' +
  parityContexts + ' launch contexts with exact parity through the product rewrite, and rebinding to 319 changes only the preceding selector.');
