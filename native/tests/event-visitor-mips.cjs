'use strict';
// Execute the exact retail48-byte visitor requirement fixture and edited
// variants through the original resident VM/getters. No build or emulator.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const filename=process.env.LORDLY_TEST_ROM||path.resolve(__dirname,'../../../Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64');
const rom=fs.readFileSync(filename);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
assert.equal(crypto.createHash('sha256').update(rom).digest('hex'),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
const original=Buffer.from('67fe606f6a8190026a01b2116a00660a6b1e6007e3066016ac0a602e9804bc070107603eac0460569804bc0101012f00','hex');
const m=new FloatMips(),SP=0x807ff000,LOCAL=SP-144+80,PROGRAM=0x80700000;
m.load(0x80163910,rom.subarray(0x39810,0x66e10));
// Original service1 reads formation-list bytes at801971F2+squad*25+position;
// service13 reads the flags byte immediately before that list. FE reads the
// visiting squad ID at801976DC. HP/status getters use the documented56-byte
// army records and52-byte opposing records, with status mask04=petrification.
// See docs/event-semantics/vm-reference.md, Getter and setter ABI. These native
// getters execute unchanged: no property helper or service is trapped.
const stateStart=0x80193678,stateBytes=0x4288;
const read=(at,n)=>Buffer.from(m.ram.subarray(m.phys(at),m.phys(at)+n));
const gameSeed=Buffer.alloc(stateBytes,0x5c);
let cases=0,playerCases=0,enemyCases=0;
function edit(formation,side,member,hp,petrified){
 const b=Buffer.from(original);b[5]=0;b[9]=0;b[13]=member;
 for(const [requirement,mask]of [[formation,1],[side,128]])if(requirement>=0){b[5]|=mask;if(requirement)b[9]|=mask;}
 for(const [requirement,positions]of [[hp,[24,36]],[petrified,[30,42]]])for(const at of positions){
  if(requirement<0)b[at+1]=0;
  else b[at]=requirement?0xac:0xbc;
 }
 // Every edit changes only its reviewed immediate/check fields. Both source
 // branches must carry exactly the same HP and petrification requirements.
 const allowed=new Set([5,9,13,24,25,30,31,36,37,42,43]);
 for(let i=0;i<b.length;i++)if(!allowed.has(i))assert.equal(b[i],original[i]);
 assert.equal(b[24],b[36]);assert.equal(!b[25],!b[37]);
 assert.equal(b[30],b[42]);assert.equal(!b[31],!b[43]);
 return b;
}
function execute(program,enemy,flags,aliveByPosition,stoneByPosition,entry=0){
 const squad=enemy?33:3;
 m.load(stateStart,gameSeed);m.load(PROGRAM,program);
 m.w8(0x801976dc,squad);m.w8(0x801971f1+squad*25,flags);
 for(let position=0;position<5;position++){
  const id=11+position;m.w8(0x801971f2+squad*25+position,id);
  const base=(enemy?0x80195560:0x80193bc0)+id*(enemy?52:56);
  m.w16(base+24,aliveByPosition[position]?500:0);
  m.w8(base+51,0x81|(stoneByPosition[position]?4:0));
 }
 const before=read(stateStart,stateBytes);
 for(let r=0;r<8;r++)m.w16(LOCAL+r*2,0xa5a5);
 let pc=entry,steps=0;
 while(pc!==entry+46&&pc!==entry+48){
  assert(pc>=entry&&pc<entry+46&&!(pc&1),`invalid pc ${pc}`);assert(++steps<=24);
  m.set('sp',SP);m.set('a0',PROGRAM);m.w16(0x80197b04,pc/2);
  m.call(0x80170954,0x80171b58,20000);pc=(m.r16(0x80197b04)+1)*2;
 }
 assert.deepEqual(read(stateStart,stateBytes),before,'guard/getters must preserve all seeded game state');
 assert.deepEqual(read(PROGRAM,program.length),program,'VM must not change authored instructions');
 return pc===entry+48;
}
for(const enemy of [false,true])for(const formation of [-1,0,1])for(const side of [-1,0,1])
for(const flags of [0x2a,0x2b,0xaa,0xab])for(let member=0;member<5;member++)
for(const hp of [-1,0,1])for(const petrified of [-1,0,1])
for(const alive of [false,true])for(const stone of [false,true]){
 const program=edit(formation,side,member,hp,petrified);
 // Deliberately opposite other members establish that the authored selector
 // chooses this member, rather than always checking the leader/first member.
 const positions=Array.from({length:5},(_,p)=>p===member);
 const actual=execute(program,enemy,flags,positions.map(p=>p?alive:!alive),positions.map(p=>p?stone:!stone));
 const mask=(formation<0?0:1)|(side<0?0:128),value=(formation===1?1:0)|(side===1?128:0);
 const expected=(flags&mask)===value&&(hp<0||alive===!!hp)&&(petrified<0||stone===!!petrified);
 assert.equal(actual,expected,JSON.stringify({enemy,formation,side,flags,member,hp,petrified,alive,stone}));
 cases++;if(enemy)enemyCases++;else playerCases++;
}
console.log(JSON.stringify({cases,playerCases,enemyCases,fixtureBytes:original.length,nativeGetters:'untrapped',gameStatePreserved:true}));
console.log('PASS original native visitor guard: formation/side masks, all five member selectors, HP and petrification requirements including Either, and both army/opposing branches. Uses an exact standalone retail signature plus edited variants; no exported event program, scheduler, cutscene or full mission is exercised.');
const directory=process.argv[2];
if(directory){
 const files=fs.readdirSync(directory).filter(n=>n.endsWith('.v64'));assert.equal(files.length,1,'candidate directory must contain exactly one .v64');
 const candidate=fs.readFileSync(path.join(directory,files[0]));
 if(candidate.readUInt32BE(0)===0x37804012)candidate.swap16();assert.equal(candidate.readUInt32BE(0),0x80371240);
 const program=fs.readFileSync(path.join(directory,'event-10.bin')),intended=edit(1,0,2,0,-1);
 const entry=program.indexOf(intended);
 assert(entry>=0,'exported visitor with intended controls is missing');
 assert.equal(program.indexOf(intended,entry+1),-1,'intended exported visitor must be unique');
 assert.deepEqual(program.subarray(entry,entry+48),intended,
  'exported visitor must require present friendly squad, third member, zero HP, either petrification');
 // Load the exported resident code and full exported event program. The bytes
 // actually executed are never regenerated or rewritten by this probe.
 m.load(0x80163910,candidate.subarray(0x39810,0x66e10));
 let exportedCases=0;
 for(const enemy of [false,true])for(const flags of [0x2a,0x2b,0xaa,0xab])
 for(let hpBits=0;hpBits<32;hpBits++)for(let statusBits=0;statusBits<32;statusBits++){
  const alive=Array.from({length:5},(_,p)=>!!(hpBits&(1<<p))),stone=Array.from({length:5},(_,p)=>!!(statusBits&(1<<p)));
  const actual=execute(program,enemy,flags,alive,stone,entry);
  assert.equal(actual,(flags&129)===1&&!alive[2],JSON.stringify({enemy,flags,hpBits,statusBits}));
  exportedCases++;
 }
 console.log(JSON.stringify({exportedCases,program:10,guardByteOffset:entry,member:2,hp:'zero',petrification:'either',gameStatePreserved:true}));
 console.log('PASS exported candidate visitor bytes in exported native VM: all32 member-HP patterns and32 petrification patterns, both roster branches and all formation/side combinations. No scheduler, cutscene or full mission is exercised.');
}
