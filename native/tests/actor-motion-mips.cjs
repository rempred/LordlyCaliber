'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const delta=0x8022e9e8-0x29e218,entry=0x2a0b14+delta;
const constants=[0x80239318,0x80239320,0x80239328].map(a=>rom.readDoubleBE(a-delta));
const rows=[];
function run(name,initial,request){
 const input={position:[2,3,4],motion:[2,17,-3,3,99,0x5a],present:true,occupied:true,mode:0,flags:0,...initial};
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x29d000+delta,rom.subarray(0x29d000,0x2a8d00));
 const scene=0x80300010,actor=0x80310010,motion=0x80320010,sp=0x803ffe00;
 m.load(actor-16,Buffer.alloc(0x170,0xa7));m.load(motion-16,Buffer.alloc(48,0xa7));
 m.w32(0x8022a974,scene);m.w32(scene+0x18,input.occupied?actor:0);m.w32(scene+0xf8,input.present?motion:0);
 input.position.forEach((v,i)=>m.writeFloat(actor+0x11c+i*4,v));m.w8(actor+0x145,input.flags);m.w8(0x8018fc19,input.mode);
 input.motion.slice(0,3).forEach((v,i)=>m.writeFloat(motion+i*4,v));m.w16(motion+12,input.motion[3]);m.w8(motion+14,input.motion[4]);m.w8(motion+15,input.motion[5]);
 let allocations=0;const terrain=[];
 m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),16);allocations++;c.set('v0',motion);});
 // The original creator's relocated terrain call is the accepted overlay helper.
 m.trap(0x801bc35c,c=>{terrain.push([c.get('a0')|0,c.get('a1')|0]);c.setSingle(0,7.25);});
 m.set('sp',sp);m.set('a0',0);
 request.slice(0,3).forEach((v,i)=>{m.writeFloat(sp+4+4*i,v);m.set('a'+(i+1),m.r32(sp+4+4*i));});
 m.writeFloat(sp+16,request[3]);m.w32(sp+20,request[4]);m.w32(sp+24,request[5]);
 m.call(entry);assert.equal(m.get('sp'),sp);
 for(let i=0;i<16;i++){assert.equal(m.r8(actor-16+i),0xa7);assert.equal(m.r8(actor+0x150+i),0xa7);assert.equal(m.r8(motion-16+i),0xa7);assert.equal(m.r8(motion+16+i),0xa7);}
 const bits=v=>{const b=Buffer.alloc(4);b.writeFloatBE(v);return b.readUInt32BE();};
 rows.push({name,input,request,inputPositionBits:input.position.map(bits),requestBits:request.slice(0,4).map(bits),expected:{position:[0,4,8].map(o=>m.readFloat(actor+0x11c+o)),
  motion:[m.readFloat(motion),m.readFloat(motion+4),m.readFloat(motion+8),m.r16(motion+12),m.r8(motion+14),m.r8(motion+15)],
  positionBits:[0,4,8].map(o=>m.r32(actor+0x11c+o)),motionBits:[0,4,8].map(o=>m.r32(motion+o)),
  present:m.r32(scene+0xf8)!==0,allocations,terrain}});
}
run('empty slot',{occupied:false},[-1,-1,100,200,10,0]);
run('teleport retains motion',{},[-1,-1,100,200,1,0]);
run('explicit start',{},[20,-1,100,200,10,0]);
run('default start allocate',{present:false},[-1,-1,100,200,10,0]);
run('negative duration',{},[-1,-1,100,200,-3,0]);
run('short wrap fixed',{},[-1,-1,100,200,65538,0]);
run('both snapped',{},[-1,-1,2.01,4.01,10,0]);
run('x snapped keeps pre snap local',{},[-1,-1,2.01,100,10,0]);
run('z snapped keeps pre snap local',{},[-1,-1,100,4.01,10,0]);
run('scaled',{},[-1,-1,100,200,0,99]);
run('negative scaled',{},[-1,-1,100,200,0,-99]);
run('scaled short wrap',{},[0,0,100,0,0,3]);
run('teleport terrain',{mode:2,flags:1},[-1,-1,65539.75,-32769.5,1,0]);
run('teleport terrain disabled',{mode:0,flags:1},[-1,-1,100,200,1,0]);
for(let i=0;i<64;i++)run('finite movement '+i,{position:[i*0.37,3,i*-0.19],present:i%2===0},[-1,-1,(i-32)*1.73,(i+1)*-1.27,i+2,i%3?0:i+17]);
fs.writeFileSync(process.argv[3],JSON.stringify({constants,rows}));
console.log(`Original actor movement creator: ${rows.length} cases; allocation and terrain are controlled services.`);
