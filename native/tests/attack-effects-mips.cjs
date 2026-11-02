'use strict';
// Execute the exported adapters and original status applier in isolated RAM.
// Native actor discovery, RNG, timing and event sinks are controlled services.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const directory=path.dirname(process.argv[2]);
function entries(file){const elf=fs.readFileSync(path.join(directory,file));const offset=elf.readUInt32BE(32),width=elf.readUInt16BE(46),count=elf.readUInt16BE(48),out={};for(let i=0;i<count;i++){const at=offset+i*width;if(elf.readUInt32BE(at+4)!==2)continue;const labels=offset+elf.readUInt32BE(at+24)*width,strings=elf.readUInt32BE(labels+16),start=elf.readUInt32BE(at+16),size=elf.readUInt32BE(at+20);for(let j=start;j<start+size;j+=16){const n=strings+elf.readUInt32BE(j),end=elf.indexOf(0,n);out[elf.toString('utf8',n,end)]=elf.readUInt32BE(j+4);}}return out;}
const e=entries('attack_runtime.elf'),h=rom.subarray(0x27c5300,0x27c5360),records=h.readUInt32BE(28),settings=h.readUInt32BE(32),scratch=e.lc_attack_scratch;
const m=new FloatMips();m.ram=new Uint8Array(0x800000); // Existing OBSO animation module occupies 0x80460000.
const execute=m.exec.bind(m);m.exec=(pc,next)=>{try{return execute(pc,next);}catch(error){error.message+=` at PC 0x${pc.toString(16)}`;throw error;}};
m.load(0x80070000,rom.subarray(0x400,0x2f000));
if(rom.toString('ascii',0x27b0000,0x27b0004)==='OBSO'){
 m.load(0x800a4c00,rom.subarray(0x35000,0x35100));
 m.load(0x80460000,rom.subarray(0x27b0000,0x27b0800));
}
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
const cp=rom.subarray(0x5dc80,0x5dd00),base=cp.readUInt32BE(24),at=cp.readUInt32BE(12),end=cp.readUInt32BE(48);m.load(base,rom.subarray(at,at+end-base));
const stack=0x803ff000,actors=0x803c0000,global=0x803e0000,ids=global+0x1000,packet=global+0x1100;
m.set('sp',stack);m.w32(0x801ce8c0,global);m.w32(0x801ce8bc,global);
function call(name,args,limit=200000){args.slice(0,4).forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));args.slice(4).forEach((v,i)=>m.w32(stack+16+i*4,v));m.call(typeof name==='number'?name:e[name],0x80000100,limit);assert.equal(m.get('sp'),stack);return m.get('v0')|0;}
const actor=i=>actors+i*0x100;
const services={lookup:0x801c8fe8,dead:0x801c8e30,immune:0x801f46dc,up:0x801da4b4,down:0x801da4d8,rebuild:0x801da4fc};
// Addresses follow accepted calls, including the nonuniform overlay placement.
services.lookup=0x20c478+0x7ffbcb70;services.immune=0x2379ac+0x7ffbcd30;
m.trap(services.lookup,c=>c.set('v0',c.get('a0')<18?actor(c.get('a0')):0));
m.trap(services.dead,c=>c.set('v0',!c.get('a0')||!!(c.r32(c.get('a0')+0x40)&1)));
let immunity=true;m.trap(services.immune,c=>c.set('v0',immunity?1:0));
m.trap(0x801c8d24,c=>c.set('v0',0)); // Undead predicate.
const events=[];for(const pc of [0x21d200,0x21d230,0x21d25c,0x21d28c,0x21d334])m.trap(pc+0x7ffbcd30,c=>events.push({event:c.get('a0'),time:c.get('a1'),actor:c.get('a2'),kind:c.get('a3'),pose:c.r32(c.get('sp')+16),extra:c.r32(c.get('sp')+32),anchor:c.get('a2')===actors+4*0x100?c.r32(c.get('a2')+0x68):-1}));
const visualEvents=[];
function captureVisual(c){
 events.push({event:c.get('a0'),time:c.get('a1'),actor:c.get('a2'),kind:c.get('a3'),
  pose:c.r32(c.get('sp')+24),nameToken:c.r32(c.get('sp')+20)});
 if(c.get('a0')!==0x16)return;
 const list=at=>{const result=[];for(let i=0;i<10;i++){const id=c.r32(at+i*4)|0;if(id===-1)return result;result.push(id);}throw Error('Unterminated visual actor list');};
 visualEvents.push({slot:c.get('a3'),sources:list(c.r32(c.get('sp')+16)),targets:list(c.r32(c.get('sp')+20))});
}
m.trap(0x21d2c0+0x7ffbcd30,captureVisual);
m.trap(0x22257c+0x7ffbcd30,c=>c.set('v0',4));
m.trap(0x2224f4+0x7ffbcd30,c=>c.set('v0',0));
for(const [pc,delta] of [[services.up,1],[services.down,-1]])m.trap(pc,c=>c.w32(c.get('a0')+0x24,c.r32(c.get('a0')+0x24)+delta));
m.trap(services.rebuild,()=>{});
m.trap(0x20d4c8+0x7ffbcb70,c=>c.set('v0',Math.max(0,Math.min(c.get('a1'),c.r16(c.get('a0')+0x22)-c.r16(c.get('a0')+0x20)))));
m.trap(0x22a7b8+0x7ffbcd30,c=>{const a=c.get('a0'),hp=Math.max(0,c.r16(a+0x20)-c.get('a3'));c.w16(a+0x20,hp);if(!hp)c.w32(a+0x40,c.r32(a+0x40)|1);});
for(let i=0;i<18;i++){const own=i<9,cell=i%9,row=Math.floor(cell/3),col=cell%3;m.w32(actor(i)+0x40,own?0x100:0);m.w32(actor(i)+0x54,own?col:2-col);m.w32(actor(i)+0x58,own?2-row:6+row);m.w16(actor(i)+0x20,100);m.w16(actor(i)+0x22,500);m.w32(actor(i)+0x7c,159);assert.equal(call('lc_attack_cell',[actor(i)]),cell);}
const rules=settings+159*32,source=actor(4),target=actor(13);
// An enemy selected by the original damage routine cannot anchor a self/allied
// row. In particular, a front-row dragon must not lose its action against a
// middle-row enemy when the authored side is User only.
m.w8(rules+2,7);m.w8(rules+3,3);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),1,'self row with enemy anchor');
assert.equal(m.r32(ids),0);
m.w8(rules+3,2);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'allied row with enemy anchor');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[0,1,2]);
// Keep a valid native anchor, including one on a different row from the user.
assert.equal(call('lc_attack_target_list',[actor(0),ids,7]),3,'valid allied rear-row anchor');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[6,7,8]);
m.w8(rules+3,1);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'ordinary enemy middle row');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[12,13,14]);
// A wrong-side or invalid anchor must be replaced with a live actor in formation.
for(const anchor of [-1,20,4]){
 assert.equal(call('lc_attack_target_list',[actor(0),ids,anchor]),3,`enemy row fallback ${anchor}`);
 assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[9,10,11]);
}
m.w32(actor(13)+0x40,1);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'dead enemy anchor');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[9,10,11]);
m.w32(actor(13)+0x40,0);m.w32(actor(13)+0x54,99);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'off-formation enemy anchor');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[9,10,11]);
m.w32(actor(13)+0x54,1);
// Columns use the corrected anchor too; mirrored formation coordinates apply.
m.w8(rules+2,6);m.w8(rules+3,3);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),1,'self column with enemy anchor');
assert.equal(m.r32(ids),0);
m.w8(rules+3,2);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'allied column with enemy anchor');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[0,3,6]);
// Automatic side follows the authored main effect, not the old anchor.
const mainAddress=records+159*16+7,originalMain=m.r8(mainAddress);
m.w8(rules+2,7);m.w8(rules+3,0);m.w8(mainAddress,2);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'automatic healing side');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[0,1,2]);
m.w8(mainAddress,1);
assert.equal(call('lc_attack_target_list',[actor(0),ids,13]),3,'automatic damage side');
assert.deepEqual([0,1,2].map(i=>m.r32(ids+i*4)),[12,13,14]);
m.w8(mainAddress,originalMain);
console.log('Target-side anchors: self/allies/enemies, rows/columns, valid/dead/missing/off-formation anchors PASS.');
for(const side of [1,2])for(const [pattern,expected] of [[1,[4]],[2,[0,1,2,3,4,5,6,7,8]],[3,[0,1,2]],[4,[3,4,5]],[5,[6,7,8]],[6,[1,4,7]],[7,[3,4,5]],[8,[1,3,4,5,7]],[12,[0,8]]]){
 m.w8(rules+2,pattern);m.w8(rules+3,side);m.w16(rules+4,257);const n=call('lc_attack_target_list',[source,ids,side===1?13:4]);const actual=Array.from({length:n},(_,i)=>m.r32(ids+i*4));assert.deepEqual(actual,expected.map(x=>x+(side===1?9:0)),`side ${side}, pattern ${pattern}`);
}
m.w8(rules+2,9);for(const side of [0,1,2,3]){m.w8(rules+3,side);assert.equal(call('lc_attack_target_list',[source,ids,4]),1);assert.equal(m.r32(ids),4);}
for(const side of [1,2])for(const [anchor,expected] of [[0,[0,1,3,4]],[1,[0,1,2,3,4,5]],[4,[0,1,2,3,4,5,6,7,8]],[8,[4,5,7,8]]]) {
 m.w8(rules+2,10);m.w8(rules+3,side);m.w32(ids+36,0xfeedface);
 const shift=side===1?9:0,n=call('lc_attack_target_list',[source,ids,anchor+shift]);
 assert.deepEqual(Array.from({length:n},(_,i)=>m.r32(ids+i*4)),expected.map(i=>i+shift));
 assert.equal(m.r32(ids+36),0xfeedface);
}
console.log('3 × 3 targeting: center, edges and corners on both mirrored sides PASS.');
// Compare repeated selection to the original target builder, not a copy of
// its algorithm. Actor lookup/RNG are controlled; native ordering/weights run.
const repeatRng=0x80092a60, coordinateLookup=0x801ca2e0, actorId=0x801ca29c;
let repeatSample=0,repeatCalls=0;
m.trap(repeatRng,c=>{repeatCalls++;c.set('v0',repeatSample);});
m.trap(coordinateLookup,c=>{
 const x=c.get('a0')|0,y=c.get('a1')|0;
 const i=Array.from({length:18},(_,i)=>i).find(i=>(c.r32(actor(i)+0x54)|0)===x&&(c.r32(actor(i)+0x58)|0)===y);
 c.set('v0',i===undefined?0:actor(i));
});
m.trap(actorId,c=>c.set('v0',(c.get('a0')-actors)/0x100));
let repeatedCases=0;
for(const side of [1,2])for(let center=0;center<9;center++)for(const holes of [[],[0,2,6,8],[1,3,5,7]])for(const anchorKind of ['actor','coordinate3','coordinate4']) {
 const shift=side===1?9:0,anchor=center+shift;
 const x=m.r32(actor(anchor)+0x54),y=m.r32(actor(anchor)+0x58);
 const anchorKey=anchorKind==='actor'?anchor:anchorKind==='coordinate3'?20+y*3+x:47+y*4+x;
 for(let i=0;i<18;i++)m.w32(actor(i)+0x40,(i<9?0x100:0)|(holes.includes(i-shift)&&(i!==anchor||anchorKind!=='actor')?1:0));
 m.w8(rules+2,11);m.w8(rules+3,side);m.w8(rules+22,6);
 for(const sample of [0,39,40,54,55,159,160,12345])for(const nativePattern of [9,10]) {
  repeatSample=sample;repeatCalls=0;for(let i=0;i<128;i++)m.w8(packet+i,0);
  call(0x237e2c+0x7ffbcd30,[packet,nativePattern,anchorKey]);
  assert.equal(m.r32(packet+12),6);assert.equal(repeatCalls,6);
  const expected=Array.from({length:6},(_,i)=>m.r32(packet+16+i*4));
  repeatCalls=0;m.w32(ids+36,0xfeedface);
  const n=call('lc_attack_target_list',[source,ids,anchorKey]);
  assert.equal(n,6);assert.equal(repeatCalls,6);assert.equal(m.r32(ids+36),0xfeedface);
  assert.deepEqual(Array.from({length:n},(_,i)=>m.r32(ids+i*4)),expected,`retail repeat ${side}/${center}/${holes}/${sample}/${nativePattern}/${anchorKind}`);
  repeatedCases++;
 }
}
for(let i=0;i<18;i++)m.w32(actor(i)+0x40,i<9?0x100:0);
for(const side of [1,2,3])for(let count=1;count<=18;count++) {
 m.w8(rules+3,side);m.w8(rules+22,count);repeatSample=0;repeatCalls=0;
 m.w32(ids+72,0xfeedface);const n=call('lc_attack_target_list',[source,ids,13]);
 assert.equal(n,count);assert.equal(repeatCalls,count);assert.equal(m.r32(ids+72),0xfeedface);
 assert.deepEqual(Array.from({length:n},(_,i)=>m.r32(ids+i*4)),Array(count).fill(side===1?13:side===2?0:4));
}
// Legacy/default count and absent-side behavior must remain bounded.
m.w8(rules+3,1);m.w8(rules+22,0);assert.equal(call('lc_attack_target_list',[source,ids,13]),6);
for(let i=9;i<18;i++)m.w32(actor(i)+0x40,1);
repeatCalls=0;assert.equal(call('lc_attack_target_list',[source,ids,13]),0);assert.equal(repeatCalls,0);
for(let i=9;i<18;i++)m.w32(actor(i)+0x40,0);
m.traps.delete(repeatRng);m.traps.delete(coordinateLookup);m.traps.delete(actorId);m.w8(rules+22,0);
console.log(`Repeated strikes: ${repeatedCases} original-MIPS comparisons, 1–18 counts, both sides/self, holes, empty side and bounds PASS.`);
// Dead cells are skipped; no target may escape the 9-entry caller allocation.
m.w8(rules+2,2);m.w8(rules+3,1);m.w32(actor(10)+0x40,1);m.w32(ids+36,0xfeedface);assert.equal(call('lc_attack_target_list',[source,ids,13]),8);assert.equal(m.r32(ids+36),0xfeedface);
for(const [effect,flag,event] of [[3,2,0x23],[4,4,0x25],[8,8,0x1a],[10,16,0x27],[5,0,0x29],[6,0,0x2a]]){
 m.w32(target+0x40,0);m.w16(target+0x20,100);events.length=0;assert.equal(call('lc_attack_status',[target,effect,50]),1);assert.equal(m.r32(target+0x40)&0x1e,flag);assert(events.some(x=>x.event===event));
}
m.w32(target+0x40,0x1e);events.length=0;assert.equal(call('lc_attack_status',[target,11,50]),1);assert.equal(m.r32(target+0x40)&0x3e,0);assert(events.some(x=>x.event===0x2b));
m.w32(target+0x40,4);assert.equal(call('lc_attack_status',[target,10,50]),0);assert.equal(m.r32(target+0x40),4);
immunity=false;m.w32(target+0x40,0);assert.equal(call('lc_attack_status',[target,3,50]),0);immunity=true;
// Precisely chosen samples cover every integer percentage boundary.
let sample=0,part=0;m.trap(0x8009c7cc,c=>c.set('v0',(part++%3)===2?sample:0));
for(const chance of [0,1,25,50,99,100]){let applied=0;for(sample=0;sample<100;sample++){part=0;m.w32(target+0x40,0);m.w16(target+0x20,100);m.w8(rules+8,10);m.w8(rules+9,1);m.w8(rules+10,chance);call('lc_attack_begin',[source]);call('lc_attack_record',[source,target,20,120,1,50]);if(m.r32(target+0x40)&16)applied++;}assert.equal(applied,chance);}
m.w32(target+0x40,0);m.w8(rules+10,100);call('lc_attack_begin',[source]);call('lc_attack_record',[source,target,0,100,0,50]);assert.equal(m.r32(target+0x40),0);m.w8(rules+9,0);call('lc_attack_record',[source,target,0,100,0,50]);assert.equal(m.r32(target+0x40),16);
// Sum actual HP lost, not overkill or the last calculated amount; cap healing.
m.w8(rules+8,0);m.w8(rules+11,1);m.w8(rules+12,0);m.w16(rules+14,50);m.w32(source+0x40,0x100);m.w16(source+0x20,200);m.w16(source+0x22,500);call('lc_attack_begin',[source]);m.w16(target+0x20,0);call('lc_attack_record',[source,target,100,10,1,50]);m.w16(target+0x20,50);call('lc_attack_record',[source,target,50,100,1,60]);call('lc_attack_finish',[source,70]);assert.equal(m.r16(source+0x20),230);
m.w8(rules+11,2);m.w8(rules+12,2);m.w16(rules+14,100);call('lc_attack_begin',[source]);call('lc_attack_finish',[source,80]);assert.equal(m.r16(source+0x20),0);assert.equal(m.r32(source+0x40)&1,1);
console.log(JSON.stringify({mirroredFormationCells:18,targetPatterns:23,statusesAndImmunity:'PASS',chanceBoundarySamples:600,multiTargetDrainAndLethalRecoil:'PASS',limit:'Compiled adapters and original status applier; controlled actor lookup, immunity, timing, RNG and event sinks. Visual replay and full console battle are not simulated.'}));

