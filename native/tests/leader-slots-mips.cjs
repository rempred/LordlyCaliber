'use strict';
// Execute complete original/rebuilt normalizers; no service traps or host predicate.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const load=p=>{const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;};
const rom=load(process.argv[2]),candidate=load(process.argv[3]),baseline=process.argv[5]==='baseline';
const mappings=fs.readFileSync(process.argv[4]),header=candidate.subarray(0x27c50a0,0x27c5100);
assert.equal(header.subarray(0,8).toString(),'LC05LEAD');assert.equal(header.readUInt32BE(12)*8,mappings.length);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801bdb70,rom.subarray(0x201000,0x212f00));
m.load(0x802395c0,candidate.subarray(0x2a8d20,0x2ae338));
const scene=0x80300010,cast=0x80310010,actors=0x80320010,table=0x80340010,sp=0x803ffe00;
m.w32(0x8022a974,scene);m.w32(0x801ce8bc,cast);m.w32(0x801d0690,table);
m.load(table-mappings.length,mappings);
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
function run(code,rows,before){
 m.load(0x8023be14,code);m.load(scene-16,Buffer.alloc(28*4+24+32,0xa7));m.load(scene,Buffer.alloc(28*4+24));
 rows.forEach((r,i)=>m.load(cast+0x1c4+i*248,r));
 before.forEach((a,i)=>{if(!a)return;const p=actors+i*384;m.load(p-16,Buffer.alloc(368,0xa7));m.load(p,a);m.w32(scene+24+i*4,p);});
 m.set('sp',sp);m.call(0x8023be14);assert.equal(m.get('sp'),sp);
 const after=Array.from({length:28},(_,i)=>{const p=m.r32(scene+24+i*4);return p?read(p,336):null;});
 before.forEach((a,i)=>{if(!a)return;for(let j=0;j<16;j++){assert.equal(m.r8(actors+i*384-16+j),0xa7);assert.equal(m.r8(actors+i*384+336+j),0xa7);}});
 rows.forEach((r,i)=>assert.deepEqual(read(cast+0x1c4+i*248,248),r));
 assert.deepEqual(read(table-mappings.length,mappings.length),mappings);
 for(let j=0;j<16;j++){assert.equal(m.r8(scene-16+j),0xa7);assert.equal(m.r8(scene+136+j),0xa7);}
 return after;
}
function actor(slot,row,pose,flags=0,source=10,owner=10){const a=Buffer.alloc(336);a.writeUInt32BE(slot+1,0xdc);a.writeUInt32BE(slot,0xe4);a.writeUInt32BE(source,0xe8);a.writeUInt32BE(owner,0xec);a.writeInt16BE(pose,0x138);a.writeInt16BE(flags&1,0x13a);a[0x146]=flags>>1;a[0x147]=row;return a;}
const original=rom.subarray(0x2ab574,0x2ab6bc),rebuilt=candidate.subarray(0x2ab574,0x2ab6bc),cases=[];
const record=(rows,before,after)=>cases.push({rows:rows.map(x=>x.toString('hex')),before:before.map(x=>x&&x.toString('hex')),after:after.map(x=>x&&x.toString('hex'))});
for(let seed=0;seed<48;seed++){
 const rows=Array.from({length:20},(_,i)=>{const r=Buffer.alloc(248);r.writeUInt32BE([0,0x100,0x200,0x300,0x201,0xffffffff][(i+seed)%6],0x40);r.writeUInt32BE(10,0x48);r.writeUInt32BE([10,25,123,0,30,255][(i*5+seed)%6],0x4c);return r;});
 const poses=[-32768,-1,0,49,50,99,102,255,256,32767];
 const before=Array.from({length:28},(_,i)=>(i+seed)%5===0?null:actor(i,(i+seed)%20,poses[(i+seed*3)%poses.length],3));
 const expected=run(original,rows,before),actual=run(rebuilt,rows,before);assert.deepEqual(actual,expected,'legacy scan '+seed);record(rows,before,actual);
}
let extended=0;
for(const side of [0,1])for(const [pose,flags,source,owner] of [[100,0,10,10],[101,0,10,10],[102,0,10,10],[256,0,10,10],[257,0,10,10],[100,1,10,10],[100,0,11,10],[100,0,10,25]]){
 const rows=Array.from({length:20},()=>Buffer.alloc(248));rows[3].writeUInt32BE(0x200|side<<8,0x40);rows[3].writeUInt32BE(10,0x48);rows[3].writeUInt32BE(10,0x4c);
 const before=Array(28).fill(null);before[side]=actor(side,0,0);before[5]=actor(5,3,pose,flags,source,owner);before[8]=actor(8,3,256);
 const expected=before.map(x=>x&&Buffer.from(x));
 if(!baseline&&[100,101].includes(pose)&&!flags&&source===10&&owner===10){[expected[side],expected[5]]=[expected[5],expected[side]];expected[side].writeUInt32BE(side,0xe4);expected[5].writeUInt32BE(5,0xe4);}
 const actual=run(rebuilt,rows,before);assert.deepEqual(actual,expected,'private leader '+[side,pose,flags,source,owner]);record(rows,before,actual);extended++;
}
fs.writeFileSync(process.argv[6],JSON.stringify({cases}));
console.log(`Leader slots: 48 complete original comparisons and ${extended} ${baseline?'restored':'private'} primary/companion cases passed in 4 MiB modeled RAM.`);
