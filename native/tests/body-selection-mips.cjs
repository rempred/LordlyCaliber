'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const raw=k=>Buffer.from(OB64.art.readResource(rom,k).stored),decode=k=>Buffer.from(OB64.art.readCompressedResource(rom,k).decoded);
const handles=raw(0x315736),directory=raw(0x3b6cd0),start=handles.readUInt16BE(0),end=handles.readUInt16BE(2);
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022d7d0,rom.subarray(0x29d000,0x2a8d00));m.load(0x802395c0,rom.subarray(0x2a8d20,0x2ae338));
m.load(0x801bdb70,rom.subarray(0x201000,0x212f00));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
if(process.argv[4])m.load(0x80239c04,fs.readFileSync(process.argv[4]));
m.load(0x801d0458,rom.subarray(0x2138e8,0x213990)); // Three 14-entry registration switch tables.
m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
const scene=0x80300000,cast=0x80301010,actors=0x80310010,table=0x80320000,program=0x80340010,pool=0x80350000,sp=0x803ffe00;
m.load(table,handles.subarray(start,end));m.w32(0x801d0690,table);m.w32(0x801d0680,table+0x1000);m.w32(0x801d0728,pool);
m.w32(0x8022a974,scene);m.w32(0x801ce8bc,cast);m.w32(0x801ce8fc,0x3c);
let registrations=[],before=[],poseCalls=0,extendedBank=false;
// The original registration code runs, including its special-class variants.
// Resource allocation is replaced at the loader boundary; pose getter below
// supplies the actual current-ROM counted program for the requested handle.
m.trap(0x801c27f8,c=>{
 const p=c.get('a0'),source=c.get('a1')|0,owner=c.get('a2')|0,flagA=c.get('a3')|0;
 registrations.push([source,owner,flagA,c.r32(c.get('sp')+16)|0,c.r32(c.get('sp')+20)|0]);
 c.set('v0',p);
});
m.trap(0x801c2ce4,c=>{
 const handle=c.get('a0'),desc=raw(directory.readUInt32BE((handle-1)*4)),bytes=decode(desc.readUInt32BE(4));
 let current=bytes;
 if(extendedBank){const count=bytes.readUInt32BE(0)/4,rows=[];for(let state=0;state<258;state++){const selected=state>=256?90:state>=100?40:state;const start=bytes.readUInt32BE(selected*4),end=selected+1<count?bytes.readUInt32BE((selected+1)*4):bytes.length;rows.push(bytes.subarray(start,end));}const header=Buffer.alloc(258*4);let at=header.length;rows.forEach((row,index)=>{header.writeUInt32BE(at,index*4);at+=row.length;});current=Buffer.concat([header,...rows]);}
 m.load(program,current);c.set('v0',program);
});
m.trap(0x801c5938,()=>{});
const exec=m.exec.bind(m);m.exec=(pc,next)=>{
 if(pc===0x8022e9e8){const ptr=m.get('a0');before.push({slot:m.r32(ptr+0xe4),record:read(ptr,336)});poseCalls++;}
 return exec(pc,next);
};
function read(ptr,size){return Buffer.from(m.ram.subarray(m.phys(ptr),m.phys(ptr)+size)).toString('hex');}
function record(slot,identity=255,secondary=0){
 const b=Buffer.alloc(336,0xa7);b.writeUInt32BE(slot,0xe4);b.writeInt32BE(30,0xe8);b.writeInt32BE(30,0xec);
 b.writeInt16BE(10,0x138);b.writeInt16BE(0,0x134);b.writeInt16BE(0,0x13a);b[0x13d]=0;b[0x146]=0;b[0x147]=identity;b[0x149]=secondary;
 b.writeFloatBE(1.25,0x11c);b.writeFloatBE(-2.5,0x120);b.writeFloatBE(3.75,0x124);return b;
}
const results=[];
function run(name,args,options={}){
 extendedBank=!!options.extended;
 const mapping=Buffer.alloc(16,255);
 if(extendedBank){mapping.writeUInt32BE(0x0a0a0064,0);mapping.writeUInt16BE(256,4);mapping.writeUInt16BE(0,6);mapping.writeUInt32BE(0x0a0a0065,8);mapping.writeUInt16BE(257,12);mapping.writeUInt16BE(0,14);}
 m.load(table-16,mapping);
 const rows=Array.from({length:28},()=>Buffer.alloc(248));
 if(options.paired){rows[3].writeUInt32BE(10,0x48);rows[3].writeUInt32BE(options.ownerKind??10,0x4c);rows[3].writeUInt16BE(options.item??1,0x36);}
 let initial=[{slot:4,record:record(4,options.paired?3:255,options.secondary?1:0)}];
 if(options.paired&&!options.secondary)initial.push({slot:8,record:record(8,3,1)});
 if(options.paired)for(const a of initial){a.record.writeInt32BE(10,0xe8);a.record.writeInt32BE(10,0xec);}
 if(options.occupied)for(let i=0;i<options.occupied;i++)initial.push({slot:i<4?i:i+1,record:record(i<4?i:i+1,i)});
 if(options.empty)initial=[];
 m.load(scene,Buffer.alloc(0x100));m.load(cast-16,Buffer.alloc(0x1c4+28*248+32,0xa7));
 rows.forEach((r,i)=>m.load(cast+0x1c4+i*248,r));m.load(pool,Buffer.alloc(20*0xb0));
 for(let i=0;i<28;i++)m.load(actors+i*0x200-16,Buffer.alloc(368,0xa7));
 initial.forEach(a=>{m.load(actors+a.slot*0x200,a.record);m.w32(scene+24+a.slot*4,actors+a.slot*0x200);});
 registrations=[];before=[];poseCalls=0;m.set('sp',sp);
 args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));
 try{m.call(0x80239c04,0x80000100,500000);}catch(e){throw Error(name+': '+e.message);}
 assert.equal(m.get('sp'),sp);
 const after=initial.map(a=>({slot:a.slot,record:read(actors+a.slot*0x200,336)}));
 for(const a of initial)for(let i=0;i<16;i++){assert.equal(m.r8(actors+a.slot*0x200-16+i),0xa7);assert.equal(m.r8(actors+a.slot*0x200+336+i),0xa7);}
 for(let i=0;i<16;i++){assert.equal(m.r8(cast-16+i),0xa7);assert.equal(m.r8(cast+0x1c4+28*248+i),0xa7);}
 results.push({name,args,initial:initial.map(a=>({slot:a.slot,record:a.record.toString('hex')})),rows:rows.map(r=>r.toString('hex')),
  after,before,registrations,poseCalls,afterRows:rows.map((r,i)=>read(cast+0x1c4+i*248,248))});
}
run('Graduation body',[4,32,32,10,1,0,0,0]);
run('inherit all',[4,-1,-1,-1,-1,-1,0,0]);
run('keep ground and identity',[4,30,30,10,0,0,1,1],{paired:true,ownerKind:30});
run('paired primary',[4,10,10,10,0,0,0,0],{paired:true});
run('paired inherited',[4,-1,-1,-1,-1,-1,0,0],{paired:true});
run('secondary offset',[4,10,10,0,0,0,0,0],{paired:true,secondary:true});
run('secondary already high',[4,10,10,50,0,0,0,0],{paired:true,secondary:true});
run('other paired owner',[4,10,10,10,0,0,0,0],{paired:true,ownerKind:25});
run('third paired owner',[4,10,10,10,0,0,0,0],{paired:true,ownerKind:123});
run('non-paired owner',[4,10,10,10,0,0,0,0],{paired:true,ownerKind:124});
run('allocate identity twenty',[4,32,32,10,1,0,0,0],{occupied:20});
run('no actor',[4,30,30,0,0,0,0,0],{empty:true});run('no slot',[-1,30,30,0,0,0,0,0]);
for(const source of [1,10,32,83,123])run('family owner '+source,[4,source,rom[0x5db14+source*72+69],0,0,0,0,0]);
run('different owner',[4,1,32,10,1,0,0,0]);
for(let source=1;source<165;source++)for(let flags=0;flags<4;flags++)
 run('class '+source+' flags '+flags,[4,source,source,0,flags&1,flags>>1,0,0]);
