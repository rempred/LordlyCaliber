'use strict';
// Compare the adapted member helper with retail instructions using identical
// initialization/equipment service doubles, then run the real squad constructor.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const fixture = JSON.parse(fs.readFileSync(process.argv[2]));
const owner = require('../game/scenarios/squad-member.json'), S = owner.symbols;
function rom(path) {
  const b = fs.readFileSync(path);
  if (b.readUInt32BE(0) === 0x37804012) b.swap16();
  else if (b.readUInt32BE(0) === 0x40123780) b.swap32();
  assert.equal(b.readUInt32BE(0), 0x80371240); return b;
}
const original = rom(fixture.source), candidate = rom(fixture.rom);
const STOP = 0x80000100, SP = 0x803ffe00, TEMPLATE = 0x80300000;
function clobber(m, value = 0) {
  for (const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9']) m.set(r,0xdeadbeef);
  m.set('v0',value);
}
function setup(code, special, base) {
  const m = new Mips(), calls = [];
  m.ram = new Uint8Array(0x400000);
  m.load(owner.address,code);
  for(let cls=0;cls<256;cls++)m.w8(S.lc_classes+cls*72+69,cls%2 ? 0 : 5);
  m.w8(S.lc_scenario_level,base);
  m.trap(S.lc_initialize,c=>{
    const at=c.get('a0'), level=c.get('a1');
    calls.push(['initialize',at,c.r8(at+17),c.r8(at+18),level]);
    c.w8(at+16,level); c.w16(at+22,100+level); clobber(c);
  });
  m.trap(S.lc_equipment,c=>{
    const at=c.get('a1'),item=c.get('a0'); calls.push(['equipment',at,item]);
    if(item)c.w16(at+42+(item%4)*2,item); clobber(c);
  });
  m.trap(S.lc_finish_character,c=>{calls.push(['finish',c.get('a0'),c.get('a1')]);clobber(c);});
  m.trap(S.lc_special_scenario,c=>{calls.push(['special']);clobber(c,special);});
  m.trap(S.lc_clear,c=>{const at=c.get('a0'),size=c.get('a1');calls.push(['clear',at,size]);c.load(at,Buffer.alloc(size));clobber(c);});
  m.trap(S.lc_copy_name,c=>{calls.push(['name',c.get('a0'),c.get('a1')]);c.load(c.get('a0'),Buffer.from('SPECIAL\0'));clobber(c);});
  return {m,calls};
}
function call(m,entry,args) {
  m.set('sp',SP);
  args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(SP+16+(i-4)*4,v));
  const saved=['s0','s1','s2','s3','s4','s5','s6','s7','fp'];
  saved.forEach((r,i)=>m.set(r,0xabcd1000+i));
  m.call(entry,STOP);
  saved.forEach((r,i)=>assert.equal(m.get(r),0xabcd1000+i));
  assert.equal(m.get('sp'),SP);
}
let comparisons=0;
for(const selector of [0,1,2])for(const source of [0,29,30,49])for(const flag of [0,1])
for(const special of [0,1])for(const base of [1,99,250]) {
  const template=Buffer.alloc(35);
  [0,7,16].forEach((at,i)=>{template[at]=[5,165,255][i];template[at+1]=[0,255,127][i];template.writeUInt16BE(1+i,at+2);template.writeUInt16BE(i?510:0,at+4);});
  const outputs=[];
  for(const image of [original,candidate]) {
    const {m,calls}=setup(image.subarray(owner.start,owner.start+owner.size),special,base);
    m.load(TEMPLATE,template);
    const at=(flag||source>=30)?S.lc_enemy_records+3*52:S.lc_player_records+3*56;
    m.load(at-8,Buffer.alloc(72,0xa5));
    call(m,owner.address,[3,TEMPLATE,selector,flag,source]);
    outputs.push({calls,bytes:Buffer.from(m.ram.slice(m.phys(at-8),m.phys(at+64)))});
  }
  assert.deepEqual(outputs[1],outputs[0],`selector ${selector}, source ${source}, flag ${flag}, special ${special}, level ${base}`);
  comparisons++;
}
let constructions=0;
const complete=Buffer.from(fixture.record,'hex');
for(const count of [2,3,4,5])for(const source of [0,29,30,49,158,177]) {
  const record=Buffer.from(complete.subarray(0,count*35));
  for(const at of [13,14,15,22])record[at]=0;
  for(let i=1;i<5;i++) {
    record.writeUInt16BE(i<count?i:0,25+i*2);
    if(i<count)record[i<4?12+i:22]=9-i;
  }
  const {m,calls}=setup(candidate.subarray(owner.start,owner.start+owner.size),0,10);
  m.load(0x80215340,original.subarray(0x1957d0,0x195d9c));
  m.load(TEMPLATE,record);
  m.trap(0x801d4928,c=>{calls.push(['squad-finalize',c.get('a0')]);clobber(c,7);});
  m.trap(0x80093540,()=>assert.fail('Constructor exhausted records'));
  call(m,0x80215340,[TEMPLATE,source]);
  const sourceId=source&127, row=S.lc_sources+sourceId*25;
  const pool=sourceId>=30 ? S.lc_enemy_records:S.lc_player_records, stride=sourceId>=30?52:56;
  for(let i=0;i<count;i++) {
    const id=m.r8(row+2+i),at=pool+id*stride;
    assert.equal(id,i+1);
    assert.equal(m.r8(at+17),5+i);
    assert.equal(m.r8(at+16),10+i);
    assert.equal(m.r8(at+51),i?0x80:0x82);
    assert.equal(m.r8(row+7+i),8-i);
    const eq=calls.filter(c=>c[0]==='equipment'&&c[1]===at).map(c=>c[2]);
    assert.deepEqual(eq,[record.readUInt16BE(i*35+2),record.readUInt16BE(i*35+4)]);
  }
  for(let i=count;i<5;i++)assert.equal(m.r8(row+2+i),0);
  assert.equal(m.r8(row+24),7);
  constructions++;
}
console.log(`PASS: ${comparisons} retail helper comparisons; ${constructions} original-constructor runs with two through five independent members, both pools and allied flags. Initialization/equipment/finalizer services are controlled doubles; no emulator boot.`);
