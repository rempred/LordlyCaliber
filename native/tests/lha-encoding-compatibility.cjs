'use strict';
// Independent browser LH5 decoder and CRC routines consume native writer output.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'), context={Uint8Array,Uint16Array,Uint32Array,Int32Array,DataView,ArrayBuffer};
context.window=context;context.OB64={};vm.createContext(context);
for(const name of ['parsers.js','repack.js'])vm.runInContext(fs.readFileSync(path.join(root,name),'utf8'),context,{filename:name});
const fixtures=JSON.parse(fs.readFileSync(process.argv[2]));let headers=0;
for(const f of fixtures) {
  const data=Buffer.from(f.data,'hex'),packed=Buffer.from(f.packed,'hex');
  assert.deepEqual(Buffer.from(context.OB64.lh5Decompress(packed,data.length)),data);
  if(f.header) {
    const header=Buffer.from(f.header,'hex');assert.equal(header[20],2);
    assert.equal(header.readUInt32LE(7),packed.length);assert.equal(header.readUInt32LE(11),data.length);
    assert.equal(header.readUInt16LE(21),context.OB64.crc16(data));
    let filename=false;
    for(let at=24;header.readUInt16LE(at);at+=header.readUInt16LE(at)) {
      const size=header.readUInt16LE(at);assert(size>=3&&at+size+2<=header.length);
      if(header[at+2]===1) {assert.equal(header.toString('latin1',at+3,at+size),f.name);filename=true;}
      if(header[at+2]===0) {
        const copy=Buffer.from(header),crc=copy.readUInt16LE(at+3);copy.writeUInt16LE(0,at+3);
        assert.equal(context.OB64.crc16(copy),crc);headers++;
      }
    }
    assert(filename);
  }
}
console.log(`PASS independent browser decoder: ${fixtures.length} native LH5 streams and ${headers} rebuilt header CRCs; includes real squad/scenario payloads, repeated matches, dictionary boundary and multiple blocks. This checks the archive format, not the game's resource-loading path.`);
