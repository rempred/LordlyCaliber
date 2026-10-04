'use strict';
// Execute each compiled gate with controlled state-flag/RNG services.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
const rom=fs.readFileSync(fixture.rom);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const ram=offset=>(offset+0x800ab8c0)>>>0;
let count=0;
let rngChecks=0;
for(const seed of [0,1,2,3,42,65535,123456789,0x3fffffff,0xffffffff]){
  const m=new Mips();m.ram=new Uint8Array(0x400000);m.load(0x80092a60,rom.subarray(0x22e60,0x22e8c));
  let expected=BigInt(seed);m.w32(0x800aa740,seed);
  for(let step=0;step<64;step++){
    expected=(4n*expected*expected+5n*expected+1n)%(1n<<30n);
    m.call(0x80092a60);assert.equal(m.get('v0'),Number(expected));
    assert.equal(m.r32(0x800aa740),Number(expected));assert(m.get('v0')<0x40000000);rngChecks++;
  }
}
for(const variant of fixture.variants)for(const flag of [0,1]){
  const d=variant.rate.divisor,pass=variant.rate[flag?'normalPassCount':'alternatePassCount'];
  const inputs=[0,1,Math.max(0,pass-1),pass,d-1,d,(d+1)>>>0,0x7fffffff,0xffffffff];
  for(const value of inputs){
    const random=value>>>0,m=new Mips();m.ram=new Uint8Array(0x400000);
    m.load(ram(0x13c060),Buffer.from(variant.code,'hex'));
    let outcome=null,flags=0,randoms=0;
    m.trap(0x80186210,c=>{assert.equal(c.get('a0'),17);c.set('v0',flag);flags++;});
    m.trap(0x80092a60,c=>{
      for(const r of ['v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(r,0xcccccccc);
      c.set('v0',random);randoms++;
    });
    for(const [site,passed] of [[0x13c230,true],[0x13c46c,false]])m.trap(ram(site),c=>{outcome=passed;c.set('ra',0x80000100);});
    m.call(ram(0x13c1e8),0x80000100,200);
    assert.equal(outcome,random%d<pass,`${variant.name}, flag${flag}, RNG${random}`);
    assert.equal(flags,1);assert.equal(randoms,1);count++;
  }
}
console.log(`PASS global encounter gate: ${fixture.variants.length} compiled variants, ${count} flag/RNG boundary cases, ${rngChecks} original RNG steps in 4 MiB RAM. RNG distribution over a full period and full dispatch remain outside this check.`);
