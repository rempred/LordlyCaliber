'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');
// RF-R18-F1/F2 fixtures adapted from the frozen R18 public-runtime acceptance probes.

const root = path.resolve(__dirname, '../..');
const editor = path.join(root, 'editor');
const sha256 = value => crypto.createHash('sha256').update(value).digest('hex');

global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
const loadedSources = [
  'data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js',
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-runtime.js'
];
const sourceHashes = {};
for (const file of loadedSources) {
  const source = fs.readFileSync(file === 'cutscene-runtime.js' && process.argv[2] ? path.resolve(process.argv[2]) : path.join(editor, file), 'utf8');
  sourceHashes[file] = sha256(source);
  vm.runInThisContext(source, { filename: file });
}

function actorRow({ art = 3, member = 7, flags = 0x100 } = {}) {
  const row = Buffer.alloc(0xF8);
  row.writeUInt32BE(flags >>> 0, 0x40);
  row.writeUInt32BE(art >>> 0, 0x48);
  row[0xF6] = member;
  return row.toString('hex');
}

function actorRecord({ slot, rowOrdinal, bank = 30, stateIndex = 12, x = 0, z = 0 }) {
  const record = Buffer.alloc(0x150);
  record.writeFloatBE(x, 0x11C);
  record.writeFloatBE(z, 0x124);
  record.writeInt16BE(stateIndex, 0x134);
  record.writeInt32BE(slot, 0xE4);
  record.writeInt32BE(bank, 0xE8);
  record.writeInt32BE(-1, 0xF0);
  record[0x147] = rowOrdinal;
  return record.toString('hex');
}

function movementRecord({ vx = 1, vz = 0, remaining = 4, pause = 0 } = {}) {
  const movement = Buffer.alloc(16);
  movement.writeFloatBE(vx, 0);
  movement.writeFloatBE(vz, 8);
  movement.writeUInt16BE(remaining, 12);
  movement[14] = pause;
  return movement.toString('hex');
}

function launchInput(scene, { rows, slots, invocationId }) {
  return {
    schema: 'ob64-cutscene-launch-inputs.v1',
    assetId: scene.assetId,
    invocationId,
    sourceIdentity: 'synthetic-independent-review:accepted-contract-composition',
    evidenceGrade: 'Candidate',
    actorInputRows: { status: 'known', value: rows },
    existingActors: { status: 'known', value: { slots, otherJobsEmpty: true } },
    currentUnitMembers: { status: 'known', value: [7, 255, 255, 255, 255] },
    schedulerBranch: { status: 'known', value: 'normal' }
  };
}

