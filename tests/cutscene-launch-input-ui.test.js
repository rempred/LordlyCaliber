'use strict';
const assert=require('assert');
const fs=require('fs');
const path=require('path');
const vm=require('vm');
const crypto=require('crypto');
const source=fs.readFileSync(path.join(__dirname,'../cutscene-ui.js'),'utf8');
const start=source.indexOf("    if (scene.engine === 'director') {\n      inspector.appendChild(node('h3', '', 'Actor launch snapshot'));");
const end=source.indexOf('    if (eventContextChoices.length)',start);
assert(start>=0 && end>start);
function node(){return {children:[],listeners:{},appendChild(n){this.children.push(n);},
  setAttribute(){},addEventListener(k,f){this.listeners[k]=f;}};}
(async()=>{
const scene={engine:'director',assetId:'fixture',sceneId:'scene'};
const state={selectedSceneId:'scene',projectionEpoch:0,launchImportRequest:0,
  nativeLaunchInputsByAssetId:{},callbacks:{onStatus(s){state.error=s;}}};
const inspector=node();let published=0,loads=0,renders=0;const view={frame:90};
const sandbox={scene,state,inspector,document:{},rom:{},node,
  field:(label,input)=>input,button:(label,cls,handler)=>({label,handler}),
  setNativeLaunchInputs(s,sc,input){published++;s.launchImportRequest++;
    if(input)s.nativeLaunchInputsByAssetId[sc.assetId]=input;else delete s.nativeLaunchInputsByAssetId[sc.assetId];},
  async loadScene(){loads++;},viewFor:()=>view,rerender(){renders++;},
  OB64:{cutscenePreview:{sceneDurationFrames:()=>4}}};
vm.runInNewContext(source.slice(start,end),sandbox);
const control=inspector.children.find(n=>n.type==='file');assert(control);
let finish;control.files=[{size:20,text:()=>new Promise(r=>{finish=r;})}];
const pending=control.listeners.change();
state.launchImportRequest++;
finish('{"sourceIdentity":"stale"}');await pending;
assert.strictEqual(published,0,'a newer import/clear prevents the old file read from publishing');
control.files=[{size:20,text:async()=>'{"sourceIdentity":"current"}'}];
await control.listeners.change();
assert.strictEqual(published,1);assert.strictEqual(loads,1);assert.strictEqual(renders,1);assert.strictEqual(view.frame,3);
control.files=[{size:20,text:async()=>'{invalid'}];await control.listeners.change();
assert.strictEqual(published,1);assert(state.error);
control.files=[{size:131073,text:async()=>{throw Error('must not read');}}];await control.listeners.change();
assert.strictEqual(state.error,'Launch snapshot exceeds 128 KiB.');
let release;control.files=[{size:20,text:()=>new Promise(r=>{release=r;})}];
const resetPending=control.listeners.change();state.projectionEpoch++;release('{}');await resetPending;
assert.strictEqual(published,1,'Project reset invalidates a pending file read');
console.log(JSON.stringify({status:'pass',sourceSha256:crypto.createHash('sha256').update(source).digest('hex'),
  checks:['stale-file-read','valid-input-before-reload','retained-frame-clamp','parse-error-preserves-input','size-bound','project-reset-cancellation'],
  boundary:'Actual inspector handler with deterministic DOM substitutes; browser layout and file chooser interaction remain untested.'},null,2));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
