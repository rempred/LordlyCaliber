'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');const {FloatMips}=require('./float-mips.cjs');
const read=p=>{const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;};
const rom=read(process.argv[2]),original=read(process.argv[3]);
const h=rom.lastIndexOf('LCCAMP01');assert(h>=0);const at=rom.readUInt32BE(h+12),size=rom.readUInt32BE(h+16),count=rom.readUInt32BE(h+20),roads=rom.readUInt32BE(h+24),dots=rom.readUInt32BE(h+28);
const version=rom.readUInt32BE(h+8),bendCapacity=version>=4?14:7,stride=bendCapacity*4;
function machine(){const m=new FloatMips();m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(0x80197b70,rom.subarray(0x76b00,0x87200));m.load(0x8019a7a0,rom.subarray(at,at+size));m.load(0x80196a81,Buffer.alloc(5,255));m.load(0x80196a86,Buffer.alloc(10,0x55));m.set('sp',0x807ff000);return m;}
const m=machine();
for(let id=1;id<=count;id++){
 m.set('a0',id);m.call(0x8019e9d0);assert.equal(m.get('v0'),id===1?0:1);
 const a=m.r8(roads+(id-1)*2),b=m.r8(roads+(id-1)*2+1);
 if(!a){assert.equal(b,0);continue;}
 for(const [from,to,expected] of [[a,b,id],[b,a,-id]]){m.set('a0',from);m.set('a1',to);m.call(0x8019ec78);assert.equal(m.get('v0')|0,expected);}
}
m.set('a0',original[0x858e4]);m.set('a1',original[0x858e5]);m.call(0x8019ec78);assert.equal(m.get('v0'),0,'removed connection cannot be selected');
for(const direction of [1,-1]){
 const m=machine(),ctx=0x80740000,id=2,a=m.r8(roads+2),b=m.r8(roads+3),from=direction>0?a:b,to=direction>0?b:a;
 m.w32(0x80197af8,ctx);m.w8(ctx+0x5f,0x5a);m.w8(ctx+0xa0,0xa5);
 m.w8(ctx+0xa1,from);m.w8(ctx+0xa2,to);m.w8(ctx+0xe1,id*direction);
 const flags=Buffer.from(m.ram.subarray(m.phys(0x80196a81),m.phys(0x80196a93)));
 m.set('a0',ctx);m.set('t0',1);m.call(0x8019c2a0,0x8019c434,100000);
 const shape=[];for(let n=0;n<bendCapacity;n++){const x=m.r16(dots+stride+n*4),y=m.r16(dots+stride+n*4+2);if(x)shape.push([x,y]);}if(direction<0)shape.reverse();
 const pos=id=>[rom.readInt16BE(0x864a0+(id-1)*8),rom.readInt16BE(0x864a2+(id-1)*8)];
 const points=[];let pages=0;
 for(;;){
  const start=m.r8(ctx+0xe4),page=[];let end=start;
  for(;end<16;end++){const x=m.r16(ctx+0x60+end*2),y=m.r16(ctx+0x80+end*2);if(!x)break;page.push([x,y]);}
  assert(end<16,'page has a terminator inside the native buffer');
  if(pages)assert.deepEqual(page[0],points.at(-1),'sections meet at the same position');
  points.push(...(pages?page.slice(1):page));pages++;
  assert(pages<=2,'at most two seven-bend sections');
  if(version<4)break;
  m.w8(ctx+0xe4,end-1);
  // Execute the installed continuation hook, including its unchanged native branch.
  m.w16(ctx+0x1c6,10);
  m.call(0x8019c650,0x8019c668,100000);
  if(!m.get('v0'))break;
  m.call(0x8019c668,0x8019da3c,100000);
  if(m.r8(ctx+0xe4)===end-1)break;
  assert.equal(m.r8(ctx+0xe4),1,'continued page bypasses native road reinitialization');
  m.call(0x8019c28c,0x8019c434,100000);
  assert.equal(m.r8(ctx+0xe4),1);
 }
 assert.deepEqual(points,[pos(from),...shape,pos(to)],'native travel consumes every bend in order');
 assert.equal(pages,shape.length>7?2:1);
 assert.equal(m.r8(ctx+0x5f),0x5a);assert.equal(m.r8(ctx+0xa0),0xa5);
 assert.equal(m.r8(ctx+0xa1),from);assert.equal(m.r8(ctx+0xa2),to);assert.equal(m.r8(ctx+0xe2),0);
 assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(0x80196a81),m.phys(0x80196a93))),flags,'section handoff leaves availability/story state alone');
}
const ctx=0x80740000;m.w32(0x80197af8,ctx);m.w32(ctx+4,0x80600000);const rendered=[];m.trap(0x8019f560,()=>{});m.trap(0x8019f81c,()=>rendered.push([m.get('a0'),m.get('a1'),m.get('a2')]));
const oldCall=0x0c000000|((0x801a347c>>>2)&0x3ffffff);let entry=0;for(let off=0x79730;off<0x85820;off+=4)if(original.readUInt32BE(off)===oldCall)entry=((m.r32(off+0x80121070)&0x3ffffff)<<2|0x80000000)>>>0;
assert(entry);m.set('a0',0);m.set('a1',0);m.call(entry,0x80000100,2000000);
const expected=[];
const directory=version>=2?rom.readUInt32BE(h+64):0,display=version>=2?rom.readUInt32BE(h+68):0;
for(let r=0;r<count;r++){
 if(directory){for(let n=m.r32(directory+r*4);n<m.r32(directory+(r+1)*4);n++)expected.push([m.r16(display+n*4),m.r16(display+n*4+2),5]);}
 else for(let n=0;n<7;n++){const x=m.r16(dots+r*28+n*4),y=m.r16(dots+r*28+n*4+2);if(x)expected.push([x,y,5]);}
}
if(directory)assert(m.r32(directory+8)-m.r32(directory+4)>7,'stretched road submits additional visible dots');
assert.deepEqual(rendered,expected,'renderer draws changed points and no removed connection');
console.log('PASS: removed-road availability/lookup, retained connection lookup, native travel over edited dots in both directions, and actual native dot-render submissions.');
