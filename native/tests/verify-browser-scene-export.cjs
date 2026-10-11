// Independently read the packaged export with the browser codec and compare its
// decoded program to the fixture produced by the browser's own compiler.
const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert/strict');
global.window=global;global.OB64={};
vm.runInThisContext(fs.readFileSync(path.join(__dirname,'../../cutscene-codec.js'),'utf8'));
const raw=fs.readFileSync(process.argv[2]);
const rom=raw[0]===0x37?Uint8Array.from(raw,(_,i)=>raw[i^1]):raw[0]===0x40?Uint8Array.from(raw,(_,i)=>raw[(i&~3)|(3-(i&3))]):Uint8Array.from(raw);
const view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),key=view.getUint32(0x1f3ca88+91*4),at=key+0x594280,size=view.getUint32(at);
const actual=OB64.cutsceneCodec.decodeCustomLz(rom.slice(at+4,at+4+size)).bytes;
const expected=Buffer.from(JSON.parse(fs.readFileSync(process.argv[3],'utf8')).expectedProgram,'hex');
assert.deepEqual(Buffer.from(actual),expected);
console.log(`PASS: packaged browser import/export matches ${actual.length} expected scene-program bytes.`);
