'use strict';
// Installed callers, real exported profile/reward data, original small-read DMA.
// Lower PI transfer, RNG and actor/constructor services remain controlled doubles.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixtures=JSON.parse(fs.readFileSync(process.argv[2]));
const owners=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/encounters/custom-owners/catalog.json'))).owners;
let cases=0,transfers=0;
for(const f of fixtures) for(const alignment of [0,8]) for(const result of ['victory','persuaded','retail']) {
 const rom=fs.readFileSync(f.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
 const m=new Mips();m.ram=new Uint8Array(0x400000);
 m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));m.load(0x8009da50,rom.subarray(0x2de50,0x2def4));
 const S=f.symbols,state=f.state,sp=0x803ffe00+alignment,STOP=0x80000100;
 const clobber=c=>{for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xdeadbeef);};
 m.load(sp-0x500,Buffer.alloc(0x580,0xa6));m.set('sp',sp);
 m.trap(0x80089f80,c=>{
  const at=c.get('a0'),to=c.get('a1'),size=c.get('a2');assert(at>=0x2800000&&at+size<=rom.length);
  c.load(to,rom.subarray(at,at+size));transfers++;clobber(c);
 });
 let rolls=0;m.trap(S.lc_random,c=>{rolls++;clobber(c);c.set('v0',0);});
 m.w8(S.lc_staged_scenario,result==='retail'?32:33);m.set('s0',41);m.set('s1',0x80260000);m.set('s6',29);
 m.set('v0',5);m.set('v1',0x801ed794+40*20+18);
 const scenarioRam=at=>(at+0x800ab8c0)>>>0;
 m.load(scenarioRam(0x13c060),rom.subarray(0x13c060,0x13c49c));
 m.call(scenarioRam(0x13c414),S.lc_selection_success);
 assert.equal(m.get('v0'),261);assert.equal(m.get('s1'),0x80260000);assert.equal(m.get('s6'),29);
 assert.equal(m.r32(state+4),result==='retail'?0:1);
 if(result!=='retail') {
  const profile=rom.readUInt32BE(S.lc_custom_index+(33*10+9)*4);
  assert.deepEqual(Buffer.from(m.ram.slice(m.phys(state+16),m.phys(state+144))),rom.subarray(profile,profile+128));
  const source=0x801971f0+49*25,owner=owners.find(o=>o.name==='materializer');
  m.load(owner.address,rom.subarray(owner.start,owner.start+owner.size));
  m.load(S.lc_character_records,Buffer.alloc(120*52,1));for(const id of [1,119])m.w8(S.lc_character_records+id*52+17,0);
  m.load(source,Buffer.alloc(25));m.w8(source,0x81);m.set('s1',source);m.set('s2',10);m.set('s3',5);
  const created=[];m.trap(S.lc_character_create,c=>{
   created.push([c.get('a0'),c.get('a1'),c.get('a2'),c.get('a3'),c.r32(c.get('sp')+16)]);
   c.w8(c.get('a0')+17,1);clobber(c);
  });
  m.call(owner.address+owner.sites[0].offset,S.lc_materializer_epilogue);
  assert.deepEqual(created,[[S.lc_character_records+52,5,10,277,0],[S.lc_character_records+119*52,71,9,0,0]]);
  assert.equal(m.r8(source),0x81);assert.equal(m.r8(source+2),1);assert.equal(m.r8(source+3),119);
  assert.equal(m.r8(source+7),8);assert.equal(m.r8(source+8),6);assert.equal(m.r32(state),0);
  if(result==='persuaded') {
   const owner=owners.find(o=>o.name==='persuasion');m.load(owner.address,rom.subarray(owner.start,owner.start+owner.size));
   m.set('s0',0x80260000);m.set('s1',19);m.set('s2',0);
   m.call(owner.address+owner.sites[0].offset,S.lc_persuasion_epilogue);
   assert.equal(m.get('v0'),0);assert.equal(m.r32(state+8),1);assert.equal(m.r32(state+12),0x80260000);
  }
 }
 m.load(scenarioRam(0x101ddc),rom.subarray(0x101ddc,0x102f00));
 m.set('s0',0xaaaaaaaa);m.set('s1',0xbbbbbbbb);m.set('s2',0xcccccccc);m.set('s4',5);m.w8(0x80193670,0x7f);
 m.call(scenarioRam(0x10245c),scenarioRam(0x1024d4));
 assert.equal(m.get('s3'),result==='retail'?0x8115:result==='victory'?44:0);assert.equal(m.get('v0'),0x7f);
 assert.equal(rolls,result==='persuaded'?0:1);
 assert.equal(m.get('s0'),0xaaaaaaaa);assert.equal(m.get('s1'),0xbbbbbbbb);assert.equal(m.get('s2'),0xcccccccc);
 assert.equal(m.get('s4'),5);assert.equal(m.get('sp'),sp);
 for(let i=0;i<16;i+=4)assert.equal(m.r32(state+i),0);
 for(let i=0;i<0x80;i++)if(i<0x20||i>=0x30)assert.equal(m.r8(sp+i),0xa6);
 for(let i=-0x500;i< -0x480;i++)assert.equal(m.r8(sp+i),0xa6);
 cases++;
}
console.log(`PASS ${cases} installed encounter flows, ${transfers} lower transfers through the original small-read DMA wrapper: ROM profile selection, per-member construction, victory/persuasion/class rewards and state cleanup at both stack alignments. 4 MiB guarded memory; actor services and PI hardware are simulated, not a full-game proof.`);
