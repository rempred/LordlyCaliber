'use strict';
// Execute an exported shop ROM, with service traps and guarded 4 MiB RAM.
// Inventory expectations come from the authored project, not the ROM decoder.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
const rom=fs.readFileSync(fixture.rom);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const catalog=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/shops/owners.json')));
const ram=offset=>(offset+0x8007d7a0)>>>0;
const temp=0x80300000, equipment=0x8021a010, consumables=0x8021a0d8;
assert.equal(rom.toString('ascii',0x27c4000,0x27c4008),'LC05SHOP');
const data=rom.readUInt32BE(0x27c400c);
const clobber=m=>{
  for(const r of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])m.set(r,0xcccccccc);
};
function cpu(){
  const m=new Mips(); m.ram=new Uint8Array(0x400000);
  for(const owner of catalog.owners)m.load(ram(owner.start),rom.subarray(owner.start,owner.start+owner.size));
  m.load(temp-16,Buffer.alloc(544,0xa6));m.load(temp,Buffer.alloc(512));
  m.load(equipment-16,Buffer.alloc(298,0xa6));
  m.w32(0x80219f20,temp);m.set('sp',0x80060000);
  return m;
}
let producers=0,splitters=0,readers=0;
for(const id of [...Array(36).keys(),36,255,0xffffffff]){
  const m=cpu(), expected=fixture.shops.records[id<36?id:0];
  const saved=['s0','s1','s2','s3','s4','s5','s6','s7'];
  saved.forEach((r,i)=>m.set(r,0x12340000+i));
  let transfers=0;
  m.trap(0x8009da50,c=>{
    assert.equal(c.get('a0'),data+(id<36?id:0)*176);
    assert.equal(c.get('a1'),temp);assert.equal(c.get('a2'),176);
    c.load(temp,rom.subarray(c.get('a0'),c.get('a0')+176));
    transfers++;clobber(c);
  });
  m.set('v1',id);m.call(ram(0x19bf18),ram(0x19bfc4));
  assert.equal(transfers,1);saved.forEach((r,i)=>assert.equal(m.get(r),0x12340000+i));
  const combined=[...expected.consumables,...expected.equipment.map(x=>x|0x8000),0];
  combined.forEach((v,i)=>assert.equal(m.r16(temp+i*2),v));
  assert.equal(m.r8(temp-1),0xa6);assert.equal(m.r8(temp+512),0xa6);
  assert(m.ram.slice(m.phys(temp+combined.length*2),m.phys(temp+512)).every(x=>x===0));
  producers++;
  m.trap(0x80093380,c=>{
    const address=c.get('a0'),size=c.get('a1');
    assert([[equipment,200],[consumables,60],[0x8021a114,6]].some(x=>x[0]===address&&x[1]===size));
    c.load(address,Buffer.alloc(size));clobber(c);
  });
  m.trap(0x80218950,c=>{const category=c.get('a0')%6;clobber(c);c.set('v0',category);});
  m.call(ram(0x19782c),ram(0x197904));
  expected.equipment.forEach((v,i)=>assert.equal(m.r32(equipment+i*4),v));
  expected.consumables.forEach((v,i)=>assert.equal(m.r16(consumables+i*2),v));
  assert.equal(m.get('s0'),expected.equipment.length);assert.equal(m.get('s3'),expected.consumables.length);
  assert.equal(m.get('s5'),temp); // the original cleanup frees the original allocation base
  assert.equal(m.r8(equipment-1),0xa6);assert.equal(m.r8(0x8021a11a),0xa6);
  const counts=Array(6).fill(0);expected.equipment.forEach(v=>counts[v%6]++);
  counts.forEach((v,i)=>assert.equal(m.r8(0x8021a114+i),v));
  m.call(ram(0x197954),ram(0x19795c));
  assert.equal(m.get('v0'),expected.consumables[0]||0);
  splitters++;
  if(id!==1&&id!==35)continue;
  for(let index=0;index<expected.consumables.length;index++){
    for(const [begin,end,input,output] of [
      [0x197da4,0x197db4,'v0','v1'],[0x198020,0x198030,'v0','a1'],
      [0x198cdc,0x198cec,'s1','s0'],[0x198f58,0x198f64,'s1','v1'],
      [0x199550,0x199560,'v0','s0'],[0x19a330,0x19a340,'v0','s1']
    ]){
      m.set(input,index);m.set('s6',consumables);
      m.call(ram(begin),ram(end));
      assert.equal(m.get(output),expected.consumables[index],`reader ${begin.toString(16)} index ${index}`);
      readers++;
    }
  }
}
console.log(JSON.stringify({producers,splitters,readers,ramBytes:0x400000,result:'PASS'}));
