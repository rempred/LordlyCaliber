'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
m.load(0x80230e50,rom.subarray(0x2a0680,0x2a0804));
const calls=[0x8022cbd4,0x8023c71c,0x8023ead0,0x8023a7f4,0x802401b4,0x80241610,
 0x8024049c,0x8023bddc,0x80227694,0x8022ae84,0x802316c0,0x8022cf60,0x80230fd4,
 0x8023e354,0x801c3d64,0x80239af8,0x8023dac4,0x802282b8,0x80230b60,0x802345e0,
 0x80234770,0x80178e30];
const root=0x80300000,sp=0x803ffe00;let sequence,mutation;
for(const address of calls)m.trap(address,()=>{
 sequence.push({address,mode:m.r8(0x8018fc19),alternate:m.r8(root+0x1cb1),counter:m.r8(0x801cfc70)});
 if(mutation&&mutation.address===address){m.w8(0x8018fc19,mutation.mode);m.w8(root+0x1cb1,mutation.alternate);m.w8(0x801cfc70,mutation.counter);}
});
function run(mode,alternate,counter,change=null){
 sequence=[];mutation=change;m.w32(0x8022a974,root);m.w8(root+0x1cb1,alternate);m.w8(0x8018fc19,mode);m.w8(0x801cfc70,counter);m.set('sp',sp);
 m.call(0x80230e50);assert.equal(m.get('sp'),sp);
 return {mode,alternate,counter,change,sequence,after:{mode:m.r8(0x8018fc19),alternate:m.r8(root+0x1cb1),counter:m.r8(0x801cfc70)}};
}
const results=[];
for(const mode of [0,1,2,255])for(const alternate of [0,1,255])for(const timer of [0,1,9,10,39,40,41,126,127,128,255])results.push(run(mode,alternate,timer));
for(const mode of [0,2])for(const address of calls)for(const target of [0,1,2])results.push(run(mode,0,40,{address,mode:target,alternate:1,counter:0}));
// Alternate scheduling is decided at entry, even if its callback clears the flag.
results.push(run(2,1,40,{address:0x80227694,mode:0,alternate:0,counter:0}));
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original scheduler: ${results.length} callback-order cases. Domain callbacks are controlled; branches and delayed-counter arithmetic execute original instructions.`);
