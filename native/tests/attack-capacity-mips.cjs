'use strict';
// Fresh exported C must use expanded army storage, not the abandoned retail
// arrays. Actor eligibility/RNG are controlled; the real combined-spell gate runs.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const capacity=rom.lastIndexOf(Buffer.from('LC07CAPA'));
assert(capacity>=0x2800000,'requires an expanded-army export');
const base=rom.readUInt32BE(capacity+24);
function entries(name){
 const b=fs.readFileSync(path.join(path.dirname(process.argv[2]),name+'.elf')),out={};
 const at=b.readUInt32BE(32),width=b.readUInt16BE(46),count=b.readUInt16BE(48);
 for(let i=0;i<count;i++){
  const h=at+i*width;if(b.readUInt32BE(h+4)!==2)continue;
  const names=b.readUInt32BE(at+b.readUInt32BE(h+24)*width+16),begin=b.readUInt32BE(h+16),size=b.readUInt32BE(h+20);
  for(let j=begin;j<begin+size;j+=16){const n=names+b.readUInt32BE(j);out[b.toString('utf8',n,b.indexOf(0,n))]=b.readUInt32BE(j+4);}
 }return out;
}
const combined=entries('attack_combined'),playback=entries('attack_playback');
assert.equal(combined.D_801971F0,base+0x3800);
assert.equal(playback.D_801971F2,base+0x3802);
assert.equal(playback.D_80195560,base);
const m=new FloatMips();m.ram=new Uint8Array(0x800000);
m.load(0x80070000,rom.subarray(0x400,0x2f000));
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
const cp=0x5dc80,begin=rom.readUInt32BE(cp+24),start=rom.readUInt32BE(cp+12),end=rom.readUInt32BE(cp+48);
m.load(begin,rom.subarray(start,start+end-begin));
const stack=0x807ff000,input=0x80710000,actors=0x80711000;
for(let i=0;i<3;i++)m.w32(input+i*4,actors+i*0x100);
let side=1,checks=0;
m.trap(combined.func_00023780,()=>{});
m.trap(combined.func_00022e60,c=>c.set('v0',50));
m.trap(combined.func_0020C014,c=>c.set('v0',side));
m.trap(combined.func_0020C2C0,c=>{checks++;c.set('v0',1);});
for(side of [0,1])for(const slot of [30,59,60])for(const morale of [0,100]){
 m.w8(combined.D_801976DC,slot);m.w8(combined.D_801976E8,slot);
 m.w8(0x801971f0+slot*25+12,100-morale); // Opposite result in stale memory.
 m.w8(base+0x3800+slot*25+12,morale);checks=0;
 m.set('sp',stack);m.set('a0',input);m.set('a1',input+16);
 m.call(combined.lc_attack_combined,0x80000100,200000);
 assert.equal(m.get('v0'),0);assert.equal(m.get('sp'),stack);
 assert.equal(checks,morale?3:0,'combined spell reads relocated squad morale');
}
console.log('Expanded armies + attack C: 12 real combined-spell gate cases pass; playback roster/source bindings match the exported allocation.');
