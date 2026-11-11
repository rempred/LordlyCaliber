'use strict';
// Execute the native battle playback consumer (event11), including the real
// inverse spell/name readers. Rendering, actor discovery and timing are sinks.
const fs=require('node:fs'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const h=rom.subarray(0x27c5300,0x27c5360),records=h.readUInt32BE(28),settings=h.readUInt32BE(32);
const m=new FloatMips();m.ram=new Uint8Array(0x800000);
m.load(0x80070000,rom.subarray(0x400,0x2f000));
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
const p=rom.subarray(0x5dc80,0x5dd00),base=p.readUInt32BE(24),at=p.readUInt32BE(12),end=p.readUInt32BE(48);
m.load(base,rom.subarray(at,at+end-base));
const stack=0x803ff000,actor=0x803c0000,stream=0x803d0000;
m.set('sp',stack);m.w32(0x801ce8c0,stream);
function call(pc,args=[]){args.forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));m.call(pc);assert.equal(m.get('sp'),stack);return m.get('v0');}
function text(at){let result='';while(m.r8(at)){assert(result.length<512);result+=String.fromCharCode(m.r8(at++));}return result;}
let displayed='',logged=-1;
m.trap(0x801c8fe8,c=>c.set('v0',actor));
m.trap(0x801c8b54,c=>c.set('v0',0)); // training/event predicate
m.trap(0x801c8b84,c=>c.set('v0',1)); // caster side
m.trap(0x2267a8+0x7ffbcd30,c=>{assert.equal(c.get('a1'),14);logged=c.get('a2');});
m.trap(0x1f2bc0+0x7ffbcb70,c=>{displayed=text(c.get('a0'));});
m.trap(0x21c970+0x7ffbcd30,c=>c.set('v0',32));
const rows=[];
// Include a renamed/replaced retail Gaze as well as independent new attacks.
// The retail name token comes from the actual exported forward reader.
for(const id of [25,...Array.from({length:Math.max(0,h.readUInt32BE(36)-159)},(_,i)=>159+i)]) {
 if(m.r8(records+id*16+4)<2)continue;
 // The copied spell produces a direct-name token while its effect program
 // remains its retail animation. Test the same command bytes as that producer.
 const token=id>=159?id:call(0x801bdfbc,[id]);
 m.w32(stream+4,0);m.w32(stream+0x810,0);m.w8(stream+0x32,255);
 m.load(stream+0x10,Buffer.from([11,8,0,0,0,0,token,0,0,0]));
 displayed='';logged=-1;
 call(0x21ebbc+0x7ffbcd30);
 const expected=text(m.r32(records+id*16));
 assert.equal(displayed,expected,`battle name ${id}`);assert.equal(logged,id);
 rows.push(`${id}: ${displayed} (visual donor ${m.r8(settings+id*32+1)})`);
}
assert(rows.length>0);
console.log('PASS native event11 battle labels and action-log IDs: '+rows.join('; '));
