'use strict';
// Execute an exported authored custom-flag tree through the native VM service
// table. No source compilation, ROM writes, emulator or UI operations occur.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const directory=process.argv[2];if(!directory)throw Error('Supply the custom-flags export test directory');
const files=fs.readdirSync(directory).filter(name=>name.toLowerCase().endsWith('.v64'));
assert.equal(files.length,1,'candidate directory must identify one ROM');
const rom=fs.readFileSync(path.join(directory,files[0]));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(rom.readUInt32BE(0),0x80371240);
const script=fs.readFileSync(path.join(directory,'event-4.bin'));
const descriptor=0x5dc80;
assert.equal(rom.toString('ascii',descriptor,descriptor+8),'LC05CLAS');
assert.equal(rom.readUInt32BE(descriptor+8),6,'custom flag runtime descriptor');
const dataOffset=rom.readUInt32BE(descriptor+12),dataSize=rom.readUInt32BE(descriptor+16);
const dataAddress=rom.readUInt32BE(descriptor+24),discovery=rom.readUInt32BE(descriptor+44),end=rom.readUInt32BE(descriptor+48);
assert.equal(dataAddress+dataSize,end);assert(dataOffset+dataSize<=rom.length);
const metadata=rom.subarray(dataOffset+dataSize-32,dataOffset+dataSize);
assert.equal(metadata.toString('ascii',0,8),'LCFLAGS1');
assert.equal(metadata.readUInt32BE(20),256);assert.equal(metadata.readUInt32BE(24),512);
const bank=metadata.readUInt32BE(8),getter=metadata.readUInt32BE(12),setter=metadata.readUInt32BE(16);
assert(bank>=discovery+32&&bank+64<=end-32&&bank%16===0,'separate bounded custom bank');
assert.deepEqual(rom.subarray(dataOffset+bank-dataAddress,dataOffset+bank-dataAddress+64),Buffer.alloc(64));
assert.equal(rom.readUInt32BE(0x650e0),getter);assert.equal(rom.readUInt32BE(0x65150),setter);
const residentEnd=0x80187f00+rom.readUInt32BE(descriptor+84);
assert(getter>=0x80187f00&&getter<residentEnd);assert(setter>=0x80187f00&&setter<residentEnd);
const group=script.readUInt16BE(6);assert(group+2<=script.length);
const entry=script.readUInt16BE(group);assert(entry!==65535&&entry<script.length&&entry%2===0);
const SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000,NATIVE=0x80193686;
const bytes=(m,address,length)=>Buffer.from(m.ram.subarray(m.phys(address),m.phys(address)+length));
function setBit(buffer,id,value){const index=id-256,mask=1<<(index&7);buffer[index>>3]=(buffer[index>>3]&~mask)|(value?mask:0);}
let cases=0,totalReads=0,totalWrites=0;
for(const target of [29,30,31])for(const input of [0,1])for(const initialOutput of [0,1])for(const pattern of [0,0xa5,255]){
 class ObservedMips extends FloatMips {
  constructor(){super();this.reads=[];this.writes=[];}
  exec(pc,next){
   if(pc===getter)this.reads.push(this.get('a0'));
   if(pc===setter)this.writes.push([this.get('a0'),this.get('a2')]);
   return super.exec(pc,next);
  }
 }
 const m=new ObservedMips();
 m.load(0x80163910,rom.subarray(0x39810,0x66e10));
 m.load(dataAddress,rom.subarray(dataOffset,dataOffset+dataSize));m.load(PROGRAM,script);
 for(let r=0;r<8;r++)m.w16(LOCAL+r*2,0xa5a5);
 const nativeBefore=Buffer.from(Array.from({length:32},(_,i)=>(pattern+i*37)&255));
 const customBefore=Buffer.from(Array.from({length:64},(_,i)=>(pattern+i*53)&255));
 setBit(customBefore,256,input);setBit(customBefore,767,initialOutput);
 m.load(NATIVE,nativeBefore);m.load(bank,customBefore);m.w8(0x801976e8,target);m.w8(0x80197b00,3);
 let pc=entry,ended=false;const trace=[];
 for(let n=0;n<300;n++){
  assert(pc>=0&&pc+2<=script.length&&pc%2===0,'native cursor outside event row');trace.push(pc);
  m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
  if(script[pc]===0x2f){m.call(0x80170954);ended=true;break;}
  m.call(0x80170954,0x80171b58,20000);pc=(m.r16(0x80197b04)+1)*2;
 }
 const context=JSON.stringify({target,input,initialOutput,pattern,trace});assert(ended,'tree failed to terminate '+context);
 const expected=Buffer.from(customBefore);if(target===30)setBit(expected,767,input);
 assert.deepEqual(bytes(m,bank,64),expected,'only custom output767 may change '+context);
 assert.deepEqual(bytes(m,NATIVE,32),nativeBefore,'native flags must remain intact '+context);
 assert.deepEqual(m.reads,target===30?[256]:[],'VM must invoke the installed custom getter '+context);
 assert.deepEqual(m.writes,target===30?[[767,input]]:[],'VM must invoke the installed custom setter '+context);
 assert.equal(m.get('sp'),SP,'native VM stack restored');
 totalReads+=m.reads.length;totalWrites+=m.writes.length;cases++;
}
console.log(`PASS ${cases} fresh exported event cases: target29/30/31 guards, custom flag256 tests, THEN/ELSE flag767 writes, ${totalReads} installed getter calls, ${totalWrites} installed setter calls, all native and unrelated custom bits preserved.`);
console.log('Instruction-level exported VM coverage; full mission and console playback are outside this test.');
