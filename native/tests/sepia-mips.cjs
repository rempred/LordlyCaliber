'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
class SepiaMips extends Mips {
 exec(pc,next){
  const w=this.r32(pc),op=w>>>26,rt=w>>>16&31,rs=w>>>21&31;
  if([34,38,42,46].includes(op)){
   // Original stack copies use aligned word pairs; do not generalize the shim.
   const a=(this.r[rs]+(w<<16>>16))>>>0;assert.equal(a&3,op===38||op===46?3:0);
   const aligned=(a&~3)>>>0;
   if(op===34||op===38){if(rt)this.r[rt]=this.r32(aligned);}else this.w32(aligned,this.r[rt]);
   return[next,(next+4)>>>0];
  }
  return super.exec(pc,next);
 }
}
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new SepiaMips();m.ram=new Uint8Array(0x400000);
m.load(0x801bfba0,rom.subarray(0x203030,0x2032b8));
const at=0x80300010,sp=0x803ffe00,cases=[];
function run(format,width,height,fill){
 const stride=format===3?Math.ceil(width/4)*16:Math.ceil(width/4)*8;
 const input=Buffer.alloc(8+stride*height,0xa7);input[3]=format;input.writeUInt16BE(width,4);input.writeUInt16BE(height,6);
 for(let y=0;y<height;y++)for(let x=0;x<width;x++)fill(input,8+y*stride+x*(format===3?4:2),y*width+x);
 m.load(at-16,Buffer.alloc(input.length+32,0xa7));m.load(at,input);m.set('sp',sp);m.set('a0',at);m.call(0x801bfba0,0x80000100,12000000);
 assert.equal(m.get('sp'),sp);for(let i=0;i<16;i++){assert.equal(m.r8(at-16+i),0xa7);assert.equal(m.r8(at+input.length+i),0xa7);}
 cases.push({format,width,height,stride,input:input.toString('hex'),output:Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+input.length)).toString('hex')});
}
run(2,256,256,(b,p,i)=>b.writeUInt16BE(i,p));
const values=[0,1,7,8,15,16,31,32,63,64,127,128,191,192,254,255],pixels=[];
for(const r of values)for(const g of values)for(const b of values)pixels.push(((r<<24)|(g<<16)|(b<<8)|((r+g+b)&255))>>>0);
run(3,64,64,(b,p,i)=>b.writeUInt32BE(pixels[i],p));
run(2,3,5,(b,p,i)=>b.writeUInt16BE((i*4093)&65535,p));
run(3,5,3,(b,p,i)=>b.writeUInt32BE((i*0x1234567)>>>0,p));
fs.writeFileSync(process.argv[3],JSON.stringify({cases}));
console.log('Original sepia: all 65,536 RGBA16 values, 4,096 RGBA32 colors, and padded-row samples. No game services replaced.');
