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
function call(name,args){args.slice(0,4).forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));args.slice(4).forEach((v,i)=>m.w32(stack+16+i*4,v));m.call(typeof name==='number'?name:e[name]);assert.equal(m.get('sp'),stack);return m.get('v0')|0;}
const actor=i=>actors+i*0x100;
const services={lookup:0x801c8fe8,dead:0x801c8e30,immune:0x801f46dc,up:0x801da4b4,down:0x801da4d8,rebuild:0x801da4fc};
// Addresses follow accepted calls, including the nonuniform overlay placement.
services.lookup=0x20c478+0x7ffbcb70;services.immune=0x2379ac+0x7ffbcd30;
m.trap(services.lookup,c=>c.set('v0',c.get('a0')<18?actor(c.get('a0')):0));
m.trap(services.dead,c=>c.set('v0',!c.get('a0')||!!(c.r32(c.get('a0')+0x40)&1)));
let immunity=true;m.trap(services.immune,c=>c.set('v0',immunity?1:0));
m.trap(0x801c8d24,c=>c.set('v0',0)); // Undead predicate.
const events=[];for(const pc of [0x21d200,0x21d230,0x21d25c,0x21d28c,0x21d334])m.trap(pc+0x7ffbcd30,c=>events.push({event:c.get('a0'),time:c.get('a1'),actor:c.get('a2'),kind:c.get('a3'),extra:c.r32(c.get('sp')+32)}));
const visualEvents=[];
function captureVisual(c){
 events.push({event:c.get('a0'),pose:c.r32(c.get('sp')+24)});
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
  for(const i of [9,17]){
    const expectedHp=effect===1?40:effect===2?160:effect===16?0:100;
    assert.equal(m.r16(actor(i)+0x20),expectedHp,`generic effect ${effect}, target ${i}`);
    const expectedFlag=({3:2,4:4,8:8,10:16,16:1})[effect]||0;
    assert.equal(m.r32(actor(i)+0x40)&0x1f,expectedFlag,`generic flags ${effect}, target ${i}`);
  }
  assert.equal(m.r32(source+0x6c),1);assert.equal(m.r32(scratch),0);
}
console.log('Full generic executor: 11 main effects × 2 targets PASS (controlled score and event services).');

// Preserve the actual melee executor and physical HP sink while adding targets.
// Artwork timing and already-tested numeric kernels are fixed here.
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
console.log('Assigned visual events: generic, melee/ranged/alternate, combined and Pedra select independent slots and retain target IDs PASS.');

if(process.env.LORDLY_ATTACK_PATTERN_AUDIT){
 m.trap(0x20d770+0x7ffbcb70,c=>{let found=0;for(let i=0;i<18;i++)if(c.r32(actor(i)+0x54)===c.get('a0')&&c.r32(actor(i)+0x58)===c.get('a1'))found=actor(i);c.set('v0',found);});
 for(let i=0;i<18;i++)m.w32(actor(i)+0x40,i<9?0x100:0);
 for(let p=1;p<=11;p++){for(let i=0;i<128;i++)m.w8(packet+i,0);call(0x237e2c+0x7ffbcd30,[packet,p,13]);const count=m.r32(packet+12);assert(count<=10);console.log({nativePattern:p,ids:Array.from({length:count},(_,i)=>m.r32(packet+16+4*i)),weights:Array.from({length:count},(_,i)=>m.r8(packet+0x56+i))});}
}
