'use strict';
// Compare the actual original/exported scenario initialization block. Only the
// story-flag query is controlled; the native copy, loop and branch delays run.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
function read(file){const b=fs.readFileSync(file);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const candidate=read(process.argv[2]),original=read(process.argv[3]);
const header=0x27c5700,offset=candidate.readUInt32BE(header+12),address=candidate.readUInt32BE(header+16);
const data=candidate.subarray(offset,offset+candidate.readUInt32BE(header+20));
const rows=data.readUInt32BE(44),initial=address+data.readUInt32BE(24),working=address+data.readUInt32BE(28);
assert(rows>=8);
assert.equal(candidate.readUInt32BE(0x101ddc),original.readUInt32BE(0x101ddc),'scenario setup must retain its native entry');
function run(image,key,flag,expanded){
 const m=new FloatMips();m.load(0x80070000,image.subarray(0x400,0x2f000));
 m.load(0x801ad5a0,image.subarray(0x101ce0,0x145210));m.set('sp',0x807ff000);
 if(expanded){const at=image.readUInt32BE(0x5dc8c),size=image.readUInt32BE(0x5dc90),ram=image.readUInt32BE(0x5dc98);m.load(ram,image.subarray(at,at+size));}
 const count=expanded?rows:8,src=expanded?initial:0x801eb318,dst=expanded?working:0x801f36d8;
 const pattern=Buffer.from(Array.from({length:count*64},(_,i)=>(i*37+17)&255));
 m.load(src,pattern);m.load(dst,Buffer.alloc(count*64,0xa5));
 const tail=Buffer.from(m.ram.subarray(m.phys(dst+count*64),m.phys(dst+count*64)+16));
 m.w8(0x8018f481,key);let queried=[];
 m.trap(0x80186210,c=>{queried.push(c.get('a0'));c.set('v0',flag);});
 // Arguments already prepared by the preceding native instructions.
 m.set('a0',0x801eb318);m.set('a1',0x801f36d8);
 m.call(0x801ae670,0x801ae6f4); // z64 102DB0..102E34, scenario overlay.
 const result=Buffer.from(m.ram.subarray(m.phys(dst),m.phys(dst)+count*64));
 const expected=Buffer.from(pattern);
 if(flag&&(key===13||key===17))for(let r=0;r<count;r++)expected.writeUInt16BE(0xffff,r*64+46);
 assert.deepEqual(result,expected,`table initialization ${key}/${flag}/${expanded}`);
 assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(src),m.phys(src)+count*64)),pattern,'source remains intact');
 assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(dst+count*64),m.phys(dst+count*64)+16)),tail,'no write beyond table');
 assert.equal(m.get('sp'),0x807ff000,'helper restores caller stack');
 return {table:result.subarray(0,512),queried};
}
for(const key of [1,13,17])for(const flag of [0,1])
 assert.deepEqual(run(candidate,key,flag,true),run(original,key,flag,false));
console.log(`PASS original/exported movement setup: 6 scenario/flag cases, ${rows} rows, native copy, storm column, branch continuation and bounds.`);
