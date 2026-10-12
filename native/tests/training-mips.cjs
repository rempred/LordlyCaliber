'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
function read(p){const b=fs.readFileSync(p);if(b.readUInt32BE(0)===0x37804012)b.swap16();return b;}
const edited=read(process.argv[2]),retail=read(process.argv[3]);
function run(image,record,levels,mode,already=false){
 const m=new Mips(),calls=[],transitions=[],frees=[];
 m.load(0x801d0a8c,image.subarray(0x23a5ec,0x23b048));m.set('sp',0x803fff00);
 m.load(0x80300000,record);m.w32(0x802ff000,0x123456);m.w8(0x80193672,30);
 m.w8(0x801976d8,mode);m.w8(0x801976dc,2);m.w8(0x801976e8,already?30:0);
 levels.forEach((level,i)=>{m.w8(0x801971f0+50+2+i,i+1);m.w8(0x80193bd3+(i+1)*56,level);});
 m.trap(0x8009dbb8,c=>c.set('v0',c.get('a1')===0x21ae74c?0x802ff000:0x802fe000));
 m.trap(0x8009daf4,c=>c.set('v0',record.length));
 m.trap(0x800712c4,c=>frees.push(c.get('a0')));
 m.trap(0x8007acb0,c=>c.w32(c.get('a1')+8,0x80300000));
 m.trap(0x80080998,c=>c.load(c.get('a0'),c.ram.slice(c.phys(c.get('a1')),c.phys(c.get('a1'))+c.get('a2'))));
 m.trap(0x80093380,c=>c.load(c.get('a0'),new Uint8Array(c.get('a1'))));
 m.trap(0x801d0840,c=>{const args=['a0','a1','a2','a3'].map(n=>c.get(n));args.push(c.r32(c.get('sp')+16));calls.push(args);
  c.w8(args[0]+17,args[1]);c.w16(args[0]+22,100);});
 for(const a of [0x80172394,0x8017223c,0x801707f4])m.trap(a,c=>transitions.push([a,a===0x8017223c?c.get('a0'):0]));
 m.call(0x801d0a8c);assert.equal(m.get('sp'),0x803fff00);
 return {calls,transitions,frees,unit:[...m.ram.slice(m.phys(0x801974de),m.phys(0x801974de)+25)],
   soldiers:[...m.ram.slice(m.phys(0x801953f0),m.phys(0x801953f0)+0x16c)],
   characters:[...m.ram.slice(m.phys(0x80195560),m.phys(0x80195560)+0x141c)]};
}
let cases=0;
for(const cls of [1,2,9])for(const followers of [0,1,3,4])for(const delta of [-3,0,7])for(const levels of [[],[1],[4,9,7],[99,70]])for(const mode of [0,8]){
 const r=Buffer.alloc(35);r[0]=cls;r[1]=delta&255;r[6]=9;
 r[7]=5;r[8]=2;r[16]=1;r[17]=255;r.writeUInt16BE(23,2);r.writeUInt16BE(255,4);
 for(let i=0;i<followers;i++)r[i<3?13+i:22]=[1,3,5,7][i];
 assert.deepEqual(run(edited,r,levels,mode),run(retail,r,levels,mode));cases++;
}
const r=Buffer.alloc(175);r.write('LS',25);r[6]=9;
for(let i=0;i<5;i++){r[i*35]=i+2;r[i*35+1]=(i-2)&255;r.writeUInt16BE(20+i,i*35+2);r.writeUInt16BE(30+i,i*35+4);
 if(i){r.writeUInt16BE(i,25+i*2);r[i<4?12+i:22]=[0,1,3,5,7][i];}}
r.set(r.subarray(35,41),7);r.set(r.subarray(140,146),16);
const expanded=run(edited,r,[12,7,9],8);
assert.deepEqual(expanded.calls.slice(1).map(c=>c.slice(1)),Array.from({length:5},(_,i)=>[i+2,10+i,20+i,30+i]));
assert.deepEqual(expanded.unit.slice(2,7),[1,2,3,4,5]);assert.deepEqual(expanded.unit.slice(7,12),[8,0,2,4,6]);
assert.deepEqual(run(edited,r,[12],8,true).calls,[]);
console.log(`PASS ${cases} original-versus-rebuilt training constructor cases, independent followers, formation and already-created guard.`);

const priceCpu=new Mips();priceCpu.load(0x8019dff8,retail.subarray(0x7cf88,0x7d00c));
for(const price of [0,1,15,20,255])for(let level=1;level<=99;level++){
 priceCpu.r.fill(0);priceCpu.set('a1',level);priceCpu.w8(0x801a7ae2,price);priceCpu.w32(0x80197af8,0x80300000);
 priceCpu.w16(0x80300318,0);priceCpu.call(0x8019dff8,0x8019e07c);
 assert.equal(priceCpu.r32(0x80300328),(level<=20?level*2:level<=40?level+20:(level-20)*3)*price);
}
console.log('PASS 495 native training price calculations, including both level boundaries and free training.');

const {FloatMips}=require('./float-mips.cjs');let xpCases=0;
for(const [image,bonus] of [[retail,0],[edited,20]])for(const training of [0,8])for(const story of [0,1])for(const manual of [0,1])for(const base of [0,1,25,99,100]) {
 const m=new FloatMips();m.load(0x801d5f38,image.subarray(0x219208,0x219444));
 m.set('sp',0x803fff00);m.set('s1',0);m.setSingle(20,base);
 m.trap(0x801c8aec,c=>c.set('v0',training));m.trap(0x801c8afc,c=>c.set('v0',story));
 m.trap(0x801cc51c,c=>c.set('v0',manual));
 m.call(0x801d5f88,0x801d6174);
 const extra=training?bonus:story?0:1;
 assert.equal(m.get('v0'),Math.min(100,base+extra*(manual?2:1)));xpCases++;
 // Existing eligibility gates still short-circuit with zero award.
 for(const blocked of [0,1,2,3,4]){
  m.r.fill(0);m.set('sp',0x803fff00);m.set('s6',0x80310000);m.setSingle(20,0);
  m.w8(0x80310031,blocked===4?99:10);
  [0x801c8b84,0x801c8e30,0x801c8c74,0x801c8e9c].forEach((a,i)=>m.trap(a,c=>c.set('v0',i===0?blocked!==0:blocked===i)));
  m.call(0x801d5f38,0x801d6174);assert.equal(m.get('v0'),0);xpCases++;
 }
}
console.log(`PASS ${xpCases} training XP decisions, normal/story behavior, Manual of Warfare, cap and eligibility exits.`);
