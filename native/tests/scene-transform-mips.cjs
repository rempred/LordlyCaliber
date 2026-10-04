'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),vm=require('node:vm'),path=require('node:path'),{FloatMips}=require('./float-mips.cjs');
const rootDir=path.resolve(__dirname,'../..');global.window=global;global.OB64={};
for(const f of ['parsers.js','repack.js','art.js'])vm.runInThisContext(fs.readFileSync(path.join(rootDir,f),'utf8'),{filename:f});
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8023a98c,rom.subarray(0x2af78c,0x2af854));
m.load(0x8023c214,rom.subarray(0x2b1014,0x2b15e4));
m.load(0x800987e0,rom.subarray(0x28be0,0x28c40));
const root=0x80300000,sp=0x803ffe00;
const table=Buffer.from(OB64.art.readResource(rom,0x19a63ec).stored),assets=[];
for(let selector=0;selector<table.length/4;selector++){
 const key=table.readUInt32BE(selector*4),bytes=key?Buffer.from(OB64.art.readCompressedResource(rom,key).decoded):null;
 assets.push({selector,key,bytes});
}
let cursor,owners,override;
function alloc(bytes){const p=cursor+16;cursor+=(bytes.length+63)&~15;assert(cursor<0x803f0000);m.load(p-16,Buffer.alloc(bytes.length+32,0xa7));m.load(p,bytes);owners.set(p,bytes.length);return p;}
m.trap(0x80070f30,c=>{const n=c.get('a0');assert([1760,656].includes(n));c.set('v0',alloc(Buffer.alloc(n)));});
m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x800712c4,c=>{assert(owners.has(c.get('a0')));});
m.trap(0x8009dbb8,c=>{assert.equal(c.get('a0'),0);assert.equal(c.get('a1'),0x19a63ec);c.set('v0',alloc(table));});
m.trap(0x8009daf4,c=>{assert(assets.some(a=>a.key===c.get('a0')));c.set('v0',1);});
m.trap(0x8009dd38,c=>{const found=assets.find(a=>a.key===c.get('a0'));assert(found);c.set('v0',alloc(override||found.bytes));});
const memory=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
function channels(){return Buffer.concat(Array.from({length:20},(_,i)=>memory(m.r32(root+0x1c54+i*4),88))).toString('hex');}
function state(){const job=m.r32(root+0x1bb0);m.call(0x8023c214);const delta=Buffer.alloc(job?480:0);if(job)for(let i=0;i<20;i++)for(let f=0;f<6;f++){const offsets=[0xa4+i*8,0xa8+i*8,0x144+i*8,0x148+i*8,0x1e4+i*4,0x234+i*4];delta.writeUInt32BE(m.r32(job+offsets[f]),(i*6+f)*4);}return{channels:channels(),done:!!m.get('v0'),remaining:job?m.r32(job+0x284):0,next:job?m.r32(job+0x288):-1,deltas:delta.toString('hex')};}
const results=[],rejected=[];let identity;
function run(selector,group,duration,changed){
 cursor=0x80310000;owners=new Map();override=changed||null;
 m.load(root,Buffer.alloc(0x1cc0));m.w32(0x8022a974,root);m.w8(0x8018fc19,0);m.set('sp',sp);m.call(0x8023a98c);
 identity=channels();m.set('a0',selector);m.set('a1',group);m.set('a2',duration);m.call(0x8023c228);
 const states=[state()];
 const bound=duration<0?6:650;
 for(let step=0;step<bound&&!states[states.length-1].done;step++){m.call(0x8023c71c);states.push(state());}
 if(duration>=0)assert(states[states.length-1].done);
 assert.equal(m.get('sp'),sp);
 for(const [p,size]of owners)for(let k=0;k<16;k++){assert.equal(m.r8(p-16+k),0xa7);assert.equal(m.r8(p+size+k),0xa7);}
 results.push({selector,group,duration,changed:changed?changed.toString('hex'):null,states});
}
for(const a of assets)if(a.bytes){let cursor=8+a.bytes.readUInt32BE(0)*480;for(let group=0;group<a.bytes.readUInt32BE(4);group++){
 let valid=true;for(;;){const frame=a.bytes.readUInt32BE(cursor),end=a.bytes.readInt32BE(cursor+4)===-1;cursor+=8;valid=valid&&frame<a.bytes.readUInt32BE(0);if(end)break;}
 if(valid)for(const duration of [1,3])run(a.selector,group,duration);else rejected.push({selector:a.selector,group});
}}
for(const selector of [0,11,16,37,50,51,54,61])run(selector,0,30);
const changed=Buffer.alloc(8+960+16);changed.writeUInt32BE(2,0);changed.writeUInt32BE(1,4);
for(let i=0;i<120;i++){changed.writeInt32BE([2147483647,-2147483648,1234567,-2345678,1000,-999][i%6],8+i*4);changed.writeInt32BE([-2147483648,2147483647,-3456789,4567891,7,6001][i%6],488+i*4);}
changed.writeUInt32BE(0,968);changed.writeUInt32BE(99,972);changed.writeUInt32BE(1,976);changed.writeInt32BE(-1,980);
for(const duration of [1,3,30,-3])run(0,0,duration,changed);
const single=Buffer.from(changed.subarray(0,976));single.writeInt32BE(-1,972);run(0,0,0,single);
fs.writeFileSync(process.argv[3],JSON.stringify({identity,assets:assets.map(a=>({selector:a.selector,key:a.key,decoded:a.bytes?a.bytes.toString('hex'):null})),rejected,results}));
console.log(`Original scene transforms: ${assets.length} resource selectors, ${results.length} sequences, ${results.reduce((n,r)=>n+r.states.length,0)} complete channel/job snapshots; allocation/free and resource IO controlled.`);
