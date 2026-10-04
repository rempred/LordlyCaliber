'use strict';
// Original selector/rate instructions, exported tables, explicit authored expectations.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2])),rom=fs.readFileSync(fixture.rom);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const ram=offset=>(offset+0x800ab8c0)>>>0,expected=fixture.encounters;
function cpu(){const m=new Mips();m.ram=new Uint8Array(0x400000);
  m.load(ram(0x13c060),rom.subarray(0x13c060,0x13c49c));
  m.load(ram(0x141e80),rom.subarray(0x141e80,0x142258));return m;}
let pairs=0,rates=0;
for(let pool=1;pool<=45;pool++)for(let terrain=0;terrain<32;terrain++){
  const lookup=expected.terrainSlots[terrain];if(!lookup){assert.equal(expected.terrainRates[terrain],0);continue;}
  for(const random of [0,1,32766,32767]){
    const m=cpu(),id=expected.pools[pool-1][(lookup-1)*2+random%2];
    m.set('s0',pool);m.set('v1',terrain);m.set('v0',random);
    m.call(ram(0x13c3cc),ram(0x13c414));assert.equal(m.get('v0'),id);
    m.call(ram(0x13c414),ram(id?0x13c14c:0x13c41c));
    assert.equal(m.get('v0'),id+0x100); // branch-delay encoding, including unsigned high class IDs
    pairs++;
  }
}
for(let terrain=0;terrain<32;terrain++)for(const doubled of [0,1]){
  const threshold=expected.terrainRates[terrain]*(doubled?2:1);
  for(const random of [0,Math.max(0,threshold-1),threshold,99,100,32767]){
    const m=cpu();m.set('v0',terrain);
    m.trap(0x801dcd90,c=>{assert.equal(c.get('a1'),242);c.set('v0',doubled);});
    m.trap(0x80092a60,c=>c.set('v0',random));
    m.call(ram(0x13c334),ram(0x13c390));
    assert.equal(m.get('v0'),Number(random%100<threshold));rates++;
  }
}
console.log(`PASS encounter instructions: ${pairs} pool selections/marker branches and ${rates} terrain rolls, including rate doubling; 4 MiB RAM. Full encounter materialization is outside this check.`);
