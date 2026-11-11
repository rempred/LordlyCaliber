'use strict';
// Real ROM cycle/profile instructions. Item checks, heap services and flight
// duration inputs are controlled: this proves rule routing, not visual timing.
const fs=require('fs'),assert=require('assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const owners=require('../game/accepted/rom-owners.json').sources;
function read(p){const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const candidate=read(process.argv[2]),original=read(process.argv[3]);
const unit=0x80600000,sp=0x807ff000;
function machine(rom){const m=new FloatMips();m.ram=new Uint8Array(0x800000);m.set('sp',sp);
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));m.load(0x801ad5a0,rom.subarray(0x101ce0,0x145210));
 if(rom.subarray(0x5dc80,0x5dc88).toString()==='LC05CLAS'){const at=rom.readUInt32BE(0x5dc8c),size=rom.readUInt32BE(0x5dc90),ram=rom.readUInt32BE(0x5dc98);m.load(ram,rom.subarray(at,at+size));}return m;}
function cycle(m,start,type,flags,hour,before,item,town=-1,townFlags=0){let calls=0;
 m.set('sp',sp);m.set('a0',unit);m.w32(unit,flags);m.w8(unit+4,0);m.w32(unit+0x70,type);
 m.w32(unit+0x74,town);m.w16(unit+0xb6,before);m.w8(0x801f0e0a,hour);m.w32(0x801f0de0,0);
 m.w16(0x801951b0+0x1c,townFlags);m.w32(0x801f0cb0,unit+0x1000);
 m.w32(unit+0x1000,0);m.w32(unit+0x1074,0);m.w8(0x801969b8+2,0);
 m.trap(0x801dcd40,c=>c.set('v0',item));m.trap(0x801dd244,c=>c.set('v0',0));m.trap(0x801da228,c=>c.set('v0',0));
 for(const a of [0x801df34c,0x801df4a0])m.trap(a,c=>{calls++;c.set('v0',40);});
 m.call(start+0x800ab8c0);return[m.r16(unit+0xb6),m.get('v0'),calls];}
const stock=machine(original),edited=machine(candidate);let comparisons=0;
if(process.argv[4]==='global') {
 assert.equal(candidate.readUInt16BE(0x1339f2),91);assert.equal(candidate.readUInt16BE(0x1338ae),360);
 assert.deepEqual(cycle(edited,0x133944,12,0x40001,12,88,0),[5,40,1]);
 assert.deepEqual(cycle(edited,0x133944,9,0x40001,12,88,0),[1,40,1]);
 assert.deepEqual(cycle(edited,0x133700,12,0,12,0,0),[360,40,1]);
 assert.deepEqual(cycle(edited,0x133700,12,0,20,0,0),[357,40,1]);
 console.log('PASS per-type fatigue/recovery together with exported global cycle periods and native recovery/gain callbacks.');process.exit(0);
}
if(process.argv[4]!=='assignment') {
for(const start of [0x133944,0x133700])for(const type of [1,2,3,4,5,6,7])for(const hour of [5,6,17,18])
 for(const before of [0,1,2,3,6,178,179,180,181])for(const item of [0,1])
 for(const [flags,town,townFlags]of [[0,-1,0],[0x40001,-1,0],[0x40041,-1,0],[0x41,0,4],[1,0,4],[1,0,0],[0x20001,-1,0]]) {
  assert.deepEqual(cycle(edited,start,type,flags,hour,before,item,town,townFlags),cycle(stock,start,type,flags,hour,before,item,town,townFlags),`retail cycle ${[start,type,flags,hour,before,item,town,townFlags]}`);comparisons++;
 }
for(const hour of [12,20])for(const item of [0,1])for(const group of [false,true]) {
 const flags=0x40001|(group?0x40:0),night=hour===20?1:0;
 const sky=cycle(edited,0x133944,9,flags,hour,0,item);
 assert.equal(sky[0],((group?2:3)>>(item?1:0))+night);
 const custom=cycle(edited,0x133944,12,flags,hour,0,item);
 assert.equal(custom[0],((group?4:7)>>(item?1:0))+night*2);
 const recovery=cycle(edited,0x133700,12,0,hour,100,item);
 assert.equal(recovery[0],100-(item?10:5)-night*3);
}
}
// The selected airborne policy must reach both native producers, including a
// ground class newly given flight, and complete the real spline routine.
let flights=0;
for(const name of ['func_0021D7F0','func_0021E50C'])for(const [cls,expected]of (process.argv[4]==='assignment'?[[Number(process.argv[5]),7]]:[[7,7],[6,name==='func_0021D7F0'?7:4],[0x24,7]])){
 const m=machine(candidate),o=owners.find(s=>s.name===name),b=o.bindings;
 // Battle setup separately loads this second bank; descriptor 10 alone ends
 // before it. Use the same qualified placement as attack-playback-machine.cjs.
 m.load(0x801ad5a0,candidate.subarray(0x1f0a30,0x213b10));
 m.load(0x801d0840,candidate.subarray(0x213b10,0x24bca0));
 const spline=owners.find(s=>s.name==='func_0026C9CC');for(const p of spline.parts)m.load(p.address,candidate.subarray(p.start,p.start+p.size));
 const state=unit,actor=unit+0x1000,context=unit+0x2000;let allocation=unit+0x3000,count=null;
 m.w32(state+0x40,actor);m.w32(actor+0x48,cls);m.w32(actor+0x4c,cls);m.w32(b.D_801CE8BC,context);
 m.w16(state+0x2c,10);m.set('a0',state);m.set('a1',0);m.set('a2',10);
 for(const key of ['func_00201670','func_002016F4'])if(b[key])m.trap(b[key],c=>c.setSingle(0,1));
 for(const key of ['func_00222190','func_00222344'])if(b[key])m.trap(b[key],c=>c.set('v0',60));
 m.trap(b.resource_alloc,c=>{c.set('v0',allocation);allocation+=0x1000;});m.trap(b.resource_free,()=>{});
 m.trap(b.memset_00023780,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
 m.trap(b.memcpy,c=>{for(let i=0;i<c.get('a2');i++)m.w8(c.get('a1')+i,m.r8(c.get('a0')+i));});
 const exec=m.exec.bind(m);m.exec=(pc,next)=>{if(pc===spline.address)count=m.get('a0');return exec(pc,next);};
 m.call(o.address);assert.equal(count,expected,`${name} class ${cls} flight profile`);flights++;
}
console.log(`PASS ${comparisons} original/edited fatigue and rest comparisons, independent custom rates, custom Sky fatigue, and ${flights} complete native flight-profile/spline paths. Timing inputs are controlled; this is not console playback.`);
