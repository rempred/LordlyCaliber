'use strict';
// Private integration check: final exported hook + original ground ray, height,
// camera projection and vector/trig instructions, using naturally loaded RAM.
// No height/projection doubles. Does not prove controller/menu path or rendering.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]),ram=fs.readFileSync(process.argv[3]);
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
class CursorMips extends FloatMips {
 exec(pc,next){
  const w=this.r32(pc);
  if(w>>>26===17&&(w>>>21&31)===16&&[14,15].includes(w&63)){
   const value=this.single(w>>>11&31);
   assert(Number.isFinite(value)&&value>=-2147483648&&value<2147483648);
   this.fp[w>>>6&31]=((w&63)===15?Math.floor:Math.ceil)(value);
   return[next,(next+4)>>>0];
  }
  return super.exec(pc,next);
 }
}
const word=rom.readUInt32BE(0x1038ac);assert.equal(word>>>26,3,'native caller uses JAL');
const entry=(0x80000000|((word&0x3ffffff)<<2))>>>0;
const offset=rom.indexOf(Buffer.from('LCCURS01')),address=entry-192,size=rom.readUInt32BE(offset+12);
assert(offset>0&&offset+size<=rom.length&&address+size<=0x80400000);
assert.equal(rom.readUInt32BE(offset+8),entry);
assert.equal(rom.readUInt32BE(0x1038b0),0x24140001,'original delay slot retained');
const m=new CursorMips();m.load(0x80000000,ram);m.load(address,rom.subarray(offset,offset+size));
const camera=m.r32(0x8018f58c),cursor=m.r32(0x801ab850),selector=m.r8(0x800e7ab9);
const vec=a=>[0,4,8].map(n=>m.readFloat(a+n));
const bounds=[0,4,8,12].map(n=>m.readFloat(0x801f0db8+n)),eye=vec(camera+20),look=vec(camera+32);
const radius=Math.hypot(eye[0]-look[0],eye[2]-look[2]);
function query(target,x,y){
 m.set('sp',0x803f0000);m.set('a0',selector);m.set('a1',camera);
 m.writeFloat(0x803e0020,x);m.writeFloat(0x803e0024,y);
 m.set('a2',m.r32(0x803e0020));m.set('a3',m.r32(0x803e0024));m.w32(0x803f0010,0x803e0040);
 m.call(target);return {result:m.get('v0'),point:vec(0x803e0040)};
}
let cases=0,corrected=0,unchanged=0,failures=0;
for(const angle of [0,Math.PI/4,Math.PI/2,3*Math.PI/4,Math.PI,5*Math.PI/4,3*Math.PI/2,7*Math.PI/4]){
 m.writeFloat(camera+20,look[0]+radius*Math.sin(angle));m.writeFloat(camera+28,look[2]+radius*Math.cos(angle));
 for(const x of [40,80,120,160,200,240,280])for(const y of [30,60,90,120,150,180,210]){
  const original=query(0x801aa8a4,x,y);
  m.writeFloat(cursor+8,x);m.writeFloat(cursor+12,y);
  const actual=query(entry,x,y),p=original.point;
  const expected=[Math.max(bounds[0],Math.min(bounds[2],p[0])),Math.max(bounds[1],Math.min(bounds[3],p[2]))];
  assert.equal(actual.point[0],expected[0]);assert.equal(actual.point[2],expected[1]);
  assert.equal(actual.result,original.result,'preserve native query status');
  if(p[0]===expected[0]&&p[2]===expected[1]){
   assert.deepEqual(actual,original);assert.equal(m.readFloat(cursor+8),x);assert.equal(m.readFloat(cursor+12),y);unchanged++;
  }else{
   const projected=query(0x801aa8a4,m.readFloat(cursor+8),m.readFloat(cursor+12));
   // Mesh seams/occlusion may select another triangle; record such material
   // discrepancies instead of silently asserting idealized flat geometry.
   if(!projected.result||Math.hypot(projected.point[0]-actual.point[0],projected.point[2]-actual.point[2])>.002)failures++;
   corrected++;
  }
  m.w8(0x8018f481,2); // untouched Volmus must retain its exact query result
  m.writeFloat(cursor+8,x);m.writeFloat(cursor+12,y);
  assert.deepEqual(query(entry,x,y),original);assert.equal(m.readFloat(cursor+8),x);assert.equal(m.readFloat(cursor+12),y);
  m.w8(0x8018f481,1);cases++;
 }
}
assert(corrected>0&&unchanged>0);assert.equal(failures,0,'corrected ground and visible cursor agree');
console.log(`PASS ${cases} original-ROM projection cases at 8 headings; ${corrected} corrected, ${unchanged} unchanged, unedited-map pass-through exact.`);
