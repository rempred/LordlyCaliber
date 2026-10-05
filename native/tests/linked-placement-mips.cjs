'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023ac50,rom.subarray(0x2aa3b0,0x2aa608));m.load(0x801c8c90,rom.subarray(0x20c120,0x20c164));
for(const p of [0x8023aa8c,0x80092a90,0x80098b60])m.trap(p,()=>{}); // Camera basis and scale already qualified separately.
const root=0x80300000,cast=0x80310000,pool=0x80320010,sp=0x803ffe00;let calls=[];
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
m.trap(0x8023a370,c=>calls.push({slot:m.r32(c.get('a2')+0xe4),position:memory(c.get('a2')+0x11c,12).toString('hex')}));
const layouts=[[[0,3,1],[6,3,0]],[[0,3,0],[6,3,1]],[[0,3,1]],[[0,3,1],[6,4,0]],[[0,3,0],[3,3,0],[6,3,1]],[]];
const cases=[];
for(const layout of layouts)for(const owner of [0,10,25,123,99])for(const present of [false,true])for(const facing of [-1,0,1,2]){
 m.load(root,Buffer.alloc(0x1d00));m.w32(0x8022a974,root);m.w32(0x801ce8bc,cast);m.set('sp',sp);calls=[];
 const rows=Array.from({length:28},()=>Buffer.alloc(248));for(const row of rows){row.writeUInt32BE(present?0x1234:0,0x48);row.writeUInt32BE(owner,0x4c);}
 rows.forEach((r,i)=>m.load(cast+0x1c4+i*248,r));
 const initial=[];
 for(const [slot,identity,secondary] of layout){
  const p=pool+slot*368,b=Buffer.alloc(336);b.writeUInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt32BE(30,0xec);b.writeInt16BE(facing,0x13a);b[0x147]=identity;b[0x149]=secondary;b[0x146]=0;b[0x13c]=255;
  for(let i=0;i<16;i++)b[i]=255;for(let i=0;i<3;i++){b[0x142+i]=255;b.writeFloatBE(1,0x104+i*4);}
  b.writeFloatBE(slot*9+.25,0x11c);b.writeFloatBE(slot*2-4.5,0x120);b.writeFloatBE(slot*-5+3.75,0x124);
  m.load(p-16,Buffer.alloc(368,0xa7));m.load(p,b);m.w32(root+0x18+slot*4,p);initial.push({slot,record:b.toString('hex')});
 }
 m.call(0x8023ac50);assert.equal(m.get('sp'),sp);assert.deepEqual(calls.map(c=>c.slot),layout.map(r=>r[0]));
 for(const [slot]of layout){const p=pool+slot*368;assert(memory(p-16,16).equals(Buffer.alloc(16,0xa7)));assert(memory(p+336,16).equals(Buffer.alloc(16,0xa7)));}
 cases.push({rows:rows.map(r=>r.toString('hex')),initial,calls});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log(`${cases.length} original linked placement prepasses; native classification and position writes before matrix callbacks; camera/matrix services controlled.`);
