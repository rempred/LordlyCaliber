#!/usr/bin/env node
'use strict';
// Presentation data only. Rebuild from the reviewed parent research; portable
// builds consume the checked-in result and do not require that research tree.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {labelRegisters} = require('./event-register-labels.cjs');
const {createNameResolver} = require('./event-reference-names.cjs');
const parent = path.resolve(process.argv[2] || path.join(__dirname, '../../..'));
const read = name => JSON.parse(fs.readFileSync(path.join(parent, name), 'utf8'));
const catalog = read('docs/event-semantics/catalog.json');
if (catalog.status !== 'reconciled-retail-event-corpus' ||
    catalog.coverage.unfinishedInstructions || catalog.coverage.unresolvedRowQuestions)
  throw Error('Event semantics must pass reconciliation before importing');
const resolveName = createNameResolver(catalog.entries,
  read('docs/event-semantics/object-index.json').catalog.eventScenarioBindings);
const vm = read('docs/event-semantics/vm-semantics.json');
const requestClosure = read('docs/Plans/task-logs/event-request-closure-20261103.json');
if (requestClosure.requests.length !== 256) throw Error('Incomplete native request closure');
const requests = Object.fromEntries(requestClosure.requests.map(proof => {
  const original = vm.requests[proof.request];
  if (proof.status !== 'verified-native-behavior' || !proof.evidence?.length ||
      Number(proof.code) !== original?.code) throw Error(`Unverified request ${proof.request}`);
  // Only explicit writes are meaningful. Seeded subtype fields that the mapper
  // leaves alone are not defaults to be applied to the game.
  return [proof.request, {...proof, code:Number(proof.code), status:'verified',
    name:proof.name.replace(/Director mode/g, 'cutscene mode')}];
}));
const pickerLabels = read('tools/event-semantics/flag-picker-labels.json');
const scenarioBindings = read('docs/event-semantics/object-index.json').catalog.eventScenarioBindings;
const {analyzeProgram} = require(path.join(parent, 'tools/event-semantics/vm.cjs'));
const {deriveRegisterRoles} = require(path.join(parent, 'tools/event-semantics/register-closure-audit.cjs'));
function clean(text, refs = []) {
  text = text.replace(/post-battle intermediate UI/g, requests[27].name);
  for (const ref of refs) {
    const entry = resolveName(ref);
    if (!entry || entry.status !== 'verified') throw Error(`Unresolved ${ref.kind} reference`);
    const old = new Set([ref.name, entry.proposedName, ...(entry.aliases || []),
      ...(entry.proposals || []).map(x => x.name)]);
    const alternatives = [...new Set([entry.preferredName, ...old])]
      .filter(x => typeof x === 'string' && x.length > 5).sort((a,b) => b.length-a.length);
    if (alternatives.length) {
      const escaped = alternatives.map(x => x.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'));
      // A single pass protects an already-canonical name that contains an alias.
      text = text.replace(new RegExp(escaped.join('|'), 'g'), () => entry.preferredName);
    }
  }
  // Remove repeated trace citations, not instruction behavior. Evidence stays
  // in the research documents, outside the editing surface.
  text = text.replace(/; consumer 0x[0-9a-f]+/gi, '')
    .replace(/; native deferred operation consumer/g, '')
    .replace(/\bDirector selector\b/g, 'cutscene')
    .replace(/\bDirector\s*(?=\d)/g, 'cutscene ')
    .replace(/\b(into|in|from|result)(R[0-7])\b/g, '$1 $2')
    .replace(/\b(scenario|squad|template|operand|result|index|with|from|value|argument|bit|class|identity|at|into|the|following)(?=\d|R[0-7]\b)/g, '$1 ')
    .replace(/\btruncated16 bits\b/g, 'truncated to 16 bits')
    .replace(/\b(\d+)(Goth|OFF|ON)\b/g, '$1 $2');
  return [...new Set(text.split('; ').map(x => x.trim()).filter(Boolean))].join('; ');
}
const flags = {}, rows = {};
for (const entry of catalog.entries)
  if (entry.kind === 'flag' && entry.scope === 'global' && entry.status === 'verified')
    flags[entry.id] = entry.preferredName;
let instructionCount = 0;
for (const info of catalog.rows) {
  if (!info.instructions) continue;
  const row = read(`docs/event-semantics/rows/row-${info.row}.json`);
  const corpus = read(`runs/all-event-semantics-20261103/corpus/rows/row-${info.row}.json`);
  const bytes = Buffer.from(corpus.hex, 'hex');
  const digest = crypto.createHash('sha256').update(bytes).digest('hex');
  if (digest !== row.source.decodedSha256.toLowerCase() || bytes.length !== row.source.byteLength)
    throw Error(`Source identity changed for row ${row.row}`);
  const rowFlags = {};
  for (const entry of catalog.entries)
    if (entry.kind === 'flag' && entry.scope === `row:${row.row}` && entry.status === 'verified')
      rowFlags[entry.id] = entry.preferredName;
  for(const [context,flag,name] of pickerLabels.labels)if(context===row.row){
    if(rowFlags[flag]||flags[flag])throw Error(`Picker label would replace an existing flag meaning: ${context}/${flag}`);
    rowFlags[flag]=name;
  }
  const handlers = [...new Map(row.handlers.map(h => [h.entry, {
    entry:h.entry, group:h.group, title:clean(h.title,(h.cutscenes || []).map(id=>({kind:'scene',id,scope:'global'})))
  }])).values()].sort((a,b) => a.entry-b.entry);
  const instructions = [...row.instructions].sort((a,b) => a.offset-b.offset);
  // Older reviewed rows omit inputRegisters. Recover facts from their exact
  // authenticated program, including local calls and the native value stack.
  const analysis = analyzeProgram(corpus, vm);
  const closure = deriveRegisterRoles(row, analysis, vm, corpus);
  const decoded = new Map(analysis.instructions.map(i => [i.offset,i]));
  const native = instructions.map(i => {
    const value = decoded.get(i.offset);
    if (!value || value.word !== i.word) throw Error(`Missing register analysis row ${row.row} @${i.offset}`);
    return value;
  });
  const steps = instructions.map((i, index) => {
    if (i.status !== 'verified' || i.unresolved?.length || !i.summary || bytes.readUInt16BE(i.offset) !== i.word)
      throw Error(`Unverified instruction row ${row.row} @${i.offset}`);
    const scoped = {};
    for (const ref of i.references || []) {
      const entry = resolveName(ref);
      if (entry?.kind === 'flag' && entry.status === 'verified') scoped[entry.id] = entry.preferredName;
    }
    // Store the original word and typed constant identity as well as prose.
    // Editing uses instruction boundaries, never guesses data words as code.
    const opcode=i.word>>>8, operand=i.word&255;
    const combined = opcode >= 0x68 && opcode <= 0x6f &&
      bytes[i.offset+2] === opcode+0x20 ? operand | (bytes[i.offset+3]<<8) : operand;
    const candidates = (i.references || []).filter(ref => Number(ref.id) === combined &&
      !['local-code','native-command','property','event-scenario-binding'].includes(ref.kind));
    const ref = candidates.length === 1 && opcode >= 0x68 && opcode <= 0x6f &&
      !/value (ON|OFF)|comparison value/.test((i.semanticRoles || []).join(' ')) ? candidates[0] : null;
    return [i.offset, labelRegisters(clean(i.summary, i.references), i, instructions, index, native, closure.steps[i.offset]), scoped,
      i.word, ref ? {kind:ref.kind, scope:ref.scope || 'global'} : {}];
  }).sort((a,b) => a[0]-b[0]);
  const inventory = new Set(steps.map(x => x[0]));
  const structure = [];
  for (let at=0; at<bytes.length; at+=2)
    if (!inventory.has(at)) structure.push(at, bytes.readUInt16BE(at));
  rows[row.row] = {title:row.title, sha256:digest, byteLength:bytes.length,
    flags:rowFlags, handlers, steps, structure,
    edges:instructions.flatMap(i => (i.successors || []).filter(s => Number.isInteger(s.offset) &&
      s.offset !== i.offset+2 && inventory.has(s.offset) && (s.row === undefined || s.row === row.row))
      .map(s => [i.offset,s.offset])),
    scenarioKeys:scenarioBindings.find(b => b.row === row.row)?.scenarioKeys || []};
  instructionCount += steps.length;
}
if (Object.keys(rows).length !== 113 || instructionCount !== 55471)
  throw Error('Unexpected corpus coverage');
const opcodes = Object.fromEntries(Object.values(vm.opcodes).map(op => [op.opcode, op.name]));
const out = {format:'lordly-event-semantics', version:1,
  generator:'native/tools/import-event-semantics.cjs',
  inputs:['docs/event-semantics/catalog.json', 'docs/event-semantics/rows/row-*.json',
    'docs/event-semantics/object-index.json', 'docs/event-semantics/vm-semantics.json', 'tools/event-semantics/vm.cjs', 'tools/event-semantics/flag-picker-labels.json',
    'tools/event-semantics/register-closure-audit.cjs', 'docs/Plans/task-logs/event-request-closure-20261103.json'],
  flags, opcodes, rows,
  names:catalog.entries.filter(e => e.status === 'verified' && Number.isInteger(e.id))
    .map(e => [e.kind,e.scope,e.id,e.preferredName]),
  banks:vm.banks, properties:vm.properties, requests,
  answers:catalog.entries.filter(e => e.kind === 'dialogue-choice' && e.status === 'verified')
    .flatMap(e => e.evidence.filter(x => x.answers).flatMap(x => x.answers.map(a =>
      [Number(e.scope.replace('selector:','')),a.controlSlot,a.returnValue,a.text])))};
const destination = path.resolve(__dirname, '../game/director/event-semantics.json');
fs.writeFileSync(destination, JSON.stringify(out) + '\n');
console.log(`Imported ${Object.keys(rows).length} event programs / ${instructionCount} instructions`);
