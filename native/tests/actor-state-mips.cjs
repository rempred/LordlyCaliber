'use strict';
// Original integer resolver against the current ROM vocabulary. No service doubles.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const root=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['art.js','cutscene-data.js'])vm.runInThisContext(fs.readFileSync(path.join(root,f),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const table=OB64.art.readCompressedResource(rom,0x0109eeb0).decoded;
const m=new Mips();m.ram=new Uint8Array(0x400000);m.set('sp',0x803ffe00);
// Qualified fixed cutscene overlay placement, not the early-boot address delta.
m.load(0x80231bbc,rom.subarray(0x2a13ec,0x2a1610));
m.load(0x80300000,Buffer.alloc(table.length+32,0xa7));m.load(0x80300010,table);m.w32(0x8022a97c,0x80300010);
const rows=[];
function run(bank,key,facing,option,expected){
 [bank,key,facing,option].forEach((v,i)=>m.set('a'+i,v));m.call(0x80231bbc);
 const result=m.get('v0')|0;if(expected!==undefined)assert.equal(result,expected,`bank${bank}/key${key}/facing${facing}/option${option}`);
 assert.equal(m.get('sp'),0x803ffe00);rows.push({bank,key,facing,option,result});
}
for(const s of OB64.cutsceneData.poseSelectors){run(s.bank,s.animationKey,s.facing,0,s.stateIndex);run(s.bank,s.animationKey,s.facing,1,-1);}
for(let bank=1;bank<=68;bank++)run(bank,9999,0,0,-1);
run(1,-2147483648,0,0,0);run(1,2147483647,10,0,0);
for(let i=0;i<16;i++){assert.equal(m.r8(0x80300000+i),0xa7);assert.equal(m.r8(0x80300010+table.length+i),0xa7);}
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
console.log(`Original actor resolver: ${rows.length} state, option, miss and 32-bit-wrap cases.`);
