'use strict';
// Execute the exported normal-game story list, reveal completion, flag helpers
// and renderer. No Testing-tab seeding is involved. Audio/drawing are captured.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const read=p=>{const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;};
const rom=read(process.argv[2]),stock=read(process.argv[3]);
const overlay=((rom.readUInt32BE(0x51524)&65535)*65536+rom.readInt16BE(0x5152a))>>>0;
const h=overlay-80;assert.equal(rom.toString('ascii',h,h+8),'LCCAMP01');
const size=rom.readUInt32BE(h+16),count=rom.readUInt32BE(h+20),roads=rom.readUInt32BE(h+24);
const ram=0x8019a7a0,flagsAddress=0x801a8270+8192-100,ctx=0x80740000,list=0x80720000;
function machine(){const m=new FloatMips();m.load(0x80163910,rom.subarray(0x39810,0x66e10));m.load(0x80197b70,rom.subarray(0x76b00,0x87200));m.load(ram,rom.subarray(overlay,overlay+size));m.set('sp',0x807ff000);m.w32(0x80197af8,ctx);m.w32(ctx+4,0x80600000);m.trap(0x800ea604,()=>{});m.trap(0x80093380,()=>{});return m;}
const bytes=(m,a,n)=>Buffer.from(m.ram.subarray(m.phys(a),m.phys(a)+n));
function state(m,id,value){const at=0x80196a86+((id-1)>>2),shift=((id-1)&3)*2;m.w8(at,(m.r8(at)&~(3<<shift))|(value<<shift));}
function open(m,id){m.set('a0',id);m.call(0x8019e9d0);return m.get('v0');}
function story(m,a,b,value){m.w32(0x80193a8c,list);m.load(list,Buffer.from([a,value?3:4,b,3,255]));m.call(0x8019ea3c);const queued=m.r8(ctx+0x312);if(queued){m.set('s0',ctx+0x312);m.call(0x8019d664,0x8019d670);}return queued;}
// Recreate the reported retail pair under a new ID. A v5 ROM fails this check.
{
 const m=machine();m.w8(0x80196a86,2);const before=bytes(m,0x80196a86,24);
 story(m,35,30,1);assert.deepEqual(bytes(m,0x80196a86,24),before,'story unlock must not overwrite location state');
}
assert.equal(rom.readUInt32BE(h+8),6,'story-safe campaign format');
for(const [a,b] of [[0,0],[0,6],[6,0],[41,6],[6,41],[255,255],[6,6]]) {
 const m=machine();m.set('a0',a);m.set('a1',b);m.call(0x8019ec78);
 assert.equal(m.get('v0'),0,'invalid locations must not match deleted road slots');
}
let storyCases=0,removed=0;
for(let id=1;id<=count;id++){
 const probe=machine(),a=probe.r8(roads+(id-1)*2),b=probe.r8(roads+(id-1)*2+1),flag=probe.r8(flagsAddress+id-1);
 if(!a){assert.equal(open(probe,id),0);removed++;continue;}
 for(const reverse of [false,true])for(const value of [0,1]){
  const m=machine();m.load(0x80196a81,Buffer.alloc(5,value?0:255));m.load(0x80196a86,Buffer.alloc(24,0x55));
  const before=bytes(m,0x80196a81,29),expected=Buffer.from(before);
  assert.equal(story(m,reverse?b:a,reverse?a:b,value),id);
  if(flag){const n=flag-1,mask=1<<(n&7);expected[n>>3]=(expected[n>>3]&~mask)|(value?mask:0);}
  assert.deepEqual(bytes(m,0x80196a81,29),expected,`only owned road bit may change: ${id}`);
  assert.equal(open(m,id),flag?value:1);
  if(!flag){state(m,a,0);assert.equal(open(m,id),0);state(m,a,1);state(m,b,0);assert.equal(open(m,id),0);}
  storyCases++;
 }
}
// Deleted original pairs remain absent even when old saves have their bits set.
for(let id=1;id<=40;id++){
 const a=stock[0x858e4+(id-1)*2],b=stock[0x858e5+(id-1)*2],m=machine();
 m.set('a0',a);m.set('a1',b);m.call(0x8019ec78);
 if(m.get('v0'))continue;
 m.load(0x80196a81,Buffer.alloc(29,0x55));const before=bytes(m,0x80196a81,29);
 assert.equal(story(m,a,b,1),0);assert.deepEqual(bytes(m,0x80196a81,29),before);
}
// Both resident VM helpers must stay safe without a world overlay loaded.
for(let id=0;id<256;id++)for(const value of [0,1]){
 const m=machine();m.load(ram,Buffer.alloc(size,0xcc));m.load(0x80196a81,Buffer.alloc(29,0x55));
 const expected=bytes(m,0x80196a81,29);if(id>=1&&id<=40){const n=id-1,mask=1<<(n&7);expected[n>>3]=(expected[n>>3]&~mask)|(value?mask:0);}
 m.set('a0',id);m.set('a1',value);m.call(0x80172b68);assert.deepEqual(bytes(m,0x80196a81,29),expected);
 m.set('a0',id);m.call(0x80172bbc);assert.equal(m.get('v0'),id>=1&&id<=40?value:0);
}
// Same native renderer call site, every reveal phase, each direction, opening
// and closing. Deleted roads never draw; authored dots are the sole coordinates.
const oldCall=(0x0c000000|((0x801a347c>>>2)&0x3ffffff))>>>0;let drawEntry=0;
for(let at=0x79730;at<0x85820;at+=4)if(stock.readUInt32BE(at)===oldCall)drawEntry=((machine().r32(at+0x80121070)&0x3ffffff)*4+0x80000000)>>>0;
assert(drawEntry);const directory=rom.readUInt32BE(h+64),points=rom.readUInt32BE(h+68);let renders=0;
for(let id=1;id<=count;id++)for(const reverse of [0,1])for(const opening of [0,1])for(let phase=0;phase<7;phase++){
 const m=machine(),draws=[];m.load(0x80196a81,Buffer.alloc(5,0));m.load(0x80196a86,Buffer.alloc(10,0));
 m.w8(ctx+0x312,id);m.w8(ctx+0x313,reverse);m.w8(ctx+0x314,phase);m.w8(ctx+0x315,opening);m.w8(ctx+0x316,1);
 m.trap(0x8019f560,()=>{});m.trap(0x8019f81c,()=>draws.push([m.get('a0'),m.get('a1')]));
 m.set('a0',0);m.set('a1',0);m.call(drawEntry,0x80000100,200000);
 const expected=[];let first=m.r32(directory+(id-1)*4),last=m.r32(directory+id*4),n=last-first;
 if(reverse&&opening)first+=Math.floor(n*phase/7);
 else if(reverse||opening)last=first+Math.ceil(n*(phase+1)/7);
 else first+=Math.ceil(n*(phase+1)/7);
 if(m.r8(roads+(id-1)*2))for(let p=first;p<last;p++)expected.push([m.r16(points+p*4),m.r16(points+p*4+2)]);
 assert.deepEqual(draws,expected,`reveal road ${id}, phase ${phase}, reverse ${reverse}, open ${opening}`);renders++;
}
console.log(`PASS: ${storyCases} native story open/close cases, ${removed} removed slots, all 256 resident command IDs, ${renders} native reveal renders; adjacent save state preserved.`);