assert.deepEqual(results.slice(0,675),JSON.parse(fs.readFileSync(process.argv[5],'utf8')).results,'Rebuilt C must preserve the original 675 body-selection cases');
if(process.argv[6]!=='legacy'){
run('legacy paired attack',[4,10,10,40,0,0,0,0],{paired:true});
run('extended paired attack100',[4,10,10,100,0,0,0,0],{paired:true,extended:true});
run('extended paired attack101',[4,-1,-1,101,-1,-1,0,0],{paired:true,extended:true});
run('unregistered paired attack102',[4,10,10,102,0,0,0,0],{paired:true,extended:true});
const reference=results[results.length-4];
for(const [i,state] of [[results.length-3,100],[results.length-2,101]]){
 const actual=results[i];assert.equal(actual.poseCalls,2);assert.deepEqual(actual.registrations,reference.registrations);assert.deepEqual(actual.afterRows,reference.afterRows);
 for(let actor=0;actor<2;actor++)for(const field of ['before','after']){
  const got=Buffer.from(actual[field][actor].record,'hex'),want=Buffer.from(reference[field][actor].record,'hex');
  assert.equal(got.readInt16BE(0x134),actor ? state + 156 : state);assert.equal(got.readInt16BE(0x138),actor ? state + 156 : state);
  want.copy(got,0x134,0x134,0x136);want.copy(got,0x138,0x138,0x13a);assert.deepEqual(got,want,actual.name+' '+field+' '+actor);
 }
}
const skipped=results[results.length-1];assert.equal(skipped.poseCalls,1);assert.equal(skipped.after[1].record,skipped.initial[1].record);
console.log('C agrees with675 original cases. Extended paired100/101 with companions256/257: both actors match original40/90 apart from selectors; unregistered102 retains skipped secondary.');
}
fs.writeFileSync(process.argv[3],JSON.stringify({results}));
console.log(`Original body initialization: ${results.length} cases; actual appearance/item/classification/registration/pose code. Allocation and resource getter controlled; linked sound objects absent.`);
