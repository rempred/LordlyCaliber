'use strict';
// Execute the exported resident routine. Expected effects come from the authored
// model and explicit game limits; the random service is controlled, not emulated.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
const rom=fs.readFileSync(fixture.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const models=fixture.models,entry=0x8016b198,characters=0x80193bc0;
function cpu(){
  const m=new Mips();m.ram=new Uint8Array(0x400000);
  m.load(entry,rom.subarray(0x41098,0x415fc));
  m.load(0x8018fe60,rom.subarray(0x65d60,0x65dac));
  m.set('sp',0x803f0000);
  return m;
}
function run(m,id,actor,apply,value=0,random=0){
  m.set('a0',apply?1:0);m.set('a1',value);m.set('a2',id);m.set('a3',actor);
  let randomCalls=0,levelCalls=0;
  m.trap(0x8009c7cc,c=>{
    for(const r of ['v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xcccccccc);
    c.set('v0',random);randomCalls++;
  });
  m.trap(0x8016eba4,c=>{
    assert.equal(c.get('a0'),characters+actor*56);levelCalls++;
    for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xcccccccc);
  });
  m.call(entry);
  assert.equal(m.get('sp'),0x803f0000);
  return {value:m.get('v0'),randomCalls,levelCalls};
}
const slice=(m,a,n)=>Buffer.from(m.ram.slice(m.phys(a),m.phys(a)+n));
let queries=0,commits=0,special=0;
const ranges=[['cupOfLife',10,0x16,2,999],
  ...Array.from({length:6},(_,i)=>['sharedStatBoosters11To16',i+11,0x1c+i*2,2,999]),
  ['scrollOfDiscipline',17,0x1b,1,100],['urnOfChaos',18,0x1b,1,100],
  ['gobletOfDestiny',19,0x28,1,100]];
for(const [key,id,offset,bytes,limit] of ranges){
  const {minimum,maximum}=models[key],width=maximum-minimum+1;
  for(const actor of [0,43,99])for(const random of [0,1,width-1,width,width*3+2,32767]){
    const m=cpu(),base=characters+actor*56;m.load(base-8,Buffer.alloc(72,0x55));
    const before=slice(m,base-8,72),delta=minimum+random%width;
    const result=run(m,id,actor,false,0,random);
    assert.equal(result.value,delta>>>0);assert.equal(result.randomCalls,1);
    assert.deepEqual(slice(m,base-8,72),before);queries++;
    for(const initial of [0,1,limit-1,limit]){
      m.load(base-8,before);if(bytes===2)m.w16(base+offset,initial);else m.w8(base+offset,initial);
      const expected=slice(m,base-8,72),value=Math.max(0,Math.min(limit,initial+delta));
      if(bytes===2)expected.writeUInt16BE(value,offset+8);else expected[offset+8]=value;
      assert.equal(run(m,id,actor,true,result.value).randomCalls,0);
      assert.deepEqual(slice(m,base-8,72),expected);commits++;
    }
  }
}
for(const [key,id] of [['healLeaf',1],['healSeed',2],['healPack',3],['powerFruit',4],['angelFruit',5]]){
  const amount=models[key].magnitude;
  for(const actor of [0,99])for(const initial of [0,1,249,250]){
    const m=cpu(),base=characters+actor*56;m.load(base-8,Buffer.alloc(72,0x55));
    m.w16(base+0x16,250);m.w16(base+0x18,initial);m.w8(base+0x32,initial);
    const before=slice(m,base-8,72),result=run(m,id,actor,false);
    assert.equal(result.value,amount);assert.equal(result.randomCalls,0);
    assert.deepEqual(slice(m,base-8,72),before);queries++;
    const expected=Buffer.from(before);
    if(id<=3)expected.writeUInt16BE(initial===0?0:Math.min(250,initial+amount),0x18+8);
    else expected[0x32+8]=Math.max(0,initial-amount);
    run(m,id,actor,true,result.value);assert.deepEqual(slice(m,base-8,72),expected);commits++;
  }
  for(const actor of [100,101,127])for(const initial of [0,1,299,300,301,599,600,601,899,900]){
    const m=cpu(),pool=0x80190ebc+actor*2;m.load(0x80190f80,Buffer.alloc(64,0x55));
    m.w16(0x80190f82,900);m.w16(pool,initial);
    const before=slice(m,0x80190f80,64),result=run(m,id,actor,false);
    assert.equal(result.value,amount);assert.deepEqual(slice(m,0x80190f80,64),before);queries++;
    const expected=Buffer.from(before);
    if(id<=3)expected.writeUInt16BE(Math.min(initial+amount,Math.min(3,Math.ceil(initial/300))*300),pool-0x80190f80);
    run(m,id,actor,true,result.value);assert.deepEqual(slice(m,0x80190f80,64),expected);special++;
  }
}
// Cure removes only Petrification, from the correct ordinary or Soldier status.
let retained=0;
for(const actor of [0,99,100,127])for(const status of [0,4,0xff]){
  const m=cpu(),at=actor<100?characters+actor*56+0x33:0x80191010+actor;
  m.load(at-8,Buffer.alloc(17,0x55));m.w8(at,status);
  const before=slice(m,at-8,17);
  assert.equal(run(m,6,actor,false).value,0);assert.deepEqual(slice(m,at-8,17),before);
  const expected=Buffer.from(before);expected[8]=status&0xfb;
  assert.equal(run(m,6,actor,true,999).value,0);assert.deepEqual(slice(m,at-8,17),expected);retained++;
}
// Revival returns maximum HP when queried, applies the supplied amount, and
// clears Petrification without disturbing other flags or nearby character data.
for(const actor of [0,99])for(const hp of [0,100,249,250])for(const value of [1,37,250,999]){
  const m=cpu(),base=characters+actor*56;m.load(base-8,Buffer.alloc(72,0x55));
  m.w16(base+0x16,250);m.w16(base+0x18,hp);m.w8(base+0x33,0xff);
  const before=slice(m,base-8,72);assert.equal(run(m,7,actor,false).value,250);
  assert.deepEqual(slice(m,base-8,72),before);
  const expected=Buffer.from(before);expected.writeUInt16BE(Math.min(250,hp+value),0x18+8);expected[0x33+8]=0xfb;
  assert.equal(run(m,7,actor,true,value).value,250);assert.deepEqual(slice(m,base-8,72),expected);retained++;
}
// Level-up queries consume RNG, but application invokes the existing level-up
// service exactly once with this character. That service is trapped, not tested.
for(const actor of [0,43,99]){
  const m=cpu(),base=characters+actor*56;m.load(base-8,Buffer.alloc(72,0x55));
  const before=slice(m,base-8,72);
  assert.deepEqual(run(m,9,actor,false,0,54321),{value:1,randomCalls:1,levelCalls:0});
  assert.deepEqual(run(m,9,actor,true,1),{value:0,randomCalls:0,levelCalls:1});
  assert.deepEqual(slice(m,base-8,72),before);retained++;
}
// Ordinary healing/fatigue must apply the caller's amount, even if it differs
// from the configured query result (e.g. a caller scales an effect).
for(const id of [1,2,3,4,5]){
  const m=cpu(),base=characters;m.load(base-8,Buffer.alloc(72,0x55));
  m.w16(base+0x16,250);m.w16(base+0x18,100);m.w8(base+0x32,100);
  const expected=slice(m,base-8,72);
  if(id<=3)expected.writeUInt16BE(137,0x18+8);else expected[0x32+8]=63;
  run(m,id,0,true,37);assert.deepEqual(slice(m,base-8,72),expected);retained++;
}
for(const id of [0,8,20,65535])for(const apply of [false,true]){
  const m=cpu();m.load(characters,Buffer.alloc(56,0x55));
  assert.equal(run(m,id,0,apply,123).value,0);
  assert.deepEqual(slice(m,characters,56),Buffer.alloc(56,0x55));
}
console.log(`PASS consumable instructions: ${queries} queries, ${commits} ordinary commits, ${special} special-target commits, ${retained} cure/revival/level-up/caller-amount cases, 8 unsupported dispatch cases; guarded 4 MiB RAM`);
