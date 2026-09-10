'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');


// Adapted from the frozen R18B public compact-context acceptance probe.
const root=path.resolve(__dirname,'../..'),editor=path.join(root,'editor');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
const loadedSources = [
  'data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js',
  'cutscene-director.js', 'cutscene-codec.js', 'cutscene-runtime.js'
];
const sha256 = value => crypto.createHash('sha256').update(value).digest('hex');
const sourceHashes = {};
for (const file of loadedSources) {
  const source = fs.readFileSync(file === 'cutscene-runtime.js' && process.argv[2] ? path.resolve(process.argv[2]) : path.join(editor,file), 'utf8');
  sourceHashes[file] = sha256(source);
  vm.runInThisContext(source, { filename: file });
}

function actorRow(member) {
  const row = Buffer.alloc(0xF8);
  row.writeUInt32BE(0x100, 0x40);
  row.writeUInt32BE(3, 0x48);
  row[0xF6] = member;
  return row.toString('hex');
}
function actorRecord(slot, rowOrdinal, x) {
  const record = Buffer.alloc(0x150);
  record.writeInt32BE(slot, 0xE4);
  record.writeInt32BE(30, 0xE8);
  record.writeInt32BE(-1, 0xF0);
  record.writeFloatBE(x, 0x11C);
  record.writeInt16BE(12, 0x134);
  record.writeInt16BE(12, 0x138);
  record[0x147] = rowOrdinal;
  return record.toString('hex');
}
function movementRecord() {
  const movement = Buffer.alloc(16);
  movement.writeFloatBE(1, 0);
  movement.writeUInt16BE(4, 12);
  return movement.toString('hex');
}
function actorView(frame) {
  return frame.actors.map(actor => ({
    id: actor.id,
    slot: actor.slot,
    baseX: actor.baseX,
    movementFrame: actor.movementFrame,
    activeMovementId: actor.activeMovementId
  })).sort((left, right) => left.id.localeCompare(right.id));
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
  const scene = catalog.scenes.find(row =>
    row.sceneId === 'scene:director:01f56d8e');
  if (!scene) throw new Error('Context-compatible fixture scene is missing.');
  const sceneSource = await OB64.cutsceneCodec.loadSceneSource(z64, scene);
  const projected = OB64.cutsceneCodec.projectSceneDocument(
    scene, sceneSource, catalog);
  const rows = Array(20).fill('00'.repeat(0xF8));
  rows[2] = actorRow(7);
  rows[3] = actorRow(8);
  const slots = Array(28).fill(null);
  slots[4] = {
    identity: 'context-moving-A',
    recordHex: actorRecord(4, 2, 0),
    movementHex: movementRecord()
  };
  slots[2] = {
    identity: 'context-stationary-B',
    recordHex: actorRecord(2, 3, 100),
    movementHex: null
  };
  function launchInput(includeActors) {
    const input = {
      schema: 'ob64-cutscene-launch-inputs.v1',
      assetId: scene.assetId,
      invocationId: includeActors ? 'cpr-r18b-context-parent-r2' :
        'cpr-r18b-context-child-r2',
      sourceIdentity: 'synthetic-cpr-r18b:public-compact-context',
      evidenceGrade: 'Candidate',
      actorInputRows: { status: 'known', value: rows },
      currentUnitMembers: { status: 'known', value: [7, 8, 255, 255, 255] },
      schedulerBranch: { status: 'known', value: 'normal' }
    };
    if (includeActors) {
      input.existingActors = {
        status: 'known',
        value: { slots, otherJobsEmpty: true }
      };
    }
    return input;
  }
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
    const ir = OB64.cutsceneCodec.createIr(fixtureScene, bytes);
    return OB64.cutsceneRuntime.compile(projected.document, ir.program,
      fixtureScene, catalog, {
        z64,
        maxTicks: 8,
        diagnosticAssumptions: false,
        nativeLaunchInputs,
        ...extra
      });
  }

  const parent = run([
    0, 0x80000000,
    0, 0x80000000,
    0x80000001
  ], launchInput(true));
  const context = OB64.cutsceneRuntime.compactContextRuntime(parent);
  const contextOptions = {
    contextRuntime: context,
    contextTickOffset: 1
  };
  const occupiedSwap = run([
    0xA6, 2, 0,
    0, 0x80000000,
    0x80000001
  ], launchInput(false), contextOptions);
  const vacancyReoccupation = run([
    0xA6, 1, 0,
    0xA6, 4, 1,
    0, 0x80000000,
    0x80000001
  ], launchInput(false), contextOptions);

  const parentActor = parent.states[1].actors.find(actor =>
    actor.id === 'context-moving-A');
  const occupiedActor = occupiedSwap.states[0].actors.find(actor =>
    actor.id === 'context-stationary-B');
  const reoccupyingActor = vacancyReoccupation.states[0].actors.find(actor =>
    actor.id === 'context-stationary-B');
  const checks = {
    contextApplied: !!occupiedSwap.concurrentContext &&
      !!vacancyReoccupation.concurrentContext,
    producerHasActiveSlotJob: !!parentActor && parentActor.slot === 4 &&
      parentActor.activeMovementId === 'launch-movement:4' &&
      parentActor.movementFrame === 1,
    occupiedSwapControl: !!occupiedActor && occupiedActor.slot === 4 &&
      occupiedActor.activeMovementId === 'launch-movement:4' &&
      occupiedActor.movementFrame === 1,
    vacancyReoccupation: !!reoccupyingActor && reoccupyingActor.slot === 4 &&
      reoccupyingActor.activeMovementId === 'launch-movement:4' &&
      reoccupyingActor.movementFrame === 1
  };
  const repeated = run([0xA6,1,0,0xA6,4,1,0xA6,5,1,0xA6,4,0,0x80000001],launchInput(false),contextOptions);
  const repeatedActor=repeated.states[0].actors.find(a=>a.id==='context-moving-A');
  checks.repeatedVacancy=!!repeatedActor&&repeatedActor.slot===4&&repeatedActor.activeMovementId==='launch-movement:4'&&repeatedActor.movementFrame===1;
  const fullContext=run([0xA6,1,0,0xA6,4,1,0x80000001],launchInput(false),{contextRuntime:parent,contextTickOffset:1});
  const fullActor=fullContext.states[0].actors.find(a=>a.id==='context-stationary-B');
  checks.fullContextVacancy=!!fullActor&&fullActor.slot===4&&fullActor.activeMovementId==='launch-movement:4'&&fullActor.movementFrame===1;
  const finishedParent=run([...Array.from({length:5},()=>[0,0x80000000]).flat(),0x80000001],launchInput(true));
  const finishedChild=run([0xA6,1,0,0xA6,4,1,0x80000001],launchInput(false),{contextRuntime:OB64.cutsceneRuntime.compactContextRuntime(finishedParent),contextTickOffset:4});
  const finishedActor=finishedChild.states[0].actors.find(a=>a.id==='context-stationary-B');
  checks.finishedActivityStaysCleared=!!finishedActor&&finishedActor.slot===4&&finishedActor.activeMovementId===null&&finishedActor.movementFrame===4;
  const result = {
    status: Object.values(checks).every(Boolean) ? 'pass' : 'fail',
    command: process.argv,
    node: process.version,

    scriptSha256: sha256(fs.readFileSync(__filename)),
    romV64Sha256: sha256(raw),
    romZ64Sha256: sha256(z64),
    sourceHashes,
    scene: {
      assetId: scene.assetId,
      sceneId: scene.sceneId,
      launchSceneStatePolicy: occupiedSwap.launchSceneStatePolicy
    },
    checks,
    parent: {
      states: parent.states.slice(0, 2).map(frame => ({
        tick: frame.frame,
        actors: actorView(frame)
      })),
      compactFrameCount: context.contextFrames.length,
      compactFirstTwoActorDeltas: context.contextFrames.slice(0, 2)
        .map(frame => frame.actors || [])
    },
    occupiedSwap: {
      outcome: occupiedSwap.outcome,
      concurrentContext: occupiedSwap.concurrentContext,
      missingInputs: occupiedSwap.missingInputs,
      actors: actorView(occupiedSwap.states[0])
    },
    vacancyReoccupation: {
      outcome: vacancyReoccupation.outcome,
      concurrentContext: vacancyReoccupation.concurrentContext,
      missingInputs: vacancyReoccupation.missingInputs,
      actors: actorView(vacancyReoccupation.states[0])
    }
  };
  console.log(JSON.stringify(result,null,2));
  if (result.status !== 'pass') process.exitCode = 1;
})().catch(error => {
  console.error(error && error.stack || error);
  process.exitCode = 1;
});
