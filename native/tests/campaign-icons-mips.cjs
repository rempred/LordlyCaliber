'use strict';
// Executes the complete native location renderer. Texture setup/drawing services
// are captured; this checks its submissions, not final RDP pixels or gameplay.
const fs = require('node:fs'), assert = require('node:assert/strict');
const {FloatMips} = require('./float-mips.cjs');
function read(path) {
 const b=fs.readFileSync(path);if(b.readUInt32BE(0)===0x37804012)b.swap16();
 assert.equal(b.readUInt32BE(0),0x80371240);return b;
}
const image=read(process.argv[2]),original=read(process.argv[3]);
const h=image.lastIndexOf('LCCAMP01');assert(h>=0);assert.equal(image.readUInt32BE(h+8),3);
const offset=image.readUInt32BE(h+12),size=image.readUInt32BE(h+16);
const icons=image.readUInt32BE(h+24)-12;
assert.deepEqual(image.subarray(offset+icons-0x8019a7a0,offset+icons-0x8019a7a0+5),Buffer.alloc(5,255),'exported fixture selects Castle for all 40 locations');
function render(edited,id,icon,state,tick) {
 const m=new FloatMips(),ctx=0x80740000,art=0x80600000,draw=0x80640000;
 m.load(0x80163910,original.subarray(0x39810,0x66e10));
 m.load(0x80197b70,original.subarray(0x76b00,0x87200));
 if(edited)m.load(0x8019a7a0,image.subarray(offset,offset+size));
 // Isolate one location while comparing identical native geometry/state.
 for(let n=0;n<40;n++){m.w16(0x801a7510+n*8,0);m.w16(0x801a7512+n*8,0);}
 m.w16(0x801a7510+(id-1)*8,100);m.w16(0x801a7512+(id-1)*8,120);
 m.load(0x80196a86,Buffer.alloc(10,0));m.w8(0x80196a86+((id-1)>>2),state<<(((id-1)&3)*2));
 m.load(0x80196a93,Buffer.alloc(5,255));
 if(edited)m.load(icons,Buffer.alloc(5,icon?255:0));
 else m.load(0x801a8218,Buffer.alloc(10,icon?id:0));
 m.w32(0x80197af8,ctx);m.w32(ctx+4,art);m.w32(ctx+0x108,tick);m.w32(0x800e9ba0,draw);
 const textures=[],markers=[];
 m.trap(0x8019f560,()=>textures.push([m.get('a0')-art,m.get('a1')-art,m.get('a2'),m.get('a3')]));
 m.trap(0x8019f81c,()=>markers.push([m.get('a0'),m.get('a1'),m.get('a2')]));
 m.set('sp',0x807ff000);m.set('a0',0);m.set('a1',0);m.call(0x801a2d64,0x80000100,100000);
 assert.equal(m.get('sp'),0x807ff000);
 const end=m.r32(0x800e9ba0);assert(end>=draw&&end<draw+4096);
 const commands=[];for(let a=draw;a<end;a+=4)commands.push(m.r32(a));
 return {textures,markers,commands};
}
let cases=0;
for(let id=1;id<=40;id++)for(const icon of [0,1])for(const state of [0,1,2,3])for(const tick of [0,12,24,36]) {
 const actual=render(true,id,icon,state,tick),expected=render(false,id,icon,state,tick);
 assert.deepEqual(actual,expected,`location ${id}, icon ${icon}, state ${state}, tick ${tick}`);
 if(state===1) {
  assert.equal(actual.textures.length,1);
  assert.equal(actual.textures[0][0],icon?0x140:0x100);
  if(icon)assert.equal(actual.textures[0][1],0xf68);
 }
 cases++;
}
console.log(`PASS: ${cases} native renderer comparisons: all 40 locations, Flag/Castle, four game states, animation frames, texture and display-list submissions. No full-console pixel claim.`);
