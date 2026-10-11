'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const results=[];
function run(name,input){
 const {from=4,destination=1,occupied=true,flags=0x100,art=3,member=7,requested=7,sourceRow=2}=input;
 const m=new Mips();m.ram=new Uint8Array(0x400000);m.set('sp',0x803ffe00);
 // This separate validated slab is ROM 2A8D20..2AE338 -> RAM 802395C0.
 m.load(0x802395c0,rom.subarray(0x2a8d20,0x2ae338));
 m.load(0x801c8b68,rom.subarray(0x20bff8,0x20c014));
 const scene=0x80300000,cast=0x80310000,first=0x80320010,second=0x80321010;
 m.w32(0x8022a974,scene);m.w32(0x801ce8bc,cast);m.w8(0x801976e8,0);m.w8(0x801971f2,requested);
 const rows=Array.from({length:20},()=>Buffer.alloc(248));rows[sourceRow].writeUInt32BE(flags,0x40);rows[sourceRow].writeUInt32BE(art,0x48);rows[sourceRow][0xf6]=member;
 rows.forEach((row,i)=>m.load(cast+0x1c4+i*248,row));
 function record(slot,ordinal){const b=Buffer.alloc(336);b.fill(255,0,16);b.writeInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt32BE(-1,0xf0);b.writeInt16BE(12,0x134);b[0x147]=ordinal;return b;}
 const a=record(from,sourceRow),b=record(destination&255,9);
 m.load(first-16,Buffer.alloc(368,0xa7));m.load(first,a);m.w32(scene+24+4*from,first);
 m.load(second-16,Buffer.alloc(368,0xa7));m.load(second,b);if(occupied)m.w32(scene+24+4*(destination&255),second);
 m.set('a0',destination);m.set('a1',0);m.call(0x8023dcd4);assert.equal(m.get('sp'),0x803ffe00);
 const slots=[];for(let slot=0;slot<28;slot++){const ptr=m.r32(scene+24+slot*4);if(ptr)slots.push({slot,record:Buffer.from(m.ram.subarray(m.phys(ptr),m.phys(ptr)+336)).toString('hex')});}
 for(const ptr of[first,second])for(let i=0;i<16;i++){assert.equal(m.r8(ptr-16+i),0xa7);assert.equal(m.r8(ptr+336+i),0xa7);}
 results.push({name,destination,from,rows:rows.map(b=>b.toString('hex')),members:[requested,255,255,255,255],
  initial:[{slot:from,record:a.toString('hex')},...(occupied?[{slot:destination&255,record:b.toString('hex')}]:[])],slots});
}
run('occupied',{});run('empty',{occupied:false});run('highest source',{from:27});run('narrow destination',{destination:257});
run('inactive row',{flags:0x200});run('empty art',{art:0});run('absent member',{requested:8});run('last caller row',{sourceRow:19});
fs.writeFileSync(process.argv[3],JSON.stringify({results}));console.log(`Original member binding: ${results.length} pointer/record cases, no helper doubles.`);
