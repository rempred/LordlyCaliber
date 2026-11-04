'use strict';
// Execute freshly exported authored predicates and jumps in the retail VM.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const folder=process.argv[2];if(!folder)throw Error('Supply event-authoring export folder');
const rom=fs.readFileSync(path.join(folder,fs.readdirSync(folder).find(x=>x.endsWith('.v64'))));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.equal(rom.readUInt32BE(0),0x80371240);
const script=fs.readFileSync(path.join(folder,'event-4.bin'));
const entry=script.readUInt16BE(script.readUInt16BE(6));let cases=0;
for(const target of [29,30,31])for(const a of [0,1])for(const b of [0,1])for(const cf of [49,50,80]){
 const m=new FloatMips(),SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000;
 m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(PROGRAM,script);
 for(let r=0;r<8;r++)m.w16(LOCAL+r*2,0xa5a5);
 const setFlag=(id,value)=>{const at=0x80193686+(id>>3),mask=1<<(id&7);m.w8(at,(m.r8(at)&~mask)|(value?mask:0));};
 const getFlag=id=>!!(m.r8(0x80193686+(id>>3))&(1<<(id&7)));
 setFlag(200,a);setFlag(201,b);setFlag(202,0);setFlag(203,0);m.w8(0x801936a9,cf);m.w8(0x801976e8,target);m.w8(0x80197b00,3);
 let pc=entry,ended=false;
 for(let n=0;n<300;n++){
   m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
   if(script[pc]===0x2f){m.call(0x80170954);ended=true;break;}
   m.call(0x80170954,0x80171b58,20000);pc=(m.r16(0x80197b04)+1)*2;
 }
 assert(ended,'tree did not terminate');const match=!!a&&(cf>=50||!!b);
 assert.equal(getFlag(202),target===30&&match,JSON.stringify({target,a,b,cf}));
 assert.equal(getFlag(203),target===30&&!match,JSON.stringify({target,a,b,cf}));cases++;
}
console.log(`PASS ${cases} exported native event cases: target filter, AND/OR, both branches, long jumps and flag writes.`);
// The former squad30 guard now belongs to town visits and selects town2.
for(const group of [2,3]){const entries=[];for(let at=script.readUInt16BE(group*2);script.readUInt16BE(at)!==65535;at+=2)entries.push(script.readUInt16BE(at));assert.equal(entries.includes(0x1bc),group===2);}
for(const town of [1,2,3]){
 const m=new FloatMips(),SP=0x807ff000,PROGRAM=0x80700000;
 m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(PROGRAM,script);m.w8(0x801976e8,town-1);let pc=0x1bc;
 for(let n=0;n<3;n++){m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);m.call(0x80170954,0x80171b58,10000);pc=(m.r16(0x80197b04)+1)*2;}
 assert.equal(pc,town===2?0x1c4:0x1c2);
}
console.log('PASS exported WHEN relocation and 3 native town-target guard cases.');
const longEntry=script.readUInt16BE(script.readUInt16BE(6)+2);
for(const flag of [0,1]){
 const m=new FloatMips(),SP=0x807ff000,PROGRAM=0x80700000;m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(PROGRAM,script);
 m.w8(0x801976e8,30);m.w8(0x80193686+(200>>3),flag);let pc=longEntry,ended=false;
 for(let n=0;n<1000;n++){m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);if(script[pc]===0x2f){m.call(0x80170954);ended=true;break;}m.call(0x80170954,0x80171b58,10000);pc=(m.r16(0x80197b04)+1)*2;}
 assert(ended);const flags=m.r8(0x80193686+(200>>3));assert.equal(!!(flags&32),!flag);assert.equal(!!(flags&64),!!flag);
}
console.log('PASS NOT and both paths across a branch longer than the native short-jump range.');
console.log('Does not execute cutscene rendering, recruitment construction or a full mission.');
