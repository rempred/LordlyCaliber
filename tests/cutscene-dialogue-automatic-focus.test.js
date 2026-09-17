'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.resolve(__dirname,'../..'),editor=path.join(root,'editor');
new Function('require','__dirname',fs.readFileSync(path.join(editor,'tests/cutscene-shared-actor-integration.test.js'),'utf8').split('\n(async()=>')[0])(require,path.join(editor,'tests'));
for(const file of ['cutscene-njpg.js','cutscene-dialogue-draw.js','cutscene-captured-scheduler-data.js','cutscene-captured-continuous-data.js','cutscene-captured-scheduler.js'])vm.runInThisContext(fs.readFileSync(path.join(editor,file),'utf8'),{filename:file});
(async()=>{
 const input=require('./fixtures/cutscene-graduation-continuous.json').input;
 const v=fs.readFileSync(path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),z64=new Uint8Array(v.length);
 for(let i=0;i<v.length;i+=2){z64[i]=v[i+1];z64[i+1]=v[i];}
 const rom={z64,archives:OB64.findArchives(z64),layout:{id:'us-rev0'}},ui=OB64.cutsceneUI.initialize(rom),scene=ui.catalog.getScene(input.assetId);
 const source=await OB64.cutsceneCodec.loadSceneSource(z64,scene),p=OB64.cutsceneCodec.projectSceneDocument(scene,source,ui.catalog);
 const words=p.program.primitives.find(n=>n.id==='node:01F3EAD2:w0141').rawWords;
 const drain=g=>{let r;do{r=g.next();}while(!r.done);return r.value;};
 const results={};
 for(const scenario of ['neutral','automatic','manual','automatic-manual-release']){
  const services=structuredClone(input.capturedResume.value.resourceServices),e=new OB64.cutsceneDialogue.Engine(services.initialDialogue,z64);
  OB64.cutsceneDirectorLaunch.installAudioQueue(e,z64);
  const schedule=services.resourceSchedule;schedule.pageAdvancePolicy=scenario.startsWith('automatic')?'automatic':'neutral';
  if(scenario==='manual'||scenario==='automatic-manual-release'){
   const pass=scenario==='manual'?144:145;
   schedule.controller.changes.push({pass,actionMask:0x8000,directionMask:0,historyMask:0x8000,dummyMask:0},{pass:pass+1,actionMask:0,directionMask:0,historyMask:0,dummyMask:0});
  }
  const s=new OB64.cutsceneResourceScheduler.Scheduler(e,schedule,z64),registered=[],deliveries=[];
  // Normal constructors and callbacks produce both completed pages from ROM text.
  for(const id of [1,2]){const w=words.slice();w[1]=id;registered.push(e.lifecycle.create(w,'focus-control-'+id,{x:147,y:123}));}
  let currentPass;const service=e.service;
  e.service=function*(event,...args){if(event.service==='callback')deliveries.push({...event.controller,pass:currentPass,slot:event.slot});return yield* service.call(this,event,...args);};
  const history=[];
  for(let pass=0;pass<=146;pass++){
   currentPass=pass;drain(s.before(pass));
   const prior=registered.map(r=>e.owners[r.slot]?.payload?.[0x3c]??null),focus=e.machine.get(0x800c4c10,2);
   const declared={...s.control};drain(s.after(false));
   const post=registered.map(r=>e.owners[r.slot]?.payload?.[0x3c]??null),pulse=s.trace.find(t=>t.service==='automatic-page-acknowledgement');
   if(pass>=143)history.push({pass,focus,pulse:pulse||null,prior,post,declared});
  }
  results[scenario]={registered,history,deliveries:deliveries.filter(r=>r.pass>=143)};
 }
 const at=(name,pass)=>results[name].history.find(r=>r.pass===pass),masks=(name,pass)=>results[name].deliveries.filter(r=>r.pass===pass).map(r=>[r.slot,r.actionMask,r.historyMask]);
 for(const result of Object.values(results))assert.deepStrictEqual(result.history[0].post,[6,6]);
 assert.deepStrictEqual(at('neutral',146).post,[6,6]);
 assert.equal(at('automatic',144).focus,1);assert.equal(at('automatic',144).pulse.slot,1);
 assert.deepStrictEqual(at('automatic',144).post,[2,6]);
 assert.deepStrictEqual(masks('automatic',144),[[1,0x8000,0x8000],[2,0,0]]);
 assert.equal(at('automatic',144).declared.actionMask,0);assert.equal(at('automatic',144).declared.historyMask,0);
 assert.equal(at('automatic',145).pulse,null);assert.deepStrictEqual(masks('automatic',145),[[1,0,0],[2,0,0]]);
 assert.deepStrictEqual(at('automatic',146).post,[2,6]);
 // A declared manual history pulse keeps the existing dispatcher semantics.
 assert.deepStrictEqual(masks('manual',144),[[1,0x8000,0x8000],[2,0x8000,0x8000]]);
 assert.deepStrictEqual(at('manual',144).post,[2,2]);
 assert.deepStrictEqual(masks('manual',145),[[1,0,0],[2,0,0]]);
 // The generated release interval cannot erase an independently declared A.
 assert.deepStrictEqual(at('automatic-manual-release',144).post,[2,6]);
 assert.equal(at('automatic-manual-release',145).pulse,null);
 assert.deepStrictEqual(masks('automatic-manual-release',145),[[1,0x8000,0x8000],[2,0x8000,0x8000]]);
 assert.deepStrictEqual(at('automatic-manual-release',145).post,[6,2]);
 console.log(JSON.stringify({status:'pass',scope:'Two supported lifecycle constructors; generated owner-only pulse and release; unchanged declared manual dispatch',words,results}));
})().catch(e=>{console.error(e.stack||e);process.exitCode=1;});
