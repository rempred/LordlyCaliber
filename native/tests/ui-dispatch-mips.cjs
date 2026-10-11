'use strict';
// Original callback dispatcher and priority/input/release/RNG helpers. Callback
// bodies, heap/payload services and record copy/clear are controlled explicitly.
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
for(const [a,b] of [[0x69d8,0x6ee8],[0x7688,0x7714],[0x7894,0x79ec],[0x8380,0x84d4],[0x859c,0x86ec],[0x2cbcc,0x2cc00]])m.load(0x8006fc00+a,rom.subarray(a,b));
const pool=0x800e82c8,scratch=0x800e7a30,data=0x800e91d0,queue=0x800c4c10,context=0x803e0000;
const init=0x802ff100,update=0x802ff104,results=[];let trace=[],mode=0,created=false,spawned=false;
const bytes=(a,n)=>Buffer.from(m.ram.subarray(m.phys(a),m.phys(a)+n));
const slot=()=>m.r32(0x800c4c20)|0;
const payload=i=>0x80300000+i*256;
const fresh=i=>{const b=Buffer.alloc(168);b.writeUInt16BE(0x8000,0);b[4]=2;b.writeUInt16BE(19,14);b.writeUInt32BE(init,16);b.writeUInt32BE(update,20);b.writeUInt16BE(32,32);b.writeUInt32BE(payload(i),36);b.writeUInt16BE(0xffff,0xa2);return b;};
m.trap(0x80093060,c=>m.load(c.get('a1'),bytes(c.get('a0'),c.get('a2'))));
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x800712c4,c=>trace.push({op:'free',handle:c.get('a0')}));
m.trap(0x800780d4,c=>{trace.push({op:'save',slot:slot(),head:!!(m.r8(scratch+2)&4),data:bytes(data,32).toString('hex')});m.load(m.r32(c.get('a3')),bytes(data,c.get('a2')));});
m.trap(0x80078164,c=>{trace.push({op:'restore',slot:slot()});m.load(c.get('a1'),bytes(c.get('a0'),32));});
m.trap(init,c=>{
 const i=c.get('a0');trace.push({op:'init',slot:i,head:!!(m.r8(scratch+2)&4),input:m.r32(0x800c4c4c)===0x800e8700,speed:m.r8(scratch+4),context:m.r32(scratch+0x98)});
 m.load(data,Buffer.alloc(32,0x30+i));m.w8(scratch+0x60,m.r8(scratch+0x60)+1);
 if(mode===2&&i===5&&!created&&!(m.r16(pool)&0x8000)){m.load(pool,fresh(0));created=true;}
 if(mode===2&&i===2)m.w8(scratch+2,m.r8(scratch+2)|2);
});
m.trap(update,c=>{
 const i=c.get('a0');trace.push({op:'update',slot:i,head:!!(m.r8(scratch+2)&4),input:m.r32(0x800c4c4c)===0x800e8700,context:slot(),before:bytes(data,32).toString('hex')});
 m.w8(scratch+0x60,m.r8(scratch+0x60)+3);m.w8(data,m.r8(data)+1);
 if(mode===2&&i===1)m.w8(scratch+2,m.r8(scratch+2)|2);
 if(mode===2&&i===3&&!spawned&&!(m.r16(pool+4*168)&0x8000)){m.load(pool+4*168,fresh(4));spawned=true;}
});
for(let mask=0;mask<64;mask++)for(mode=0;mode<3;mode++){
 const initial=Buffer.alloc(1008);for(let i=0;i<6;i++){
  const r=fresh(i);r.writeUInt16BE(mask&(1<<i)?(mode===1&&i%2?0xa000:0x8000):0,0);
  r.writeUInt16BE([1,9,0,9,4,0][i],14);r[3]=mode===1&&i%3===0?4:0;
  if(mode===1&&i===2)r.writeUInt32BE(0,0x10);
  if(mode===1&&i===5)r.writeUInt32BE(0,0x14);
  r.copy(initial,i*168);m.load(payload(i),Buffer.alloc(32,0x80+i));
 }
 m.load(pool,initial);m.load(data,Buffer.alloc(32,0xee));m.w32(context+4,0);m.w32(0x800af0b0,context);
 const seed=(0xfedcba98+mask*7919)>>>0,status=mode===1?0:0xffff,selected=!!(mask&1);
 m.w32(0x800c47d0,seed);m.w16(0x800c4c26,status);m.w32(0x800c4c4c,selected?0x800e8700:0x800af0a6);
 for(let i=0;i<6;i++)m.w16(queue+i*2,0xffff);
 m.set('sp',0x803ffe00);m.call(0x8007819c);const start=bytes(pool,1008).toString('hex');
 const entries=Array.from({length:6},(_,i)=>m.r16(queue+i*2)),count=m.r16(0x800c49d0);
 created=spawned=false;const snapshots=[];
 for(let frame=0;frame<3;frame++){
  trace=[];m.set('sp',0x803ffe00);m.call(0x800765d8);
  snapshots.push({trace,pool:bytes(pool,1008).toString('hex'),seed:m.r32(0x800c47d0),count:m.r16(0x800c49d0),entries:Array.from({length:6},(_,i)=>m.r16(queue+i*2)),current:slot(),input:m.r32(0x800c4c4c)===0x800e8700,payloads:Array.from({length:6},(_,i)=>bytes(payload(i),32).toString('hex'))});
 }
 results.push({id:`${mask}/${mode}`,initial:start,mode,seed,status,selected,count,entries,snapshots});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} dispatcher sequences / ${results.length*3} passes; callbacks, payload/heap services and record copy/clear controlled. Original scan, priority, input, recursive release, cleanup and RNG execute. Render lists explicitly empty.`);
