'use strict';
// Exported attack hooks and original amount kernel in isolated 4 MB RAM.
// Context services are controlled traps; animation playback and console boot
// are not simulated. Private RAM changes below do not modify either ROM file.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');
function read(path) { const b=fs.readFileSync(path);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b; }
const original=read(process.argv[2]), built=read(process.argv[3]);
const h=built.subarray(0x27c5300,0x27c5360);
assert.equal(h.subarray(0,8).toString(),'LCATK001');
const records=h.readUInt32BE(28), settings=h.readUInt32BE(32), stack=0x803ff000,
      source=0x803d0000, target=source+0x100;
const f=Math.fround, bits=new DataView(new ArrayBuffer(4));
function floatBits(n){bits.setFloat32(0,n);return bits.getUint32(0);}
function machine(rom) {
  const m=new FloatMips();m.ram=new Uint8Array(0x400000);
  m.load(0x80070000,rom.subarray(0x400,0x2f000));
  m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
  m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
  m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
  m.load(0x801f36ac,rom.subarray(0x23697c,0x237750));
  m.load(0x801f6830,rom.subarray(0x239b00,0x23a900));
  if(rom===built){const p=rom.subarray(0x5dc80,0x5dd00);const base=p.readUInt32BE(24),at=p.readUInt32BE(12),end=p.readUInt32BE(48);m.load(base,rom.subarray(at,at+end-base));}
  m.set('sp',stack);
  return m;
}
const m=machine(built), retail=machine(original);
function call(c,pc,args){args.slice(0,4).forEach((v,i)=>c.set(['a0','a1','a2','a3'][i],v));args.slice(4).forEach((v,i)=>c.w32(stack+16+4*i,v));c.call(pc);assert.equal(c.get('sp'),stack);return c.get('v0');}
for(let id=1;id<159;++id)assert.equal(call(m,0x801bdfbc,[id]),call(retail,0x801bdfbc,[id]),`retail spell index ${id}`);
for(let donor=1;donor<159;++donor){m.w8(settings+159*32+1,donor);assert.equal(call(m,0x801bdfbc,[159]),call(retail,0x801bdfbc,[donor]),`copied spell index ${donor}`);}
// The native element and name getters must reach the relocated new record.
assert.equal(call(m,0x8016e470,[159,0,0,0,0]),2);
const pointer=call(m,0x8016e440,[159]);
let name='';for(let at=pointer;m.r8(at);++at){assert(name.length<128);name+=String.fromCharCode(m.r8(at));}
assert.equal(name,'Copied Slash');
const count=h.readUInt32BE(36);
for(let id=159;id<count;++id){assert.equal(call(m,0x8016e458,[id]),m.r8(records+id*16+4),`new ID ${id} mode reader`);const n=call(m,0x8016e440,[id]);assert(n>=h.readUInt32BE(16)&&n<h.readUInt32BE(16)+h.readUInt32BE(20),`new ID ${id} name bounds`);}
if(count===255){assert.equal(call(m,0x8016e230,[2,2,0]),254);assert.equal(call(m,0x8016e230,[2,2,1]),159);assert.equal(call(m,0x8016e338,[2,2,0]),3);assert.equal(call(m,0x8016e338,[2,2,1]),2);}
// A copied elemental template resolves exactly like its retail donor. These
// cases control equipment choice/element, while executing both real resolvers.
const resolver=0x4440c+0x8012a100,out=source+0x400;
let itemElement=1,templateCases=0;
for(const c of [m,retail]){
 c.trap(0x45a50+0x8012a100,x=>x.set('v0',243));
 c.trap(0x45934+0x8012a100,x=>x.set('v0',243));
 c.trap(0x454e0+0x8012a100,x=>x.set('v0',itemElement));
 c.trap(0x44130+0x8012a100,x=>x.set('v0',4));
}
for(const donor of [45,46,47,48,49,50,51,52,53,54,145])for(itemElement=0;itemElement<=7;itemElement++){
 for(let i=0;i<12;i++)m.w8(records+159*16+4+i,original[0x60980+donor*16+i]);m.w8(settings+159*32,donor);
 const result=call(retail,resolver,[out,2,2,donor,1,1,1,1,1]),resolved=retail.r8(out);
 assert.equal(call(m,resolver,[out,2,2,159,1,1,1,1,1]),result);
 assert.equal(m.r8(out),resolved===donor?159:resolved,`template ${donor}, element ${itemElement}`);templateCases++;
}
// Actual battle selector: copied best-spell and breath templates keep their
// special selection logic. Class choice, equipment and score services are fixed.
let template=49;
m.trap(0x44130+0x8012a100,c=>c.set('v0',159));
m.trap(0x233f94+0x7ffbcd30,c=>c.set('v0',0));
m.trap(0x20c478+0x7ffbcb70,c=>c.set('v0',target));
m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',c.r32(source+0x7c)));
m.trap(0x20c448+0x7ffbcb70,c=>c.set('v0',1));
m.w32(source+0x48,2);m.w32(source+0x4c,2);m.w32(source+0x58,6);
for(template of [49,50,145])for(itemElement=0;itemElement<=6;++itemElement){
 m.w8(settings+159*32,template);m.w8(settings+159*32+1,template);
 for(let i=0;i<12;i++)m.w8(records+159*16+4+i,original[0x60980+template*16+i]);
 call(m,0x22ef50+0x7ffbcd30,[source]);assert.equal(m.r32(source+0x7c),template===145?146+itemElement:93);assert.equal(m.r8(source+0x78),template===145?4:3);
}
for(const c of [m,retail]) {
  c.trap(0x801f2d10,x=>x.set('v0',40));
  c.trap(0x801f26d4,x=>x.set('v0',40));
  c.trap(0x8016e470,x=>x.set('v0',0));
  c.trap(0x8016d98c,x=>x.set('v0',0));
  c.trap(0x8016f83c,x=>x.set('v0',0));
  c.trap(0x8016f644,x=>x.set('v0',0));
  c.trap(0x801cd108,x=>x.set('v0',0));
  c.trap(0x8016fef4,x=>x.set('v0',0));
  c.w32(source+0x7c,4);c.w16(source+0x20,300);c.w16(target+0x20,121);
}
let cases=0;
function amount(c,power,secondary,flags){return call(c,0x801f36ac,[source,target,floatBits(power),secondary,77,flags]);}
for(const mode of [0,3])for(const payload of [0,1,2,3,4,5,6,7,8,9,10])for(const percent of [0,25,100,150,275,1000])for(const secondary of [0,1]) {
  m.w8(source+0x78,mode);retail.w8(source+0x78,mode);
  for(const [c,base] of [[m,records+4],[retail,0x8018aa80]]) {c.w8(base+4*16+4,payload);c.w8(base+4*16+8,payload);}
  m.w16(settings+4*32+6,percent);
  const scaled=secondary?100:f(f(100*percent)/100);
  assert.equal(amount(m,100,secondary,0),amount(retail,scaled,secondary,0),`mode=${mode} payload=${payload} percent=${percent} secondary=${secondary}`);
  cases++;
}
m.w8(records+4+4*16+4,1);m.w16(settings+4*32+6,100);
const ordinary=amount(m,100,0,0);
m.w16(settings+4*32+6,200);
const doubledPower=amount(m,100,0,0);
assert.equal(ordinary,60);assert.equal(doubledPower,160);
assert.notEqual(doubledPower,ordinary*2);
// The combined spell owns edited power/calculation, while the first caster's
// original action identity is restored after calculating each target.
const scratch=settings+256*32;
m.w32(scratch,source);m.w32(scratch+4,110);m.w32(source+0x7c,55);m.w8(source+0x78,3);
m.w8(records+110*16+5,0);m.w8(records+110*16+8,4);m.w16(settings+110*32+6,200);
assert.equal(amount(m,100,0,0),60);assert.equal(m.r32(source+0x7c),55);
m.w8(records+110*16+8,1);assert.equal(amount(m,100,0,0),160);assert.equal(m.r32(source+0x7c),55);m.w32(scratch,0);
console.log(JSON.stringify({spellIndexCases:316,relocatedNameAndElement:'PASS',amountCases:cases,
  newIdsTested:count-159,templateCases,bestSpellAndBreathSelectors:21,combinedPowerAndCalculation:'PASS',
  comparison:{ordinary,doubledPower,doubledFinal:ordinary*2},
  limit:'Original and exported instruction execution with controlled context; no full battle or animation playback.'}));
