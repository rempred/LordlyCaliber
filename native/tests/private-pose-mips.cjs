const fs=require('fs'),assert=require('assert/strict'),{FloatMips}=require('./float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new FloatMips();m.ram=new Uint8Array(0x400000);
m.load(0x8022d7d0,rom.subarray(0x29d000,0x2a8d00));m.load(0x801bdb70,rom.subarray(0x201000,0x212f00));
const candidate=fs.readFileSync(process.argv[3]);if(candidate.readUInt32BE(0)===0x37804012)candidate.swap16();
assert.equal(candidate.subarray(0x27c5000,0x27c5008).toString(),'LC05POSE');
const mapCount=candidate.readUInt32BE(0x27c500c),query=candidate.readUInt32BE(0x27c5014);
const baseline=process.argv[4]==='baseline';
const wide=candidate.readUInt32BE(0x27c5010)!==0;
assert.ok(baseline?mapCount===0:mapCount>=2&&mapCount<=256);
m.load(0x8022e9e8,candidate.subarray(0x29e218,0x29e70c));
m.load(0x801c1aa4,candidate.subarray(0x204f34,0x2050ac));m.load(0x801ceee0,candidate.subarray(0x212370,0x2123a0));
assert.equal(m.r8(0x801ceee0+22),wide?3:0);
m.load(0x8009c970,rom.subarray(0x2cd70,0x2cda0));
const actor=0x80300010,program=0x80310010,table=0x80320010,sp=0x803ffe00;
m.w32(0x801d0690,table);
const mapping=Buffer.alloc(mapCount*8);
if(!baseline)Buffer.from('8787006401000000a1130064ffff0000','hex').copy(mapping);
for(let i=2;i<mapCount;i++){mapping.writeUInt32BE(((200<<24)|((i&255)<<16)|100)>>>0,i*8);mapping.writeUInt16BE(65535,i*8+4);}
m.load(table-mapping.length,mapping);
m.trap(0x801be138,c=>c.set('v0',1));m.trap(0x801c2ce4,c=>c.set('v0',program));
const rows=Array.from({length:258},(_,i)=>Buffer.from([1,1,i&255,2]));
rows[100]=Buffer.from([2,1,9,4,5,100]);rows[256]=Buffer.from([2,1,42,2,22,0,1]);rows[257]=Buffer.from([2,22,0,1,0]);
if(!wide)rows[256]=Buffer.from([2,1,42,2,4,0]);
const header=Buffer.alloc(rows.length*4);let pos=header.length;rows.forEach((p,i)=>{header.writeUInt32BE(pos,i*4);pos+=p.length;});const bank=Buffer.concat([header,...rows]);
m.load(program-16,Buffer.alloc(bank.length+32,0xa7));m.load(program,bank);
let cases=0;
function reset(source,owner,state,flags=0){m.load(actor-16,Buffer.alloc(368,0xa7));m.load(actor,Buffer.alloc(336));m.w32(actor+0xe8,source);m.w32(actor+0xec,owner);m.w16(actor+0x134,state);m.w32(actor+0xf0,0xffffffff);m.w8(actor+0x13d,1);m.w8(actor+0x146,flags>>1);m.w16(actor+0x13a,flags&1);m.set('sp',sp);}
function tick(){m.set('a0',actor);m.call(0x8022e9e8,0x80000100,200000);assert.equal(m.get('sp'),sp);for(let i=0;i<16;i++){assert.equal(m.r8(actor-16+i),0xa7);assert.equal(m.r8(actor+336+i),0xa7);assert.equal(m.r8(program-16+i),0xa7);assert.equal(m.r8(program+bank.length+i),0xa7);}cases++;}
for(const [source,owner,flags,wantState,wantFrame] of [[135,135,0,100,9],[161,19,0,100,9],[135,135,1,50,50],[135,134,0,50,50],[136,136,0,50,50],[10,10,0,100,9]]){reset(source,owner,100,flags);const clamped=baseline&&[135,136,161].includes(source);for(let i=0;i<9;i++){tick();assert.equal(m.r16(actor+0x134),clamped?50:wantState);assert.equal(m.r32(actor+0xf8),clamped?50:wantFrame);}}
for(const start of baseline?[]:wide?[256,257]:[256]){reset(10,10,start);for(let i=0;i<9;i++){tick();assert.equal(m.r16(actor+0x134),256);assert.equal(m.r32(actor+0xf8),42);if(i&&wide)assert.equal(m.r16(actor+0x136),256);}}
for(const flags of [0,1]){reset(135,135,256,flags);for(let i=0;i<9;i++){tick();assert.equal(m.r16(actor+0x134),!baseline&&!flags?256:50);assert.equal(m.r32(actor+0xf8),!baseline&&!flags?42:50);}}
for(const tuple of [[135,135,0,100],[161,19,0,100],[135,135,0,101],[135,135,1,100],[-1,135,0,100],[135,256,0,100],[135,135,0,256],[135,135,-1,100],[135,135,0,65535]]){tuple.forEach((v,i)=>m.set('a'+i,v));m.set('sp',sp);m.call(query);assert.equal(m.get('v0'),!baseline&&(tuple[0]===135&&tuple[1]===135&&tuple[2]===0&&[100,256].includes(tuple[3])||tuple[0]===161&&tuple[1]===19&&tuple[2]===0&&tuple[3]===100)?1:0);cases++;}
console.log((baseline?'Restored pose policy: ':'Private pose extension: ')+cases+' fixed-policy, transfer and registration-domain cases passed in 4 MiB modeled RAM.');

// The record getter also serves callers which request only some operands.
const instructions=[];
for(let op=0;op<22;op++){const bytes=Buffer.alloc(rom[0x212370+op],op);bytes[0]=op;instructions.push(bytes);}
const all=Buffer.concat([Buffer.from([0,0,0,4,22]),...instructions]);
m.load(program,all);
function readOperands(code,cursor,mask,alias){
 m.load(0x801c1aa4,code);m.load(0x801ceee0,rom.subarray(0x212370,0x2123a0));
 const out=0x80360010;m.load(out-16,Buffer.alloc(48,0xa7));m.set('sp',sp);
 for(let i=0;i<4;i++)m.set('a'+i,0);
 m.w32(sp+16,0);m.w32(sp+20,cursor);
 for(let i=0;i<3;i++)m.w32(sp+24+i*4,mask&(1<<i)?out+(alias?0:i):0);
 m.call(0x801c1aa4);assert.equal(m.get('sp'),sp);
 return [m.get('v0'),Buffer.from(m.ram.subarray(m.phys(out-16),m.phys(out+32))).toString('hex')];
}
let reads=0;
for(let cursor=-2;cursor<=24;cursor++)for(let mask=0;mask<8;mask++)for(const alias of [false,true]){
 const original=readOperands(rom.subarray(0x204f34,0x2050ac),cursor,mask,alias),
       rebuilt=readOperands(candidate.subarray(0x204f34,0x2050ac),cursor,mask,alias);
 assert.deepEqual(rebuilt,original,'Reader cursor='+cursor+' mask='+mask+' alias='+alias);reads++;
}
console.log('Class reader: '+reads+' original-instruction comparisons for opcode widths, missing/shared output pointers and invalid record indices passed.');
