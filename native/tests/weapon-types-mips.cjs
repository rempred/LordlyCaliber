'use strict';
// Actual exported equipment, shop and combat instructions in isolated RAM.
// Combat context modifiers are held at zero; rendering and console boot are not modeled.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');
const fixture = JSON.parse(fs.readFileSync(process.argv[2]));
function readRom(path) {
  const bytes = fs.readFileSync(path);
  if (bytes.readUInt32BE(0) === 0x37804012) bytes.swap16();
  assert.equal(bytes.readUInt32BE(0), 0x80371240);
  return bytes;
}
const original = readRom(fixture.original), built = readRom(fixture.rom);
// Independently recompute CIC-6102 header checksums from the saved ROM file.
let t1=0xf8ca4ddc,t2=t1,t3=t1,t4=t1,t5=t1,t6=t1;
for(let at=0x1000;at<0x101000;at+=4) {
  const d=built.readUInt32BE(at),sum=t6+d,n=d&31,r=((d<<n)|(d>>>(32-n)))>>>0;
  if(sum>0xffffffff)t4=(t4+1)>>>0;
  t6=sum>>>0;t3=(t3^d)>>>0;t5=(t5+r)>>>0;
  t2=(t2^(t2>d?r:t6^d))>>>0;t1=(t1+(t5^d>>>0))>>>0;
}
assert.equal(built.readUInt32BE(0x10),(t6^t4^t3)>>>0);
assert.equal(built.readUInt32BE(0x14),(t5^t2^t1)>>>0);
const header = built.subarray(0x5dc80, 0x5dd00), base = header.readUInt32BE(24),
      dataRom = header.readUInt32BE(12), end = header.readUInt32BE(48),
      itemsBase = header.readUInt32BE(116), stack = 0x803ff000, actor = 0x803d0000;
