'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const EDITOR = path.resolve(__dirname, '..');
const PARENT = path.resolve(EDITOR, '..');
const MASTER = path.join(PARENT,
  'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const MASTER_SHA256 =
  '6CA0A1AFE224831E202857AD64EF26BD429A034A4EA48404BB09621641A07B12';

global.window = global;
global.module = undefined;
global.btoa = value => Buffer.from(value, 'binary').toString('base64');
global.atob = value => Buffer.from(value, 'base64').toString('binary');
if (!global.crypto?.subtle) {
  Object.defineProperty(global, 'crypto', { value: crypto.webcrypto });
}
const preferences = new Map();
global.localStorage = {
  getItem: key => preferences.has(key) ? preferences.get(key) : null,
  setItem: (key, value) => preferences.set(key, value)
};

vm.runInThisContext('var OB64 = window.OB64 = {};');
const moduleFiles = [
  'data.js',
  'parsers.js',
  'editor-interaction.js',
  'art.js',
  'army-sprites.js',
  'sprite-library.js',
  'animation-corpus-data.js',
  'animation-art.js',
  'combat-animation-overrides-data.js',
  'combat-animation-overrides.js',
  'animation-sequences.js',
  'animation-ui.js',
  'sprite-editor-ui.js', 'art-ui.js', 'army-sprite-ui.js'
];
for (const filename of moduleFiles) {
  let source = fs.readFileSync(path.join(EDITOR, filename), 'utf8');
  vm.runInThisContext(source, { filename });
}

const A = OB64.art;
const L = OB64.spriteLibrary;
const M = OB64.animationArt;
const S = OB64.animationSequences;
const U = OB64.animationUI;

const master = fs.readFileSync(MASTER);
const masterHash = crypto.createHash('sha256').update(master)
  .digest('hex').toUpperCase();
assert.strictEqual(masterHash, MASTER_SHA256);
const z64 = new Uint8Array(master);
for (let index = 0; index < z64.length; index += 2) {
  [z64[index], z64[index + 1]] = [z64[index + 1], z64[index]];
}

async function freshRom() {
  const rom = { z64: z64.slice(), layout: { id: 'us-rev0' } };
  await A.initialize(rom);
  rom.classDefs = OB64.parseClassDefs(rom.z64);
  OB64.combatAnimationOverrides.initialize(rom);
  S.initialize(rom);
  return rom;
}

function giantAdvance(rom) {
  const rows = OB64.spriteEditorUI.animationSources(rom, 77);
  const lanes = new Set(rows.map(row => S.laneFor(row)));
  for (const lane of ['idle', 'advance', 'return', 'hit']) {
    assert(lanes.has(lane), 'Giant source catalog contains ' + lane);
  }
  assert(rows.some(row => row.spriteSourceAssigned));
  const donor = rows.find(row =>
    S.laneFor(row) === 'advance' && U.selectorFlags(row) === '1/1');
  assert(donor, 'Giant player-side advance route exists');
  return { donor, rows };
}

function candidateFor(plan) {
  const bytes = z64.slice();
  const allocations = {};
  let cursor = A.constants.ARENA_START;
  for (const row of plan.relocatedResources) {
    const pad = row.stored.length & 1;
    const allocation = {
      name: row.name,
      stored: row.stored,
      entry: cursor,
      key: cursor - A.constants.RESOURCE_BASE,
      pad,
      end: cursor + 4 + row.stored.length + pad
    };
    assert(allocation.end <= A.constants.ARENA_END);
    allocations[row.name] = allocation;
    cursor = allocation.end;
  }
  S.finalizeAllocations(plan, allocations);
  for (const row of Object.values(allocations)) {
    A.writeU32(bytes, row.entry, row.stored.length);
    bytes.set(row.stored, row.entry + 4);
    if (row.pad) bytes[row.end - 1] = 0;
  }
  S.applyPlan(plan, bytes, [], []);
  return bytes;
}



const { installDom } = require('./helpers/editor-dom');
(async () => {
  const rom = await freshRom(), { donor, rows } = giantAdvance(rom);
  const first = S.separateAndAssign(rom, donor, null, donor);
  const otherDonor=rows.find(row=>S.laneFor(row)==='return' && U.selectorFlags(row)==='1/1');
  const second=S.separateAndAssign(rom,otherDonor,null,otherDonor);
  S.removeFrame(rom,second,second.syntheticAnimation.frames.length-1);
  const before=S.collectProject(rom);
  const ids=[first.id,second.id];
  let plan=U.prepareAlignment(rom,ids,'all',0,{},'',3,-2);
  assert.deepStrictEqual(plan.targets.map(row=>row.frames.length),ids.map(id=>rom.animationSequences.separations[id].syntheticAnimation.frames.length));
  const positions=plan.targets.map(row=>row.animation.frames.map(frame=>frame.layers.map(layer=>[layer.drawOffsetX,layer.drawOffsetY])));
  U.applyAlignment(rom,plan);
  plan.targets.forEach((row,j)=>row.animation.frames.forEach((frame,i)=>frame.layers.forEach((layer,k)=>{
    assert.strictEqual(layer.drawOffsetX,positions[j][i][k][0]+3);assert.strictEqual(layer.drawOffsetY,positions[j][i][k][1]-2);
  })));
  const moved=JSON.stringify(S.collectProject(rom));assert.throws(()=>U.applyAlignment(rom,plan),/changed/);assert.strictEqual(JSON.stringify(S.collectProject(rom)),moved);
  assert.throws(()=>U.prepareAlignment(rom,ids,'advanced',0,{},'999',1,1));assert.strictEqual(JSON.stringify(S.collectProject(rom)),moved);
  plan=U.prepareAlignment(rom,ids,'visible',0,{[first.id]:[0,2],[second.id]:[1]},'',-1,0);assert.deepStrictEqual(plan.targets.map(row=>row.frames),[[0,2],[1]]);
  U.applyAlignment(rom,plan);plan=U.prepareAlignment(rom,ids,'current',0,{},'',0,1);assert(plan.targets.every(row=>row.frames.length===1));U.applyAlignment(rom,plan);
  const nativePlan=S.buildPlan(rom,z64);S.verifyPlan(nativePlan,candidateFor(nativePlan));
  console.log('PASS U7 different-length All/Current/Visible targets, stale/invalid rejection, vector application and native export');
  const dom=installDom(), panel=document.createElement('main');document.body.appendChild(panel);
  const library=L.initialize(rom), animation=first.syntheticAnimation;
  let asset=L.addAsset(library,L.assetFromFrames(library,{name:'Context source',kind:'sequence',width:animation.canvas.width,height:animation.canvas.height,
    anchor:{x:-animation.canvas.originX,y:-animation.canvas.originY},frames:[0,1].map(index=>({name:'Frame '+index,ticks:index+2,layers:[{name:'Layer',rgba:U.framePixels(animation,animation.frames[index],rom.art.animations,null,null,0)}]}))}));
  Object.assign(library.ui,{assetId:asset.id,frameIndex:1,layerIndex:0});
  OB64.artUI.render(panel,rom,{},false);const priorUi=rom.art.ui;
  OB64.artUI.beginLibraryTransfer(rom,{assetId:asset.id,frameIndex:1,layerIndex:0});
  assert.strictEqual(OB64.spriteEditorUI.transferSource(rom,false).asset.id,asset.id);
  let changes=0;
  OB64.artUI.render(panel,rom,{onChange(){changes++;},onAnimationRouteChange(){changes++;}},false);
  const scope=panel.querySelector('[aria-label="Transfer scope"]');assert(scope);assert.strictEqual(scope.value,'sequence');
  const prepare=panel.querySelector('[data-art-focus-key="contextual-transfer-prepare"]');assert(prepare && !prepare.disabled);
  const unchanged=JSON.stringify(S.collectProject(rom)), selection=JSON.stringify({key:rom.art.ui.animationKey,frame:rom.art.ui.animationFrame,layer:rom.art.ui.animationLayer});
  prepare.focus();prepare.click();let modal=document.body.querySelector('[role="dialog"]');assert(modal);assert(modal.textContent.includes('Prepare Sprite Library Frame Sequence'));
  dom.findText(modal,'Cancel').click();assert.strictEqual(JSON.stringify(S.collectProject(rom)),unchanged);assert.strictEqual(changes,0);assert.strictEqual(document.activeElement,prepare);
  assert.strictEqual(JSON.stringify({key:rom.art.ui.animationKey,frame:rom.art.ui.animationFrame,layer:rom.art.ui.animationLayer}),selection);
  prepare.click();modal=document.body.querySelector('[role="dialog"]');dom.findText(modal,'Import Frame Sequence').click();assert.strictEqual(changes,1);
  OB64.artUI.endLibraryTransfer(rom);assert.strictEqual(rom.art.ui,priorUi);assert.strictEqual(library.ui.assetId,asset.id);assert.strictEqual(library.ui.frameIndex,1);
  OB64.artUI.beginLibraryTransfer(rom,{assetId:asset.id,frameIndex:1,layerIndex:0});L.renameAsset(library,asset.id,'Changed source');assert.throws(()=>OB64.spriteEditorUI.transferSource(rom,false),/changed/);OB64.artUI.endLibraryTransfer(rom);
  console.log('PASS U5 actual contextual target selection, guarded sequence preparation, cancel/apply, return context and stale-source rejection');
  const current=first.syntheticAnimation, frame=current.frames[0], layer=frame.layers[0];
  const canvas=document.createElement('canvas');document.body.appendChild(canvas);
  const ui={animationTool:'pencil',animationBrushSize:1,animationPaletteIndex:2,animationIntensity:15,animationWeaponChild:0};
  const source=current.artByKey[layer.sourceKey], child=layer.selectedChildOrdinal||0;
  U.installEditing(canvas,rom.art,rom,first,current,frame,layer,ui,{onChange(){changes++;}},()=>{});
  canvas.dispatch('keydown',{key:'ArrowRight'});canvas.dispatch('keydown',{key:' '});assert.strictEqual(M.currentEdit(rom.art.animations,layer.sourceKey,child).indices[1],2);
  canvas.dispatch('keydown',{key:'ArrowDown',shiftKey:true});assert(ui.animationPixelSelection);canvas.dispatch('keydown',{key:'c',ctrlKey:true});assert(ui.animationPixelClipboard);
  console.log('PASS U4 native canvas keyboard paint, rectangular selection and clipboard');
  for(const subtab of ['avatars','icons','army']) {
    OB64.artUI.beginLibraryTransfer(rom,{assetId:asset.id,frameIndex:1,layerIndex:0});rom.art.ui.subtab=subtab;
    const nativeBefore=JSON.stringify(A.collectProjectPayload(rom));
    const opts={onChange(){changes++;}};OB64.artUI.render(panel,rom,opts,false);
    const button=panel.querySelector('[data-art-focus-key="contextual-transfer-prepare"]');assert(button && !button.disabled,subtab+' destination available');
    button.focus();button.click();let dialog=document.body.querySelector('[role="dialog"]');assert(dialog,subtab+' opens preparation');dom.tick(0);
    const cancel=dialog.querySelectorAll('button').find(row=>row.textContent==='Cancel');assert(cancel);cancel.click();
    assert.strictEqual(JSON.stringify(A.collectProjectPayload(rom)),nativeBefore,subtab+' cancel preserves native content');
    assert.strictEqual(document.activeElement,button,subtab+' restores launcher');
    button.click();dialog=document.body.querySelector('[role="dialog"]');dom.tick(1);
    const apply=dialog.querySelectorAll('button').find(row=>/Apply Converted Avatar|Apply to Selected Item Icon|Import Sprite|Apply Converted Sprite|Apply Army Sprite/.test(row.textContent));
    assert(apply,subtab+' apply action: '+dialog.querySelectorAll('button').map(row=>row.textContent).join('|'));assert(!apply.disabled,subtab+' conversion ready');apply.click();
    assert(!document.body.querySelector('[role="dialog"]'),subtab+' applies and closes');
    OB64.artUI.endLibraryTransfer(rom);
  }
  console.log('PASS U5 avatar/icon/Army contextual preparation, cancellation, focus restoration and apply');
  rom.art.ui.subtab='animations';rom.art.ui.animationAlignmentOpen=true;OB64.artUI.render(panel,rom,{},true);
  let comparison=panel.querySelector('[data-art-focus-key="animation-compare"]');assert(comparison.options.length>1);comparison.value=comparison.options[1].value;comparison.dispatch('change');
  assert.strictEqual(panel.querySelectorAll('.animation-sequence-preview').length,3,'main preview plus two shared-origin comparisons');
  const plays=panel.querySelectorAll('button').filter(row=>row.textContent==='Play');assert.strictEqual(plays.length,1,'comparison uses the main transport');
  plays[0].click();assert.strictEqual(dom.scheduled.size,1,'actual comparisons share one scheduled timeline');dom.tick(10);dom.tick(80);
  OB64.artUI.render(panel,rom,{},true);assert(dom.scheduled.size<=1,'comparison rerender has no duplicate work');OB64.editorInteraction.stopAll();assert.strictEqual(dom.scheduled.size,0);
  console.log('PASS U2 actual combat comparisons share controls/timing and release rerendered work');


})().catch(error=>{console.error(error);process.exitCode=1;});