// Full adapted generic executor: real loop, effect dispatch, status applier and
// after-effect adapter. Scores, native target choice and event queue are fixed.
const generic=entries('attack_generic.elf');
m.trap(e.lc_original_targets,c=>{const p=c.get('a0'),a=c.get('a1');for(let i=0;i<128;i++)c.w8(p+i,0);for(let i=0;i<8;i++)c.w8(p+i,c.r8(records+a*16+6+i));c.w8(p+8,10);c.w32(p+12,2);c.w32(p+16,9);c.w32(p+20,17);c.w32(c.get('a2')+0x68,9);});
// Expanded packets must bridge to the unchanged native ABI without overlapping
// adjacent arrays. Distinct field values expose offset/stride mistakes.
for(const count of [1,6,9,10]) {
 for(let i=0;i<244;i++)m.w8(packet+i,0);
 m.w8(packet,3);m.w8(packet+8,9);m.w32(packet+12,count);
 for(let i=0;i<count;i++) {
  m.w32(packet+16+i*4,9+i%9);m.w16(packet+56+i*2,100+i);
  m.w8(packet+76+i,20+i);m.w8(packet+86+i,30+i);
  m.w16(packet+96+i*2,1000+i);m.w8(packet+116+i,50+i);
 }
 m.w32(packet+240,0xfeedface);call('lc_attack_expand_packet',[packet]);
 assert.equal(m.r32(packet+240),0xfeedface);assert.equal(m.r32(packet+12),count);
 for(let i=0;i<count;i++) {
  assert.equal(m.r32(packet+16+i*4),9+i%9);assert.equal(m.r16(packet+96+i*2),100+i);
  assert.equal(m.r8(packet+136+i),20+i);assert.equal(m.r8(packet+156+i),30+i);
  assert.equal(m.r16(packet+176+i*2),1000+i);assert.equal(m.r8(packet+216+i),50+i);
 }
}
let batchAt=0,batchSizes=[];
m.trap(0x239338+0x7ffbcd30,c=>{
 const p=c.get('a0'),count=c.r32(p+12);assert(count>0&&count<=9);batchSizes.push(count);
 assert.equal(c.get('a1'),source);assert.equal(c.get('a2'),actor(5));assert.equal(c.get('a3'),actor(6));
 for(let i=0;i<count;i++) {
  const k=batchAt++;assert.equal(c.r32(p+16+i*4),9+k%9);assert.equal(c.r8(p+86+i),k%10);
  c.w16(p+56+i*2,200+k);c.w8(p+76+i,60+k);c.w16(p+96+i*2,500+k);c.w8(p+116+i,80+k);
 }
});
for(const count of [1,6,9,10,18]) {
 for(let i=0;i<240;i++)m.w8(packet+i,0);
 m.w32(packet+12,count);m.w32(packet+240,0xfeedface);batchAt=0;batchSizes=[];
 for(let i=0;i<count;i++){m.w32(packet+16+i*4,9+i%9);m.w8(packet+156+i,i%10);}
 call('lc_attack_calculate',[packet,source,actor(5),actor(6)]);
 assert.deepEqual(batchSizes,count<=9?[count]:[9,count-9]);assert.equal(m.r32(packet+240),0xfeedface);
 for(let i=0;i<count;i++) {
  assert.equal(m.r32(packet+16+i*4),9+i%9);assert.equal(m.r16(packet+96+i*2),200+i);
  assert.equal(m.r8(packet+136+i),60+i);assert.equal(m.r8(packet+156+i),i%10);
  assert.equal(m.r16(packet+176+i*2),500+i);assert.equal(m.r8(packet+216+i),80+i);
 }
}
m.w8(rules+2,11);m.w8(rules+3,1);m.w8(rules+22,18);m.trap(repeatRng,c=>c.set('v0',0));
for(const [entry,size,expected] of [['lc_attack_targets',128,9],['lc_attack_expanded_targets',240,18]]) {
 m.w32(packet+size,0xfeedface);call(entry,[packet,159,source]);
 assert.equal(m.r32(packet+12),expected);assert.equal(m.r32(packet+size),0xfeedface);
}
const repeatFlags=packet+0x300;
for(let i=0;i<18;i++){m.w32(ids+i*4,9+i%9);m.w32(repeatFlags+i*4,i<9?0:i-8);}
m.w32(ids+72,-1);m.w32(repeatFlags+72,-1);m.w32(ids+80,0xfeedface);
call('lc_attack_visual_targets',[159,ids,repeatFlags]);
assert.deepEqual(Array.from({length:9},(_,i)=>m.r32(ids+i*4)),[9,10,11,12,13,14,15,16,17]);
assert.deepEqual(Array.from({length:9},(_,i)=>m.r32(repeatFlags+i*4)),[1,2,3,4,5,6,7,8,9]);
assert.equal(m.r32(ids+36)|0,-1);assert.equal(m.r32(ids+80),0xfeedface);
m.traps.delete(repeatRng);m.w8(rules+2,0);m.w8(rules+3,0);m.w8(rules+22,0);
console.log('Expanded strike packets: native ABI, all field offsets, 9-entry calculation batches, 18-strike bounds and unique visual targets PASS.');
// Run the original evaluator itself on each native row, then on expanded
// batches. Only its score/amount services are fixed; selector arithmetic and
// hit-chance routing remain original instructions.
m.traps.delete(0x239338+0x7ffbcd30);
const calculationServices=[0x801f2f1c,0x801f28e0,0x801f2af8,0x801f36ac];
const calculationTraps=calculationServices.map(p=>m.traps.get(p));
m.trap(calculationServices[0],c=>c.set('v0',90));
for(const p of calculationServices.slice(1,3))m.trap(p,c=>c.set('v0',80+(c.get('a0')-actors)/0x100));
m.trap(calculationServices[3],c=>{m.bits.setUint32(0,c.get('a2'));c.set('v0',Math.trunc(m.bits.getFloat32(0))+7);});
const nativeRow=packet+0x400;
m.w8(source+0x78,3);
for(const count of [6,9,10,18])for(const casters of [1,2,3]) {
 for(let i=0;i<240;i++)m.w8(packet+i,0);
 m.w8(packet+1,1);m.w32(packet+12,count);const expected=[];
 for(let i=0;i<count;i++) {
  for(let k=0;k<128;k++)m.w8(nativeRow+k,0);
  m.w8(nativeRow+1,1);m.w32(nativeRow+12,1);m.w32(nativeRow+16,9+i%9);
  m.w16(nativeRow+56,i%3);m.w8(nativeRow+86,i%10);
  call(0x239338+0x7ffbcd30,[nativeRow,source,casters>1?actor(5):0,casters>2?actor(6):0]);
  expected.push([m.r16(nativeRow+56),m.r16(nativeRow+96)]);
  m.w32(packet+16+i*4,9+i%9);m.w16(packet+96+i*2,i%3);m.w8(packet+156+i,i%10);
 }
 call('lc_attack_calculate',[packet,source,casters>1?actor(5):0,casters>2?actor(6):0]);
 assert.deepEqual(Array.from({length:count},(_,i)=>[m.r16(packet+96+i*2),m.r16(packet+176+i*2)]),expected);
}
calculationServices.forEach((p,i)=>{if(calculationTraps[i])m.trap(p,calculationTraps[i]);else m.traps.delete(p);});
console.log('Expanded calculation matches original evaluator: 6/9/10/18 rows, all ten selectors and 1/2/3 casters PASS.');
m.trap(0x239338+0x7ffbcd30,c=>{const p=c.get('a0');for(let i=0;i<c.r32(p+12);i++){c.w16(p+0x38+i*2,100);c.w16(p+0x60+i*2,60);}});
for(const pc of [0x20bfe4,0x20c104,0x20c3b4])m.trap(pc+0x7ffbcb70,c=>c.set('v0',0));
for(const pc of [0x2363fc,0x237af8,0x233210])m.trap(pc+0x7ffbcd30,c=>c.set('v0',0));
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',100));
m.trap(0x20d72c+0x7ffbcb70,c=>c.set('v0',(c.get('a0')-actors)/0x100));
m.trap(0x8016e230,c=>c.set('v0',159));
sample=0;part=0;
for(const effect of [0,1,2,3,4,5,6,8,10,11,16]){
  m.w32(source+0x40,0x100);m.w16(source+0x20,100);m.w32(source+0x7c,159);m.w8(source+0x78,3);m.w32(source+0x6c,0);m.w32(source+0x94,0);
  for(const i of [9,17]){m.w32(actor(i)+0x40,effect===11?4:0);m.w16(actor(i)+0x20,100);m.w16(actor(i)+0x22,500);m.w32(actor(i)+0x94,0);}
  for(let i=0;i<12;i++)m.w8(records+159*16+4+i,0);
  m.w8(records+159*16+4,3);m.w8(records+159*16+6,1);m.w8(records+159*16+7,effect);m.w8(records+159*16+9,1);m.w8(records+159*16+10,255);
  m.w8(rules+2,0);m.w8(rules+3,0);m.w8(rules+8,0);m.w8(rules+11,0);m.w16(rules+6,100);m.w32(global+0x828,0);m.w32(ids,0);events.length=0;
  m.w8(rules+21,136);visualEvents.length=0;
  assert.equal(call(generic.lc_attack_generic,[source,ids]),1,`generic effect ${effect}`);
  assert.deepEqual(visualEvents,[{slot:136,sources:[4],targets:[9,17]}],'generic visual handoff');
  assert(events.some(x=>x.event===11&&x.nameToken===159),'new spell carries its own battle-name token');
  for(const i of [9,17]){
    const expectedHp=effect===1?40:effect===2?160:effect===16?0:100;
    assert.equal(m.r16(actor(i)+0x20),expectedHp,`generic effect ${effect}, target ${i}`);
    const expectedFlag=({3:2,4:4,8:8,10:16,16:1})[effect]||0;
    assert.equal(m.r32(actor(i)+0x40)&0x1f,expectedFlag,`generic flags ${effect}, target ${i}`);
  }
  assert.equal(m.r32(source+0x6c),1);assert.equal(m.r32(scratch),0);
}
console.log('Full generic executor: 11 main effects × 2 targets PASS (controlled score and event services).');

