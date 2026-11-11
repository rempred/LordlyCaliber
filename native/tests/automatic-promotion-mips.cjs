'use strict';
// Execute the exported retail selectors, not a reimplementation of their tests.
// No full-console scene playback is claimed. Angel testing isolates its stat
// block after equipment eligibility and before the native random draw.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
function machine(){
 const m=new FloatMips();m.ram=new Uint8Array(0x800000);
 m.load(0x80070000,rom.subarray(0x400,0x2f000));
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
 m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
 m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
 const p=rom.subarray(0x5dc80,0x5dd00);
 if(p.subarray(0,8).toString()==='LC05CLAS'){
  const base=p.readUInt32BE(24),at=p.readUInt32BE(12),end=p.readUInt32BE(48);
  m.load(base,rom.subarray(at,at+end-base));
 }
 m.set('sp',0x803ff000);return m;
}
const actor=0x803c0000;
function attack(cls,str,vit,trigger,side=0,training=false){
 const m=machine();m.w32(actor+0x48,cls);m.w32(actor+0x4c,cls);
 m.w32(actor+0x40,side?0x100:0);m.w16(actor+0x24,str);m.w16(actor+0x26,vit);
 m.w8(0x801976d8,training?8:0);m.set('a0',actor);m.set('a1',trigger);
 m.call(0x801f4828);return m.get('v0');
}
let cases=0;
for(const side of [0,1])for(const [cls,target,str,vit,trigger] of [[78,79,10,11,0],[79,80,12,13,1]]){
 for(const [s,v,expected] of [[str,vit,target],[str-1,vit,0],[str,vit-1,0],[str+1,vit+1,target]]){
  assert.equal(attack(cls,s,v,trigger,side),expected,`attack conversion ${cls}/${s}/${v}/${side}`);cases++;
 }
 assert.equal(attack(cls,str,vit,(trigger+1)%3,side),0);cases++;
 assert.equal(attack(cls,str,vit,trigger,side,true),0);cases++;
}
for(const [cls,target,trigger]of [[45,47,2],[46,47,2],[47,48,1]]){
 assert.equal(attack(cls,0,0,trigger,1),target);cases++;
}
function evolution(str,agi,dex,alignment){
 const m=machine();m.w32(actor+0x48,69);m.w32(actor+0x4c,69);
 [str,0,0,0,agi,dex].forEach((v,i)=>m.w16(actor+0x24+i*2,v));
 m.w8(actor+0x34,alignment);m.w8(actor+0x33,0);m.set('a0',actor);m.call(0x801d6460);return m.get('v0');
}
for(const [str,agi,dex,ali,result]of [[14,15,16,10,70],[13,15,16,10,0],[14,14,16,10,0],
 [14,15,15,10,0],[14,15,16,9,0],[14,15,16,70,70],[14,15,16,71,0]]){
 assert.equal(evolution(str,agi,dex,ali),result,`evolution ${str}/${agi}/${dex}/${ali}`);cases++;
}
// Capacity's qualified owner gives this overlay placement; do not apply the
// early-boot ROM/RAM mapping to the scenario-end overlay.
const angelRom=0x1b6538,angelRam=0x80218e48,pc=offset=>angelRam+offset-angelRom;
function angel(int,men,dex,ali){
 const m=machine();m.load(angelRam,rom.subarray(angelRom,angelRom+0xedc));
 m.set('s0',56);m.w16(0x80193be0+56,int);m.w16(0x80193be2+56,men);m.w16(0x80193be6+56,dex);m.w8(0x80193bdb+56,ali);
 m.trap(pc(0x1b6794),c=>c.set('v0',0));m.trap(pc(0x1b66d8),c=>c.set('v0',1));
 m.call(pc(0x1b6674));return m.get('v0');
}
for(const [int,men,dex,ali,result]of [[17,18,19,20,1],[16,18,19,20,0],[17,17,19,20,0],
 [17,18,18,20,0],[17,18,19,19,0],[17,18,19,80,1],[17,18,19,81,0]]){
 assert.equal(angel(int,men,dex,ali),result,`Angel requirements ${int}/${men}/${dex}/${ali}`);cases++;
}
console.log(`PASS ${cases} exported instruction cases: Golems on both sides, exact thresholds, trigger/training rejection, undead conversions, Wyrm evolution and Angel Knight stat interval.`);
