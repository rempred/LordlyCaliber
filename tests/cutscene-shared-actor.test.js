'use strict';
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),R=path.resolve(__dirname,'../..'),fixture=require('./fixtures/cutscene-shared-actor.json');global.window=global;global.OB64={};
for(const f of ['cutscene-model.js','cutscene-runtime.js','cutscene-dialogue-data.js','cutscene-dialogue.js','cutscene-resource-scheduler-data.js','cutscene-resource-scheduler.js','cutscene-image-echo-data.js','cutscene-image-echo.js','cutscene-shared-actor-data.js','cutscene-shared-actor.js'])vm.runInThisContext(fs.readFileSync(path.join(R,'editor',f),'utf8'),{filename:f});
const v=fs.readFileSync(path.join(R,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64')),rom=new Uint8Array(v.length);for(let i=0;i<v.length;i+=2){rom[i]=v[i+1];rom[i+1]=v[i];}
function camera(a){return {evidenceStatus:'declared-native-test',fovYDegrees:a[0],aspect:a[1],near:a[2],far:a[3],eye:{x:a[5],y:a[6],z:a[7]},target:{x:a[8],y:a[9],z:a[10]},up:{x:a[11],y:a[12],z:a[13]}};}
let requests=0;
for(const row of fixture.cases){
 const echo=new OB64.cutsceneImageEcho(rom),service=new OB64.cutsceneSharedActor(echo,{kind:'native-ordinary-actor-v1',worldScale:Math.fround(.1)},rom),b=new DataView(new ArrayBuffer(336));
 [row.scale,row.scale,1].forEach((x,i)=>b.setFloat32(0x104+i*4,x));row.position.forEach((x,i)=>b.setFloat32(0x11c+i*4,x));b.setFloat32(0x130,1);
 const cameras={actor:camera(row.camera),registered:camera([38,4/3,1,5000,1,0,0,400,0,0,0,0,1,0])},channels=Array.from({length:20},()=>({rotationX:0,rotationY:0,translateX:0,translateY:0,translateZ:0,uniformScale:1}));
 echo.machine.put(0x8022a730,0x3e000000);
 b.setUint8(0x13f,row.fields.facing);b.setUint8(0x145,row.fields.heightMode);[row.fields.secondaryY,row.fields.yaw,row.fields.uniformScale].forEach((v,i)=>b.setFloat32(0x128+i*4,v));['translateX','translateY','rotationX','rotationY','translateZ','uniformScale'].forEach((k,i)=>channels[0][k]=row.fields.channel[i]);
 const result=service.prepare([{slot:0,bytes:new Uint8Array(b.buffer)}],cameras,channels)[0].bytes;
 assert.equal(Buffer.from(result.slice(0xa0,0xe0)).toString('hex'),row.matrixHex,row.name+' matrix');
 assert.equal(echo.machine.get(0x8022a730),0x3e000000);
 const out=service.project(result,cameras);assert.equal(out.outputHex,row.outputHex,row.name+' projection');assert.equal(Buffer.from(out.projectionInput).toString('hex'),row.inputHex);
 for(const r of row.requests){
  const selected=OB64.cutsceneRuntime.nativeExternal.selectSharedRequest(r.opcode,[r.key>>>8,r.key&255],0,out,o=>rom[o]*256+rom[o+1]);
  assert.equal(selected.request,(r.opcode===17||r.opcode===18)?r.A:r.B);requests++;
 }
 // Requests use the same qualified queue code as the resource engine.
 echo.machine.regions.push({address:0x800eb8f0,bytes:new Uint8Array(128),writable:true});
 for(const q of row.queue){echo.invoke(0x800ea604,[q.context,q.request]);assert.equal(Buffer.from(echo.read(0x800eb8f0,128)).toString('hex'),q.hex);}
 const before=Buffer.from(result);const second=service.prepare([{slot:27,bytes:new Uint8Array(b.buffer)}],cameras,channels)[0].bytes;assert.deepStrictEqual(second,result);second[0xa0]^=255;assert.deepStrictEqual(Buffer.from(result),before);
 assert.throws(()=>service.prepare([{slot:28,bytes:new Uint8Array(b.buffer)}],cameras,channels));
 const alternate=result.slice();alternate[0x13d]=1;assert.throws(()=>service.project(alternate,cameras));
 assert.equal(OB64.cutsceneRuntime.nativeExternal.selectSharedRequest(18,[0,1],0,{projectionInput:Uint8Array.of(1,...new Array(11).fill(0))},()=>0).boundary,'shared-pose-projection-input');
 // All byte alignments preserve untouched portions of unaligned loads/stores.
 const m=echo.machine,a=0x80382e80;for(let k=0;k<4;k++){
  [0x11,0x22,0x33,0x44].forEach((x,i)=>m.put(a+i,x,1));m.r[4]=a;m.r[5]=0xaabbccdd;m.code[0x806f0000]=(34<<26)|(4<<21)|(5<<16)|k;m.step(0x806f0000);
  const expected=[0xaa,0xbb,0xcc,0xdd];for(let j=k;j<4;j++)expected[j-k]=[0x11,0x22,0x33,0x44][j];assert.equal(m.r[5],new DataView(Uint8Array.from(expected).buffer).getUint32(0));
  m.r[5]=0xaabbccdd;m.code[0x806f0000]=(38<<26)|(4<<21)|(5<<16)|k;m.step(0x806f0000);const right=[0xaa,0xbb,0xcc,0xdd];for(let j=0;j<=k;j++)right[3-k+j]=[0x11,0x22,0x33,0x44][j];assert.equal(m.r[5],new DataView(Uint8Array.from(right).buffer).getUint32(0));
  m.r[5]=0xaabbccdd;m.code[0x806f0000]=(46<<26)|(4<<21)|(5<<16)|k;m.step(0x806f0000);for(let j=0;j<4;j++)assert.equal(m.get(a+j,1),j<=k?[0xaa,0xbb,0xcc,0xdd][3-k+j]:[0x11,0x22,0x33,0x44][j]);
 }
}
// Paging preserves every writable byte and keeps caller-owned snapshots separate.
const m=new OB64.cutsceneDialogue.Machine({},[{address:0x80400000,bytes:new Uint8Array(700).map((_,i)=>i&255),writable:true}]),engine={machine:m,owners:[]},snap=OB64.cutsceneDialogue.Engine.prototype.snapshot;
const changed=rom.slice();changed[0x2a8a70]=1;assert.throws(()=>new OB64.cutsceneSharedActor(new OB64.cutsceneImageEcho(changed),{kind:'native-ordinary-actor-v1',worldScale:.1},changed),e=>e.code==='shared-actor-image');
const full=snap.call(engine),paged=snap.call(engine,256);assert.equal(paged.memory.map(r=>r.hex).join(''),full.memory[0].hex);assert.deepStrictEqual(paged.memory.map(r=>r.address),[0x80400000,0x80400100,0x80400200]);m.put(0x80400123,255,1);assert.notEqual(snap.call(engine,256).memory[1].hex,paged.memory[1].hex);assert.equal(paged.memory.map(r=>r.hex).join(''),full.memory[0].hex);assert.throws(()=>snap.call(engine,128));
const code=Object.fromEntries(OB64.cutsceneDialogueWords.map(r=>[r[0],r[2]]));
for(const q of fixture.priority){
 const machine=new OB64.cutsceneDialogue.Machine(code,[{address:0x800e82c8,bytes:new Uint8Array(1008),writable:true},{address:0x800c49d0,bytes:Uint8Array.of(0,1),writable:true},{address:0x800c4c10,bytes:Uint8Array.of(0,0),writable:true},{address:0x807fe000,bytes:new Uint8Array(8192),writable:true}]);
 [1,8,3,8,0,5].forEach((p,i)=>{machine.put(0x800e82c8+168*i,0xa000,2);machine.put(0x800e82c8+168*i+14,p,2);});
 new OB64.cutsceneResourceScheduler.Scheduler({machine,lifecycle:true},{kind:'resident-resource-pass-v1',directorSlot:0,directorCallback:0x80225abc,helperOutcomes:[],controller:{throughPass:0,changes:[{pass:0,actionMask:0,directionMask:0,historyMask:0,dummyMask:0}]}},rom);
 for(let i=2;i<12;i++)machine.put(0x800c4c10+i,q.poison,1);
 const g=machine.run(0x8007819c,[],[],4096);while(!g.next().done){}assert.equal(machine.get(0x800c49d0,2),q.count);assert.equal(Buffer.from(Array.from({length:12},(_,i)=>machine.get(0x800c4c10+i,1))).toString('hex'),q.hex);
}
console.log(JSON.stringify({status:'pass',matrices:fixture.cases.length,projections:fixture.cases.length,requestSelections:requests,queueComparisons:fixture.cases.length*3,priorityCapacityControls:fixture.priority.length,unalignedByteOffsets:4,pagingLossless:true}));

