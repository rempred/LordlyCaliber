'use strict';
// Actual exported resolver and battle selector; equipment and damage scoring
// are controlled services. No ROM file is modified.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80070000,rom.subarray(0x400,0x2f000));
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
const placement=rom.subarray(0x5dc80,0x5dd00),base=placement.readUInt32BE(24),offset=placement.readUInt32BE(12),end=placement.readUInt32BE(48);
m.load(base,rom.subarray(offset,offset+end-base));
const stack=0x803ff000,source=0x803d0000,target=source+0x100,out=source+0x400;
m.set('sp',stack);
function call(pc,args){args.slice(0,4).forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));args.slice(4).forEach((v,i)=>m.w32(stack+16+i*4,v));m.call(pc);assert.equal(m.get('sp'),stack);return m.get('v0');}
let element=1,item=243,command=45;
m.trap(0x8016fb50,c=>c.set('v0',item)); // spellbook / weapon selection
m.trap(0x8016fa34,c=>c.set('v0',item));
m.trap(0x8016f5e0,c=>c.set('v0',element));
m.trap(0x8016e230,c=>c.set('v0',command));
const resolver=0x8016e50c;
let resolutions=0;
for(const [id,el,expected,kind] of [[45,1,159,3],[45,2,63,3],[160,1,55,3],[160,2,159,3],[51,3,159,3],[145,1,159,3],[145,2,148,4],[46,1,57,3]]){
 element=el;assert.equal(call(resolver,[out,2,2,id,1,1,1,1,1]),kind);
 assert.equal(m.r8(out),expected,`array ${id}, element ${el}`);resolutions++;
}
element=7;
for(item=249;item<=252;item++)for(const id of [45,160]) {
 assert.equal(call(resolver,[out,2,2,id,1,1,1,1,1]),3);assert.equal(m.r8(out),id===45&&item===249?159:99+item-249);resolutions++;
}
// Best-spell selection must score the remapped attacks, not map only the winner.
m.trap(0x801f0cc4,c=>c.set('v0',0));
m.trap(0x801c8fe8,c=>c.set('v0',target));
m.trap(0x801f4480,c=>c.set('v0',c.r32(source+0x7c)));
m.trap(0x801c8fb8,c=>c.set('v0',1));
m.w32(source+0x48,2);m.w32(source+0x4c,2);m.w32(source+0x58,6);
command=49;element=1;call(0x801ebc80,[source]);assert.equal(m.r32(source+0x7c),159);assert.equal(m.r8(source+0x78),3);
command=145;call(0x801ebc80,[source]);assert.equal(m.r32(source+0x7c),159);assert.equal(m.r8(source+0x78),3);
console.log(`PASS ${resolutions} exported spell-array resolutions, independent copies, spellbooks, unchanged arrays, strongest-spell scoring and breath-to-spell routing.`);
