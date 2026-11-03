'use strict';
// Execute the actual original/exported naming functions with real name resources.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const read=p=>{const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;};
const [edited,original,expanded]=process.argv.slice(2).map(read);
function runtime(rom){
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);
 const capacityAt=rom.indexOf(Buffer.from("LC07CAPA"),0x2800000);
 const enemyBase=capacityAt<0?0x80195560:rom.readUInt32BE(capacityAt+24),enemyCount=capacityAt<0?100:256;
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
 if(rom.subarray(0x5dc80,0x5dc88).toString()==='LC05CLAS'){
  const d=rom.subarray(0x5dc80);m.load(d.readUInt32BE(24),rom.subarray(d.readUInt32BE(12),d.readUInt32BE(12)+d.readUInt32BE(16)));
 }
 let randomCalls=0,resourceCalls=0;
 m.trap(0x8009dd38,c=>{const key=c.get('a0');const real=OB64.art.readCompressedResource(rom,key).decoded;m.load(0x803f0000,real);resourceCalls++;c.set('v0',0x803f0000);});
 m.trap(0x8009c7cc,c=>{randomCalls++;c.set('v0',42);});
 m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
 const str=at=>{const b=[];while(m.r8(at)){b.push(m.r8(at++));assert(b.length<=16,'bounded character name');}return Buffer.from(b);};
 m.trap(0x8009c550,c=>{const b=str(c.get('a1'));m.load(c.get('a0'),Buffer.concat([b,Buffer.from([0])]));c.set('v0',c.get('a0'));});
 m.trap(0x8009c5e0,c=>c.set('v0',Buffer.compare(str(c.get('a0')),str(c.get('a1')))));
 m.trap(0x800712c4,()=>{});
 function run(id,mode,sex=0,duplicate){
  randomCalls=resourceCalls=0;
  const player=Buffer.alloc(56*100),enemy=Buffer.alloc(52*enemyCount);player.write('Existing saved',56*10);player[56*10+17]=5;
  enemy.write('Existing enemy',52*10);enemy[52*10+17]=5;
  if(duplicate){enemy.write(duplicate.name,52*200);enemy[52*200+17]=5;}
  m.load(0x80193bc0,player);m.load(enemyBase,enemy);
  if(capacityAt>=0){m.load(enemyBase+0x3400,Buffer.alloc(256,1));if(duplicate)m.w8(enemyBase+0x3400+200,duplicate.tag);}
  const base=mode?enemyBase:0x80193bc0,stride=mode?52:56,count=mode?enemyCount:100,at=base+stride*2;
  m.w8(at+17,id);m.w8(at+18,5);m.w8(at+20,sex);
  const before=Buffer.from(m.ram.subarray(m.phys(base),m.phys(base)+stride*count));
  m.set('sp',0x803ffe00);m.set('a0',mode);m.set('a1',2);m.call(0x8016bd20);
  const after=Buffer.from(m.ram.subarray(m.phys(base),m.phys(base)+stride*count));
  assert.deepEqual(after.subarray(0,2*stride),before.subarray(0,2*stride));
  assert.deepEqual(after.subarray(2*stride+17),before.subarray(2*stride+17));
  assert.equal(m.get('sp'),0x803ffe00);
  return {name:str(at).toString('hex'),randomCalls,resourceCalls};
 }
 return {run};
}
const before=runtime(original),after=runtime(edited);let comparisons=0;
for(let mode=0;mode<2;mode++)for(let sex=0;sex<3;sex++)for(let id=0;id<165;id++){
 if([0x51,0x75,0x76].includes(id))continue;
 assert.deepEqual(after.run(id,mode,sex),before.run(id,mode,sex),`native identity ${id}, side ${mode}, name pool ${sex}`);comparisons++;
}
for(let mode=0;mode<2;mode++){
 assert.equal(Buffer.from(after.run(0x51,mode).name,'hex').toString(),'Rowan');
 assert.equal(Buffer.from(after.run(165,mode).name,'hex').toString(),'Rowan');
 assert.deepEqual(after.run(0x75,mode),before.run(0x5b,mode));
 assert.deepEqual(after.run(0x76,mode),before.run(5,mode));
}
console.log(`PASS: ${comparisons} native identity/side/random-pool comparisons; independent shared-name assignment, custom ID165, random reassignment, unchanged other records and non-name fields.`);

if(expanded){
 const capacity=runtime(expanded);let checks=0;
 for(let mode=0;mode<2;mode++)for(let sex=0;sex<3;sex++)for(let id=0;id<165;id++){
  assert.deepEqual(capacity.run(id,mode,sex),before.run(id,mode,sex),`expanded identity ${id}, side ${mode}, name pool ${sex}`);checks++;
 }
 const name=Buffer.from(before.run(5,1).name,'hex').toString();
 assert.equal(capacity.run(5,1,0,{name,tag:1}).randomCalls,32,'duplicate search reaches normal character in slot200');
 assert.equal(capacity.run(5,1,0,{name,tag:2}).randomCalls,1,'Soldier slot200 is excluded from name comparisons');
 console.log(`PASS: ${checks} expanded-army naming comparisons, slot200 duplicate retries and Soldier exclusion.`);
}
