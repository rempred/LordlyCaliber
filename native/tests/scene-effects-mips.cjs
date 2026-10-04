'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x8022c988,rom.subarray(0x29c1b8,0x29c530));
m.load(0x802395c0,rom.subarray(0x2a8d20,0x2ae338));
const root=0x80300000,pool=0x80310010,input=0x80340000,display=0x80370000,sp=0x803ffe00,late=p=>p+0x802395c0-0x2a8d20;
let allocations=[];
m.trap(0x80070f30,c=>{const size=c.get('a0');assert([4,80].includes(size));const p=pool+allocations.length*112;allocations.push({p,size});m.load(p-16,Buffer.alloc(size+32,0xa7));m.load(p,Buffer.alloc(size));c.set('v0',p);});
m.trap(0x800712c4,c=>assert(allocations.some(a=>a.p===c.get('a0'))));
m.trap(0x802397c4,()=>{}); // Camera preparation, after the alpha display-list command.
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n)).toString('hex');
const results=[];
function start(a){a.slice(0,6).forEach((v,i)=>m.w32(input+i*4,v));m.set('a0',input);m.set('a1',input+12);m.set('a2',a[6]);m.set('a3',a[7]);m.call(0x8022c988);}
for(const slot of [-1,0,1,19])for(const duration of [0,1,2,3,30,32767,32768,65535,65536,65537,-1])for(const wide of [false,true]){
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.load(0x801d0828,Buffer.from('1122334455','hex'));m.set('sp',sp);allocations=[];
 const initial=[];
 if(wide){const b=Buffer.alloc(80,0xa7);b.writeInt16BE(3,0);b.writeInt16BE(1,2);b.set([5,10,15,10,20,30,60,50,40],4);const p=pool;allocations.push({p,size:80});m.load(p-16,Buffer.alloc(112,0xa7));m.load(p,b);m.w32(root+0x1bb4,p);initial.push({slot:0,record:b.toString('hex')});}
 const args=[wide?257:21,wide?-2:42,wide?0x7fffffff:63,200,17,96,duration,slot];
 const operations=[{args},...Array.from({length:6},()=>({})),{args:[99,88,77,11,22,33,2,1]},...Array.from({length:3},()=>({})),{args:[11,22,33,200,210,220,1,-1]},{},{}];
 const states=[];
 for(const op of operations){
  if(op.args)start(op.args);else m.call(0x8022cbd4);
  const records=Array.from({length:20},(_,i)=>{const p=m.r32(root+0x1bb4+i*4);return p?{slot:i,record:memory(p,80)}:null;}).filter(Boolean);
  states.push({records,shared:memory(0x801d0828,5)});assert.equal(m.get('sp'),sp);
  for(const a of allocations)for(let i=0;i<16;i++){assert.equal(m.r8(a.p-16+i),0xa7);assert.equal(m.r8(a.p+a.size+i),0xa7);}
 }
 results.push({initial,operations,states});
}
// Effect lifecycle and the renderer's actual primitive alpha, with no actors.
m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);allocations=[];m.set('sp',sp);
const special=[];
for(let step=0;step<70;step++){
 const restart=step===0||step===66;if(restart)m.call(late(0x2ada80));else m.call(late(0x2adab4));
 const p=m.r32(root+0x1cac);m.w32(0x800e9ba0,display);m.call(late(0x2adaf4));assert.equal(m.r32(display+0x28),0xfa000000);
 special.push({restart,record:memory(p,4),alpha:m.r32(display+0x2c)});assert.equal(m.get('sp'),sp);
}
const sources=[];
for(const source of [...Array.from({length:256},(_,i)=>i),-1,-2147483648,2147483647]){
 m.w32(input+0xe8,source);m.set('a0',input);m.call(late(0x2aa608));sources.push({source,visible:!!m.get('v0')});
}
fs.writeFileSync(process.argv[3],JSON.stringify({results,special,sources}));
console.log(`Original scene effects: ${results.length} color sequences, ${special.length} special-fade/render samples, ${sources.length} source predicates; allocation/free and camera preparation controlled.`);
