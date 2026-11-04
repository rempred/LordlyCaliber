'use strict';
// Read-only native lookup checks. No console/emulator is started. Resource I/O
// and release are traps; the world-map ID selection and record scan are retail
// instructions. Pass a newly exported ROM to cover relocated text as well.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const image=fs.readFileSync(process.argv[2]);
if(image.readUInt32BE(0)===0x37804012)image.swap16();
assert.equal(image.readUInt32BE(0),0x80371240);
const parser=fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8');
const begin=parser.indexOf('OB64.lzssDecode = function'),end=parser.indexOf('\n};',begin)+3,codec={OB64:{}};
vm.runInNewContext(parser.slice(begin,end),codec);
const key=image.readUInt32BE(0x66d78),at=0x594280+key;
assert.ok(at+image.readUInt32BE(at)+4<=image.length);
const text=Buffer.from(codec.OB64.lzssDecode(image,at+8,image.readUInt32BE(at+4)).output);
const rows=[];for(let start=0;start<text.length;){const end=text.indexOf('\r\n',start);assert.ok(end>=start);rows.push(text.subarray(start,end));start=end+2;}
assert.equal(rows.length,48);
function machine(){
 const m=new Mips();m.load(0x80163910,image.subarray(0x39810,0x66e10));
 m.load(0x8019a7a0,image.subarray(0x79730,0x87200));
 const header=image.lastIndexOf('LCCAMP01');
 if(header>=0)m.load(0x8019a7a0,image.subarray(image.readUInt32BE(header+12),image.readUInt32BE(header+12)+image.readUInt32BE(header+16)));
 m.set('sp',0x807ff000);return m;
}
for(let location=1;location<=40;location++){
 const m=machine(),context=0x80700000;let selector;
 m.set('v0',context);m.w16(context+0x1da,0);m.w8(context+0x1de,location);
 m.trap(0x8019e294,()=>{selector=m.get('a0');m.set('ra',0x80000100);});
 m.call(0x8019b0c0);assert.equal(selector,0x3b000|(location-1),'native location ID selects its own description');
}
for(let record=0;record<rows.length;record++){
 const m=machine(),output=0x80710000;
 m.trap(0x8009dd38,()=>{assert.equal(m.get('a0'),key);m.load(0x80600000,text);m.set('v0',0x80600000);});
 m.trap(0x800712c4,()=>{});
 m.load(output,Buffer.alloc(0x502,0xa5));
 m.set('a0',output);m.set('a1',m.r32(0x80190e4c+11*4));m.set('a2',record);
 m.call(0x8017faf8,0x80000100,1000000);
 assert.equal(m.get('v0'),output+rows[record].length);
 assert.deepEqual(Buffer.from(m.ram.subarray(m.phys(output),m.phys(output)+rows[record].length+2)),Buffer.concat([rows[record],Buffer.from('\r\n')]));
 assert.equal(m.r8(output+rows[record].length+2),0xa5);
 assert.equal(m.get('sp'),0x807ff000);
}
console.log('Location descriptions: 40 native location selectors and 48 native record reads passed. Rendering and full-game loading are not simulated.');
