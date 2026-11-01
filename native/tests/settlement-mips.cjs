'use strict';
// Full settlement entry setup with deterministic mission/service inputs.
// Drawing/overlay transitions are services; reward and reserve code execute.
const assert=require('node:assert/strict');
const catalog=require('../game/tools/goth-owner.json');
const callerSaved=['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'];
function setup(m,rom,{key=1,cf=50,pop=0,base=500,money=100,soldiers=0}={}){
 const header=catalog.headerOffset, modern=rom.toString('ascii',header,header+8)==='LCGOTH01';
 const owner=catalog[modern?(rom.readUInt32BE(header+12)?'reserve':'plain'):'retail'];
 m.load(owner.address,rom.subarray(owner.start,owner.start+catalog.allocationSize));
 for(const t of owner.tables)m.load(t.address,rom.subarray(t.start,t.start+t.size));
 const events={items:[],cache:[],loads:0},symbols=owner.symbols;
 const finish=(c,value=0)=>{for(const r of callerSaved)c.set(r,0xcccccccc);c.set('v0',value);};
 for(const [name,at]of Object.entries(symbols))if(name.startsWith('func_'))m.trap(at,c=>finish(c));
 m.trap(symbols.func_0005c110,c=>finish(c,0));
 m.trap(symbols.func_00130C2C,c=>finish(c,0xffffffff));
 m.trap(symbols.func_00127BFC,c=>finish(c,pop));
 m.trap(symbols.func_00127C50,c=>finish(c,soldiers));
 m.trap(0x801dc720,c=>{events.items.push([c.get('a0'),c.get('a1')]);finish(c,77);});
 for(const at of [0x800900c0,0x80090010])m.trap(at,c=>{
  if(c.get('a0')===0x804c0000){assert.equal(c.get('a1'),4096);events.cache.push(at);}
  finish(c);
 });
 m.trap(0x8009da50,c=>{
  if(c.get('a0')===0x27c6000){assert.equal(c.get('a1'),0x804c0000);assert.equal(c.get('a2'),4096);m.load(0x804c0000,rom.subarray(0x27c6000,0x27c7000));events.loads++;}
  finish(c);
 });
 m.trap(0x80093380,c=>{m.load(c.get('a0'),Buffer.alloc(c.get('a1')));finish(c);});
 m.w32(0x801f3610,1);m.w32(0x801f0de0,1);m.w8(0x801f1037,0);m.w8(0x801f1036,2);m.w8(0x801f0fde,0);
 m.w8(0x8018f481,key);m.w8(0x801936a9,cf);m.w16(0x801e7e1a+key*12,base);
 m.w16(0x801936b8,48);m.w32(0x80196a6c,money);m.w32(0x800eb0dc,11);
 m.w8(0x801f365d,0);m.set('sp',0x803ffe00);
 return {events,run:()=>m.call(owner.address)};
}
module.exports={setup};
