'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
// All three complete functions uniquely match retained opening-scene RAM.
// Interpolation's full54-byte instruction span independently matches ROM283D94.
m.load(0x8023df70,rom.subarray(0x2ad6d0,0x2ada48));
m.load(0x80227dc4,rom.subarray(0x283d94,0x283de8));
const record=0x80300010,dl=0x80310000,sp=0x803ffe00;
let allocations=0,frees=0;
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),14);allocations++;c.set('v0',record);});
m.trap(0x800712c4,c=>{assert.equal(c.get('a0'),record);frees++;});
const call=(pc,args=[])=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(pc);assert.equal(m.get('sp'),sp);return m.get('v0');};
const snapshot=()=>({record:m.r32(0x8022a978)?Buffer.from(m.ram.subarray(m.phys(record),m.phys(record)+14)).toString('hex'):'',query:call(0x8023e2c0),allocations,frees});
const draw=()=>{m.w32(0x800e9ba0,dl);m.load(dl,Buffer.alloc(32));const rendered=call(0x8023e050);assert.equal(m.r32(0x800e9ba0),dl+(rendered?24:0));return {...snapshot(),scissor:rendered?[m.r32(dl+8),m.r32(dl+12)]:[0xed000060,0x004fc35c]};};
const rows=[];
for(const initial of [[-1,-1],[0xffff,0xffff],[12,29],[-2,241]])
for(const final of [[0,0],[80,17],[-1,-1],[-32768,32767]])
for(const duration of [0,1,7,0xffff])for(const persistence of [0,1]) {
 m.load(record-16,Buffer.alloc(46,0xa7));m.w32(0x8022a978,0);allocations=frees=0;
 const args=[...initial,...final,duration,persistence].map(x=>x>>>0),steps=[];
 call(0x8023df70,args);steps.push({start:args,...snapshot()});
 const count=duration===0xffff?4:duration+2;
 for(let i=0;i<count;++i)steps.push(draw());
 const replacement=[0xffffffff,0xffffffff,10,20,3,1];
 call(0x8023df70,replacement);steps.push({start:replacement,...snapshot()});
 for(let i=0;i<5;++i)steps.push(draw());
 for(let i=0;i<16;++i){assert.equal(m.r8(record-16+i),0xa7);assert.equal(m.r8(record+14+i),0xa7);}
 rows.push({steps});
}
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
console.log(`${rows.length} original crop sequences; allocation/free controlled, interpolation and scissor emission execute.`);