// The authored undead-slaying option enters the retail lethal-result marker
// path. Run the real undead predicate and status applier, including dead targets.
m.traps.delete(0x801c8d24);
let slayingCases=0;
for(const element of [0,2,5])for(const undead of [false,true])for(const hp of [50,100])for(const hit of [false,true])for(const chance of [0,100]) {
 m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',hit?100:0));sample=0;part=0;
 m.w32(source+0x40,0x100);m.w16(source+0x20,100);m.w32(source+0x7c,159);m.w8(source+0x78,3);m.w32(source+0x6c,0);m.w32(source+0x94,0);
 for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,hp);m.w32(actor(i)+0x4c,undead?45:2);m.w32(actor(i)+0x94,0);}
 for(let i=0;i<12;i++)m.w8(records+159*16+4+i,0);
 m.w8(records+159*16+4,3);m.w8(records+159*16+5,element);m.w8(records+159*16+6,1);m.w8(records+159*16+7,1);m.w8(records+159*16+9,1);m.w8(records+159*16+10,255);
 m.w8(rules+2,0);m.w8(rules+3,0);m.w8(rules+8,7);m.w8(rules+9,2);m.w8(rules+10,chance);m.w8(rules+11,0);m.w32(global+0x828,0);m.w32(ids,0);events.length=0;
 call(generic.lc_attack_generic,[source,ids]);
 const markers=events.filter(e=>e.event===0x1f&&e.extra===7);
 assert.equal(markers.length,undead&&hp===50&&hit&&chance===100?2:0,`slaying ${element}/${undead}/${hp}/${hit}/${chance}`);
 slayingCases++;
}
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',100));m.w8(rules+8,0);m.trap(0x801c8d24,c=>c.set('v0',0));
console.log(`Undead slaying: ${slayingCases} full spell executions × 2 targets, Physical/Flame/Virtue, living/undead, nonlethal/lethal, misses and chance PASS.`);
// Adding slaying must not replace a native drain descriptor (D6=11).
m.traps.delete(0x801c8d24);m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',60));
m.w16(source+0x20,100);m.w32(source+0x40,0x100);m.w32(source+0x6c,0);m.w32(source+0x94,0);m.w32(ids,0);
for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,50);m.w32(actor(i)+0x4c,45);}
m.w8(records+159*16+10,11);m.w8(records+159*16+11,2);m.w8(records+159*16+12,1);
m.w8(rules+8,7);m.w8(rules+11,255);events.length=0;
call(generic.lc_attack_generic,[source,ids]);
assert.equal(events.filter(x=>x.event===0x1f&&x.extra===7).length,2);
assert.equal(m.r16(source+0x20),160,'native drain retained alongside undead slaying');
m.w8(rules+8,0);m.w8(rules+11,0);m.w8(records+159*16+10,255);m.trap(0x801c8d24,c=>c.set('v0',0));
console.log('Undead slaying and existing native drain coexist PASS.');

