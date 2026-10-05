'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
// Exact constructor/mask code. Allocation and host trigonometry are controlled.
m.load(0x8023d4a4,rom.subarray(0x2b22a4,0x2b2868));
m.load(0x8023e500,rom.subarray(0x2b3300,0x2b3704));
m.load(0x80240244,rom.subarray(0x2b5044,0x2b506c));
m.load(0x8022c200,rom.subarray(0x29ba30,0x29ba58));
m.load(0x801be9a8,rom.subarray(0x201e38,0x201f00));
for(const [a,b]of [[0x22e90,0x231b0],[0x28740,0x29160]])m.load(a+0x8006fc00,rom.subarray(a,b));
m.load(0x800aa750,rom.subarray(0x3ab50,0x3ab54));
m.load(0x80243c98,rom.subarray(0x2b8a98,0x2b8ae8));
m.trap(0x80092db0,c=>c.setSingle(0,Math.sin(c.single(12))));
m.trap(0x800907f0,c=>c.setSingle(0,Math.cos(c.single(12))));
m.trap(0x8009cfe0,c=>c.setSingle(0,Math.atan2(c.single(12),c.single(14))));
const context=0x80300000,root=0x80302000,source=0x80310000,processed=0x80380000,sp=0x803ffe00;
const read=(p,n)=>Buffer.from(m.ram.subarray(m.phys(p),m.phys(p)+n));
const word=v=>{const b=Buffer.alloc(4);b.writeFloatBE(v);return b.readUInt32BE();};
const call=(p,args=[])=>{m.set('sp',sp);args.forEach((v,i)=>i<4?m.set('a'+i,v):m.w32(sp+16+(i-4)*4,v));m.call(p,0x80000100,30000000);assert.equal(m.get('sp'),sp);return m.get('v0');};
m.w32(0x8022a970,context);m.w32(0x8022a974,root);
let allocation=0;
m.trap(0x80070f30,c=>{allocation=c.get('a0');assert(allocation<0x60000);m.load(processed-16,Buffer.alloc(allocation+32,0xa7));c.set('v0',processed);});
const pixels=[];
for(const [width,height]of [[4,8],[15,9],[64,48],[65,49],[320,240],[416,304]])for(const cap of [0,1,130,255]) {
 const stride=(width+3)&~3,src=Buffer.alloc(8+stride*height*2,0xa7);
 src.writeUInt32BE(0x36340002);src.writeUInt16BE(width,4);src.writeUInt16BE(height,6);
 for(let y=0;y<height;y++)for(let x=0;x<width;x++)src.writeUInt16BE(((y*width+x)*977+0x9345)&65535,8+(y*stride+x)*2);
 m.load(context,Buffer.alloc(0x848));m.w32(context+0x4c,source);m.load(source,src);
 assert.equal(call(0x8023d4a4,[0,cap]),processed);
 const w=width>>1,h=height>>1,storage=(w+3)&~3,visible=Buffer.alloc(w*h*4);
 assert.equal(allocation,8+storage*h*4);assert.equal(m.r32(processed),0x37340003);
 assert.equal(m.r16(processed+4),w);assert.equal(m.r16(processed+6),h);
 assert.equal(m.r32(context+0x50),source);assert.equal(m.r8(context+0x83d),cap);
 for(let y=0;y<h;y++)read(processed+8+y*storage*4,w*4).copy(visible,y*w*4);
 assert(read(source,src.length).equals(src));assert(read(processed-16,16).equals(Buffer.alloc(16,0xa7)));assert(read(processed+allocation,16).equals(Buffer.alloc(16,0xa7)));
 pixels.push({width,height,cap,rgba:visible.toString('base64')});
}
// The matrix constructor calls the already separately checked pixel builder.
m.trap(0x8023d4a4,c=>{m.w8(context+0x83d,c.get('a1'));m.w32(context+0x50,1);m.w32(context+0x4c,2);c.set('v0',2);});
const matrices=[];
for(const eye of [[272,80,127],[0,0,120],[-200,30,100],[0,100,0]])
 for(const flags of [0,4,8,0xffffffff])for(const placement of [[-102,39,29,29],[20,-70,125,60],[0,0,-10,100]])for(const ownerScale of [[1,1],[.7,1.5]]) {
  m.load(context,Buffer.alloc(0x848));m.w32(context+0x4c,1);m.writeFloat(context+0x818,ownerScale[0]);m.writeFloat(context+0x81c,ownerScale[1]);
  const camera=[45,4/3,1,20000,1,...eye,0,0,0,0,1,0];camera.forEach((v,i)=>m.writeFloat(0x8022a778+i*4,v));
  [12,34,56].forEach((v,i)=>m.writeFloat(0x8022a984+i*4,v));
  const before=read(context,0x848).toString('hex');
  call(0x8023e694,[2,word(placement[0]),word(placement[1]),123,word(placement[2]),word(placement[3]),130,flags]);
  assert.equal(m.r32(root+0x1c0c),2);[12,34,56].forEach((v,i)=>assert.equal(m.readFloat(0x8022a984+i*4),v));
  matrices.push({camera,flags,placement,before,after:read(context,0x848).toString('hex')});
 }
fs.writeFileSync(process.argv[3],JSON.stringify({pixels,matrices}));
console.log(`${pixels.length} complete pixel constructions and ${matrices.length} complete matrix constructions; allocator and libm controlled.`);
