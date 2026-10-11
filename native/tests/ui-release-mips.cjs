'use strict';
const fs=require('node:fs'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x80077f88,rom.subarray(0x8388,0x84d4));
const pool=0x800e82c8,results=[];let callbacks=[],freed=[],change=false;
for(const address of [0x802ff100,0x802ff104])m.trap(address,c=>{
 const slot=c.get('a0');callbacks.push({slot,address});
 if(change&&slot===1)m.w16(pool+4*168+0xa2,5);
 m.w32(pool+slot*168+0x38,0x12340000+slot);
});
m.trap(0x800712c4,c=>{freed.push(c.get('a0'));});
m.trap(0x80093380,c=>{if(c.get('a1')!==168)throw Error('unexpected clear');m.load(c.get('a0'),Buffer.alloc(168));});
for(let mask=0;mask<64;mask++)for(const root of [0,2,5])for(const mutate of [false,true]){
 const initial=Buffer.alloc(1008);for(let i=0;i<initial.length;i++)initial[i]=(i*17+mask*3)&255;
 for(let slot=0;slot<6;slot++){
  const at=slot*168;initial.writeUInt16BE(mask&(1<<slot)?0x8000:0,at);initial[at+3]=slot%2?8:0;
  initial.writeUInt16BE([2,5,5,2,0xffff,0xffff][slot],at+0xa2);initial.writeUInt32BE(slot%3?0x802ff100+(slot%2)*4:0,at+0x1c);
  initial.writeUInt32BE(slot%2?0x81000000+slot:0,at+0x24);
  for(let i=0;i<10;i++)initial.writeUInt32BE((i+slot)%3?0x82000000+slot*256+i:0,at+0x38+i*4);
 }
 m.load(pool,initial);callbacks=[];freed=[];change=mutate;m.set('sp',0x803ffe00);m.set('a0',root);m.call(0x80077f88);
 results.push({initial:initial.toString('hex'),root,mutate,callbacks,freed,after:Buffer.from(m.ram.subarray(m.phys(pool),m.phys(pool)+1008)).toString('hex')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`${results.length} recursive release sequences. Callback mutations, allocator release and zero-fill controlled; native recursion and handle traversal execute.`);