// Preserve the actual melee executor and physical HP sink while adding targets.
// Artwork timing and already-tested numeric kernels are fixed here.
// This phase changes the fixture from a spell to Slash, including its donor.
m.w8(rules,4);m.w8(rules+1,4);
m.trap(e.lc_original_anchor,c=>c.set('v0',13));
for(const pc of [0x22222c,0x2223e0,0x237810,0x237890,0x22a4e0])m.trap(pc+0x7ffbcd30,c=>c.set('v0',0));
m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',60));
m.trap(0x2361ec+0x7ffbcd30,c=>c.set('v0',100));
m.trap(0x22a964+0x7ffbcd30,()=>{});
m.trap(0x801ad9d4,()=>{});
m.trap(0x801ae4ec,c=>{const p=c.get('a2');if(p){c.w32(p,10);c.w32(p+4,-1);}c.set('v0',20);});
m.w32(source,global+0x2000);m.w32(source+0x48,2);m.w32(source+0x4c,2);
for(const i of [9,17]){m.w32(actor(i),global+0x2200);m.w32(actor(i)+0x48,2);m.w32(actor(i)+0x4c,2);}
m.trap(e.lc_original_ranged_anchor,c=>c.set('v0',13));
m.trap(0x21d2c0+0x7ffbcd30,captureVisual);
// Animation duration / impact marker services used by ranged and alternate bodies.
m.trap(0x22257c+0x7ffbcd30,c=>{if(c.get('a2')){c.w32(c.get('a2'),10);c.w32(c.get('a2')+4,-1);}c.set('v0',20);});
for(const route of ['melee','ranged','alternate'])for(const effect of [1,2,10])for(const action of [159,254]){
  m.w32(source+0x40,0x100);m.w16(source+0x20,100);m.w32(source+0x7c,action);m.w8(source+0x78,0);m.w32(source+0x6c,0);m.w32(source+0x94,0);m.w32(global+0x828,0);m.w32(ids,0);
  m.w8(records+159*16+4,0);m.w8(records+159*16+6,1);m.w8(records+159*16+7,effect);
  m.w8(rules+2,12);m.w8(rules+3,1);m.w16(rules+4,257);m.w8(rules+8,0);m.w8(rules+11,0);
  if(action!==159){for(let i=0;i<16;i++)m.w8(records+action*16+i,m.r8(records+159*16+i));for(let i=0;i<32;i++)m.w8(settings+action*32+i,m.r8(rules+i));}
  for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,100);m.w32(actor(i)+0x94,0);}
  visualEvents.length=0;
  assert.equal(call(e['lc_attack_'+route],[source,ids]),1,route);
  assert.deepEqual(visualEvents,[{slot:136,sources:[4],targets:[9,17]}],`${route} visual handoff`);
  for(const i of [9,17]){assert.equal(m.r16(actor(i)+0x20),effect===1?40:effect===2?160:100,`${route} main ${effect}`);assert.equal(m.r32(actor(i)+0x40)&16,effect===10?16:0);}
  assert.equal(m.r32(source+0x6c),1,'one action count for the whole custom pattern');
}
console.log('Original melee/ranged/alternate executors + physical HP sink: IDs159/254, damage/healing/status × 2 targets PASS; one attack count.');
// Repeated IDs must reach the damage writer once per requested strike, even
// when the animation is shared. Keep the victim alive to count every impact.
m.trap(repeatRng,c=>c.set('v0',0));
// Pose-route tables are battle-loaded state, outside this isolated HP test.
const priorRepeatAnimation=m.traps.get(e.lc_original_animation);
m.trap(e.lc_original_animation,c=>c.set('v0',0x28));
for(const route of ['generic','melee','ranged','alternate'])for(const shared of (route==='generic'?[0]:[0,1]))for(const count of [1,6,9,10,18]) {
 m.w8(rules,route==='generic'?55:4);m.w8(rules+1,route==='generic'?55:4);
 for(let i=0;i<18;i++) {
  m.w32(actor(i),global+0x2200);m.w32(actor(i)+0x40,i<9?0x100:0);
  m.w16(actor(i)+0x20,2000);m.w16(actor(i)+0x22,4000);m.w32(actor(i)+0x94,0);
  m.w32(actor(i)+0x48,2);m.w32(actor(i)+0x4c,2);
 }
 m.w32(source+0x7c,159);m.w8(source+0x78,route==='generic'?3:0);m.w32(source+0x6c,0);
 m.w32(ids,0);m.w32(global+0x828,0);events.length=0;
 for(let i=0;i<12;i++)m.w8(records+159*16+4+i,0);
 m.w8(records+159*16+4,route==='generic'?3:0);m.w8(records+159*16+6,1);m.w8(records+159*16+7,1);
 m.w8(records+159*16+9,1);m.w8(records+159*16+10,255);
 m.w8(rules+2,11);m.w8(rules+3,1);m.w8(rules+8,0);m.w8(rules+11,0);m.w8(rules+13,shared);
 m.w8(rules+21,136);m.w8(rules+22,count);visualEvents.length=0;
 try { call(route==='generic'?generic.lc_attack_generic:e['lc_attack_'+route],[source,ids],600000); }
 catch(error) {error.message+=` (repeated ${route}/${shared}/${count}; ra=${m.get('ra').toString(16)} a0=${m.get('a0').toString(16)} a1=${m.get('a1').toString(16)})`;throw error;}
 const victim=route==='generic'?9:13;
 assert.equal(m.r16(actor(victim)+0x20),2000-60*count,`${route}/${shared}/${count} repeated damage`);
 assert.equal(m.r32(source+0x6c),1,`${route}/${shared}/${count} action count`);
 for(let i=9;i<18;i++)if(i!==victim)assert.equal(m.r16(actor(i)+0x20),2000,'unselected victim unchanged');
 assert.equal(visualEvents.length,1);
 assert.deepEqual(visualEvents[0].targets,Array(count>9?1:count).fill(victim));
}
m.traps.delete(repeatRng);m.w8(rules+2,12);m.w16(rules+4,257);m.w8(rules+13,0);m.w8(rules+21,136);m.w8(rules+22,0);
if(priorRepeatAnimation)m.trap(e.lc_original_animation,priorRepeatAnimation);else m.traps.delete(e.lc_original_animation);
for(let i=0;i<18;i++)m.w16(actor(i)+0x22,500);
console.log('Repeated-strike damage: 1/6/9/10/18 impacts in spell and all physical routes, separate/shared playback, one action count and bounded visual lists PASS.');
m.traps.delete(0x801c8d24);
for(const route of ['melee','ranged','alternate'])for(const undead of [false,true])for(const hp of [50,100])for(const chance of [0,100]) {
 m.w32(source+0x40,0x100);m.w32(source+0x7c,159);m.w32(source+0x6c,0);m.w32(source+0x94,0);m.w32(ids,0);m.w32(global+0x828,0);
 m.w8(records+159*16+7,1);m.w8(rules+8,7);m.w8(rules+9,2);m.w8(rules+10,chance);
 for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,hp);m.w32(actor(i)+0x4c,undead?45:2);m.w32(actor(i)+0x94,0);}
 events.length=0;call(e['lc_attack_'+route],[source,ids]);
 assert.equal(events.filter(x=>x.event===0x31).length,undead&&hp===50&&chance===100?2:0,`${route} native removal events`);
}
m.w8(rules+8,0);m.trap(0x801c8d24,c=>c.set('v0',0));
console.log('Physical slaying: 24 melee/ranged/alternate executions produce native permanent-removal events only on eligible killing blows PASS.');
m.w32(source+0x7c,159);m.w16(source+0x20,100);m.w32(source+0x6c,0);m.w32(source+0x94,0);m.w32(ids,0);m.w32(global+0x828,0);
m.w8(records+159*16+7,2);m.w8(rules+11,1);m.w8(rules+12,1);m.w16(rules+14,100);
for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,490);m.w32(actor(i)+0x94,0);}
call(e.lc_attack_melee,[source,ids]);assert.equal(m.r16(source+0x20),160,'last calculated healing amount is 60, despite only 10 missing HP');
for(const i of [9,17])assert.equal(m.r16(actor(i)+0x20),500);
m.w8(rules+11,0);
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',0));
for(const route of ['melee','ranged','alternate'])for(const condition of [0,1]){
 m.w32(source+0x7c,159);m.w32(source+0x40,0x100);m.w32(source+0x6c,0);m.w32(source+0x94,0);m.w32(ids,0);m.w32(global+0x828,0);
 m.w8(records+159*16+7,1);m.w8(rules+8,10);m.w8(rules+9,condition);m.w8(rules+10,100);
 for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,100);m.w32(actor(i)+0x94,0);}
 call(e['lc_attack_'+route],[source,ids]);
 for(const i of [9,17]){assert.equal(m.r16(actor(i)+0x20),100);assert.equal(m.r32(actor(i)+0x40)&16,condition?0:16,`${route} miss / condition ${condition}`);}
}
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',100));m.w8(rules+8,0);
console.log('Physical misses: successful-hit condition skips status; after-attempt condition applies it PASS.');

