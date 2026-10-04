'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x800775ec,rom.subarray(0x79ec,0x7ff8));
const pool=0x800e82c8,queue=0x800c4c10,results=[],order=[5,0,4,1,3,2],positions=[-20,0,100,300,400,32760,-32760,160],sizes=[0,1,271,-1,32767,-32768,640,160];
for(let mode=0;mode<16;mode++)for(const speed of [1,3,8,255])for(let sample=0;sample<8;sample++){
 const initial=Buffer.alloc(1008),count=sample===0?mode%7:6;for(let i=0;i<initial.length;i++)initial[i]=(i*31+mode*13+sample*7)&255;
 for(let slot=0;slot<6;slot++){
  const at=slot*168,half=(o,v)=>initial.writeUInt16BE(v&65535,at+o);
  half(0,[0xe800,0xec00,0xf800,0xa800,0xe800,0xe800][slot]);initial[at+2]=(((mode+slot)%16)<<4)|9;initial[at+3]=slot===4?2:1;initial[at+4]=speed;
  half(6,positions[(sample+slot)%8]);half(8,positions[(sample+slot+2)%8]);half(10,positions[(sample+slot)%8]+(sample%2?0:3));half(12,positions[(sample+slot+2)%8]+(sample%2?0:5));half(0x28,sizes[sample]);half(0x2a,sizes[(sample+3)%8]);
 }
 m.load(pool,initial);m.w16(0x800c49d0,count);for(let i=0;i<6;i++)m.w16(queue+i*2,order[i]);const snapshots=[];
 for(let update=0;update<12;update++){m.set('sp',0x803ffe00);m.call(0x800775ec);snapshots.push(Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+1008)).toString('hex'));}
 results.push({id:`${mode}/${speed}/${sample}`,initial:initial.toString('hex'),count,order,snapshots});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} opening sequences / ${results.length*12} complete pool snapshots; no helpers controlled.`);
