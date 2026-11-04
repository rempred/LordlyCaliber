'use strict';
const assert = require('node:assert/strict');
const {labelRegisters} = require('../tools/event-register-labels.cjs');
const {createNameResolver} = require('../tools/event-reference-names.cjs');
const catalog = require('../game/director/event-semantics.json');
const line = (row, at) => catalog.rows[row].steps.find(s => s[0] === at)[1];

// Same R1, different purposes in the actual Capitrium mission.
assert.match(line(28, 318), /R1 \(flag to check\) = 201/);
assert.match(line(28, 320), /R0 \(flag value: ON\/OFF\)/);
assert.match(line(28, 388), /R1 \(hour\)/);
assert.doesNotMatch(line(28, 388), /flag to check/);
assert.match(line(28, 394), /R2 \(minute\)/);
assert.match(line(28, 336), /R1 \(flag value: ON\/OFF\) = 1/);
assert.match(line(28, 396), /R1 \(time: HHMM\)/);
assert.match(line(10, 164), /R0 \(recruitment template\) = 15/);
assert.match(line(10, 164), /527: Katreda/);
assert.match(line(62, 588), /R2 \(character slot\)/);
assert.match(line(62, 590), /R1 \(last character slot\)/);
assert.match(line(89, 114), /R0 \(Goth required\)/);
assert.match(line(89, 72), /R1 \(contact target number\)/);
assert.match(line(89, 120), /R7 \(consumable ID\)/);
assert.match(line(44, 298), /R7 \(squad index\)/);
assert.match(line(44, 298), /R1 \(formation position\)/);
assert.match(line(44, 298), /R6 \(squad member\)/);
assert.doesNotMatch(line(44, 298), /R6 \(acting squad\)/);
assert.match(line(44, 106), /R0 \(calendar year\)\/R1 \(calendar month\)\/R2 \(calendar day\)/);
assert.match(line(4, 270), /R1 \(acting squad\)/);
assert.match(line(4, 854), /R3 \(caller R3, restored after helper @0x2FE\)/);
assert.match(line(4, 272), /R2 \(bound for acting squad\)/);
assert.match(line(4, 278), /R2 \(bound for squad member position\)/);
assert.match(line(4, 632), /R2 \(bound for army character slot\)/);
assert.match(line(1, 206), /R7 \(consumable ID\)/);

// Scope rules must select the approved object, not lose it or join ID namespaces.
const entries = [
  {kind:'scene',id:1,scope:'global',name:'Opening'},
  {kind:'local-code',id:90,scope:'row:44',name:'Identity predicate'},
  {kind:'dialogue-choice',id:200,scope:'selector:12',name:'Accept'},
  {kind:'scenario-squad',id:30,scope:'scenario:42',name:'Boss squad'}
];
const resolve = createNameResolver(entries,[{row:44,scenarioKeys:[42]},{row:45,scenarioKeys:[43,44]}]);
assert.equal(resolve({kind:'cutscene',id:1,scope:'row:1:offset:50'}),entries[0]);
assert.equal(resolve({kind:'local-predicate',entry:90,row:44}),entries[1]);
assert.equal(resolve({kind:'dialogue-choice',selector:12,wordOffset:200}),entries[2]);
assert.equal(resolve({kind:'opposing-squad',id:30,scope:'row:44'}),entries[3]);
assert.equal(resolve({kind:'opposing-squad',id:30,scope:'row:45'}),undefined);
assert.equal(resolve({kind:'identity-class',id:30,scope:'scenario:42'}),undefined);

// Index and result are different uses even when the getter overwrites itself.
let i = {offset:0, word:0x6060, registers:[]};
assert.equal(labelRegisters('Read persistent story flag at R0 into R0', i, [i], 0),
  'Read persistent story flag at R0 (flag to check) into R0 (flag value: ON/OFF)');
i = {offset:0, word:0x6391, registers:[{register:'R3', role:'dialogue answer'}]};
assert.equal(labelRegisters('Read dialogue answer into R3', i, [i], 0),
  'Read dialogue answer into R3 (dialogue answer)');
i = {offset:0, word:0x6903, registers:[{register:'R1', role:'dialogue answer control'}]};
assert.equal(labelRegisters('Prepare R1 = 3', i, [i], 0), 'Prepare R1 (dialogue control) = 3');

// Do not invent an identity from an untyped value or follow a use across a branch.
i = {offset:0, word:0x6861, registers:[]};
assert.equal(labelRegisters('Prepare R0 = 97', i, [i], 0), 'Prepare R0 = 97');
i = {offset:0, word:0x69c9, registers:[{register:'R1', role:'persistent flag ID'}]};
assert.equal(labelRegisters('Prepare R1 = 201', i,
  [i, {offset:2, word:0x0100}, {offset:4, word:0x6061}], 0), 'Prepare R1 (flag number) = 201');
