'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const table=OB64.art.readCompressedResource(rom,0x109eeb0).decoded;
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
const delta=0x8022e9e8-0x29e218,actor=0x80300010,scene=0x80310010,sp=0x803ffe00;
m.load(0x29d000+delta,rom.subarray(0x29d000,0x2a8d00));m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
m.load(0x801ceee0,rom.subarray(0x212370,0x212f00));
m.load(0x80320000,table);m.w32(0x8022a97c,0x80320000);m.w32(0x8022a974,scene);
let calls=0,allocations=0,terrain=[];
m.trap(0x8022e9e8,c=>{assert.equal(c.get('a0'),actor);calls++;}); // Compare the precise boundary before immediate pose execution.
m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),336);allocations++;c.set('v0',actor);});
m.trap(0x80093380,c=>{m.load(c.get('a0'),Buffer.alloc(c.get('a1')));});
m.trap(0x801bc35c,c=>{terrain.push([2,c.get('a0')|0,c.get('a1')|0]);c.setSingle(0,7.25);});
m.trap(0x8023d178,c=>{terrain.push([0,c.single(12),c.single(14)]);c.setSingle(0,7.25);});
m.trap(0x80232e44,c=>c.set('v0',0x80330000));
m.trap(0x801c5938,()=>{});
m.trap(0x8022d534,c=>{c.writeFloat(c.get('a1'),987);c.writeFloat(c.get('a1')+4,654);});
const rows=[];
function run(kind,name,args,options={}){
 const mode=options.mode??2,presentation=0x3c,slot=args[0];calls=allocations=0;terrain=[];
 const initial=Buffer.alloc(336,0xa7);
 initial.writeInt32BE(30,0xe8);initial.writeInt16BE(1,0x138);initial[0x13f]=2;initial[0x146]=1;
 initial.writeInt16BE(12,0x134);
 initial[0x13d]=0; // A valid ordinary decoder; other unrelated bytes stay poisoned.
 initial.writeFloatBE(10,0x11c);initial.writeFloatBE(20,0x120);initial.writeFloatBE(30,0x124);
 m.load(actor-16,Buffer.alloc(368,0xa7));m.load(actor,initial);m.w8(0x8018fc19,mode);m.w32(0x801ce8fc,presentation);
 m.w32(scene+24+Math.max(0,slot)*4,options.allocate?0:actor);m.set('sp',sp);
 for(let i=0;i<4;i++)m.set('a'+i,args[i]);
 for(let i=4;i<7;i++)m.writeFloat(sp+16+(i-4)*4,args[i]);
 for(let i=7;i<args.length;i++)m.w32(sp+16+(i-4)*4,args[i]);
 m.call((kind==='construct'?0x29d790:0x2a08c0)+delta);assert.equal(m.get('sp'),sp);
 for(let i=0;i<16;i++){assert.equal(m.r8(actor-16+i),0xa7);assert.equal(m.r8(actor+336+i),0xa7);}
 const expected=Buffer.from(m.ram.subarray(m.phys(actor),m.phys(actor)+336)).toString('hex');
 if(calls){
  const bank=m.r32(actor+0xe8),directory=0x109f95e+0x594284,descriptor=rom.readUInt32BE(directory+(bank-1)*4),key=rom.readUInt32BE(descriptor+0x594288);
  m.load(0x80330000,OB64.art.readCompressedResource(rom,key).decoded);
  const stub=m.traps.get(0x8022e9e8);m.traps.delete(0x8022e9e8);m.set('a0',actor);m.call(0x8022e9e8);m.traps.set(0x8022e9e8,stub);
 }
 const fullExpected=Buffer.from(m.ram.subarray(m.phys(actor),m.phys(actor)+336)).toString('hex');
 rows.push({kind,name,args,mode,presentation,initial:initial.toString('hex'),expected,fullExpected,calls,allocations,terrain});
}
run('construct','allocate',[0,30,1,2,1.25,2.5,3.75,7,2],{allocate:true});
run('construct','reuse',[27,30,1,2,1.25,2.5,3.75,7,2]);
run('construct','xy sentinels ignore z',[2,30,1,2,-1,-1,75,257,258]);
run('construct','terrain mode2',[0,30,1,2,65539.75,-1,-32769.5,7,2]);
run('construct','terrain mode0',[0,30,1,2,3.75,-1,-7.5,7,2],{mode:0});
run('construct','mode1 retains ground flag',[0,30,1,2,3.75,-1,-7.5,7,2],{mode:1});
run('construct','narrow animation',[0,30,65537,2,3,4,5,7,2]);
run('select','all inherited',[4,-1,-1,-1,-1,-1,-1,-1]);
run('select','z only ignored',[4,-1,-1,-1,-1,-1,500,-1]);
run('select','coordinates supplied',[4,-1,-1,-1,-1,6,500,-1]);
run('select','new source',[4,1,1,0,1,2,3,255]);
run('select','missing animation unchanged',[4,30,9999,0,1,2,3,255]);
run('select','no slot unchanged',[-1,30,1,0,1,2,3,255]);
run('select','full word lookup then narrowing',[4,30,-2147483648,0,-1,-1,-1,0]);
for(let bank=1;bank<=68;bank++)run('select','ordinary bank '+bank,[3,bank,0,0,1.5,-3.25,7.75,0]);
fs.writeFileSync(process.argv[3],JSON.stringify({rows}));
console.log(`Original actor records: ${rows.length} constructor/State cases, full 336-byte comparison boundary before pose; allocation, zero-fill, terrain and pose services controlled.`);
