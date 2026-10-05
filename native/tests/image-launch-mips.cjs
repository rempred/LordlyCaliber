'use strict';
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const sandbox={window:{OB64:{}}};vm.createContext(sandbox);vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../cutscene-data.js'),'utf8'),sandbox);
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x80178e60,rom.subarray(0x4ed60,0x4ef10));
const output=0x80300010,sp=0x803ffe00;let heap,program;
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const bytes=words=>{const b=Buffer.alloc(words.length*4);words.forEach((v,i)=>b.writeUInt32BE(v>>>0,i*4));return b;};
m.trap(0x8009daf4,c=>c.set('v0',c.get('a0')===0x19a8804?4:program.length));
m.trap(0x80071c04,c=>{c.set('v0',heap);heap+=(c.get('a0')+15)&~15;assert(heap<0x803f0000);});
m.trap(0x8009dbb8,c=>m.load(c.get('a0'),c.get('a1')===0x19a8804?bytes([0x123456]):program));
m.trap(0x800712c4,c=>c.set('v0',0));
m.trap(0x8007a7e0,c=>c.set('v0',program.length));
m.trap(0x8007a110,c=>m.load(c.get('a0'),read(c.get('a1'),program.length)));
const cases=[];
function run(words,key=0){
 program=bytes(words);heap=0x80310000;
 const initial=Buffer.from('0000ffffffff0000','hex');m.load(output-16,Buffer.alloc(40,0xa7));m.load(output,initial);
 m.set('sp',sp);m.set('a0',0);m.set('a1',output);m.call(0x80178e60);assert.equal(m.get('sp'),sp);
 assert(read(output-16,16).equals(Buffer.alloc(16,0xa7)));assert(read(output+8,16).equals(Buffer.alloc(16,0xa7)));
 cases.push({key,words,record:read(output,8).toString('hex')});
}
for(const words of [
 [0x80000001,0xff000004],
 [0x80000007,1,0x80000001,0xff000004],
 [0x80000007,0x10002,0x80000006,0x1ffff,0x80000001,0xff000005],
 [0x80000007,0xffff,0x80000007,5,0x80000001,0xff000004],
 [0x80000008,0,0x80000007,2,0x80000001,0xff000004],
 [0x80000006,0,0x80000008,0,0x80000007,2,0x80000001,0xff000004],
 [0x14,0,0x80000007,2,0,0,0,0x80000001,0xff000004],
 [0x80000001,0x80000007,3,0xff000004],
 [0x80000006,5,0],
 [0xff0012fe]
])run(words);
for(const scene of sandbox.window.OB64.cutsceneData.scenes){
 if(!scene.launchProfile?.oversizedImagePresentation)continue;
 const key=parseInt(scene.directorKey,16),decoded=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded);
 const words=Array.from({length:decoded.length/4},(_,i)=>decoded.readUInt32BE(i*4));run(words,key);
}
const rows=sandbox.window.OB64.cutsceneData.oversizedImagePresentationRules.rows.map(r=>({row:r.rowSelector,image:r.assetId}));
fs.writeFileSync(process.argv[3],JSON.stringify({cases,rows}));
console.log(`${cases.length} original launch scans including all117 image-launch streams;69 retained independently researched launch-row expectations. Resource transfer/decompression/allocation controlled.`);
