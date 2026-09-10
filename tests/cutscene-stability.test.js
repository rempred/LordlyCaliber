'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const crypto = require('crypto');
const editor = path.resolve(__dirname, '..');
const root = path.dirname(editor);
const hash = value => crypto.createHash('sha256').update(value).digest('hex');
global.window = global;
vm.runInThisContext('var OB64 = window.OB64 = {};');
const files = ['data.js', 'art.js', 'animation-corpus-data.js', 'animation-art.js',
  'cutscene-data.js', 'cutscene-model.js', 'cutscene-catalog.js', 'cutscene-director.js',
  'cutscene-codec.js', 'cutscene-runtime.js', 'cutscene-preview.js', 'cutscene-assets.js',
  'cutscene-renderer.js', 'cutscene-project.js', 'cutscene-export.js', 'cutscene-ui.js'];
const sourceHashes = {};
for (const file of files) {
  const source = fs.readFileSync(path.join(editor, file), 'utf8');
  sourceHashes[file] = hash(source);
  vm.runInThisContext(source, {filename:file});
}
const generatorSha256 = hash(fs.readFileSync(__filename));
const raw = fs.readFileSync(path.join(root, 'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
const z64 = new Uint8Array(raw.length);
for (let i=0;i<raw.length;i+=2) { z64[i]=raw[i+1]; z64[i+1]=raw[i]; }
const catalog = OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData);
async function project(id) {
  const scene = catalog.getScene(id);
  const source = await OB64.cutsceneCodec.loadSceneSource(z64, scene);
  return {scene, ...OB64.cutsceneCodec.projectSceneDocument(scene, source, catalog)};
}
function node(tag, cls, text) {
  return {tag, cls, text, children:[], style:{}, attrs:{}, classList:{toggle(){}},
    appendChild(child){this.children.push(child);}, setAttribute(k,v){this.attrs[k]=v;}};
}
function texts(n) { return [n.text || '', ...n.children.map(texts)].join(' '); }
(async()=>{
  if (process.argv.includes('--audit')) {
    const rom={z64,layout:{id:'us-rev0'}};
    const ui=OB64.cutsceneUI.ensureState(rom);
    const counts={},rows=[];
    const started=performance.now();
    for (const scene of catalog.directorScenes) {
      ui.selectedSceneId=scene.sceneId;
      await OB64.cutsceneUI.loadScene(rom,ui,scene);
      const rt=ui.runtimeByAssetId[scene.assetId];
      const errors=Object.assign({},ui.sourceErrors);
      if (!rt) throw Error(scene.assetId+': '+JSON.stringify(errors));
      assert(rt.retainedStateBytes<=rt.limits.maxStateBytes);
      assert(rt.trace.length<=rt.limits.maxTraceEntries);
      counts[rt.outcome]=(counts[rt.outcome]||0)+1;
      rows.push({assetId:scene.assetId,outcome:rt.outcome,states:rt.states.length,
        retainedStateBytes:rt.retainedStateBytes,traceEntries:rt.trace.length,
        missingInputs:rt.missingInputs.length,unsupportedCommands:rt.unsupportedCommands,
        concurrentContext:rt.concurrentContext});
      OB64.cutsceneUI.resetAll(ui);
      if(rows.length%100===0)process.stderr.write('Strict UI audit '+rows.length+'/'+catalog.directorScenes.length+'\n');
    }
    console.log(JSON.stringify({status:'pass',policy:'actual UI default strict input policy with default native launch context',
      limitation:'Modeled endings do not prove retail completion or rendering.',command:process.argv,node:process.version,
      generatorSha256,romV64Sha256:hash(raw),romZ64Sha256:hash(z64),
      sourceHashes,
      resources:rows.length,ms:performance.now()-started,counts,rows},null,2));
    return;
  }
  const continuation = await project('rom-director:01F450D0');
  const opts = {z64, maxTicks:60, diagnosticAssumptions:true};
  const sync = OB64.cutsceneRuntime.compile(continuation.document,continuation.program,continuation.scene,catalog,opts);
  const asyncResult = await OB64.cutsceneRuntime.compileAsync(continuation.document,continuation.program,continuation.scene,catalog,opts);
  assert.deepStrictEqual(asyncResult.states, sync.states);
  const child = sync.trace.find(e=>e.kind==='composite' && e.streamAssetId!==continuation.scene.assetId);
  assert(child && sync.programsByAssetId[child.streamAssetId].compositeById[child.compositeId]);
  const uiSource = fs.readFileSync(path.join(editor,'cutscene-ui.js'),'utf8');
  const slice = (name,next)=>uiSource.slice(uiSource.indexOf('  function '+name+'('),uiSource.indexOf('  function '+next+'('));
  const view = {};
  const sandbox = {OB64,node,button:(t,c,f)=>Object.assign(node('button',c,t),{click:f}),
    viewFor:()=>view,directorEditorTarget:()=>null,rerender(){},
    capabilityBadge:()=>node('span','','capability')};
  vm.createContext(sandbox);
  vm.runInContext(slice('directorSourceSelection','directorEditorTarget')+
    slice('renderDirectorSourceDetails','renderDirectorSource')+
    slice('renderDirectorRuntime','renderTimeline'),sandbox);
  const state={catalog,ui:{}};
  const first=node('section');
  sandbox.renderDirectorRuntime(first,{},state,continuation.scene,continuation.document,continuation.program,sync);
  const rows=sync.trace.filter(e=>e.kind==='composite');
  view.selectedSourceId=child.streamAssetId+':'+child.compositeId+':'+rows.indexOf(child);
  const selected=node('section');
  sandbox.renderDirectorRuntime(selected,{},state,continuation.scene,continuation.document,continuation.program,sync);
  assert(texts(selected).includes(child.streamAssetId),'actual inspector identifies continuation ownership');
  const owner=sync.programsByAssetId[child.streamAssetId];
  assert(texts(selected).includes(owner.primitiveById[owner.compositeById[child.compositeId].nodeIds[0]].label));
  const scrubA=OB64.cutsceneRuntime.evaluate(asyncResult,51);
  OB64.cutsceneRuntime.evaluate(asyncResult,2).actors.push({id:'test'});
  assert.deepStrictEqual(OB64.cutsceneRuntime.evaluate(asyncResult,51),scrubA);

  const unknownQuery={id:'fixture:query',name:'unmodeled_fixture_query',label:'Unresolved fixture input',
    operands:[],rawWords:[0x80000000,1,0],query:{compareMode:1,target:0}};
  const trailer={id:'fixture:terminal',name:'terminal',operands:[],rawWords:[0x80000001]};
  const envelope={id:'fixture:envelope',kind:'query-envelope',nodeIds:[unknownQuery.id,trailer.id],label:'Fixture wait'};
  const fixtureProgram={primitives:[unknownQuery,trailer],composites:[envelope],
    primitiveById:{[unknownQuery.id]:unknownQuery,[trailer.id]:trailer},
    compositeById:{[envelope.id]:envelope},compositeByNodeId:{[unknownQuery.id]:envelope.id,[trailer.id]:envelope.id}};
  const unresolvedWait=await OB64.cutsceneRuntime.compileAsync(continuation.document,fixtureProgram,continuation.scene,catalog,{z64});
  assert.strictEqual(unresolvedWait.outcome,'external-input');
  assert.strictEqual(unresolvedWait.terminated,false,'an unknown not-equal query must not pass through NaN comparison');
  assert(!unresolvedWait.executedNodeIds.includes(trailer.id));

  const expensive=await project('rom-director:01F3F70E');
  if(global.gc)global.gc();
  const before=process.memoryUsage().heapUsed;
  let pulses=0,maxGap=0,last=performance.now();
  const timer=setInterval(()=>{const now=performance.now();maxGap=Math.max(maxGap,now-last);last=now;pulses++;},1);
  const start=performance.now();
  const runtime=await OB64.cutsceneRuntime.compileAsync(expensive.document,expensive.program,expensive.scene,catalog,{z64,diagnosticAssumptions:true});
  const ms=performance.now()-start;
  clearInterval(timer);
  if(global.gc)global.gc();
  const heapDelta=process.memoryUsage().heapUsed-before;
  assert(pulses>3,'event-loop timers progress during actual expensive compilation');
  assert(runtime.safetyLimited);
  assert(runtime.retainedStateBytes<=runtime.limits.maxStateBytes);
  assert(runtime.trace.length<=runtime.limits.maxTraceEntries);
  assert(runtime.states.length<=runtime.limits.maxTicks);
  const cancellation=new AbortController();
  setTimeout(()=>cancellation.abort(),20);
  await assert.rejects(OB64.cutsceneRuntime.compileAsync(expensive.document,expensive.program,expensive.scene,catalog,
    {z64,diagnosticAssumptions:true,signal:cancellation.signal}),{name:'AbortError'});
  const strict=await OB64.cutsceneRuntime.compileAsync(expensive.document,expensive.program,expensive.scene,catalog,{z64});
  assert.strictEqual(strict.outcome,'shared-pose-control-18');
  assert.strictEqual(strict.terminated,false);
  assert(strict.missingInputs.some(value=>value.includes('shared-pose-control-18')),
    'strict Actor execution stops at the earlier unsupported shared control before reaching the external query');

  const rom={z64,layout:{id:'us-rev0'}};
  const ui=OB64.cutsceneUI.ensureState(rom);
  ui.diagnosticAssumptions=true;
  ui.selectedSceneId=expensive.scene.sceneId;
  const obsolete=OB64.cutsceneUI.loadScene(rom,ui,expensive.scene);
  await new Promise(r=>setTimeout(r,20));
  const oldController=ui.runtimeController;
  ui.selectedSceneId=continuation.scene.sceneId;
  await OB64.cutsceneUI.loadScene(rom,ui,continuation.scene);
  await obsolete;
  assert(oldController.signal.aborted);
  assert.deepStrictEqual(Object.keys(ui.runtimeByAssetId),[continuation.scene.assetId]);
  const oldDoc=ui.histories[continuation.scene.storageId].present;
  const graduation=catalog.getScene('loading-magnus-walk-opening-ceremony-cutscene');
  ui.selectedSceneId=graduation.sceneId;
  await OB64.cutsceneUI.loadScene(rom,ui,graduation);
  assert.deepStrictEqual(Object.keys(ui.runtimeByAssetId),[graduation.assetId]);
  assert.strictEqual(OB64.cutsceneRuntime.forDocument(oldDoc),null);
  assert(Object.keys(ui.concurrentRuntimeByLaunchContext).length<=1);
  const resetRom={z64,layout:{id:'us-rev0'}};
  const resetState=OB64.cutsceneUI.ensureState(resetRom);
  const pendingReset=OB64.cutsceneUI.loadScene(resetRom,resetState,expensive.scene);
  OB64.cutsceneUI.resetAll(resetState);
  await pendingReset;
  assert.deepStrictEqual(Object.keys(resetState.histories),[]);
  assert.deepStrictEqual(Object.keys(resetState.runtimeByAssetId),[]);
  assert.deepStrictEqual(Object.keys(resetState.sourceErrors),[]);
  const capped = Object.assign({},sync,{trace:Array.from({length:5000},()=>rows[0])});
  const boundedTimeline=node('section');
  sandbox.renderDirectorRuntime(boundedTimeline,{},state,continuation.scene,continuation.document,continuation.program,capped);
  function countButtons(n){return (n.tag==='button'?1:0)+n.children.reduce((sum,c)=>sum+countButtons(c),0);}
  assert(countButtons(boundedTimeline)<=1000);
  console.log(JSON.stringify({status:'pass',command:process.argv,node:process.version,
    generatorSha256,romV64Sha256:hash(raw),romZ64Sha256:hash(z64),
    sourceHashes,
    continuation:{assetId:continuation.scene.assetId,childStream:child.streamAssetId,rows:rows.length,actualTimelineAndInspector:true},
    expensive:{assetId:expensive.scene.assetId,ms,pulses,maxTimerGapMs:maxGap,heapDelta,states:runtime.states.length,
      retainedStateBytes:runtime.retainedStateBytes,traceEntries:runtime.trace.length,traceCount:runtime.traceCount,outcome:runtime.outcome},
    strict:{outcome:strict.outcome,query:strict.unresolvedQuery},cancellation:true,cacheEviction:true,scrubbing:true},null,2));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
