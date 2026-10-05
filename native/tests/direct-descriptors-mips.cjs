"use strict";
// Original loader slices through descriptor-key publication. Heap and resource
// services are controlled; this establishes dynamic directory size/indexing.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const input=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
const rom=fs.readFileSync(input.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const directory=Buffer.from(input.directory,'hex');
const specs=[
 [0x6fee4,0x6ff24,'s3',false,false], [0x1b8aac,0x1b8aec,'s3',false,false],
 [0x1d58a8,0x1d5900,'s4',false,true], [0x1e3694,0x1e36d4,'s3',true,false],
 [0x205d5c,0x205d9c,'s3',false,false], [0x207444,0x207484,'s3',true,false]
];
for(const [start,end,reg,usesS1,fixed] of specs)for(const handle of [1,37,input.count]) {
 const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);
 const base=0x80100000,record=0x80200000,resident=0x80280000;
 cpu.set('sp',0x80060000);cpu.set(reg,record);cpu.set('s1',handle);cpu.w32(record,handle);
 cpu.load(base,rom.subarray(start,end));cpu.load(resident-16,Buffer.alloc(directory.length+32,0x5a));
 let sizes=0,allocations=0,reads=0;
 cpu.trap(0x8009DAF4,c=>{
  if(sizes++===0){assert.equal(c.get('a0'),input.key);c.set('v0',directory.length);}
  else {assert.equal(c.get('a0'),directory.readUInt32BE(((fixed?37:handle)-1)*4));c.set('v0',16);}
 });
 for(const service of [0x80070F30,0x80071C04])cpu.trap(service,c=>{assert.equal(c.get('a0'),directory.length);c.set('v0',resident);allocations++;});
 cpu.trap(0x8009DBB8,c=>{assert.equal(c.get('a0'),resident);assert.equal(c.get('a1'),input.key);c.load(resident,directory);reads++;});
 cpu.call(base,base+end-start);
 assert.equal(sizes,2);assert.equal(allocations,1);assert.equal(reads,1);
 assert.equal(cpu.r32(record+4),directory.readUInt32BE(((fixed?37:handle)-1)*4));
 for(let i=0;i<16;i++){assert.equal(cpu.r8(resident-16+i),0x5a);assert.equal(cpu.r8(resident+directory.length+i),0x5a);}
}
console.log(`All six original loaders size/read ${input.count} descriptor entries; 18 first/fixed/last-handle cases passed in 4 MiB model memory.`);
