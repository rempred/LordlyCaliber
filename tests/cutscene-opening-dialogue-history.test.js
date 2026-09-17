'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert'),root=path.resolve(__dirname,'../..');
new Function('require','__dirname',fs.readFileSync(path.join(__dirname,'cutscene-shared-actor-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,__dirname);
(async()=>{
 const v=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(v.length);
 for(let i=0;i<v.length;i+=2){z64[i]=v[i+1];z64[i+1]=v[i];}
 const fixture=require('./fixtures/cutscene-opening-dialogue-history.json'),drawer=new OB64.cutsceneDialogueDraw(z64),rows=[];
 const check=(c,ds)=>{
  assert.equal(c.error,null);assert.equal(ds.recordHex,c.recordHex,c.label+': record');assert.equal(ds.payloadHex,c.payloadHex,c.label+': payload');
  const result=drawer.compose(ds.recordHex,ds.payloadHex);assert.equal(result.commands.length,c.commands.length,c.label);
  result.commands.forEach((a,i)=>{const b=c.commands[i];for(const k of ['bounds','uv','step','scissor'])assert.deepStrictEqual(a[k],b[k],c.label+':'+i+':'+k);
   assert.equal(Buffer.from(a.texture.indices).toString('hex'),b.indicesHex);a.texture.palette.forEach((color,j)=>assert.deepStrictEqual(color,OB64.art.rgba5551(b.palette[j])));});
  assert.equal(result.nativeServiceBytes,0);assert(result.artworkCacheBytes<=16384);
  rows.push({label:c.label,historyGate:Buffer.from(ds.payloadHex,'hex')[0x4f],rectangles:result.commands.length});
 };
 for(const history of [false,true]){
  const input=structuredClone(require('../../docs/reviews/cutscene-room-background-drawing-20260917/playback-input.json'));
  if(history){const changes=input.externalProducers.value.resourceSchedule.controller.changes;
   changes.push({pass:335,actionMask:0,directionMask:2048,historyMask:16,dummyMask:0},{pass:340,actionMask:0,directionMask:0,historyMask:0,dummyMask:0});changes.sort((a,b)=>a.pass-b.pass);}
  const catalog=OB64.cutsceneCatalog.createCatalog(OB64.cutsceneData),scene=catalog.getScene(input.assetId),source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,catalog);
  const run=OB64.cutsceneRuntime.compile(p.document,p.program,scene,catalog,{z64,nativeLaunchInputs:input,diagnosticAssumptions:false,maxTicks:347});
  assert.equal(run.states.length,347);
  const prefix=history?'history-route335.json:':'history-route-control.json:';
  for(const c of fixture.cases.filter(c=>c.label.startsWith(prefix))){const pass=Number(c.label.split(':').pop());
   const drawing=run.states[pass].dialogue.map(d=>d.payload.nativeDialogue.drawingState).find(ds=>ds.recordHex===c.recordHex);assert(drawing,c.label);check(c,drawing);}
 }
 assert.equal(rows.length,5);assert.deepStrictEqual(rows.filter(r=>r.historyGate).map(r=>r.rectangles),[66,73,51]);
 assert.deepStrictEqual(rows.filter(r=>!r.historyGate).map(r=>r.rectangles),[52,52]);
 const base=fixture.cases.find(c=>c.flags['4f']);for(const historyArrow of [1,128]){const p=Buffer.from(base.payloadHex,'hex');p[0x50]=historyArrow;assert.throws(()=>drawer.compose(base.recordHex,p.toString('hex')),/unsupported/);}
 console.log(JSON.stringify({status:'pass',rows,rectangles:rows.reduce((n,r)=>n+r.rectangles,0),historyArrowGuard:true,artworkCacheBytes:drawer.assetBytes}));
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
