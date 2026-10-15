'use strict';
// Real exported status instructions in private RAM; only RNG, poses and event
// emission are deterministic services. This does not emulate a complete battle.
const fs=require('fs'), assert=require('assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const image=fs.readFileSync(process.argv[2]),percent=Number(process.argv[3]),chance=Number(process.argv[4]);
if(image.readUInt32BE(0)===0x37804012)image.swap16();
const m=new FloatMips(),actor=0x80300000,context=0x80400000,stack=0x807fe000;
for(const [address,start,end] of [[0x801da180,0x21d450,0x21d7e8],[0x801c8b68,0x20bff8,0x20c050],
  [0x801c8e30,0x20c2c0,0x20c364],[0x801e6c28,0x229ef8,0x229f00],
  [0x801eb938,0x22ec08,0x22ef50],[0x801e7694,0x22a964,0x22adfc]])m.load(address,image.subarray(start,end));
for(const address of [0x801d9f30,0x801d9f60,0x801d9f8c,0x801d9fbc,0x801d9ff0])m.trap(address,()=>{});
m.trap(0x801df2ac,c=>c.set('v0',3));
m.trap(0x8009c7cc,c=>c.set('v0',++c.random%3===0?c.roll:0));
function setup(luck=50,flags=0,base=200) {
  m.r.fill(0);m.set('sp',stack);m.ram.fill(0,m.phys(actor),m.phys(actor)+0x100);
  m.w32(actor,0x80500000);m.w16(actor+0x20,123);m.w16(actor+0x22,456);
  m.w8(actor+0x30,luck);m.w32(actor+0x40,flags);m.w32(actor+0x6c,7);
  m.w32(0x801ce8c0,context);m.w32(context+0x828,20);m.w32(context,10);
  for(let i=0;i<6;i++)m.w16(0x80193bc0+0x1c+i*2,base);
  m.w8(0x80193bc0+0x28,luck);m.random=0;
}
let checks=0;
for(const base of [0,1,200,999])for(const poison of [false,true])for(const direction of [0,1,-1]) {
  setup(50,poison?0x10:0,base);const before=Buffer.from(m.ram.subarray(m.phys(0x80193bc0),m.phys(0x80193bc0)+56));
  let accumulator=Math.fround(0),count=direction?40:1;
  for(let i=0;i<count;i++) {
    accumulator=Math.fround(accumulator+Math.fround(direction*percent/100));
    m.set('a0',actor);m.call(direction>0?0x801da4b4:direction<0?0x801da4d8:0x801da4fc);
    const effective=poison?Math.fround(accumulator-percent/100):accumulator;
    const scaled=Math.trunc(Math.fround(base+Math.fround(base*effective)));
    for(let stat=0;stat<6;stat++)assert.equal(m.r16(actor+0x24+stat*2),Math.min(999,Math.max(0,scaled)),`strength=${percent}, base=${base}, direction=${direction}, poison=${poison}, iteration=${i}`);
    assert.equal(m.r8(actor+0x30),Math.min(100,Math.max(0,Math.trunc(Math.fround(50+Math.fround(50*effective))))));
    assert.equal(m.r16(actor+0x20),123);assert.equal(m.r16(actor+0x22),456);
    assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(0x80193bc0),m.phys(0x80193bc0)+56)),before);checks++;
  }
}
for(const luck of [0,50,99,100])for(let roll=0;roll<100;roll++)for(const kind of ['sleep','paralysis','hit']) {
  const flag=kind==='paralysis'?8:4;setup(luck,flag);m.roll=roll;m.set('a0',actor);
  if(kind==='hit') {m.set('a1',10);m.set('a2',1);m.set('a3',0);m.w32(stack+16,0);m.call(0x801e7694);}
  else {m.set('a1',context);m.call(kind==='sleep'?0x801ebb04:0x801eb938);assert.equal(m.r32(actor+0x6c),8,'lost action is consumed');}
  assert.equal(!(m.r32(actor+0x40)&flag),chance<0?roll<=luck:roll<chance,`${kind} chance=${chance} luck=${luck} roll=${roll}`);
  assert.equal(m.random,3);checks++;
}
console.log(`${checks} exported status instruction checks passed: strength ${percent}%, recovery ${chance<0?'Luck':chance+'%'}`);
