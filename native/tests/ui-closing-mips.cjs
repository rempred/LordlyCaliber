'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x80077bf8,rom.subarray(0x7ff8,0x8380));
const pool=0x800e82c8,queue=0x800c4c10,results=[],order=[5,0,4,1,3,2],positions=[-20,0,100,300,400,32760,-32760,160],sizes=[0,1,271,-1,32767,-32768,640,160];
let releases=[];
m.trap(0x80077f88,c=>{const slot=c.get('a0');releases.push(slot);m.load(pool+slot*168,Buffer.alloc(168));});
for(const margin of [0,1,2,-1,32768,2147483647,-2147483648])for(const speed of [1,3,8,255])for(let sample=0;sample<8;sample++){
 const mode=sample%16;
 const initial=Buffer.alloc(1008),count=sample===0?mode%7:6;for(let i=0;i<initial.length;i++)initial[i]=(i*31+mode*13+sample*7)&255;
 for(let slot=0;slot<6;slot++){
  const at=slot*168,half=(o,v)=>initial.writeUInt16BE(v&65535,at+o);
  half(0,[0xf000,0xf400,0xf800,0xa800,0xf000,0xf000][slot]);initial[at+2]=(((mode+slot)%16)<<4)|9;initial[at+3]=slot===4?2:1;initial[at+4]=speed;
  half(6,positions[(sample+slot)%8]);half(8,positions[(sample+slot+2)%8]);half(10,positions[(sample+slot)%8]+(sample%2?0:3));half(12,positions[(sample+slot+2)%8]+(sample%2?0:5));half(0x28,sizes[sample]);half(0x2a,sizes[(sample+3)%8]);
 }
 m.load(pool,initial);m.w32(0x800e79a0,margin);m.w16(0x800c49d0,count);for(let i=0;i<6;i++)m.w16(queue+i*2,order[i]);const snapshots=[];
 for(let update=0;update<12;update++){releases=[];m.set('sp',0x803ffe00);m.call(0x80077bf8);snapshots.push({releases,pool:Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+1008)).toString('hex')});}
 results.push({id:`${margin}/${speed}/${sample}`,margin,initial:initial.toString('hex'),count,order,snapshots});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} closing sequences / ${results.length*12} complete pool snapshots; release controlled to record-clear; requested slot order retained.`);