// Execute the exported shared-strike adapters, not a host reimplementation.
// Deliberately vary readiness, travel, damage and hit/miss outcomes by target.
const column=[10,13,16];let groupHits=[true,true,true];
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',groupHits[column.indexOf((c.get('a1')-actors)/0x100)]?100:0));
m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',20+10*column.indexOf((c.get('a1')-actors)/0x100)));
m.trap(0x22222c+0x7ffbcd30,c=>c.set('v0',15+7*column.indexOf((c.get('a1')-actors)/0x100)));
m.trap(0x20bfe4+0x7ffbcb70,c=>c.set('v0',1));
let sharedCases=0,maxSharedSteps=0;const originalPoses=new Map();
for(const route of ['melee','ranged','alternate'])for(const side of [0,1])for(const action of [159,254])
for(const mode of [0,1])for(const hitMask of [7,5,6,0])for(const mainEffect of [1,2,10])for(const condition of [0,1]) {
 groupHits=column.map((_,i)=>!!(hitMask&(1<<i)));
 for(let i=0;i<18;i++) {
  const own=(i<9)!==!!side,cell=i%9;
  m.w32(actor(i)+0x40,own?0x100:0);m.w32(actor(i)+0x54,own?cell%3:2-cell%3);
  m.w32(actor(i)+0x58,own?2-Math.floor(cell/3):6+Math.floor(cell/3));
  m.w16(actor(i)+0x20,100);m.w16(actor(i)+0x22,500);m.w32(actor(i)+0x94,0);
  m.w32(actor(i),global+0x2200);m.w32(actor(i)+0x48,2);m.w32(actor(i)+0x4c,2);
 }
 m.w32(source,global+0x2000);m.w32(source+4,global+0x2100);m.w32(source+0x7c,action);
 m.w32(source+0x6c,2);m.w32(source+0x94,7);m.w32(ids,3);m.w32(global+0x828,0);
 for(let i=0;i<column.length;i++)m.w32(actor(column[i])+0x94,30+i*31);
 m.w8(records+159*16+4,0);m.w8(records+159*16+6,1);m.w8(records+159*16+7,mainEffect);
 m.w8(rules+2,6);m.w8(rules+3,1);m.w8(rules+8,mainEffect===1?10:0);m.w8(rules+9,condition);m.w8(rules+10,100);
 m.w8(rules+11,1);m.w8(rules+12,0);m.w8(rules+13,mode);m.w16(rules+14,50);
 if(action!==159) {
  for(let i=0;i<16;i++)m.w8(records+action*16+i,m.r8(records+159*16+i));
  for(let i=0;i<32;i++)m.w8(settings+action*32+i,m.r8(rules+i));
 }
 events.length=0;visualEvents.length=0;
 let groupedResult;
 try { groupedResult=call(e['lc_attack_'+route],[source,ids],1000000); }
 catch(error) {error.message+=` (${route}/${side}/${action}/${mode}/${hitMask})`;throw error;}
 assert.equal(groupedResult,1);
 maxSharedSteps=Math.max(maxSharedSteps,m.steps);
 const strikes=events.filter(x=>x.actor===source&&(x.event===6||x.event===7));
 assert.equal(strikes.length,mode?1:3,`${route}, side ${side}, mode ${mode}: caster sequence count`);
 const poseKey=[route,side,action,hitMask,mainEffect,condition].join('/');
 if(!mode) originalPoses.set(poseKey,strikes[0].pose);
 else assert.equal(strikes[0].pose,originalPoses.get(poseKey),'retain packed primary pose/mode for animation overrides');
 const resultCodes=mainEffect===1?[0x0c,0x10,0x32]:mainEffect===2?[0x1f]:[0x27];
 const impacts=events.filter(x=>resultCodes.includes(x.event)&&column.some(i=>actor(i)===x.actor));
 assert.equal(impacts.length,groupHits.filter(Boolean).length,
  `one damage display per successful target: ${route}/${side}/${mode}/${hitMask}; ${JSON.stringify(events)}`);
 if(mode&&impacts.length) assert.equal(new Set(impacts.map(x=>x.time)).size,1,'shared damage timestamp');
 if(!mode&&impacts.length>1) assert(new Set(impacts.map(x=>x.time)).size>1,'default remains sequential');
 let loss=0;
 for(let i=0;i<column.length;i++) {
  const amount=groupHits[i]?20+10*i:0;if(mainEffect===1)loss+=amount;
  assert.equal(m.r16(actor(column[i])+0x20),100+(mainEffect===1?-amount:mainEffect===2?amount:0),'target-specific HP result preserved');
  const statusExpected=mainEffect===1?(groupHits[i]||condition===0):mainEffect===10&&groupHits[i];
  assert.equal(m.r32(actor(column[i])+0x40)&16,statusExpected?16:0,'status activation condition per target');
 }
 assert.equal(m.r16(source+0x20),100+Math.floor(loss/2),'one drain from total actual HP loss');
 assert.equal(m.r32(source+0x6c),3,'one attack count');
 assert.equal(visualEvents.length,1,'one authored effect submission');
 assert.deepEqual(visualEvents[0].targets,column,'all targets retained in effect handoff');
 assert(m.r32(ids)>=Math.max(...impacts.map(x=>x.time),0),'completion follows impacts');
 if(mode) assert.equal(m.r32(source+0x68),column[0],'attacker stays anchored to primary target');
 if(mode) assert(m.r32(ids)>=Math.max(...column.map(i=>m.r32(actor(i)+0x94))),'completion includes target reactions');
 if(mode&&mainEffect===1&&condition===0) {
  const statuses=events.filter(x=>x.event===0x27&&column.some(i=>actor(i)===x.actor));
  assert.equal(statuses.length,3);assert.equal(new Set(statuses.map(x=>x.time)).size,1,'after-attempt status shares impact even on misses');
 }
 sharedCases++;
}
// Roster IDs need not follow formation order: the first ID may occupy the rear.
// Execute the actual exported executors and inspect the anchor when the strike is emitted.
let nearestCases=0;
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',100));
m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',30));
for(const route of ['melee','alternate','ranged'])for(const side of [0,1])
for(const cells of [[7,4,1],[4,7,1],[1,7,4]])for(const deadFront of [false,true])for(const mode of [0,1]) {
 for(let i=0;i<18;i++) {
  const own=(i<9)!==!!side,at=column.indexOf(i),cell=at<0?i%9:cells[at];
  const dead=(i!==4&&at<0)||(deadFront&&at>=0&&cell===1);
  m.w32(actor(i)+0x40,(own?0x100:0)|(dead?1:0));
  m.w32(actor(i)+0x54,own?cell%3:2-cell%3);m.w32(actor(i)+0x58,own?2-Math.floor(cell/3):6+Math.floor(cell/3));
  m.w16(actor(i)+0x20,100);m.w16(actor(i)+0x22,500);m.w32(actor(i)+0x94,0);
  m.w32(actor(i),global+0x2200);m.w32(actor(i)+0x48,2);m.w32(actor(i)+0x4c,2);
 }
 m.w32(source,global+0x2000);m.w32(source+4,global+0x2100);m.w32(source+0x7c,159);
 m.w32(source+0x6c,2);m.w32(source+0x94,0);m.w32(ids,0);m.w32(global+0x828,0);
 m.w8(records+159*16+4,0);m.w8(records+159*16+6,1);m.w8(records+159*16+7,1);
 m.w8(rules+2,6);m.w8(rules+3,1);m.w8(rules+8,0);m.w8(rules+11,0);m.w8(rules+13,mode);
 const live=column.filter((id,i)=>!deadFront||cells[i]!==1);
 const nearest=column[cells.indexOf(deadFront?4:1)];
 const primary=mode&&route!=='ranged'?nearest:live[0];
 events.length=0;visualEvents.length=0;call(e['lc_attack_'+route],[source,ids],1000000);
 const strikes=events.filter(x=>x.actor===source&&(x.event===6||x.event===7));
 assert.equal(strikes.length,mode?1:live.length);
 assert.equal(strikes[0].anchor,primary,`${route}/${side}/${cells}/${deadFront}/${mode}: strike the closest live affected unit`);
 if(mode)assert.equal(m.r32(source+0x68),primary);
 assert.deepEqual([...visualEvents[0].targets].sort((a,b)=>a-b),live,'all selected targets retained');
 const impacts=events.filter(x=>[0x0c,0x10,0x32].includes(x.event)&&live.some(i=>actor(i)===x.actor));
 assert.equal(impacts.length,live.length);if(mode)assert.equal(new Set(impacts.map(x=>x.time)).size,1);
 for(const id of live)assert.equal(m.r16(actor(id)+0x20),70);
 assert.equal(m.r32(source+0x6c),3);nearestCases++;
}
console.log(`Shared melee anchor: ${nearestCases} shuffled formation/side/dead-slot/playback cases PASS; ranged and separate-strike order preserved.`);
// Maximum pattern, and dead slots whose stale timing must not hold the attack.
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',100));
m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',10+3*((c.get('a1')-actor(9))/0x100)));
for(const route of ['melee','ranged','alternate'])for(const deadSlots of [[],[10,14]]) {
 for(let i=0;i<18;i++) {
  const own=i<9,cell=i%9;
  m.w32(actor(i)+0x40,(own?0x100:0)|(deadSlots.includes(i)?1:0));
  m.w32(actor(i)+0x54,own?cell%3:2-cell%3);m.w32(actor(i)+0x58,own?2-Math.floor(cell/3):6+Math.floor(cell/3));
  m.w16(actor(i)+0x20,100);m.w32(actor(i)+0x94,deadSlots.includes(i)?9999:300+cell*31);
 }
 m.w32(source+0x7c,159);m.w32(source+0x6c,0);m.w32(ids,0);m.w32(global+0x828,0);
 m.w8(records+159*16+7,1);m.w8(rules+2,2);m.w8(rules+3,1);m.w8(rules+8,0);m.w8(rules+11,0);m.w8(rules+13,1);
 events.length=0;visualEvents.length=0;call(e['lc_attack_'+route],[source,ids],1000000);
 const live=Array.from({length:9},(_,i)=>i+9).filter(i=>!deadSlots.includes(i));
 const impacts=events.filter(x=>[0x0c,0x10,0x32].includes(x.event)&&live.some(i=>actor(i)===x.actor));
 assert.equal(impacts.length,live.length);assert.equal(new Set(impacts.map(x=>x.time)).size,1);
 assert(impacts[0].time>=548&&impacts[0].time<9999,'wait for occupied live targets only');
 assert.equal(events.filter(x=>x.actor===source&&(x.event===6||x.event===7)).length,1);
 const primary=route!=='ranged'&&live.includes(10)?10:live[0];
 assert.deepEqual(visualEvents[0].targets,[primary,...live.filter(i=>i!==primary)]);assert.equal(m.r32(source+0x6c),1);
 for(const i of live)assert.equal(m.r16(actor(i)+0x20),90-3*(i-9));
 for(const i of deadSlots)assert.equal(m.r16(actor(i)+0x20),100);
}
console.log('Shared full squads: all nine targets, dead-slot exclusion, bounded actor lists and readiness PASS.');
m.w8(rules+13,0);m.w8(rules+8,0);m.w8(rules+11,0);
for(let i=0;i<18;i++) {
 const own=i<9,cell=i%9;m.w32(actor(i)+0x40,own?0x100:0);
 m.w32(actor(i)+0x54,own?cell%3:2-cell%3);m.w32(actor(i)+0x58,own?2-Math.floor(cell/3):6+Math.floor(cell/3));
}
m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',100));
m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',60));m.trap(0x22222c+0x7ffbcd30,c=>c.set('v0',0));
m.trap(0x20bfe4+0x7ffbcb70,c=>c.set('v0',0));
console.log(`Shared physical playback: ${sharedCases} route/side/mode/hit/effect cases; one caster sequence, simultaneous target events, independent damage/healing/status and total-loss drain PASS (maximum ${maxSharedSteps} instructions).`);

