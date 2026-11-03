'use strict';
// Execute newly exported C with the real High Attack layout transition and
// native result consumer. Presentation/actor discovery are controlled services.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
function entries(name){
 const b=fs.readFileSync(path.join(path.dirname(process.argv[2]),name+'.elf')),out={};
 const at=b.readUInt32BE(32),width=b.readUInt16BE(46),count=b.readUInt16BE(48);
 for(let i=0;i<count;i++){
  const h=at+i*width;if(b.readUInt32BE(h+4)!==2)continue;
  const names=b.readUInt32BE(at+b.readUInt32BE(h+24)*width+16),begin=b.readUInt32BE(h+16),size=b.readUInt32BE(h+20);
  for(let j=begin;j<begin+size;j+=16){const n=names+b.readUInt32BE(j);out[b.toString('utf8',n,b.indexOf(0,n))]=b.readUInt32BE(j+4);}
 }return out;
}
const e={...entries('attack_strikes'),...entries('attack_playback')};
for(const shifted of [false,true]){
 const m=new FloatMips();m.ram=new Uint8Array(0x800000);
 m.load(0x80070000,rom.subarray(0x400,0x2f000));
 m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
 m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
 const cp=0x5dc80,base=rom.readUInt32BE(cp+24),start=rom.readUInt32BE(cp+12),end=rom.readUInt32BE(cp+48);
 m.load(base,rom.subarray(start,start+end-base));
 m.load(0x80440000,rom.subarray(0x27a0000,0x27a0000+6736));
 const stack=0x807ff000,oldOwner=0x80740000,oldStream=oldOwner+0x57e0;
 m.w32(0x801ce8bc,oldOwner);m.w32(0x801ce8c0,oldStream);m.set('sp',stack);
 m.w32(oldStream+0x814,shifted?0x7f0:0x100);m.set('s3',0x20);m.set('s0',0);m.set('s1',0);
 m.call(0x80440040,0x801d9a88,200000);
 const stream=m.r32(0x801ce8c0),owner=m.r32(0x801ce8bc),tail=stream-(shifted?0x7000:0);
 assert.equal(stream,shifted?0x80450000:oldStream);assert.equal(owner,shifted?0x8044a820:oldOwner);
 const actor=owner+0x1c4+9*0xf8,child=0x80710000;
 const delivered=[];let poseResets=0,visualCalls=0;
 m.trap(0x801c8fe8,c=>c.set('v0',c.get('a0')===9?actor:0));
 m.trap(0x801e1014,c=>delivered.push({amount:c.get('a3'),tick:c.get('a2'),target:c.get('a0')}));
 m.trap(0x801d9860,()=>{});m.trap(0x801d8e70,()=>{});
 m.trap(0x801ad974,c=>{assert.equal(c.get('a0'),child+68);poseResets++;});
 m.trap(0x8020b0e4,()=>{visualCalls++;});
 function call(pc,args=[]){args.forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));m.call(pc,0x80000100,300000);assert.equal(m.get('sp'),stack);return m.get('v0')|0;}
 const command=0x80720000,offset=shifted?0x900:0x100;
 for(const count of [1,6,18]){
  for(let i=0;i<4160;i++)m.w8(e.lc_strike_state+i,0);
  [22,0,0,0,5,0,255,...Array(count).fill(9),255,128,...Array(count-1).fill(0),255].forEach((v,i)=>m.w8(command+i,v));
  assert.equal(call(e.lc_strike_command,[command]),1);
  m.w8(tail+0x824,count);m.w32(tail+0x814,offset+0x200);delivered.length=0;
  for(let i=0;i<count;i++){
   [31,0,0,9,0,0,20+i,0,1,0].forEach((v,j)=>m.w8(stream+16+offset+i*10+j,v));
   assert.equal(call(e.lc_strike_result,[actor,offset+i*10]),1);
  }
  for(let i=0;i<count;i++){
   m.w32(stream+8,100+i*11);call(e.lc_attack_visual_impact,[9,10]);
   assert.deepEqual(delivered[i],{amount:20+i,tick:100+i*11,target:actor});
  }
  assert.equal(m.r8(tail+0x824),0,'completion counter in active trailer');
  call(e.lc_strike_end);
 }
 // A continuation damage/status result shares its impact and must reserve a
 // completion count in the relocated trailer, not overwrite packet bytes.
 m.w8(tail+0x824,1);delivered.length=0;
 [22,0,0,0,5,0,255,9,255,128,255].forEach((v,i)=>m.w8(command+i,v));call(e.lc_strike_command,[command]);
 for(let i=0;i<2;i++){
  [31,0,0,9,i?128:0,0,50+i,0,1,0].forEach((v,j)=>m.w8(stream+16+offset+i*10+j,v));
  call(e.lc_strike_result,[actor,offset+i*10]);
 }
 call(e.lc_attack_visual_impact,[9,10]);assert.deepEqual(delivered.map(x=>x.amount),[50,51]);assert.equal(m.r8(tail+0x824),0);call(e.lc_strike_end);
 // Missing actors and sprite children used by the original High Attack hooks.
 for(const [type,id] of [[1,9],[2,9],[5,255],[0x17,255]]){
  m.w32(actor,child);m.w32(actor+4,0);m.w32(actor+8,0);
  const width=m.r8(0x801e5c70+type);assert(width>=3);
  for(let i=0;i<width+12;i++)m.w8(stream+16+offset+i,0);
  [type,0,0,id,2,255].forEach((v,i)=>m.w8(stream+16+offset+i,v));
  m.w8(stream+16+offset+width,0x40);m.w8(stream+18+offset+width,1);
  m.w32(stream+4,0);m.w32(tail+0x810,offset);m.w32(tail+0x814,offset+width+8);
  call(e.lc_attack_playback);
  assert.equal(m.r32(tail+0x810),offset+width);assert.equal(m.r32(stream+4),1);
 }
 assert.equal(poseResets,1);assert.equal(visualCalls,0);
 // Terminal phase handoff stays conditional. Control the unrelated scene/UI
 // services after the handoff; this is not a whole-battle simulation.
 const ownerTail=owner-(shifted?0x7000:0);
 m.w8(ownerTail+0x6087,1);m.w8(ownerTail+0x6072,0);m.w8(ownerTail+0x606e,2);
 for(const name of ['func_00048294','func_0004813C','func_000466f4','func_0020BF7C','lc_strike_round_mode']){
  assert(e[name],name);m.trap(e[name],c=>c.set('v0',0));
 }
 for(const phase of [0,1,2])for(const terminal of [false,true]){
  m.w8(stream+16+offset,0x41);m.w8(stream+18+offset,0);m.w32(stream+4,0);
  m.w32(tail+0x810,offset);m.w32(tail+0x814,offset+3);m.w8(tail+0x864,phase);m.w32(tail+0x868,17);
  m.w16(0x800c4c26,terminal?65535:0);m.w32(0x801ce8c4,1);call(e.lc_attack_playback);
  const handoff=shifted&&terminal&&phase<=1;
  assert.equal(m.r8(tail+0x864),handoff?5:phase);assert.equal(m.r32(tail+0x868),handoff?0:17);assert.equal(m.r32(0x801ce8c4),0);
 }
 console.log(`${shifted?'Expanded':'Normal'} buffer: real layout transition, 1/6/18 timed impacts, continuation counters, missing actors/children and phase handoff PASS.`);
}
