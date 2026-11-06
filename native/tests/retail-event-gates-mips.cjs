'use strict';
// Executes the compiled branch helper and the original resident interpreter.
// The fixture is emitted by retail_event_gate_tests using the private US Rev0
// program and production Compiler. No emulator or source ROM is modified.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const folder=process.env.LORDLY_GATE_FIXTURE||process.argv[2];
if(!folder)throw Error('Set LORDLY_GATE_FIXTURE to the generated fixture folder');
const metadata=JSON.parse(fs.readFileSync(path.join(folder,'gate.json'),'utf8'));
const resident=fs.readFileSync(path.join(folder,'resident.bin'));
const program=fs.readFileSync(path.join(folder,'program.bin'));
const SP=0x807ff000,REGISTERS=SP-144+80,STACK=SP-144+16,PROGRAM=0x80700000;
let checks=0;
for(const enabled of [false,true])for(const originalPasses of [false,true]) {
    const m=new FloatMips();m.load(0x80163910,resident);m.load(PROGRAM,program);
    m.w8(0x80193686+(metadata.flag>>3),enabled?1<<(metadata.flag&7):0);
    const values=[0xa135,originalPasses?6:5,6,0xa486,0xa597,0xa6a8,0xa7b9,0xa8ca];
    for(let i=0;i<8;i++)m.w16(REGISTERS+i*2,values[i]);
    const savedStack=Buffer.alloc(64);for(let i=0;i<64;i++)savedStack[i]=(i*7+3)&255;m.load(STACK,savedStack);
    let pc=metadata.pc;
    for(let ticks=0;ticks<1000;ticks++) {
        m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
        m.call(0x80170954,0x80171b58,50000);
        pc=(m.r16(0x80197b04)+1)*2;
        if(pc===metadata.yes||pc===metadata.no)break;
    }
    assert.equal(pc,enabled&&originalPasses?metadata.yes:metadata.no);
    for(let i=0;i<8;i++)assert.equal(m.r16(REGISTERS+i*2),values[i],`register R${i} changed`);
    assert.deepEqual(Buffer.from(m.ram.subarray(STACK&0x7fffff,(STACK&0x7fffff)+64)),savedStack,'native VM stack changed');
    assert.equal(m.get('sp'),SP-144,'interpreter frame changed at increment boundary');
    checks++;
}
for(const invalid of ['signature','selector','destination']) {
    const bytes=Buffer.from(program),table=bytes.readUInt16BE(0),directory=table-1028;
    if(invalid==='signature')bytes[directory]=0;
    if(invalid==='selector')bytes[metadata.pc+1]=255;
    if(invalid==='destination')bytes.writeUInt16BE(65535,directory+4+bytes[metadata.pc+1]*4);
    const m=new FloatMips();m.load(0x80163910,resident);m.load(PROGRAM,bytes);
    for(let i=0;i<8;i++)m.w16(REGISTERS+i*2,0x1200+i);
    m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,metadata.pc/2);
    m.call(0x80170954,0x80171b58,50000);
    assert.equal((m.r16(0x80197b04)+1)*2,metadata.pc+2,`invalid ${invalid} did not preserve the native no-op`);
    for(let i=0;i<8;i++)assert.equal(m.r16(REGISTERS+i*2),0x1200+i);
}
console.log(`Passed ${checks} original/inserted outcome combinations with all eight registers and all 32 native VM stack words preserved.`);
console.log('Invalid signatures, unused selectors and out-of-range destinations preserve the native no-op.');
