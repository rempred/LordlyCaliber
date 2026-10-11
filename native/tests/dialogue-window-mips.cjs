'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x80076f5c,rom.subarray(0x735c,0x745c));m.load(0x8007f648,rom.subarray(0xfa48,0xfc44));
m.load(0x8019ee58,rom.subarray(0xf9ff8,0xfa56c));m.load(0x80230858,rom.subarray(0x2a0088,0x2a05dc));
m.load(0x80239240,rom.subarray(0x2a8a70,0x2a8a7c));
const root=0x80300000,actor=0x80310000,tracking=0x80320010,pool=0x800e82c8,sp=0x803ffe00;
let allocations=[],point;
for(const p of [0x80178534,0x80227838,0x80227b9c,0x80227cbc])m.trap(p,c=>c.set('v0',0));
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),6);const p=tracking+allocations.length*64;allocations.push(p);m.load(p-16,Buffer.alloc(38,0xa7));c.set('v0',p);});
m.trap(0x800712c4,c=>assert(c.get('a0')===0||allocations.includes(c.get('a0'))));
m.trap(0x8022d534,c=>{assert(point);m.writeFloat(c.get('a1'),point[0]);m.writeFloat(c.get('a1')+4,point[1]);});
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const results=[];
for(const mask of [0,1,0x15,0x1f])for(const mode of [0,1])for(const wide of [false,true])for(const position of [null,[-12.75,188.25],[123.99,188.99],[70000,-65540.5]]){
 const initial=Buffer.alloc(6*168);for(let i=0;i<initial.length;i++)initial[i]=(i*19+5)&255;
 for(let i=0;i<6;i++)initial.writeUInt16BE(mask&(1<<i)?0x8123:0x123,168*i);
 m.load(pool,initial);m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w8(0x8018fc19,2);
 m.w32(root+24+27*4,position?actor:0);m.load(actor,Buffer.alloc(336));
 m.w32(0x800e79a0,8);m.w32(0x800c4c20,0xffff);m.w16(0x800e810e,13);
 m.load(0x8019ee40,Buffer.alloc(8,0xa7));m.w8(0x8019ee48,2);m.set('sp',sp);allocations=[];point=position;
 const a=[wide?65537:3,wide?65556:20,wide?65538:2,27,wide?511:0,wide?65530:10,192,wide?256:1,wide?2:1,1,wide?0x80000000:84,1,mode];
 const ctor=[a[0],a[2],a[4],a[5],a[7],a[3],(a[8]===1?1:0)|(a[9]===1?2:0),a[1],a[12],(-a[10])>>>0];
 ctor.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+i*4,v));m.call(0x80230858);
 const states=[];
 function capture(operation){
  m.set('a0',a[0]);m.call(0x80230b24);const query=m.get('v0');
  const bindings=Array.from({length:28},(_,i)=>{const p=m.r32(root+0x1d8+i*4);return p?read(p,6):'';});
  states.push({operation,records:read(pool,6*168),bindings,control:read(0x8019ee40,8),query});
  assert.equal(m.get('sp'),sp);
  for(const p of allocations)for(let i=0;i<16;i++){assert.equal(m.r8(p-16+i),0xa7);assert.equal(m.r8(p+6+i),0xa7);}
 }
 capture({kind:'create'});
 // Retargeting the same Actor leaves the old window resource live, while its
 // single tracking record changes to the new slot. Leave at least two free.
 if(mask!==0x1f){ctor.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+i*4,v));m.call(0x80230858);capture({kind:'create'});}
 point=[-31.875,188.999];m.call(0x80230b60);capture({kind:'place',point});
 m.set('a0',a[0]);m.call(0x80230b44);capture({kind:'resume'});
 // First matching resource only, even when the tracking points at another.
 const matching=Array.from({length:6},(_,i)=>i).find(i=>m.r32(pool+i*168+16)===0x80198be8);
 m.w8(pool+matching*168+0x8a,m.r8(pool+matching*168+0x8a)|1);capture({kind:'pause',slot:matching});
 for(const close of [a[0]+65536,a[0],1,3]){m.set('a0',close);m.call(0x80230d18);capture({kind:'close',id:close});}
 results.push({initial:initial.toString('hex'),args:a,point:position,states});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original dialogue windows: ${results.length} sequences, ${results.reduce((n,c)=>n+c.states.length,0)} whole-pool/tracking snapshots. Projection, allocation and shared name-context services controlled.`);