// The actual generic executor selects a copied special action's donor pose.
// Execute its native direction helper with a same-column opposing target;
// only the downstream visual event sink is replaced.
m.trap(0x20bfe4+0x7ffbcb70,c=>c.set('v0',1));
m.trap(0x21d2c0+0x7ffbcd30,captureVisual);
const specialTable=global+0x2600;m.w32(0x801d073c,specialTable);
for(const [donor,owner,poseFlags,side,expected] of [[17,22,42,0,42],[32,57,170,0,43],[32,57,170,1,42]]){
 m.load(specialTable,Buffer.from([donor,owner,poseFlags,75,0,0,0,0]));
 m.w32(source+0x48,owner);m.w32(source+0x4c,owner);m.w32(source+0x54,m.r32(actor(9)+0x54));
 m.w32(source+0x40,side?0x100:0);m.w32(source+0x7c,159);m.w8(source+0x78,4);m.w32(source+0x6c,0);m.w32(source+0x94,0);
 m.w8(rules,donor);m.w8(rules+1,donor);m.w8(rules+2,0);m.w8(rules+3,0);m.w8(rules+11,0);
 m.w8(records+159*16+4,4);m.w8(records+159*16+7,1);m.w32(source+0x80,97);
 for(const i of [9,17]){m.w32(actor(i)+0x40,side?0:0x100);m.w16(actor(i)+0x20,100);m.w32(actor(i)+0x94,0);}
 m.w32(global+0x828,0);m.w32(ids,0);events.length=0;
 assert.equal(call(generic.lc_attack_generic,[source,ids]),1);
 assert(events.some(x=>x.event===11 && x.pose===expected),`copied special ${donor}, side ${side}, expected pose ${expected}`);
}
m.trap(0x20bfe4+0x7ffbcb70,c=>c.set('v0',0));m.trap(0x21d2c0+0x7ffbcd30,captureVisual);
console.log('Special animation donor and both same-column facing variants PASS through the generic executor.');

