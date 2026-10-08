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
const version=rom.readUInt32BE(0x27c4008);
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
let producers=0,splitters=0,readers=0,categories=0,guards=0;
const shopCount = fixture.shops.records.length;
for(const id of [...Array(shopCount).keys(),...([shopCount,255,0xffffffff].filter(x=>version<4||x<256))]){
  const m=cpu(), expected=fixture.shops.records[id<shopCount?id:0];
  const saved=['s0','s1','s2','s3','s4','s5','s6','s7'];
  saved.forEach((r,i)=>m.set(r,0x12340000+i));
  let transfers=0;
  m.trap(0x8009da50,c=>{
    assert.equal(c.get('a0'),data+(id<shopCount?id:0)*176);
    assert.equal(c.get('a1'),temp);assert.equal(c.get('a2'),176);
    c.load(temp,rom.subarray(c.get('a0'),c.get('a0')+176));
    transfers++;clobber(c);
  });
  if(version>=4){
    const sites=0x80311000,strongholds=0x80312000,events=[];
    m.w8(0x8018f481,3);m.w8(0x801e7e15+12*3,17);
    m.w16(sites+1+4*2,5);m.w8(strongholds+28*4+27,id);
    m.trap(0x80070f30,c=>{assert.equal(c.get('a0'),512);events.push('allocate');clobber(c);c.set('v0',temp);});
    m.trap(0x80093380,c=>{
      assert.equal(c.get('a0'),temp);assert.equal(c.get('a1'),512);
      assert.equal(c.r32(0x80219f20),temp);c.load(temp,Buffer.alloc(512));events.push('clear');clobber(c);
    });
    m.trap(0x8007938c,c=>{
      assert.equal(c.get('a0'),0x021af9f8);assert.equal(c.get('a1'),2);assert.equal(c.get('a2'),0xfffffffd);
      const index=c.get('a3');assert([17,0].includes(index));events.push(index);
      clobber(c);c.set('v0',index===17?sites:strongholds);
    });
    m.trap(0x80079618,c=>{
      assert.equal(c.get('a0'),0x021af9f8);assert.equal(c.get('a1'),2);assert.equal(c.get('a2'),0xffffffef);
      assert.equal(transfers,1);events.push('release');clobber(c);
    });
    m.set('a0',2);m.call(ram(0x19be40));
    assert.deepEqual(events,['allocate','clear',17,0,'release']);
    assert.equal(m.get('sp'),0x80060000);
  }else{
    m.set('v1',id);m.call(ram(0x19bf18),ram(0x19bfc4));
  }
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
  if(rom.readUInt32BE(0x27c4008)>=2){
    const menus=0x80310000;
    const expectedCategories=counts.map((n,i)=>n?i:-1).filter(i=>i>=0).concat(6);
    m.call(ram(0x197914),ram(0x197984));
    assert.equal(m.r8(0x8021a122)+1,expectedCategories.length);
    expectedCategories.forEach((v,i)=>assert.equal(m.r8(0x8021a11a+i),v));
    m.load(menus-16,Buffer.alloc(102,0xa6));
    m.trap(0x80070f30,c=>{
      assert.equal(c.get('a0'),expectedCategories.length*10);
      clobber(c);c.set('v0',menus);
    });
    m.call(ram(0x19798c),ram(0x197a2c));
    assert.equal(m.r32(0x80219f40),menus);
    assert.equal(m.r8(menus-1),0xa6);assert.equal(m.r8(menus+expectedCategories.length*10),0xa6);
    expectedCategories.forEach((kind,category)=>{
      const count=kind===6?expected.consumables.length:counts[kind];
      assert.equal(m.r16(menus+category*10+6),(count-1)&65535);
      for(const selection of [-1,0,count-1,count])for(const buttons of [0,0x8000]){
        m.w16(menus+category*10,selection);m.set('s0',category);
        m.set('v0',0xffff);m.set('a0',buttons);
        const allowed=!!buttons&&selection>=0&&selection<count;
        m.call(ram(0x197d6c),ram(allowed?0x197d7c:0x197f18));
        guards++;
      }
    });
    categories++;
  }
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
console.log(JSON.stringify({producers,splitters,readers,categories,guards,ramBytes:0x400000,result:'PASS'}));
