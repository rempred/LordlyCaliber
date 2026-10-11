'use strict';
// Diagnostic for the retained v21 loader. The continuation trap models its DMA
// copy; cache timing, active overlay replacement and full battles are not modeled.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const context={OB64:{}};context.window=context;vm.createContext(context);
vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../tools-data.js'),'utf8'),context);
const feature=context.OB64.TOOLS_FEATURES.find(f=>f.id==='high-attack-streamsplit');
const data=offset=>Buffer.from(feature.writes.find(w=>w.offset===offset).patched,'hex');
const bootstrap=data(0x34f00),payload=data(0x27a0000);
assert.equal(payload.readUInt32BE(4),21);
assert.equal(bootstrap.readUInt32BE(0),0x3c08a040);
const results=[];
for(const correctAddressOnly of [false,true])for(const initialVersion of [0,20,21]){
 const m=new Mips(),base=0x80440000,entry=base+0x40,boot=0x800a4b00;
 let dmaRequests=0;
 m.load(boot,bootstrap);
 if(correctAddressOnly)m.w32(boot,0x3c08a044);
 if(initialVersion){
  m.load(base,payload);m.w32(base+4,initialVersion);
  m.w32(base+0x20,1);m.w32(base+0x24,0x2000);
 }
 m.trap(0x800a020c,c=>{++dmaRequests;c.load(base,payload);});
 m.call(boot,entry);
 const row={correctAddressOnly,initialVersion,dmaRequests,
  finalVersion:m.r32(base+4),installed:m.r32(base+0x20),highWater:m.r32(base+0x24)};
 assert.equal(dmaRequests,correctAddressOnly&&initialVersion?0:1);
 if(dmaRequests){assert.equal(row.installed,0);assert.equal(row.highWater,0);}
 results.push(row);
}
const references=[];
for(let i=0;i<payload.readUInt32BE(0x10);++i){
 const at=payload.readUInt32BE(0x14)+i*8,pc=payload.readUInt32BE(at),word=payload.readUInt32BE(at+4);
 const immediate=word&65535;
 assert((immediate>=0x810&&immediate<=0x8ff)||(immediate>=0x5ff0&&immediate<=0x60df));
 references.push({pc,word,shifted:((word&0xffff0000)|((immediate-0x7000)&65535))>>>0});
}
assert.equal(references.length,546);
const contexts=[];
for(const initial of [
 {name:'first overflow',alreadyFree:false,installed:0,shiftedCode:false,length:0x7f0},
 {name:'fresh context after reload',alreadyFree:false,installed:1,shiftedCode:false,length:0x100},
 {name:'fresh context with cleared flag',alreadyFree:false,installed:0,shiftedCode:true,length:0x100},
 {name:'same context after reload',alreadyFree:true,installed:1,shiftedCode:false,length:0x100},
 {name:'same context with cleared flag',alreadyFree:true,installed:0,shiftedCode:true,length:0x100},
]){
 const m=new Mips(),base=0x80440000,free=0x80450000,owner=initial.alreadyFree?0x8044a820:0x80300000;
 const current=initial.alreadyFree?free:owner+0x57e0;
 m.load(base,payload);m.w32(base+0x20,initial.installed);
 m.w32(0x801ce8bc,owner);m.w32(0x801ce8c0,current);
 m.w32(current+(initial.alreadyFree?-0x67ec:0x814),initial.length);
 for(const row of references)m.w32(row.pc,initial.shiftedCode?row.shifted:row.word);
 m.w32(0x801d96a0,initial.shiftedCode?0x08110100:0x3c06801d);
 m.w32(0x801d96a4,initial.shiftedCode?0:0x8cc6e8c0);
 m.set('s3',0x20);m.set('s0',0x100);m.set('s1',0x200);
 m.call(base+0x40,0x801d9a88);
 const relocated=m.r32(0x801ce8c0)===free;
 const shifted=references.filter(row=>m.r32(row.pc)===row.shifted).length;
 const coherent=shifted===(relocated?546:0);
 contexts.push({...initial,relocated,shiftedReferences:shifted,installedAfter:m.r32(base+0x20),coherent});
}
assert.equal(contexts[0].coherent,true);
assert.equal(contexts[1].coherent,false);
assert.equal(contexts[2].coherent,false);
assert.equal(contexts[3].coherent,false);
assert.equal(contexts[4].coherent,true);
if(process.argv[2])fs.writeFileSync(process.argv[2],JSON.stringify({results,contexts},null,2)+'\n');
console.log(JSON.stringify({results,contexts},null,2));