const combined=entries('attack_combined.elf');
const caster2=actor(5),input=global+0x1500;
m.w32(input,source);m.w32(input+4,caster2);m.w32(input+8,0);
m.trap(0x80092a60,c=>c.set('v0',0));
m.trap(0x22ef50+0x7ffbcd30,c=>{const a=c.get('a0');c.w8(a+0x78,3);c.w32(a+0x7c,a===source?55:63);c.w32(a+0x80,a===source?0:8);});
m.trap(0x20d814+0x7ffbcb70,c=>c.set('v0',0));
m.trap(0x20c364+0x7ffbcb70,c=>c.set('v0',0));
m.trap(0x233f94+0x7ffbcd30,c=>c.set('v0',13));
m.trap(0x2397d4+0x7ffbcd30,c=>{c.w8(c.get('a2'),1);c.set('v0',36);});
m.trap(0x2014c4+0x7ffbcb70,c=>c.set('v0',110));
m.trap(0x23887c+0x7ffbcd30,c=>c.set('v0',13));
m.trap(0x8020b150,c=>c.set('v0',2));
m.trap(0x201798+0x7ffbcb70,c=>c.set('v0',40));
m.w8(0x801971f0+12,100);m.w8(0x801976dc,0);m.w8(0x801976e8,0);
for(const effect of [0,1,2,3,4,5,6,8,10,11,16]){
 for(const caster of [source,caster2]){m.w32(caster,global+0x2000);m.w32(caster+0x40,0x100);m.w16(caster+0x20,200);m.w16(caster+0x22,500);m.w32(caster+0x6c,0);m.w32(caster+0x70,2);m.w32(caster+0x94,0);}
 for(const i of [9,17]){m.w32(actor(i)+0x40,effect===11?4:0);m.w16(actor(i)+0x20,100);m.w32(actor(i)+0x94,0);}
 const rule=settings+110*32;m.w8(rule+2,12);m.w8(rule+3,1);m.w16(rule+4,257);m.w8(rule+8,0);m.w8(rule+11,1);m.w8(rule+12,0);m.w16(rule+14,50);
 m.w8(records+110*16+7,effect);m.w8(records+110*16+9,1);m.w8(records+110*16+10,255);m.w32(global,0);m.w32(global+0x828,0);m.w32(ids,0);
 m.w8(rule+21,137);visualEvents.length=0;
 assert.equal(call(combined.lc_attack_combined,[input,ids]),1,`combined effect ${effect}`);
 assert.deepEqual(visualEvents,[{slot:137,sources:[4,5],targets:[9,17]}],'combined visual handoff');
 for(const i of [9,17]){assert.equal(m.r16(actor(i)+0x20),effect===1?40:effect===2?160:effect===16?0:100,`combined main ${effect}`);assert.equal(m.r32(actor(i)+0x40)&0x1f,({3:2,4:4,8:8,10:16,16:1})[effect]||0);}
 assert.equal(m.r16(source+0x20),effect===1?260:effect===16?300:200,'combined drain uses the first caster');
 assert.equal(m.r16(caster2+0x20),200);assert.equal(m.r32(source+0x6c),1);assert.equal(m.r32(caster2+0x6c),1);
}
console.log('Full combined executor: 11 effects, custom targets, both caster counts and first-caster drain PASS.');