function machine(image, shop = false) {
  const m = new FloatMips(); const step=m.exec.bind(m); m.exec=(pc,next)=>{try{return step(pc,next);}catch(e){throw Error(`PC ${pc.toString(16)}: ${e.message}`);}}; m.ram = new Uint8Array(0x400000); m.set('sp', stack);
  m.load(0x8016af80, image.subarray(0x40e80, 0x66e10));
  if (image === built) {
    m.load(base, image.subarray(dataRom, dataRom + end - base));
    m.load(0x80070c00, image.subarray(0x1000, 0x2f000)); // class adapters if present
  }
  if (shop) {
    m.load(0x80212bb0, image.subarray(0x195410, 0x19cbc0));
    // This preceding overlay has a different placement: both internal jumps
    // (195474->195488 and 1954ac->195530) establish delta 8007fb70.
    m.load(0x80214f80, image.subarray(0x195410, 0x19554c));
    m.trap(0x802194b4, c => c.set('v0', 1234)); // retail dynamic-price service
  } else {
    m.load(0x801cfcb8, image.subarray(0xbc718, 0xbc7e4));
    m.load(0x801c8fb8, image.subarray(0x20c448, 0x20c478));
    m.load(0x801f2484, image.subarray(0x235754, 0x2359a4));
    m.load(0x801f4294, image.subarray(0x237564, 0x237590));
    m.trap(0x801f1884, c => c.setSingle(0, 0));
  }
  return m;
}
function call(m, address, ...args) {
  args.forEach((v, i) => m.set(['a0', 'a1', 'a2', 'a3'][i], v));
  m.call(address); assert.equal(m.get('sp'), stack); return m.get('v0');
}
const m = machine(built), shop = machine(built, true), retail = machine(original, true);
const type = id => Buffer.from(fixture.records[id], 'hex')[4];
const weapon = t => t >= 1 && t <= 13 || t === 24 || t >= 26 && t <= 254;
const group = t => weapon(t) ? 1 : t === 23 ? 2 : [14,15].includes(t) ? 3 : t >= 16 && t <= 20 ? 4 : t === 25 ? 5 : 0;
for (let id = 0; id < fixture.records.length; ++id) {
  assert.equal(call(m, 0x8016f9f0, id), weapon(type(id)) ? 1 : 0);
  assert.equal(call(shop, 0x80218950, id), group(type(id)));
  if (id < 278) assert.equal(call(shop, 0x80218950, id), call(retail, 0x80218950, id));
}
// Every possible category byte: no signed-byte wrap or group-table overrun.
for (let t = 0; t < 256; ++t) {
  m.w8(itemsBase + 16 + fixture.first * 32, t);
  shop.w8(itemsBase + 16 + fixture.first * 32, t);
  assert.equal(call(m, 0x8016f9f0, fixture.first), +weapon(t), `weapon category ${t}`);
  assert.equal(call(shop, 0x80218950, fixture.first), group(t), `shop category ${t}`);
}
m.w8(itemsBase + 16 + fixture.first * 32, 6);
shop.w8(itemsBase + 16 + fixture.first * 32, 6);
// Army inventory stores IDs, not an entry for every possible item ID.
const inventory = [1, fixture.first, fixture.second, fixture.other, 100, 0];
m.w32(0x80196af8, 0x803c0000);
inventory.forEach((id, i) => { m.w16(0x80196b00 + 4*i, id); m.w8(0x80196b03 + 4*i, 1); });
for (const defaultItem of [1, fixture.first, fixture.other]) {
  const expected = inventory.flatMap((id, i) => id && type(id) === type(defaultItem) ? [i] : []);
  const count = call(m, 0x801cfcb8, defaultItem);
  assert.equal(count, expected.length);
  assert.deepEqual(Array.from({length:count}, (_,i) => m.r16(0x803c1bee + 2*i)), expected);
}
// The game's equipment assignment uses the class defaults. Golem accepts its
// two weapons but not swords/another custom group; Knight accepts its sword.
const golems = fixture.golems || [fixture.golem];
for (const classId of [...golems, 5]) for (const id of [1, fixture.first, fixture.second, fixture.other]) {
  shop.ram.fill(0, shop.phys(actor), shop.phys(actor)+0x100);
  shop.w8(actor + 0x11, classId); shop.w8(actor + 0x12, classId);
  call(shop, 0x80214f80, id, actor);
  const allowed = golems.includes(classId) ? id === fixture.first || id === fixture.second : id === 1;
  assert.equal(shop.r16(actor + 0x2a), allowed ? id : 0, `class ${classId} equip ${id}`);
}
// Retail group ordering and tie behavior stay the same, including special item250.
function sorted(c, ids) {
  for(let g=0;g<6;++g)c.w8(0x8021a114+g,0);
  ids.forEach((id,i)=>{
    c.w32(0x8021a010+i*4,id);
    const g=call(c,0x80218950,id);c.w8(0x8021a114+g,c.r8(0x8021a114+g)+1);
  });
  call(c,0x80218fa0,ids.length);
  return ids.map((_,i)=>c.r32(0x8021a010+i*4));
}
for(let pass=0;pass<20;++pass) {
  const ids=Array.from({length:45},(_,i)=>(pass*19+i*73)%277+1);
  assert.deepEqual(sorted(shop,ids),sorted(retail,ids));
}
const stocked = sorted(shop,[200,fixture.second,100,fixture.other,fixture.first,1]);
for(let i=1;i<stocked.length;++i)assert(group(type(stocked[i-1]))<=group(type(stocked[i])));
// Physical attack, using actual class coefficient and equipment accessors.
// Only the terrain/formation/affinity composite service is fixed at zero.
const scores=[];
for(const id of [0,fixture.first,fixture.second,fixture.other]) {
  m.ram.fill(0,m.phys(actor),m.phys(actor)+0x100);
  m.w8(actor+0x4b,fixture.golem);m.w8(actor+0x4f,fixture.golem);
  m.w16(actor+0x24,100);m.w16(actor+0x2e,100);m.w16(actor+0x36,id);
  assert.equal(call(m,0x801c8fb8,actor),id);
  const coefficient=call(m,0x8016d2fc,fixture.golem,fixture.golem)&255;
  const wstr=id?call(m,0x8016f65c,id)|0:0, edex=id?call(m,0x8016f7ec,id)|0:0;
  const f=Math.fround;
  const expected=Math.trunc(f(f(f((coefficient+wstr)/6)*3)*f((200+edex)/100)));
  const score=call(m,0x801f2484,actor,actor+0x100);
  assert.equal(score,expected,`golem attack with item ${id}`);
  // Actual final damage kernel: zero resistance, defense10, no random increase.
  m.setSingle(24,score);m.setSingle(26,10);m.setSingle(22,0);m.setSingle(20,0);m.setSingle(6,1);
  m.call(0x801f4294,0x801f42c0);
  assert.equal(m.get('s0'),Math.trunc(f(f(score-10)*100)/100));
  scores.push(score);
}
assert(scores[2]>scores[1] && scores[1]>scores[0]);
// Every category name resolves to a terminated string in persistent loaded RAM.
const metaAddress=base+fixture.metadataRom-dataRom;
for(let t=0;t<256;++t) {
  const ptr=m.r32(metaAddress+32+t*4);assert(ptr>=metaAddress && ptr<end);
  let s='';for(let at=ptr;m.r8(at);++at){assert(at<end && s.length<128);s+=String.fromCharCode(m.r8(at));}
  if(fixture.categories[t])assert.equal(s,fixture.categories[t]);
}
console.log(JSON.stringify({categories:256,equipmentRestrictions:'PASS',shopSortCases:20,
  combatAttackScores:scores,damage:'PASS',categoryNames:'PASS',limit:'Headless routines; no rendered battle or emulator playthrough.'}));
if(fixture.help) {
  m.load(0x803b0000,Buffer.from(fixture.help,'base64'));
  const helpKey=fixture.metadataRom+built.readUInt32BE(fixture.metadataRom+20)-0x594280;
  m.trap(0x8009dd38,c=>{assert.equal(c.get('a0'),helpKey);c.set('v0',0x803b0000);});
  m.trap(0x800712c4,()=>{});
  for(const t of [6,26,254].filter(t=>fixture.categories[t])) {
    m.w8(itemsBase+16+fixture.first*32,t);m.w16(0x801939de,fixture.first);m.set('s1',actor);
    m.call(0x80180490,0x80180544);
    assert.equal(m.get('a1'),helpKey);assert.equal(m.get('a2'),76+t);
    m.set('a0',actor);m.set('a1',helpKey);m.set('a2',76+t);
    m.call(0x8017faf8,0x80000100,1000000);
    const text=Buffer.from(m.ram.subarray(m.phys(actor),m.phys(m.get('v0'))));
    assert(text.includes(Buffer.from(fixture.categories[t])));
  }
  console.log('PASS actual category-help routing and text-row selection, including highest category');
}
