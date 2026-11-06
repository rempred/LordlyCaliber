'use strict';
// Executes original resource dispatch/cache/LH5 and field selector setup. OS,
// allocation and PI services are doubles, bounded to the original4MiB RAM.
// The artifact's field instructions differ only in data-reference immediates:
// native branches, calls, opcodes and register assignments are retained. These
// tests exercise that unchanged code against the relocated native containers.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const fixture=JSON.parse(fs.readFileSync(process.argv[2]));
const image=fs.readFileSync(fixture.rom);if(image.readUInt32BE(0)===0x37804012)image.swap16();
let transfers=0,loads=0;
function cpu(){
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);m.load(0x80070c00,image.subarray(0x1000,0x3f1b0));
 m.set('sp',0x803ffe00);let heap=0x80100010;const guards=[];
 const allocate=c=>{const size=c.get('a0'),capacity=(size+15)&~15;assert(size>0&&size<0x100000,`allocation ${size} at return ${c.get('ra').toString(16)}`);
  const at=heap;heap+=capacity+32;assert(heap<0x80300000);c.load(at-16,Buffer.alloc(capacity+32,0xa7));
  guards.push(at-16,at+capacity);c.set('v0',at);};
 for(const at of [0x80070f30,0x80071288])m.trap(at,allocate);
 m.trap(0x800712c4,()=>{});m.trap(0x80093380,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
 m.trap(0x80089f80,c=>{const at=c.get('a0'),to=c.get('a1'),size=c.get('a2');assert(at+size<=image.length);
  if(process.env.LORDLY_FIELD_TRACE)console.error('PI',at.toString(16),to.toString(16),size);
  c.load(to,image.subarray(at,at+size));transfers++;});
 for(const at of [0x8007bb90,0x8007bbc0,0x8007bbf4,0x80093540])m.trap(at,c=>{throw Error('Native field resource load failed at '+at.toString(16));});
 m.check=()=>{for(const at of guards)for(let i=0;i<16;i++)assert.equal(m.r8(at+i),0xa7,'native decoder guard');};
 return m;
}
for(const resource of fixture.resources){
 const m=cpu(),expected=Buffer.from(resource.data,'hex');
 if(resource.member>=0){m.set('a0',resource.root);m.set('a1',resource.index);m.set('a2',-6);m.set('a3',0);m.call(0x8007938c,0x80000100,20000000);}
 m.set('a0',resource.root);
 m.set('a1',resource.member<0?-4:resource.index);m.set('a2',resource.member<0?resource.index:-4);m.set('a3',resource.member<0?0:resource.member);
 m.call(0x8007938c,0x80000100,20000000);const at=m.get('v0');
 assert(at>=0x80100000&&at+expected.length<0x80300000);
 assert.deepEqual(Buffer.from(m.ram.slice(m.phys(at),m.phys(at)+expected.length)),expected);
 m.check();assert.equal(m.get('sp'),0x803ffe00);loads++;
}
const descriptor=0x27c5500,tableOffset=image.readUInt32BE(descriptor+12),tableCount=image.readUInt32BE(descriptor+16),tableAddress=image.readUInt32BE(descriptor+40);
assert(tableAddress>=0x80188000&&tableAddress+tableCount*12<=0x8018aa70);
for(let model=0;model<tableCount;model++){
 const m=cpu(),delta=0x800b32a0,start=0xed904,end=0xed960,args=[];
 m.load(tableAddress,image.subarray(tableOffset,tableOffset+tableCount*12));m.w8(0x800e7ab8,model);m.set('s1',0x800e7ab8);
 m.load((start+delta)>>>0,image.subarray(start,end));m.trap(0x8007938c,c=>{args.push(['a0','a1','a2','a3'].map(r=>c.get(r)));c.set('v0',0x80200000);});
 m.call((start+delta)>>>0,(end+delta)>>>0);
 const root=image.readUInt32BE(descriptor+20),selection=image.readUInt32BE(tableOffset+model*12);
 assert.deepEqual(args,[[root,selection,0xfffffffa,0],[root,selection,0xfffffffc,0]],'actual field model selector');
}
console.log(`PASS ${loads} native field resource loads and ${tableCount} native geometry selector paths, ${transfers} PI transfers, decoder guards in4MiB RAM. Full-game camera, movement, peak heap coexistence and transitions remain gameplay checks.`);
