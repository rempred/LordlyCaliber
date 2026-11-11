'use strict';
// Execute native caption setup and the complete native frame callback through
// its image selection. Heap/window creation and final RDP draws are controlled;
// the text-to-ID scan, wrapper state and selected pixel bytes are real ROM code.
const assert=require('node:assert/strict');
const {AttackPlaybackMachine}=require('./attack-playback-machine.cjs');
const h=new AttackPlaybackMachine(process.argv[2]),m=h.m;
h.reset({action:25,retail:true,single:true,identity:73,current:73});
for(const pc of [0x801af730,0x801be034])m.traps.delete(pc);
const base=h.rom.readUInt32BE(0x27c5310),table=m.r32(base+20);
assert(table,'caption allocation present');
let heap=0x80600000,reuse=0,windows=[],drawn=[];
m.w8(0x800e9c0f,1);m.w32(0x800e9ba0,0x80500000);m.w32(0x801d0830,0x80520000);
m.trap(0x80070f30,c=>{const p=heap;heap+=(c.get('a0')+15)&~15;c.load(p,new Uint8Array(c.get('a0')));c.set('v0',p);});
m.trap(0x8017c074,c=>{const p=reuse||heap;reuse=0;heap+=256;c.load(p,new Uint8Array(256));c.w32(p+0x2c,202);c.w32(p+0x30,210);windows.push(p);c.set('v0',p);});
m.trap(0x8017c2fc,()=>{});
const border=0x80521000;m.w8(border+3,0);m.w16(border+4,24);m.w16(border+6,24);
m.trap(0x801ad60c,c=>c.set('v0',border));
m.trap(0x801b594c,()=>{});
m.trap(0x801b5cac,c=>drawn.push(c.get('a0')));
let selectedElements=[];
const execute=m.exec.bind(m);m.exec=(pc,next)=>{if(pc===h.e.lc_original_caption_element)selectedElements.push(m.get('a0'));return execute(pc,next);};
let fallback=[];m.trap(h.e.lc_original_caption_image,c=>{fallback.push(c.get('a0'));c.set('v0',border);});
const count=m.r32(base+12),ids=[25,...Array.from({length:count-159},(_,i)=>159+i)];
let n=0;
for(const side of [0,1])for(const id of ids) {
 const expected=m.r32(table+id*4);if(!expected)continue;
 h.call('lc_attack_caption_name',[id,side]);const node=windows.at(-1);
 drawn=[];selectedElements=[];h.call(0x801af1cc,[node]);assert.equal(selectedElements.at(-1),id,'palette uses actual action');assert.equal(drawn.at(-1),expected,`image for action ${id}, side ${side}`);
 assert.equal(m.r16(drawn[0]+4),90);assert.equal(m.r16(drawn[0]+6),15);n++;
}
// Keep both side labels alive and interleave drawing, including identical names.
h.call('lc_attack_caption_name',[159,0]);const left=windows.at(-1);
h.call('lc_attack_caption_name',[160,1]);const right=windows.at(-1);
for(const [node,id] of [[left,159],[right,160],[left,159]]) {drawn=[];selectedElements=[];h.call(0x801af1cc,[node]);assert.equal(drawn[0],m.r32(table+id*4));assert.equal(selectedElements.at(-1),id);}
assert.notEqual(m.r32(table+159*4),m.r32(table+160*4));
reuse=right;h.call('lc_attack_caption_name',[159,0]);drawn=[];h.call(0x801af1cc,[right]);assert.equal(drawn[0],m.r32(table+159*4),'recycled opposite-side window');
// Unedited attacks retain the native art lookup; captions are not globally replaced.
h.call('lc_attack_caption_name',[55,1]);fallback=[];h.call(0x801af1cc,[windows.at(-1)]);assert.equal(fallback.length,1);
// A permitted long name must not overwrite the adjacent side's native buffer.
const long=0x80530000;m.load(long,Buffer.from('A'.repeat(120)+'\0'));
m.w32(h.records+25*16,long);m.w8(h.owner+0x5704,0x42);
h.call('lc_attack_caption_name',[25,1]);assert.equal(m.r8(h.owner+0x5704),0x42);
drawn=[];h.call(0x801af1cc,[windows.at(-1)]);assert.equal(drawn[0],m.r32(table+25*4));
// The shared element getter must retain the native five-argument ABI outside
// caption drawing, including the halfword equipment argument on the stack.
let forwarded;
m.trap(h.e.lc_original_caption_element,c=>{forwarded=[c.get('a0'),c.get('a1'),c.get('a2'),c.get('a3'),c.r16(c.get('sp')+18)];c.set('v0',0x5a);});
assert.equal(h.call(0x8016e470,[57,0x1234,0x5678,0x9abc,0xdef0]),0x5a);
assert.deepEqual(forwarded,[57,0x1234,0x5678,0x9abc,0xdef0]);
console.log(`PASS ${n} native caption frame selections, independent same-name art, both sides, and native-art fallback`);