(async () => {
  const raw = fs.readFileSync(path.join(root,
    'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
  const z64 = new Uint8Array(raw.length);
  for (let index = 0; index < raw.length; index += 2) {
    z64[index] = raw[index + 1];
    z64[index + 1] = raw[index];
  }

  const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
  const scene = catalog.directorScenes.find(item => item.friendlyName === 'Graduation Ceremony');
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene);
  const projected = OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog);

  function run(words, nativeLaunchInputs, extra = {}) {
    const bytes = new Uint8Array(words.length * 4);
    const view = new DataView(bytes.buffer);
    words.forEach((word, index) => view.setUint32(index * 4, word >>> 0));
    const fixtureScene = {
      ...scene,
      source: {
        dynamicGrammar: true,
        terminalWithoutTrailer: true,
        decodedLength: bytes.length,
        decodedWordCount: words.length
      }
    };
    if (extra.sceneMode !== undefined) {
      fixtureScene.launchProfile = {
        ...scene.launchProfile,
        directorMode: { ...scene.launchProfile.directorMode, value: extra.sceneMode }
      };
    }
    const ir = OB64.cutsceneCodec.createIr(fixtureScene, bytes);
    return OB64.cutsceneRuntime.compile(projected.document, ir.program, fixtureScene,
      catalog, { z64, maxTicks: 8, diagnosticAssumptions: false,
        nativeLaunchInputs, ...extra });
  }

  const emptyRows = Array(20).fill('00'.repeat(0xF8));
  const emptySlots = Array(28).fill(null);
  const emptyInput = launchInput(scene, {
    rows: emptyRows,
    slots: emptySlots,
    invocationId: 'review-known-empty-move'
  });
  const emptyMove = run([
    0x07, 5, -1000, -1000, 1000, 0, 10, 0,
    0x80000001
  ], emptyInput);

  const bindingRows = Array(20).fill('00'.repeat(0xF8));
  bindingRows[2] = actorRow();
  const bindingSlots = Array(28).fill(null);
  bindingSlots[4] = {
    identity: 'moving-A',
    recordHex: actorRecord({ slot: 4, rowOrdinal: 2, x: 0 }),
    movementHex: movementRecord({ vx: 1, remaining: 4 })
  };
  bindingSlots[1] = {
    identity: 'stationary-B',
    recordHex: actorRecord({ slot: 1, rowOrdinal: 9, x: 100 }),
    movementHex: null
  };
  const bindingInput = launchInput(scene, {
    rows: bindingRows,
    slots: bindingSlots,
    invocationId: 'review-slot-owned-binding-movement'
  });
  const binding = run([
    0xA6, 1, 0,
    0, 0x80000000,
    0, 0x80000000,
    0x80000001
  ], bindingInput);
  const bindingStates = binding.states.slice(0, 3).map(frame => ({
    tick: frame.frame,
    actors: frame.actors.map(actor => ({
      id: actor.id,
      slot: actor.slot,
      baseX: actor.baseX,
      movementFrame: actor.movementFrame,
      activeMovementId: actor.activeMovementId
    })).sort((left, right) => left.id.localeCompare(right.id))
  }));

  const finalizerRows = Array(20).fill('00'.repeat(0xF8));
  const finalizerRow = Buffer.alloc(0xF8);
  finalizerRow.writeUInt32BE(0x300, 0x40);
  finalizerRows[2] = finalizerRow.toString('hex');
  const finalizerSlots = Array(28).fill(null);
  const finalizerA = Buffer.from(actorRecord({
    slot: 0, rowOrdinal: 2, x: 0
  }), 'hex');
  finalizerA.writeInt16BE(10, 0x138);
  const finalizerB = Buffer.from(actorRecord({
    slot: 1, rowOrdinal: 2, x: 100
  }), 'hex');
  finalizerB.writeInt16BE(10, 0x138);
  finalizerSlots[0] = {
    identity: 'finalizer-A',
    recordHex: finalizerA.toString('hex'),
    movementHex: movementRecord({ vx: 2, remaining: 4 })
  };
  finalizerSlots[1] = {
    identity: 'finalizer-B',
    recordHex: finalizerB.toString('hex'),
    movementHex: null
  };
  const finalizerInput = launchInput(scene, {
    rows: finalizerRows,
    slots: finalizerSlots,
    invocationId: 'review-current-slot-finalizer-movement'
  });
  const finalizer = run([
    0x45, 0, 0,
    0, 0x80000000,
    0, 0x80000000,
    0x80000001
  ], finalizerInput);
  const finalizerStates = finalizer.states.slice(0, 3).map(frame => ({
    tick: frame.frame,
    actors: frame.actors.map(actor => ({
      id: actor.id,
      slot: actor.slot,
      baseX: actor.baseX,
      movementFrame: actor.movementFrame,
      activeMovementId: actor.activeMovementId
    })).sort((left, right) => left.id.localeCompare(right.id))
  }));

  const assert = require('assert');
  const checks=[];
  function check(name,test){try{test();checks.push({name,pass:true});}catch(error){checks.push({name,pass:false,message:error.message});}}
  check('RF-R18-F1 known empty Move returns',()=>assert.equal(emptyMove.outcome,'modeled-termination'));
  const unknown = JSON.parse(JSON.stringify(emptyInput)); delete unknown.existingActors;
  check('unknown occupancy keeps boundary',()=>assert.equal(run([0x07,5,-1000,-1000,1000,0,10,0,0x80000001],unknown).outcome,'actor-input'));
  check('known empty State keeps boundary',()=>assert.equal(run([0x03,5,30,12,0,-1000,-1000,-1000,0,0x80000001],emptyInput).outcome,'actor-input'));
  for(const [name,result,sourceSlot,velocity,a,b] of [['binding',binding,4,1,'moving-A','stationary-B'],['finalization',finalizer,0,2,'finalizer-A','finalizer-B']]){
    check('RF-R18-F2 '+name+' publishes current occupant before and after update',()=>{
      for(const [index,frame] of result.states.slice(0,3).entries()){
        const former=frame.actors.find(actor=>actor.id===a),current=frame.actors.find(actor=>actor.id===b);
        assert.equal(former.activeMovementId,null);assert.equal(current.activeMovementId,'launch-movement:'+sourceSlot);
        assert.equal(current.slot,sourceSlot);assert.equal(former.baseX,0);assert.equal(current.baseX,100+index*velocity);
      }
    });
  }
  const createdInput=JSON.parse(JSON.stringify(bindingInput));createdInput.existingActors.value.slots[4].movementHex=null;
  const waits=Array.from({length:5},()=>[0,0x80000000]).flat();
  const created=run([0x07,4,-1000,-1000,4000,0,4,0,0xA6,1,0,...waits,0x80000001],createdInput);
  check('runtime-created job identity and lifetime stay with slot after binding',()=>{
    const first=created.states[0].actors.find(a=>a.id==='stationary-B');assert.match(first.activeMovementId,/^runtime-movement:/);
    for(let i=0;i<=4;i++){
      const current=created.states[i].actors.find(a=>a.id==='stationary-B'),former=created.states[i].actors.find(a=>a.id==='moving-A');
      assert.equal(current.baseX,100+i);assert.equal(former.baseX,0);assert.equal(former.activeMovementId,null);
      assert.equal(current.activeMovementId,i<4?first.activeMovementId:null);assert.equal(current.movementFrame,i);
    }
  });
  const vacantInput=JSON.parse(JSON.stringify(bindingInput));vacantInput.actorInputRows.value[9]=actorRow({member:8});vacantInput.currentUnitMembers.value[1]=8;
  const reoccupied=run([0xA6,2,0,0xA6,4,1,...waits,0x80000001],vacantInput);
  check('job survives same-update vacancy and publishes reoccupied slot',()=>{
    const current=reoccupied.states[0].actors.find(a=>a.id==='stationary-B');assert.equal(current.slot,4);assert.equal(current.activeMovementId,'launch-movement:4');
    assert.equal(reoccupied.states[1].actors.find(a=>a.id==='stationary-B').baseX,101);
  });
  console.log(JSON.stringify({status:checks.every(c=>c.pass)?'pass':'fail',node:process.version,sourceHashes,checks,bindingStates,finalizerStates},null,2));
  if(checks.some(c=>!c.pass))process.exitCode=1;
})().catch(error=>{console.error(error);process.exitCode=1;});