'use strict';
// Execute the exported menu instructions. Renderer/eligibility services are
// doubles; this verifies lists, counts, paging, clip coordinates and ABI, not RDP playback.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
global.window=global;global.OB64={};
vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../art.js'),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const ctx=JSON.parse(fs.readFileSync(process.argv[3]));
const key=((rom.readUInt32BE(0xaed4c)&65535)*65536)+(rom.readUInt32BE(0xaed54)&65535);
const pack=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded);
// The promotion container records one optional alignment NOP separately.
const menuRom=Buffer.from(rom);menuRom.writeUInt32BE(rom.readUInt32BE(0x27d0000+65536-64+8)+4,0x27d0000);
const promotion=Buffer.from(OB64.art.readCompressedResource(menuRom,0x27d0000-0x594280).decoded);
const owner=0x80300000,resource=0x80280000,texture=0x80260000,palette=0x80350000,gfx=0x80380000;
const delta=0x801135a0,list=0x801cf65c,counts=0x801cf34c;
function cpuForMenu() {
  const c=new Mips();c.ram=new Uint8Array(0x400000);c.set('sp',0x80060000);
  c.load(delta+0xbbdac,rom.subarray(0xbbdac,0xbc258));
  c.load(delta+0x9c02c,rom.subarray(0x9c02c,0x9c358));
  c.load(owner,Buffer.alloc(ctx.ownerBytes+16,0x6a));
  c.w32(0x80196af8,owner);c.w32(owner+0x80,resource);c.w32(owner+4,texture);c.w32(owner+8,palette);
  c.load(texture,pack);c.load(resource,promotion);
  c.load(ctx.aliases,Buffer.alloc(ctx.classCount));c.load(ctx.classes,Buffer.alloc(ctx.classCount*72));
  c.load(ctx.discovery,Buffer.alloc(32));c.load(0x801ef370,Buffer.from([1,2,4,8,16,32,64,128]));
  for(let id=1;id<=12;id++) c.w8(ctx.discovery+(id>>3),c.r8(ctx.discovery+(id>>3))|(1<<(id&7)));
  c.load(0x80193bf8,Buffer.alloc(99*56));
  for(let actor=0;actor<3;actor++) {c.w8(0x80193bf8+actor*56+17,actor<2?1:2);c.w8(0x80193bf8+actor*56+18,actor<2?1:2);}
  c.trap(0x80093380,x=>x.load(x.get('a0'),Buffer.alloc(x.get('a1'))));
  c.trap(0x801d2358,x=>x.set('v0',1));
  return c;
}
const c=cpuForMenu();c.call(list,0x80000100,3000000);
assert.equal(c.get('v0'),12);
assert.deepEqual(Array.from({length:12},(_,i)=>c.r8(owner+ctx.tabs+i)),Array.from({length:12},(_,i)=>i+9));
assert.equal(c.r8(owner+ctx.tabs+12),0);
assert.equal(c.r32(owner+ctx.ownerBytes),0x6a6a6a6a,'owner allocation canary');
for(const [category,id,now] of [[9,1,2],[10,2,1],[20,12,0]]) {
  c.set('a0',category);c.call(counts,0x80000100,3000000);
  assert.equal(c.get('v0'),1);assert.equal(c.r16(owner+ctx.entries),id);
  assert.equal(c.r16(owner+ctx.entries+2),0);
  assert.equal(c.r16(owner+ctx.current),now);assert.equal(c.r16(owner+ctx.eligible),3);
  assert.equal(c.r32(owner+ctx.ownerBytes),0x6a6a6a6a);
}
// None of the original eight categories includes a class moved to its own family tab.
for(let category=1;category<=8;category++) {c.set('a0',category);c.call(counts,0x80000100,3000000);assert.equal(c.get('v0'),0);}
for(const site of [0xcc924,0xccce4,0xcd2bc]) {
  c.load(delta+site,rom.subarray(site,site+4));c.set('v0',owner+11);
  c.call(delta+site,delta+site+4);assert.equal(c.get('a0'),20);
}
for(const [selected,first,n,x] of [[0,0,8,24],[8,8,4,24],[11,8,4,24],[0,0,8,-5]]) {
  c.w8(owner+0x1dff,12);c.w8(owner+0x9d,selected);c.w32(0x800e9ba0,gfx);
  c.load(gfx,Buffer.alloc(8192));c.set('s5',x);c.set('fp',40);c.set('s3',255);
  for(const reg of ['s0','s1','s2','s4','s6','s7']) c.set(reg,0x12345678);
  const calls=[];
  c.trap(0x8019b38c,r=>{
    calls.push([r.get('a0'),r.get('a1'),r.get('a2'),r.get('a3')]);
    for(const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9']) r.set(reg,0xcccccccc);
  });
  c.call(delta+0x9c02c,delta+0x9c358,100000);
  assert.equal(calls.length,n);
  calls.forEach((args,i)=>{
    assert.deepEqual(args,[palette+0x5e0,texture+23496+528+(8+first+i)*224,16,14]);
    const at=gfx+16+i*40;
    assert.equal(c.r32(at),0xfa000000);assert.equal(c.r32(at+4),selected===first+i?0xffffffff:0x606060ff);
  });
  if(x<0) {assert.equal(c.r32(gfx+16+12)>>>12,0);assert.equal(c.r32(gfx+16+20),32<<16);}
  for(const reg of ['s0','s1','s2','s4','s6','s7']) assert.equal(c.get(reg),0x12345678,'callee-saved '+reg);
}
c.load(ctx.discovery,Buffer.alloc(32));c.call(list,0x80000100,3000000);assert.equal(c.get('v0'),0);assert.equal(c.r8(owner+ctx.tabs),0);
console.log('Exported family tabs: discovery, separate linked-family lists, current/eligible counts, selection readers, four paged/clipped draws and bounds passed.');
