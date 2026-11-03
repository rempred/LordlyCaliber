'use strict';
// Native impact consumer contract and exported repeat-playback bridge. Rendering
// is covered separately by EffectRasterTests::repeatedStrikePlayback.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const reference=process.argv.includes('--reference-only'),m=new FloatMips();
m.ram=new Uint8Array(0x800000);
m.load(0x80070000,rom.subarray(0x400,0x2f000));
// Battle's relocated owner ranges, as in attack-effects-mips.cjs; these are
// different from the visual overlay descriptor numbers used by EffectPlayer.
m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
const stack=0x807ff000,battle=0x80740000,stream=0x80770000,actor=battle+0x1c4+9*0xf8;
m.set('sp',stack);m.w32(0x801ce8bc,battle);m.w32(0x801ce8c0,stream);
m.w32(actor+0x48,5);m.w16(actor+0x20,2000);
const delivered=[];
// The native consumer, its result selection, queue decrement and completion
// barrier run. Only downstream pose/damage presentation and clock services are
// controlled; no model of damage calculation is substituted.
m.trap(0x801e1014,c=>delivered.push({amount:c.get('a3'),tick:c.get('a2'),target:c.get('a0')}));
m.trap(0x801d9860,()=>{});m.trap(0x801d8e70,()=>{});
function call(pc,args) {
 args.forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));
 m.call(pc,0x80000100,200000);assert.equal(m.get('sp'),stack);return m.get('v0')|0;
}
function result(offset,amount,continuation=false) {
 const p=stream+16+offset;
 [31,0,0,9,continuation?128:0,amount>>8,amount&255,0,1,0].forEach((v,i)=>m.w8(p+i,v));
}
function entries(file) {
 const b=fs.readFileSync(path.join(path.dirname(process.argv[2]),file)),out={};
 const at=b.readUInt32BE(32),width=b.readUInt16BE(46),count=b.readUInt16BE(48);
 for(let i=0;i<count;i++) {
  const h=at+i*width;if(b.readUInt32BE(h+4)!==2)continue;
  const names=b.readUInt32BE(at+b.readUInt32BE(h+24)*width+16),begin=b.readUInt32BE(h+16),size=b.readUInt32BE(h+20);
  for(let j=begin;j<begin+size;j+=16) {
   const n=names+b.readUInt32BE(j),end=b.indexOf(0,n);out[b.toString('utf8',n,end)]=b.readUInt32BE(j+4);
  }
 }
 return out;
}
const e=reference?{}:entries('attack_strikes.elf');
if(!reference) {
 const cp=0x5dc80,base=rom.readUInt32BE(cp+24),start=rom.readUInt32BE(cp+12),end=rom.readUInt32BE(cp+48);
 m.load(base,rom.subarray(start,start+end-base));
}
// This mode needs only the unchanged ROM; it checks the recovered interface
// without compiling the editor or game code.
for(const count of [1,6,18]) {
 delivered.length=0;m.w32(stream+0x814,0);m.w8(stream+0x824,count);
 for(let i=0;i<count;i++) {
  result(0x100,17+i);m.w32(actor+0xb0,0x100);m.w8(actor+0xd0,1);m.w8(actor+0xd1,1);
  assert.equal(delivered.length,i,'no result before contact');m.w32(stream+8,25+i*7);
  call(reference?0x801df404:e.lc_original_visual_impact,[9,10]);
  assert.deepEqual(delivered[i],{amount:17+i,tick:25+i*7,target:actor});
  assert.equal(m.r8(actor+0xd0),0);assert.equal(m.r8(stream+0x824),count-i-1);
 }
}
console.log('Original impact consumer: 1/6/18 timed contacts, one queued result per contact, completion count PASS.');
if(reference)process.exit(0);
const command=0x80720000;
function begin(count) {
 for(let i=0;i<4160;i++)m.w8(e.lc_strike_state+i,0);
 const bytes=[22,0,0,0,5,0,255,...Array(count).fill(9),255,128,...Array(count-1).fill(0),255];
 bytes.forEach((v,i)=>m.w8(command+i,v));assert.equal(call(e.lc_strike_command,[command]),1);
 m.w8(actor+0xd0,0);m.w8(actor+0xd1,0);m.w32(stream+0x814,0);m.w8(stream+0x824,count);
 delivered.length=0;
}
for(const count of [1,6,9,10,18]) {
 begin(count);
 for(let i=0;i<count;i++) { result(0x100+i*10,20+i);assert.equal(call(e.lc_strike_result,[actor,0x100+i*10]),1); }
 assert.equal(delivered.length,0);assert.equal(m.r8(actor+0xd0),0,'18 results do not overflow the native actor queue');
 for(let i=0;i<count;i++) {
  m.w32(stream+8,100+i*11);call(e.lc_attack_visual_impact,[9,10]);
  assert.equal(delivered.length,i+1);assert.deepEqual(delivered[i],{amount:20+i,tick:100+i*11,target:actor});
 }
 assert.equal(m.r8(stream+0x824),0);
 call(e.lc_attack_visual_impact,[9,10]);assert.equal(delivered.length,count,'no duplicate final damage');
 call(e.lc_strike_end,[]);
 result(0x100,99);m.w32(actor+0xb0,0x100);m.w8(actor+0xd0,1);m.w8(actor+0xd1,1);m.w8(stream+0x824,1);
 call(e.lc_attack_visual_impact,[9,10]);assert.equal(delivered.at(-1).amount,99,'ordinary playback resumes');
}
begin(2);
for(const [i,amount,extra] of [[0,10,false],[1,20,true],[2,30,false]]) {
 result(0x100+i*10,amount,extra);call(e.lc_strike_result,[actor,0x100+i*10]);
}
call(e.lc_attack_visual_impact,[9,10]);assert.deepEqual(delivered.map(x=>x.amount),[10,20]);
assert.equal(m.r8(stream+0x824),1,'additional result belongs to the same impact');
call(e.lc_attack_visual_impact,[9,10]);assert.deepEqual(delivered.map(x=>x.amount),[10,20,30]);assert.equal(m.r8(stream+0x824),0);
console.log('Exported repeat bridge: 1/6/9/10/18 results, continuation effects, ordinary-attack reset and barriers PASS.');
for(const [target,answer] of [[0x801c8afc,0],[0x80000500,1]]) {
 m.w32(0x801ddfb0,0x0c000000|(target>>>2&0x3ffffff));
 m.trap(target,c=>c.set('v0',answer));
 assert.equal(call(e.lc_strike_round_mode,[]),answer,'retain retail/custom encounter round mode');
}
console.log('Round mode: retail and custom encounter call targets preserved PASS.');
