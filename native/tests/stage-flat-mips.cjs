'use strict';
// Both complete original background owners; image payloads absent and the prop
// draw callback controlled. Actual signed-depth/tie/cutoff selection executes.
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x801bb58c,rom.subarray(0x1fea1c,0x1ff108));
const records=0x80310000,dl=0x80320000,sp=0x803ffe00;
const z=[-32768,-2000,-906,-905,-904,-500,-456,-455,-454,-6,-5,-4,0,100,32767];
m.w32(0x801d07d0,records);m.w32(0x801d07d4,z.length);z.forEach((v,i)=>m.w16(records+i*100+4,v));
let passes=[],currentPass=0,transitions=[];
m.trap(0x801baa38,c=>{
 const phase=currentPass===0?0:c.get('ra')===0x801bb988?1:2;
 passes[phase].push((c.get('a0')-records)/100);
 transitions.push({phase,enabled:c.get('a1')!==0});
});
const cases=[];
for(const [base,foreground] of [[-5,-905],[0,0],[100,-100],[-100,100],[-4.5,-905.5]]){
 m.writeFloat(0x801d077c,base);m.writeFloat(0x801d07ac,foreground);
 passes=[[],[],[]];
 for(let pass=0;pass<2;pass++){
  m.w32(0x800e9ba0,dl);m.set('sp',sp);m.set('a0',pass?0x801d07a0:0x801d0770);currentPass=pass;
  m.call(pass?0x801bb87c:0x801bb58c);assert.equal(m.get('sp'),sp);
 }
 cases.push({base,foreground,z,passes});
}
const colors=[];
for(const address of [0x801b6db0,0x801b6970,0x801b619c,0x801b5d58,0x801b90a8])m.trap(address,()=>{});
for(let i=0;i<3;i++)m.w16(0x80220e70+i*2,[17,35,66][i]);
m.load(0x801d0828,Buffer.from([41,127,203,77]));
for(const mask of [0,1,2,4,8,16,32,64,127]){
 m.w8(0x801d082c,mask);transitions=[];passes=[[],[],[]];const primitives=[];
 for(let pass=0;pass<2;pass++){
  const bank=pass?0x801d07a0:0x801d0770;m.w32(bank,0x80330000);
  m.w32(0x800e9ba0,dl);m.set('sp',sp);m.set('a0',bank);currentPass=pass;
  m.call(pass?0x801bb87c:0x801bb58c);assert.equal(m.get('sp'),sp);
  const rgba=[];for(let p=dl;p<m.r32(0x800e9ba0);p+=8)if(m.r32(p)===0xfa000000)rgba.push(m.r32(p+4));
  assert.equal(rgba.length,1);primitives.push(rgba[0]);
 }
 colors.push({mask,transitions,primitives});
}
fs.writeFileSync(process.argv[3],JSON.stringify({cases,colors}));
