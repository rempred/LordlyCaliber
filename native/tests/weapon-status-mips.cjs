'use strict';
// Executes the original and exported routines in private RAM. External event,
// pose, RNG and attribute services are deterministic traps, not a full console.
const fs = require('fs'), assert = require('assert/strict');
const { Mips } = require('../../tests/lib/mips-interpreter.cjs');
const original = fs.readFileSync(process.argv[2]), patched = fs.readFileSync(process.argv[3]);
for (const image of [original, patched]) {
  if (image.readUInt32BE(0) === 0x37804012) image.swap16();
  assert.equal(image.readUInt32BE(0), 0x80371240);
}
const entry = 0x801e7694, record = 0x80300000, context = 0x80400000, stack = 0x807fe000;
function machine(image) {
  const m = new Mips();
  m.load(entry, image.subarray(0x22a964, 0x22b06c)); // hit routine + damage caller
  m.load(0x801f45c0, original.subarray(0x237890, 0x2379ac)); // actual immunity filter
  m.load(0x801c8e30, original.subarray(0x20c2c0, 0x20c364)); // actual status predicates
  m.trap(0x801c8fe8, c => c.set('v0', record + c.get('a0') * 0x100));
  for (const [address, count] of [[0x801d9f30,3], [0x801d9f60,4], [0x801d9f8c,5], [0x801d9fbc,6], [0x801d9ff0,7]])
    m.trap(address, c => {
      const args = ['a0','a1','a2','a3'].slice(0, Math.min(count,4)).map(r => c.get(r));
      for (let i=4; i<count; i++) args.push(c.r32(c.get('sp')+i*4));
      c.events.push(args);
    });
  m.trap(0x801df2ac, c => { c.poses.push(c.get('a1')); c.set('v0', 3 + c.get('a1') % 11); });
  m.trap(0x8009c7cc, c => { c.set('v0', [0x123, 0x456, 0x789][c.random++ % 3]); });
  m.trap(0x801df224, c => c.set('v0', c.scan));
  m.trap(0x801f67d4, c => c.set('v0', c.scan === 1 ? 1 : 0));
  m.trap(0x801da4fc, c => {
    assert.equal(c.r32(c.get('a0')+0x40) & 0x10, 0x10, 'Poison flag precedes attribute rebuild');
    c.rebuilt++;
  });
  m.trap(0x801c8c74, c => c.set('v0', c.soldiers)); // actual ADfc group predicate call
  m.trap(0x801c9fb4, c => c.set('v0', Math.ceil(c.get('a0') / c.get('a1'))));
  m.trap(0x801e6fb0, c => { c.killed++; c.w16(record+0x20,0); });
  m.trap(0x801e7144, () => { throw Error('Unexpected damage adjust path'); });
  m.trap(0x801c8fb8, c => c.set('v0', c.weapon));
  m.trap(0x8016f580, c => c.set('v0', c.weaponFlags));
  m.trap(0x8016f598, c => c.set('v0', c.equipmentImmunity[c.get('a0')] || 0));
  m.trap(0x801cd19c, c => c.set('v0', (c.classImmunity & c.get('a2')) ? 1 : 0));
  return m;
}
function setup(m, status=0, luck=0, scan=0, end=0) {
  m.r.fill(0); m.set('sp', stack);
  m.ram.fill(0, m.phys(record), m.phys(record)+0x2000);
  m.w32(0x801ce8bc, context); m.w16(context+0x606a, 0);
  for (let i=0; i<20; i++) {
    const p=record+i*0x100;
    m.w32(p,0x80500000); m.w32(p+0x40, i ? (i%3 ? 0:1) : status);
    m.w32(p+0x6c, 100+i); m.w32(p+0x70, 200+i);
  }
  m.w8(record+0x30, luck); m.w32(record+0x94,end); m.w16(record+0x20,30); m.w16(record+0x22,10);
  m.events=[]; m.poses=[]; m.random=0; m.rebuilt=0; m.killed=0; m.scan=scan; m.soldiers=0;
  m.weapon=1; m.weaponFlags=0; m.classImmunity=0; m.equipmentImmunity={};
}
function hit(m, flags, mode=1) {
  m.set('a0',record); m.set('a1',17); m.set('a2',mode); m.set('a3',0x17f);
  m.w32(stack+16,flags); m.call(entry);
  return {actors: Buffer.from(m.ram.subarray(m.phys(record),m.phys(record)+0x1400)),
    mask:m.r16(context+0x606a), events:m.events,poses:m.poses,random:m.random,rebuilt:m.rebuilt};
}
const retail=machine(original), built=machine(patched);
let cases=0;
// Include unknown/special weapon bits and status combinations; bit20 is tested below.
for(let flags=0;flags<256;flags++) {
  if(flags&0x20)continue;
  for(let status=0;status<32;status++) for(const mode of [0,1]) for(const luck of [0,100]) {
    const scan=cases%3, end=cases%2 ? 1000 : 0;
    setup(retail,status|0x60,luck,scan,end); setup(built,status|0x60,luck,scan,end);
    assert.deepEqual(hit(built,flags,mode),hit(retail,flags,mode),`retail behavior flags=${flags} status=${status} mode=${mode} luck=${luck}`);
    cases++;
  }
}
let poisonCases=0;
for(let status=0;status<32;status++) for(const mode of [0,1]) for(const luck of [0,100]) {
  setup(retail,status|0x60,luck); setup(built,status|0x60,luck);
  const before=hit(retail,0,mode); hit(built,0x20,mode);
  const expected=!(before.actors.readUInt32BE(0x40)&0x1f);
  assert.equal(built.rebuilt, expected?1:0);
  assert.equal(built.r32(record+0x40),before.actors.readUInt32BE(0x40)|(expected?0x10:0));
  assert.deepEqual(built.poses,before.poses);
  assert.deepEqual(built.events.slice(0,before.events.length),before.events);
  assert.equal(built.events.length,before.events.length+(expected?1:0));
  if(expected)assert.deepEqual(built.events.at(-1),[0x27,built.r32(record+0x94),record,0xff]);
  poisonCases++;
}
// Original immunity code, exercising every target equipment slot and class immunity.
for(const [flag,immune] of [[2,4],[4,8],[8,2],[0x20,0x10]]) {
  for(const slot of [-2,-1,0,1,2,3]) {
    setup(built);built.weaponFlags=flag;
    if(slot===-1)built.classImmunity=immune;
    if(slot>=0){built.w16(record+0x36+slot*2,7);built.equipmentImmunity[7]=flag;}
    built.set('a0',record+0x100);built.set('a1',record);built.call(0x801f45c0);
    assert.equal(built.get('v0'),slot===-2?flag:0);
    hit(built,built.get('v0'));
    assert.equal(built.r32(record+0x40)&0x1e,slot===-2?immune:0);
  }
}
// Actual retail damage wrapper: only a surviving target reaches the new routine.
for(const soldiers of [0,1])for(const damage of [0,5,15,30,40]) {
  setup(built);built.soldiers=soldiers;
  built.set('a0',record);built.set('a1',0);built.set('a2',10);built.set('a3',damage);
  for(const [index,value] of [[4,0],[5,0x20],[6,1],[7,0]])built.w32(stack+index*4,value);
  built.call(0x801e7b2c);
  assert.equal(built.rebuilt,damage<30?1:0,`damage=${damage} soldiers=${soldiers}`);
  assert.equal(built.killed,damage>=30?1:0);
}
console.log(`${cases} original-vs-compiled cases, ${poisonCases} Poison cases, 24 immunity cases and 10 nonlethal/lethal ordinary/Soldier cases passed.`);
