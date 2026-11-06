'use strict';
// Exported MIPS instructions and the actual native texture-setup function.
// This checks emitted graphics commands, not RDP rasterization or video timing.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
if(process.argv.includes('--previous')){
 const vm=require('node:vm'),path=require('node:path'),c={};c.window=c;vm.createContext(c);
 vm.runInContext(fs.readFileSync(path.resolve(__dirname,'../../native-rebuild-data.js'),'utf8'),c);
 for(const p of c.OB64_NATIVE_REBUILD_DATA.payload.phase3.components)
  if(['tool.character-card-luck','tool.chaos-frame-counter'].includes(p.owner))Buffer.from(p.dataHex,'hex').copy(rom,p.romStart);
}
const bytes=(m,a,n)=>Buffer.from(m.ram.slice(m.phys(a),m.phys(a)+n));
const sp=0x807ffe00,dl=0x80270000,member=0x80350000;
// Sample the emitted CI4 rectangles as individual pixels, including palette
// transparency and spacing. This is a focused point-sampling check, not a full
// RDP implementation (filtering, other menu layers and video scaling excluded).
const samples=[],parchment=[236,221,185,255],left=108,top=29,width=48,height=12;
function raster(m){
 const pixels=Buffer.alloc(width*height*4);for(let i=0;i<pixels.length;i+=4)pixels.set(parchment,i);
 const palette=m.r32(dl+12),texture=m.r32(dl+60);
 for(let i=0;i<4;i++){
  const at=dl+0x78+i*32,a=m.r32(at),b=m.r32(at+4),uv=m.r32(at+12),step=m.r32(at+20);
  const x0=(b>>>12&4095)/4,y0=(b&4095)/4,x1=(a>>>12&4095)/4,y1=(a&4095)/4;
  for(let y=Math.ceil(y0);y<y1;y++)for(let x=Math.ceil(x0);x<x1;x++){
   const s=Math.floor((uv>>>16)/32+(x+.5-x0)*(step>>>16)/1024);
   const t=Math.floor((uv&65535)/32+(y+.5-y0)*(step&65535)/1024);
   assert(s>=0&&s<64&&t>=0&&t<16);
   const packed=m.r8(texture+t*32+(s>>1)),index=(packed>>((s&1)?0:4))&15,color=m.r16(palette+index*2);
   if(!(color&1))continue;
   assert(x>=left&&x<left+width&&y>=top&&y<top+height);
   pixels.set([(color>>>11)*255/31,(color>>>6&31)*255/31,(color>>>1&31)*255/31,255],((y-top)*width+x-left)*4);
  }
 }
 return pixels;
}
let luckCases=0,chaosCases=0;
for(const value of process.argv.includes('--chaos-only')?[]:[...Array(101).keys(),255])for(const style of [2,1,0]) {
 const run=hook=>{
  const m=new Mips();m.load(0x80480000,rom.subarray(0x27c2000,0x27c3000));
  m.load(0x8019b0d0,rom.subarray(0x87b30,0x87dec));
  m.load(0x801eecd8,rom.subarray(0xdb738,0xdb738+64));
  m.set('sp',sp);m.set('s5',member);m.w8(member+0x28,value);m.w32(0x800e9ba0,dl);
  m.set('a0',0x803002e0);m.set('a1',0x803127a8);m.set('a2',16);m.set('a3',8);
  m.w32(sp+16,style);m.w32(sp+20,0xaabbccdd);
  m.call(hook?0x80480100:0x8019b0d0);
  assert.equal(m.get('sp'),sp);assert.equal(m.get('s5'),member);
  assert.equal(m.r32(sp+16),style);assert.equal(m.r32(sp+20),0xaabbccdd);
  return m;
 };
 const retail=run(false),patched=run(true);
 const nativeSize=retail.r32(0x800e9ba0)-dl;
 assert.equal(patched.r32(0x800e9ba0)-dl,nativeSize+0xf8);
 assert.deepEqual(bytes(patched,dl+0xf8,nativeSize),bytes(retail,dl,nativeSize),`capacity icon changed for LCK ${value}, style ${style}`);
 for(const [i,x,w,y,h,scale] of [[0,110,18,31,8,0x04000400],[1,131,6,31,8,0x04000400],[2,138,6,31,8,0x04000400],[3,145,6,31,8,0x04000400]]){
  const at=dl+0x78+i*32,w0=patched.r32(at),w1=patched.r32(at+4);
  assert.equal(w0>>>24,0xe4);assert.equal((w1>>>12&4095)/4,x);assert.equal((w1&4095)/4,y);
  assert.equal((w0>>>12&4095)/4,x+w);assert.equal((w0&4095)/4,y+h);
  assert.equal(patched.r32(at+20),scale);
 }
 const digits=String(Math.min(value,100)).padEnd(3,' ');
 for(let i=0;i<3;i++)assert.equal(patched.r32(dl+0xa4+i*32),digits[i]===' '?0x03000000:((Number(digits[i])*192)<<16|256)>>>0);
 if(style===2){
  const rendered=raster(patched);
  for(let i=0;i<3;i++){
   let ink=0;
   for(let y=0;y<8;y++)for(let x=0;x<6;x++){
    const n=digits[i]===' '?-1:Number(digits[i]),s=n*6+x;
    const source=n<0?0:(patched.r8(0x80480520+(y+8)*32+(s>>1))>>((s&1)?0:4))&15;
    const actual=rendered.subarray(((31+y-top)*width+131+i*7+x-left)*4,((31+y-top)*width+131+i*7+x-left)*4+4);
    assert.equal(actual.equals(Buffer.from(parchment)),source===0,`LCK ${value}, digit ${i}, pixel ${x},${y}`);
    if(source)ink++;
   }
   assert(digits[i]===' '?ink===0:ink>=8);
   if(i<2)for(let y=31;y<39;y++)assert.deepEqual(rendered.subarray(((y-top)*width+137+i*7-left)*4,((y-top)*width+137+i*7-left)*4+4),Buffer.from(parchment),'Digits need a visible gap');
  }
  if([0,7,10,56,88,100].includes(value))samples.push(rendered);
 }
 luckCases++;
}
for(const tailSize of [0x2f0,0x910])for(const [texture,palette] of [[0x801ff390,0x801fa450],[0x80320000,0x80340000]])for(const value of [0,1,9,10,50,99,100,255]) {
 const m=new Mips(),task=0x80300000,size=0x2400,insert=dl+size-tailSize;
 m.load(0x80420000,rom.subarray(0x27c1000,0x27c2000));
 const seed=Buffer.alloc(size);for(let i=0;i<size;i++)seed[i]=(i*7+19)&255;m.load(dl,seed);
 m.w32(0x80196af8,0x801f1550);m.w32(0x801f1554,texture);m.w32(0x801f1558,palette);
 m.w32(task+48,dl&0x1fffffff);m.w32(task+52,size);
 m.w32(insert,0xed064324);m.w32(insert+4,0x001b437c);
 m.w32(insert-2184,0x01004008);m.w32(insert-2180,dl-23584);
 m.w8(0x801936a9,value);m.set('sp',sp);m.set('a0',task);m.set('a1',64);
 const before=bytes(m,dl,size);let submissions=0;
 m.trap(0x80090140,c=>{assert.equal(c.get('a0'),task);assert.equal(c.get('a1'),64);submissions++;});
 m.call(0x80420100,0x80093b84);
 assert.equal(submissions,1);assert.equal(m.get('sp'),sp);
 assert.equal(m.r32(task+52),(size+0x32f)&0xffc0);
 assert.deepEqual(bytes(m,dl,size-tailSize),before.subarray(0,size-tailSize));
 assert.deepEqual(bytes(m,insert+0x2f0,tailSize),before.subarray(size-tailSize));
 assert.equal(m.r32(insert+0x234),texture+0x948,'CF digit texture must follow loaded Army resource');
 assert.equal(m.r32(0x80420894),palette+0x320,'CF title palette must follow loaded Army resource');
 const text=String(Math.min(value,100)).padStart(3,'0');
 const params=[0x340,0x200,0x00e00200,0x01c00200,0x02a00200,0x03800200,0x04600200,0x05400200,0x06200200,0x07000200];
 for(const [i,offset] of [0x27c,0x2a4,0x2cc].entries())assert.equal(m.r32(insert+offset),params[Number(text[i])]);
 chaosCases++;
}
console.log(`PASS ${luckCases} LCK/native icon command comparisons and ${chaosCases} CF relocated-resource/tail cases. Native-size LCK label and separated digits below Lv; values, stack, original icon commands and task tails preserved.`);
const pngArg=process.argv.indexOf('--png');
if(pngArg>=0){
 assert.equal(samples.length,6);const zlib=require('node:zlib'),scale=8,w=width*3*scale,h=height*2*scale;
 const scan=Buffer.alloc((w*4+1)*h);
 for(let y=0;y<h;y++)for(let x=0;x<w;x++){
  const sx=Math.floor(x/scale),sy=Math.floor(y/scale),cell=Math.floor(sy/height)*3+Math.floor(sx/width);
  const offset=((sy%height)*width+sx%width)*4;
  samples[cell].copy(scan,y*(w*4+1)+1+x*4,offset,offset+4);
 }
 const crc=b=>{let c=0xffffffff;for(const v of b){c^=v;for(let k=0;k<8;k++)c=c>>>1^((c&1)?0xedb88320:0);}return(c^0xffffffff)>>>0;};
 const chunk=(name,data)=>{const out=Buffer.alloc(data.length+12);out.writeUInt32BE(data.length);out.write(name,4);data.copy(out,8);out.writeUInt32BE(crc(out.subarray(4,-4)),out.length-4);return out;};
 const header=Buffer.alloc(13);header.writeUInt32BE(w);header.writeUInt32BE(h,4);header[8]=8;header[9]=6;
 fs.writeFileSync(process.argv[pngArg+1],Buffer.concat([Buffer.from('89504e470d0a1a0a','hex'),chunk('IHDR',header),chunk('IDAT',zlib.deflateSync(scan)),chunk('IEND',Buffer.alloc(0))]));
}