assert.equal(labelRegisters('Prepare R1 = 201', i,
  [i, {offset:2, word:0x7061}], 0), 'Prepare R1 (flag to change) = 201');
assert.equal(labelRegisters('Prepare R1 = 201', i,
  [i, {offset:2, word:0x7161}], 0), 'Prepare R1 (flag number and value) = 201');
console.log('PASS: contextual register roles, register reuse, same-register input/output and unknown values');

// Portable target-frame and source-expression checks do not need parent
// research files; they remain active in the standalone native checkout.
const portableJump = {offset:0,word:0x0100,registers:[]};
const portableTarget = {offset:16,word:0x6903,registers:[]};
assert.equal(labelRegisters('Continue @0010: Prepare R1 = 3', portableJump, [portableJump,portableTarget], 0, [], {
  roles:{R1:'wrong current-frame role'},quoted:['R1'],
  quotedTarget:{offset:16,word:portableTarget.word,roles:{R1:'dialogue control'},input:{},output:{R1:'dialogue control'}}
}), 'Continue @0010: Prepare R1 (dialogue control) = 3');
assert.match(labelRegisters('If army character status at R1 = 0', {offset:0,word:0xac00,registers:[]}, [],0),
  /R1 \(army character slot at source read\)/);

// Compact rows omit inputRegisters. Fresh VM contexts must recover actual
// operand domains without choosing one incoming literal at a control-flow join.
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const fs = require('node:fs');
if (fs.existsSync(path.join(root, 'tools/event-semantics/register-closure-audit.cjs'))) {
const {analyzeProgram} = require(path.join(root, 'tools/event-semantics/vm.cjs'));
const {deriveRegisterRoles} = require(path.join(root, 'tools/event-semantics/register-closure-audit.cjs'));
const spec = require(path.join(root, 'docs/event-semantics/vm-semantics.json'));
function closureRow(id) {
  const row = require(path.join(root, `docs/event-semantics/rows/row-${id}.json`));
  const corpus = require(path.join(root, `runs/all-event-semantics-20261103/corpus/rows/row-${id}.json`));
  const analysis = analyzeProgram(corpus, spec);
  return {row, analysis, closure:deriveRegisterRoles(row, analysis, spec, corpus)};
}
const cf = closureRow(22);
assert.equal(cf.closure.steps[244].roles.R0, 'Chaos Frame');
assert.doesNotMatch(cf.closure.steps[244].roles.R0, /100/);
const squad = closureRow(48);
assert.equal(squad.closure.steps[1318].roles.R0, 'saved squad member ID');
assert.equal(squad.closure.steps[1318].roles.R6, 'saved squad member ID');
assert.doesNotMatch(squad.closure.steps[1318].roles.R0, /acting squad/);
const preserved = closureRow(82);
assert.match(preserved.closure.steps[868].roles.R2, /caller R2, preserved through helper/);
assert.doesNotMatch(preserved.closure.steps[868].roles.R2, /member count|formation limit/);
i = {offset:0, word:0x6900, registers:[]};
assert.equal(labelRegisters('Prepare R1 = 0', i, [i], 0, [], {roles:{R1:'birthday guest speech entry'},quoted:[]}),
  'Prepare R1 (birthday guest speech entry) = 0');
i = {offset:0, word:0x0100, registers:[]};
assert.equal(labelRegisters('Continue toward decoded 0x10: Prepare R1 = 3', i, [i], 0, [], {roles:{R1:'wrong current-frame role'},quoted:['R1']}),
  'Continue toward decoded 0x10: Prepare R1 = 3');
// A quoted target has its own incoming frame. Qualify the exact referenced
// instruction; the jump's current R1 must never supply the displayed role.
const target = {offset:16,word:0x6903,registers:[]};
assert.equal(labelRegisters('Continue toward decoded 0x10: Prepare R1 = 3', i, [i,target], 0, [], {
  roles:{R1:'wrong current-frame role'},quoted:['R1'],
  quotedTarget:{offset:16,word:target.word,roles:{R1:'dialogue control'},input:{},output:{R1:'dialogue control'}}
}), 'Continue toward decoded 0x10: Prepare R1 (dialogue control) = 3');
const birthday = closureRow(67);
assert.match(birthday.closure.steps[9402].roles.R1, /caller R1, saved for helper/);
assert.doesNotMatch(birthday.closure.steps[9402].roles.R1, /calendar|birthday age/);
const bounds = closureRow(4);
assert.equal(bounds.closure.steps[272].roles.R2, 'bound for acting squad');
assert.equal(bounds.closure.steps[278].roles.R2, 'bound for squad member position');
assert.equal(bounds.closure.steps[632].roles.R2, 'bound for army character slot');
console.log('PASS: compact-schema closure, merged Chaos Frame values, member IDs, preserved callers and quoted targets');
} else {
  console.log('SKIP: parent research derivation cases (standalone editor checkout); portable catalog/helper cases remain active');
}
