'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),zlib=require('node:zlib');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.set('sp',0x803f0000);
m.load(0x80070c00,rom.subarray(0x1000,0x2f000));
m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
const dir=process.argv[2],payload=0x80350000,descriptor=0x80187444;
if(process.argv[3]==='verify'){
 const test=JSON.parse(fs.readFileSync(path.join(dir,'verify.json'),'utf8'));
 m.load(payload,Buffer.from(test.payload,'hex'));m.set('a0',payload);m.set('a1',descriptor);
 m.call(0x800742f4,0x80000100,5000000);
 for(const r of test.values){let n=0;for(let i=0;i<r.bytes;++i)n=(n*256+m.r8(0x80000000+r.address+i))>>>0;assert.equal(n,r.value,JSON.stringify(r));}
 for(const date of test.dates)for(const last of [false,true]){
  const at=0x80310000;m.w16(at,date.year);m.w8(at+2,date.month);m.w8(at+3,date.days-(last?0:1));
  m.set('a0',at);m.call(0x8016c5bc);
  assert.equal(m.r8(at+3),last?1:date.days);
  assert.equal(m.r8(at+2),last?(date.month===15?1:date.month+1):date.month);
  assert.equal(m.r16(at),last&&date.month===15?Math.min(date.year+1,998):date.year);
 }
 const rzip=fs.readFileSync(path.join(dir,'edited.state'));
 let chunks=[],at=20;
 while(at<rzip.length){const n=rzip.readUInt32LE(at);at+=4;chunks.push(zlib.inflateSync(rzip.subarray(at,at+n)));at+=n;}
 const state=Buffer.concat(chunks);assert.equal(state.length,Number(rzip.readBigUInt64LE(12)));
 const memory=state.subarray(0,0x800000).swap32();assert.equal(memory.readUInt16BE(0x193bf8+24),0);
 assert.deepEqual(state.subarray(0x800000),Buffer.alloc(123,0x59));
 console.log(JSON.stringify({originalDecoderValues:test.values.length,calendarBoundaries:test.dates.length*2,independentRzipDecode:'pass'}));
}else{
 for(let id=1;id<100;++id){
  const at=0x80193bc0+id*56;
  const name=Buffer.alloc(13);Buffer.from('Unit'+id,'ascii').forEach((c,i)=>name[i]=c+16);name[('Unit'+id).length]=255;m.load(at,name);
  m.w8(at+17,id);m.w8(at+18,164-id);m.w8(at+19,80);m.w8(at+20,6);m.w8(at+21,0xab);
  m.w16(at+22,999);m.w16(at+24,600);m.w8(at+26,4);m.w8(at+27,0);m.w8(at+40,100);
  for(let off=28;off<=38;off+=2)m.w16(at+off,777);
  for(let off=42;off<=48;off+=2)m.w16(at+off,277);
  m.w8(at+50,75);m.w8(at+51,3);m.w8(at+52,id);m.w8(at+53,99);
 }
 m.w32(0x80196a6c,1000);m.w8(0x801936a9,0);
 m.w16(0x80196a30,251);m.w8(0x80196a32,4);m.w8(0x80196a33,1);
 m.w16(0x80196b00,277);m.w8(0x80196b03,127);
 m.w16(0x80193ac0,44);m.w16(0x80193ac2,127);
 m.set('a0',payload);m.set('a1',descriptor);m.call(0x80074494,0x80000100,5000000);
 const p=Buffer.from(m.ram.slice(m.phys(payload),m.phys(payload)+6180));p[p.length-1]|=0x55;
 fs.writeFileSync(path.join(dir,'payload.bin'),p);
 // Raw state names use ASCII and retain all 17 bytes, including a longer name.
 for(let id=1;id<100;++id){const at=0x80193bc0+id*56;const n=Buffer.alloc(17);n.write(id===1?'SixteenCharName!!':'Unit'+id);m.load(at,n);}
 const ram=Buffer.from(m.ram);fs.writeFileSync(path.join(dir,'ram.bin'),ram);
 const state=Buffer.concat([Buffer.from(ram).swap32(),Buffer.alloc(123,0x59)]);
 const chunkSize=0x20000,head=Buffer.alloc(20);Buffer.from('23525a4950760123','hex').copy(head);head.writeUInt32LE(chunkSize,8);head.writeBigUInt64LE(BigInt(state.length),12);
 const parts=[head];for(let at=0;at<state.length;at+=chunkSize){const compressed=zlib.deflateSync(state.subarray(at,at+chunkSize)),len=Buffer.alloc(4);len.writeUInt32LE(compressed.length);parts.push(len,compressed);}
 fs.writeFileSync(path.join(dir,'original.state'),Buffer.concat(parts));
 fs.writeFileSync(path.join(dir,'raw.state'),state);
 console.log('Original-MIPS packed fixture and independently compressed states created');
}