// Pedra creates a temporary actor. Its construction and the visual event sink
// are controlled; the adapted action loop, target mask and status application run.
const pedra=entries('attack_pedra.elf'),pedraInput=global+0x1800;
m.w8(pedraInput+1,1);m.w8(pedraInput+2,159);
m.trap(0x20d870+0x7ffbcb70,c=>{const a=c.get('a0');for(let i=0;i<248;i++)c.w8(a+i,0);c.w32(a+0x40,0x100);c.w16(a+0x20,300);c.w16(a+0x22,500);});
m.trap(0x21d1cc+0x7ffbcd30,()=>{});
for(const effect of [0,1,2,3,4,5,6,8,10,11,16]){
 m.w32(global+0x1c4+0x48,0);m.w32(global+0x1c4+0x4c,0);m.w32(ids,0);
 for(const i of [9,17]){m.w32(actor(i)+0x40,effect===11?4:0);m.w16(actor(i)+0x20,100);m.w32(actor(i)+0x94,0);}
 m.w8(records+159*16+7,effect);m.w8(records+159*16+9,1);m.w8(records+159*16+10,255);
 m.w8(rules+2,12);m.w8(rules+3,1);m.w16(rules+4,257);m.w8(rules+8,0);m.w8(rules+11,1);m.w8(rules+12,0);m.w16(rules+14,50);
 m.w8(rules+21,138);visualEvents.length=0;
 assert.equal(call(pedra.lc_attack_pedra,[pedraInput,ids]),1,`Pedra effect ${effect}`);
 assert.equal(visualEvents.length,1);assert.equal(visualEvents[0].slot,138);assert.deepEqual(visualEvents[0].targets,[9,17],'Pedra visual handoff');
 for(const i of [9,17]){assert.equal(m.r16(actor(i)+0x20),effect===1?40:effect===2?160:effect===16?0:100,`Pedra main ${effect}`);assert.equal(m.r32(actor(i)+0x40)&0x1f,({3:2,4:4,8:8,10:16,16:1})[effect]||0);}
 assert.equal(m.r32(scratch),0);assert.equal(m.r16(global+0x1c4+0x20),0,'temporary actor cleared');
}
console.log('Full Pedra executor: 11 effects and custom targets PASS; temporary actor cleared, no persistent-attacker drain.');
for(const family of ['combined','pedra'])for(const count of [10,18])for(const hp of [120,2000]) {
 for(let i=0;i<18;i++) {
  m.w32(actor(i)+0x40,i<9?0x100:0);m.w16(actor(i)+0x20,i<9?200:hp);
  m.w16(actor(i)+0x22,4000);m.w32(actor(i)+0x94,0);
 }
 for(const caster of [source,caster2]) {
  m.w32(caster,global+0x2000);m.w32(caster+0x6c,0);m.w32(caster+0x70,2);
 }
 const action=family==='combined'?110:159,rule=settings+action*32;
 m.w8(records+action*16+7,1);m.w8(records+action*16+9,1);m.w8(records+action*16+10,255);
 m.w8(rule+2,11);m.w8(rule+3,1);m.w8(rule+22,count);m.w8(rule+8,10);m.w8(rule+9,1);m.w8(rule+10,100);
 m.w8(rule+11,1);m.w8(rule+12,0);m.w16(rule+14,50);m.w8(rule+21,138);
 m.w32(global,0);m.w32(global+0x828,0);m.w32(ids,0);visualEvents.length=0;
 call(family==='combined'?combined.lc_attack_combined:pedra.lc_attack_pedra,[family==='combined'?input:pedraInput,ids]);
 const loss=Array.from({length:9},(_,i)=>hp-m.r16(actor(9+i)+0x20)).reduce((a,b)=>a+b,0);
 assert.equal(loss,Math.min(hp,60*count),`${family}/${count}/${hp} loss`);
 assert.equal(visualEvents.length,1);assert.equal(visualEvents[0].targets.length,1);
 const victim=visualEvents[0].targets[0];assert(m.r32(actor(victim)+0x40)&16,'additional Poison applied');
 if(family==='combined') {
  assert.equal(m.r16(source+0x20),200+Math.trunc(loss/2));assert.equal(m.r16(caster2+0x20),200);
  assert.equal(m.r32(source+0x6c),1);assert.equal(m.r32(caster2+0x6c),1);
 } else assert.equal(m.r16(global+0x1c4+0x20),0);
}
for(const action of [110,159]){const rule=settings+action*32;m.w8(rule+2,12);m.w16(rule+4,257);m.w8(rule+22,0);m.w8(rule+8,0);}
for(let i=0;i<18;i++)m.w16(actor(i)+0x22,500);
console.log('Combined/Pedra 10/18 strikes: full HP loss, death/compaction, Poison, aggregate drain, one action and one bounded visual sequence PASS.');
// Combined and Pedra must not leak an old secondary marker when the authored
// chance is zero, the hit misses, or the victim survives / is not undead.
m.traps.delete(0x801c8d24);
for(const family of ['combined','pedra'])for(const undead of [false,true])for(const hp of [50,100])for(const hit of [false,true])for(const chance of [0,100]) {
 const action=family==='combined'?110:159,rule=settings+action*32;
 for(const caster of [source,caster2]){m.w32(caster+0x40,0x100);m.w16(caster+0x20,200);m.w32(caster+0x6c,0);m.w32(caster+0x70,2);m.w32(caster+0x94,0);}
 for(const i of [9,17]){m.w32(actor(i)+0x40,0);m.w16(actor(i)+0x20,hp);m.w32(actor(i)+0x4c,undead?45:2);m.w32(actor(i)+0x94,0);}
 m.w8(records+action*16+7,1);m.w8(records+action*16+9,1);
 m.w8(records+action*16+10,8);m.w8(records+action*16+11,7);m.w8(records+action*16+13,6);
 m.w8(rule+8,7);m.w8(rule+9,2);m.w8(rule+10,chance);m.w8(rule+11,0);
 m.w32(global,0);m.w32(global+0x828,0);m.w32(ids,0);events.length=0;sample=0;part=0;
 m.trap(0x2365bc+0x7ffbcd30,c=>c.set('v0',hit?100:0));
 call(family==='combined'?combined.lc_attack_combined:pedra.lc_attack_pedra,[family==='combined'?input:pedraInput,ids]);
 assert.equal(events.filter(x=>x.event===0x1f&&x.extra===7).length,undead&&hp===50&&hit&&chance===100?2:0,`${family} slaying ${undead}/${hp}/${hit}/${chance}`);
}
console.log('Combined/Pedra slaying: 64 executions × 2 targets, native-marker overrides, nonlethal/lethal, living/undead, misses and explicit chance PASS.');
console.log('Assigned visual events: generic, melee/ranged/alternate, combined and Pedra select independent slots and retain target IDs PASS.');

if(process.env.LORDLY_ATTACK_PATTERN_AUDIT){
 m.trap(0x20d770+0x7ffbcb70,c=>{let found=0;for(let i=0;i<18;i++)if(c.r32(actor(i)+0x54)===c.get('a0')&&c.r32(actor(i)+0x58)===c.get('a1'))found=actor(i);c.set('v0',found);});
 for(let i=0;i<18;i++)m.w32(actor(i)+0x40,i<9?0x100:0);
 for(let p=1;p<=11;p++){for(let i=0;i<128;i++)m.w8(packet+i,0);call(0x237e2c+0x7ffbcd30,[packet,p,13]);const count=m.r32(packet+12);assert(count<=10);console.log({nativePattern:p,ids:Array.from({length:count},(_,i)=>m.r32(packet+16+4*i)),weights:Array.from({length:count},(_,i)=>m.r8(packet+0x56+i))});}
}
