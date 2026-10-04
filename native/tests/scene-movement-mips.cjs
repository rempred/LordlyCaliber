'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x802316c0,rom.subarray(0x2a0ef0,0x2a135c));
const root=0x80300000,actors=0x80310010,motions=0x80320010,depths=0x80330010,sp=0x803ffe00;
let allocations,terrain;
const record=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),4);const p=depths+0x1000+64*allocations++;m.load(p-16,Buffer.alloc(36,0xa7));m.load(p,Buffer.alloc(4));c.set('v0',p);});
m.trap(0x800712c4,c=>{const p=c.get('a0');assert(p>=motions&&p<depths+0x2000);});
m.trap(0x801bc35c,c=>{terrain.push([2,c.get('a0')|0,c.get('a1')|0]);c.setSingle(0,7.25+terrain.length/4);});
m.trap(0x8023d178,c=>{terrain.push([0,c.single(12),c.single(14)]);c.setSingle(0,7.25+terrain.length/4);});
const results=[];
function run(options={}){
 const o={mode:2,stateFlags:8,ground:0,pause:0,remaining:2,dx:9.75,dz:5.25,depth:false,moving:true,missing:false,sameIdentity:false,extraMotion:false,x:10.25,z:20.5,...options};
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w8(0x8018fc19,o.mode);m.w32(0x80197b14,o.stateFlags);m.set('sp',sp);allocations=0;terrain=[];
 const initialActors=[];
 for(const slot of [0,2,3,27]){
  if(slot===0&&o.missing)continue;
  const b=Buffer.alloc(336,0xa7);b.writeInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt16BE(0,0x134);b[0x13d]=b[0x146]=0;b[0x145]=o.ground;b[0x147]=o.sameIdentity?0:slot;
  b.writeFloatBE(o.x+(slot===2?o.dx:slot===3?1:slot===27?-5:0),0x11c);b.writeFloatBE(6.75,0x120);b.writeFloatBE(o.z+(slot===2?o.dz:slot===3?1:slot===27?4:0),0x124);
  const p=actors+slot*512;m.load(p-16,Buffer.alloc(368,0xa7));m.load(p,b);m.w32(root+24+slot*4,p);initialActors.push({slot,record:b.toString('hex')});
 }
 const initialMotions=[];
 for(const slot of o.extraMotion?[0,27]:[0]){
  const b=Buffer.alloc(16,0xa7);b.writeFloatBE(slot===0?1.25:-0.75,0);b.writeFloatBE(9.5,4);b.writeFloatBE(slot===0?-0.5:0.25,8);b.writeUInt16BE(o.remaining,12);b[14]=o.pause;
  const p=motions+slot*64;m.load(p-16,Buffer.alloc(48,0xa7));m.load(p,b);m.w32(root+0xf8+slot*4,o.moving?p:0);initialMotions.push({slot,record:b.toString('hex'),present:o.moving});
 }
 const initialDepths=[];
 if(o.depth){const b=Buffer.from('fffda700','hex');m.load(depths-16,Buffer.alloc(36,0xa7));m.load(depths,b);m.w32(root+0x88+2*4,depths);initialDepths.push({slot:2,record:b.toString('hex')});}
 const states=[];
 for(let step=0;step<4;step++){
  m.call(0x802316c0);assert.equal(m.get('sp'),sp);
  const actorStates=initialActors.map(a=>({slot:a.slot,record:record(actors+a.slot*512,336)}));
  const motionStates=initialMotions.map(a=>({slot:a.slot,record:record(motions+a.slot*64,16),present:!!m.r32(root+0xf8+a.slot*4)}));
  const depthStates=Array.from({length:28},(_,slot)=>{const p=m.r32(root+0x88+slot*4);return p?{slot,record:record(p,4)}:null;}).filter(Boolean);
  states.push({actors:actorStates,motions:motionStates,depths:depthStates,terrain:[...terrain]});
  for(const a of initialActors)for(let i=0;i<16;i++){const p=actors+a.slot*512;assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+336+i),0xa7);}
 }
 results.push({options:o,actors:initialActors,motions:initialMotions,depths:initialDepths,states});
}
for(const mode of [0,1,2])for(const stateFlags of [0,8])for(const ground of [0,1,4,5])for(const pause of [0,1])for(const remaining of [0,1,2,65535])run({mode,stateFlags,ground,pause,remaining});
for(const dx of [-20,-19.99,0,19.99,20,40])for(const dz of [-20,-19.99,0,19.99,20,40])run({dx,dz,depth:true,moving:false});
run({sameIdentity:true});run({extraMotion:true});run({depth:true,extraMotion:true});
run({x:65539.75,z:-32769.5,ground:1});
run({mode:0,missing:true,pause:1,remaining:0,depth:true});run({mode:1,missing:true,pause:1,remaining:2,depth:true});
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original full movement: ${results.length} sequences, ${results.length*4} snapshots; terrain/allocation/free controlled.`);
