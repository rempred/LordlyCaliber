'use strict';
// Executes export-compiled source. PI completion and resident cache helpers are
// modeled; this does not validate hardware caches, scheduling or full battles.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rows=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));
const bytes=name=>Buffer.from(rows.find(r=>r.name===name).dataHex,'hex');
const payload=bytes('module'),base=0x80440000,free=0x80450000;
const c={OB64:{}};c.window=c;vm.createContext(c);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../tools-data.js'),'utf8'),c);
const retained=c.OB64.TOOLS_FEATURES.find(f=>f.id==='high-attack-streamsplit');
const old=Buffer.from(retained.writes.find(w=>w.offset===0x27a0000).patched,'hex');
assert.equal(payload.readUInt32BE(4),23);
// All retained recovery helpers and the qualified guard inventory are unchanged.
assert.deepEqual(payload.subarray(0x400),old.subarray(0x400));
const refs=[];
for(let i=0;i<payload.readUInt32BE(0x10);++i){
 const pc=payload.readUInt32BE(0x940+i*8),word=payload.readUInt32BE(0x944+i*8);
 refs.push({pc,word,shifted:((word&0xffff0000)|(((word&65535)-0x7000)&65535))>>>0});
}
class Loader extends Mips {
 constructor(){super();this.dma=0;this.pi={};this.calls=[];}
 r32(a){return (a>>>0)===0xa4600010?0:super.r32(a);}
 w32(a,v){
  a>>>=0;
  if(a>=0xa4600000&&a<=0xa4600010){
   this.pi[a-0xa4600000]=v>>>0;
   if(a===0xa460000c){
    assert.equal(this.pi[0],0x440000);assert.equal(this.pi[4],0x127a0000);
    assert.equal(v+1,payload.length);this.load(base,payload);++this.dma;
   }
  }else super.w32(a,v);
 }
}
const loaders=[];
for(const version of [0,20,21,22,23])for(const markerAtWrongAddress of [false,true]){
 const m=new Loader();m.load(0x800a4b00,bytes('loader'));m.load(0x800a020c,bytes('continuation'));
 if(version){m.load(base,payload);m.w32(base+4,version);m.w32(base+32,1);m.w32(base+36,0x2222);}
 if(markerAtWrongAddress)m.w32(0xa0400000,0x4f424853);
 const keep={a0:0x11223344,a1:0x55667788,sp:0x803f0000,s0:1,s1:2,s3:3};
 for(const [reg,value] of Object.entries(keep))m.set(reg,value);
 for(const pc of [0x800900c0,0x80090010])m.trap(pc,t=>{
  assert.equal(t.get('a0'),base);assert.equal(t.get('a1'),payload.length);m.calls.push(pc);
  for(const reg of ['at','t0','t1','t2','t3'])t.set(reg,0xdeadbeef);
 });
 m.call(0x800a4b00,base+0x40,200000,0x801d9a80);
 for(const [reg,value] of Object.entries(keep))assert.equal(m.get(reg),value,reg);
 assert.equal(m.get('ra'),0x801d9a80);assert.equal(m.r32(base+4),23);
 assert.equal(m.dma,version===23?0:1);
 assert.deepEqual(m.calls,version===23?[]:[0x800900c0,0x80090010]);
 assert.equal(m.r32(base+32),version===23?1:0);
 assert.equal(m.r32(base+36),version===23?0x2222:0);
 loaders.push({version,markerAtWrongAddress,dma:m.dma});
}
let cases=0,maxSteps=0;
for(const alreadyFree of [false,true])for(const installed of [0,1])
for(const code of ['original','shifted','partial','helper-only','foreign'])
for(const length of [0x100,0x7e0,0x7e1,0x7f0]){
 const m=new Mips(),owner=alreadyFree?0x8044a820:0x80300000,current=alreadyFree?free:owner+0x57e0;
 m.load(base,payload);m.w32(base+32,installed);m.w32(base+36,0x3000);
 for(let i=0;i<0x6094;i+=4)m.w32(owner+i,(0x12340000+i)>>>0);
 m.w32(0x801ce8bc,owner);m.w32(0x801ce8c0,current);
 const trailer=current+(alreadyFree?-0x67f0:0x810);
 for(let i=0;i<0x100;i+=4)m.w32(trailer+i,0xa5000000+i);
 m.w32(trailer+4,length);
 const expectedTrailer=Buffer.from(m.ram.slice(m.phys(trailer),m.phys(trailer)+0x100));
 const head=Buffer.from(m.ram.slice(m.phys(current),m.phys(current)+0x810));
 const ownerHead=Buffer.from(m.ram.slice(m.phys(owner),m.phys(owner)+0x57e0));
 refs.forEach((r,i)=>m.w32(r.pc,code==='foreign'&&i%3===0?0xdeadc0de:
  code==='shifted'||code==='partial'&&i%2===0?r.shifted:r.word));
 const helper=code==='shifted'||code==='partial'||code==='helper-only';
 m.w32(0x801d96a0,helper?0x08110100:0x3c06801d);m.w32(0x801d96a4,helper?0:0x8cc6e8c0);
 m.set('s3',0x20);m.set('s0',0x100);m.set('s1',0x200);m.set('a0',0xabcdef01);m.set('a1',0x76543210);
 m.call(base+0x40,0x801d9a88);
 const active=alreadyFree||installed!==0||helper||length+0x20>0x800;
 assert.equal(m.r32(0x801ce8c0),active?free:current,JSON.stringify({alreadyFree,installed,code,length}));
 assert.equal(m.r32(0x801ce8bc),active?0x8044a820:owner);
 refs.forEach((r,i)=>assert.equal(m.r32(r.pc),code==='foreign'&&i%3===0?0xdeadc0de:active?r.shifted:r.word));
 assert.equal(m.r32(0x801d96a0),active?0x08110100:0x3c06801d);
 assert.equal(m.get('v0'),active?free:current);assert.equal(m.get('s2'),0x300);assert.equal(m.get('a2'),length);
 assert.equal(m.get('a0'),0xabcdef01);assert.equal(m.get('a1'),0x76543210);
 assert.equal(m.r32(base+32),active?1:0);
 const target=active?free-0x67f0:trailer;
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(target),m.phys(target)+0x100)),expectedTrailer);
 if(active){
  assert.deepEqual(Buffer.from(m.ram.slice(m.phys(free),m.phys(free)+0x810)),head);
  assert.deepEqual(Buffer.from(m.ram.slice(m.phys(0x8044a820),m.phys(0x8044a820)+0x57e0)),ownerHead);
 }
 assert.equal(m.r32(base+36),active&&!alreadyFree?length+0x20:0x3000);
 maxSteps=Math.max(maxSteps,m.steps);++cases;
}
console.log(JSON.stringify({loaderCases:loaders.length,layoutCases:cases,maxSteps,
 result:'pass',limit:'Modeled DMA/cache helpers; no full-game or hardware-cache validation'}));
