'use strict';
// Instruction-level battle pipeline. Native command insertion, ordering, seek,
// result consumption and HP/status playback execute from the exported ROM.
// The fixture supplies actors/scores and finite pose/effect contact schedules;
// it does not emulate the renderer, audio, a whole console or battle AI.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
function symbols(directory,name) {
 const b=fs.readFileSync(path.join(directory,name+'.elf')),out={};
 const at=b.readUInt32BE(32),width=b.readUInt16BE(46),count=b.readUInt16BE(48);
 for(let i=0;i<count;i++) {
  const h=at+i*width;if(b.readUInt32BE(h+4)!==2)continue;
  const names=b.readUInt32BE(at+b.readUInt32BE(h+24)*width+16),begin=b.readUInt32BE(h+16),size=b.readUInt32BE(h+20);
  for(let j=begin;j<begin+size;j+=16) {const n=names+b.readUInt32BE(j);out[b.toString('utf8',n,b.indexOf(0,n))]=b.readUInt32BE(j+4);}
 }
 return out;
}
class AttackPlaybackMachine {
 constructor(file) {
  const rom=fs.readFileSync(file);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  this.rom=rom;this.e={};
  for(const name of ['attack_strikes','attack_runtime','attack_playback','attack_generic','attack_combined','attack_pedra'])
   Object.assign(this.e,symbols(path.dirname(file),name));
  const m=this.m=new FloatMips();
  const execute=m.exec.bind(m);m.exec=(pc,next)=>{try{return execute(pc,next);}catch(error){error.message+=` at PC 0x${pc.toString(16)}`;throw error;}};
  m.load(0x80070000,rom.subarray(0x400,0x2f000));
  m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));
  m.load(0x801bdfa0,rom.subarray(0x201430,0x212590));
  m.load(0x801cf2c0,rom.subarray(0x212590,0x23a900));
  const cp=0x5dc80,base=rom.readUInt32BE(cp+24),start=rom.readUInt32BE(cp+12),end=rom.readUInt32BE(cp+48);
  m.load(base,rom.subarray(start,start+end-base));
  if(rom.readUInt32BE(0x27a0004)===23) {
   const sites=JSON.parse(fs.readFileSync(path.join(__dirname,'../game/tools/high-attack.json'),'utf8')).sites;
   for(const site of sites.filter(s=>['loader','continuation','module'].includes(s.name)))
    m.load(site.ram,rom.subarray(site.rom,site.rom+site.size));
  }
  this.records=rom.readUInt32BE(0x27c5300+28);this.settings=rom.readUInt32BE(0x27c5300+32);
  this.stack=0x807ff000;this.scratch=0x80710000;this.trace=[];
  this.phase='setup';this.wall=0;this.impacts=[];this.presentations=[];
  this.installServices();this.initial=Uint8Array.from(m.ram);
 }
 actor(id){return this.owner+0x1c4+id*0xf8;}
 get owner(){return this.m.r32(0x801ce8bc);}
 get stream(){return this.m.r32(0x801ce8c0);}
 get tail(){return this.stream-(this.stream===0x80450000?0x7000:0);}
 id(address){return (address-this.owner-0x1c4)/0xf8;}
 call(name,args=[],limit=5000000) {
  const m=this.m,pc=typeof name==='number'?name:this.e[name];assert(pc,`Missing entry ${name}`);
  args.slice(0,4).forEach((v,i)=>m.set(['a0','a1','a2','a3'][i],v));
  args.slice(4).forEach((v,i)=>m.w32(this.stack+16+i*4,v));
  try {m.call(pc,0x80000100,limit);}catch(error){error.message+=` [${name}, ${this.phase}, wall ${this.wall}]`;throw error;}
  this.maxInstructions=Math.max(this.maxInstructions||0,m.steps);
  assert.equal(m.get('sp'),this.stack,'balanced stack');return m.get('v0')|0;
 }
 installServices() {
  const m=this.m,e=this.e;
  const named=(name,handler)=>{if(e[name])m.trap(e[name],handler);};
  const constant=(pc,value=0)=>m.trap(pc,c=>c.set('v0',value));
  // Roster discovery is a controlled fixture; actor layout and gameplay state
  // are native. Do not replace HP sinks, queue writers, impact or clock code.
  m.trap(0x801c8fe8,c=>c.set('v0',c.get('a0')<18?this.actor(c.get('a0')):0));
  m.trap(0x801ca29c,c=>c.set('v0',this.id(c.get('a0'))));
  m.trap(0x801f46dc,c=>c.set('v0',this.config.immune?0:1));
  m.trap(0x8009c7cc,c=>c.set('v0',this.config.random||0));
  m.trap(0x80092a60,c=>c.set('v0',this.config.random||0));
  constant(0x801c8b54); // Combat presentation mode.
  constant(0x801f312c);constant(0x801f4828);constant(0x801eff40);
  m.trap(0x801f32ec,c=>c.set('v0',this.config.hit===false?0:100));
  m.trap(0x801f6068,c=>{ // Native evaluator input boundary: stable scores/chances.
   const p=c.get('a0');for(let i=0;i<c.r32(p+12);i++){c.w16(p+56+i*2,100);c.w16(p+96+i*2,this.config.power??60);}
  });
  // Finite artwork metadata and child movement are controlled presentation
  // services. Their duration is varied independently from effect contact time.
  m.trap(0x801df2ac,c=>{if(c.get('a2')){c.w32(c.get('a2'),10);c.w32(c.get('a2')+4,-1);}c.set('v0',this.config.poseTicks||20);});
  constant(0x801df224);
  named('lc_original_anchor',c=>c.set('v0',13));
  named('lc_original_ranged_anchor',c=>c.set('v0',13));
  for(const pc of [0x22222c,0x2223e0,0x237810,0x237890,0x22a4e0])constant(pc+0x7ffbcd30);
  m.trap(0x237750+0x7ffbcd30,c=>c.set('v0',this.config.power??60));
  m.trap(0x2361ec+0x7ffbcd30,c=>c.set('v0',this.config.hit===false?0:100));
  constant(0x22a964+0x7ffbcd30);constant(0x801ad9d4);
  for(const pc of [0x801ae4ec,0x22257c+0x7ffbcd30])m.trap(pc,c=>{
   if(c.get('a2')){c.w32(c.get('a2'),10);c.w32(c.get('a2')+4,-1);}c.set('v0',this.config.poseTicks||20);
  });
  constant(0x8016e230,159);
  // Combined/Pedra choose their contributors through controlled roster and
  // spell-resolution services; their complete producers still execute.
  m.trap(0x801ebc80,c=>{const a=c.get('a0');c.w8(a+0x78,3);c.w32(a+0x7c,a===this.actor(4)?55:63);c.w32(a+0x80,a===this.actor(4)?0:8);});
  for(const pc of [0x801ca384,0x801c8ed4])constant(pc);
  constant(0x801f0cc4,13);constant(0x801f55ac,13);
  m.trap(0x801f6504,c=>{c.w8(c.get('a2'),1);c.set('v0',36);});
  constant(0x801be034,159);constant(0x8020b150,2);constant(0x801be308,40);
  m.trap(0x801ca3e0,c=>{const a=c.get('a0');c.load(a,new Uint8Array(248));c.w32(a+0x40,0x100);c.w16(a+0x20,300);c.w16(a+0x22,500);});
  constant(0x801d9efc);
  for(const pc of [0x801ad974,0x801ad914,0x801adb00,0x801adbc0,0x801da720,0x801da9d4,0x801db23c,0x801db6cc,0x801d8e70,0x801cfa04])constant(pc);
  for(const name of ['func_001F0E64','func_001F0E04','func_001F0F90','func_001F1050','func_0021E50C','func_0021E99C','func_0020FA04','func_001F2BC0','func_00208DC8','func_0021AE6C','func_00048294','func_0004813C','func_000466f4','lc_strike_round_mode','func_0020BF7C'])named(name,c=>c.set('v0',0));
  named('func_002267A8',c=>this.presentations.push({actor:this.id(c.get('a0')),kind:c.get('a1'),amount:c.get('a2'),wall:this.wall}));
  named('func_0025FC94',c=>{
   const list=at=>{const ids=[];for(let i=0;i<20;i++){const id=c.r8(at+i);if(id===255)return ids;ids.push(id);}throw Error('unterminated effect list');};
   const slot=c.get('a0'),sources=list(c.get('a1')),targets=list(c.get('a2'));
   this.trace.push({event:'visual',slot,sources,targets,wall:this.wall});
   const delay=this.visuals++?20:(this.config.impactDelay||30),gap=this.config.impactGap||3;
   targets.filter(id=>id<20).forEach((id,i)=>this.impacts.push({at:this.wall+delay+i*gap,id}));
   if([97,102,107].includes(slot))this.impacts.push({at:this.wall+delay+targets.length*gap,id:sources[0]});
   this.visualEnd=this.wall+delay+(targets.length+1)*gap+2;
  });
  // Keep source-first copy and destination-first move semantics explicit.
  named('memcpy',c=>{const a=c.get('a0'),b=c.get('a1'),n=c.get('a2');c.load(b,c.ram.slice(c.phys(a),c.phys(a)+n));c.set('v0',b);});
  m.trap(0x801cd01c,c=>{const a=c.get('a0'),b=c.get('a1'),n=c.get('a2');c.load(a,c.ram.slice(c.phys(b),c.phys(b)+n));c.set('v0',a);});
 }
 reset(config={}) {
  this.config={power:60,hp:200,maxHP:500,sourceHP:200,pattern:2,side:1,effect:1,status:0,after:0,afterPercent:25,count:6,impactDelay:30,...config};
  const c=this.config,m=this.m;m.ram.set(this.initial);m.r.fill(0);m.fp.fill(0);m.set('sp',this.stack);
  m.w32(0x801ce8bc,0x80740000);m.w32(0x801ce8c0,0x807457e0);m.w32(0x801ce8c4,1);
  if(c.shifted) {
   m.w32(this.tail+0x814,0x7f0);m.set('s3',0x20);m.set('s0',0);m.set('s1',0);
   m.call(0x80440040,0x801d9a88,200000);
   assert.equal(this.stream,0x80450000,'real High Attack layout transition');m.w32(this.tail+0x814,0);
  }
  this.wall=0;this.trace=[];this.impacts=[];this.presentations=[];this.visualEnd=0;this.visuals=0;this.ignoredContacts=0;this.maxInstructions=0;this.phase='setup';
  for(let i=0;i<18;i++) {
   const a=this.actor(i),own=i<9,cell=i%9,row=Math.floor(cell/3),col=cell%3,child=0x80720000+i*0x200;
   m.w32(a,child);m.w32(a+12,child+0x100);m.w32(child+64,a);m.w32(child+0x140,a);
   m.w16(a+0x20,i===4?c.sourceHP:c.hp);m.w16(a+0x22,c.maxHP);
   m.w32(a+0x40,(own?0x100:0)|(c.dead?.includes(i)?1:0)|(c.flags||0));
   m.w32(a+0x48,2);m.w32(a+0x4c,!own&&c.undead?45:2);m.w32(a+0x54,own?col:2-col);m.w32(a+0x58,own?2-row:6+row);
   m.w32(a+0x68,13);m.w32(a+0x70,1);m.w32(a+0x74,1);m.w32(a+0x78,3);m.w32(a+0x7c,159);
  }
  this.configureAttack(159,c);
  if(c.route==='pedra')m.load(this.actor(0),new Uint8Array(248));
  m.w8(0x801971f0+12,100);
  // Unused end padding and a stack boundary must never become spill storage.
  this.guards=[this.e.lc_strike_state+4128,this.stack-0x4000];
  for(const at of this.guards)m.load(at,new Uint8Array(32).fill(0xa5));
 }
 configureAttack(id,c) {
  const m=this.m,r=this.records+id*16,b=this.settings+id*32;
  const physical=['melee','ranged','alternate'].includes(c.route),donor=physical?4:55;
  m.load(r+4,Uint8Array.from([physical?0:3,0,1,c.effect,0,1,255,0,0,0,0,0]));
  m.load(b,Uint8Array.from([donor,donor,c.pattern,c.side,0,0,0,100,c.status,c.condition??1,c.chance??100,c.after,c.afterBasis||0,c.shared||0,0,0]));
  m.w16(b+14,c.afterPercent);
  m.w16(b+4,c.mask||257);m.w8(b+21,c.visual||101);m.w8(b+22,c.count);
 }
 generate() {
  this.phase='calculation';const m=this.m;
  this.before=Array.from({length:18},(_,i)=>m.ram.slice(m.phys(this.actor(i)),m.phys(this.actor(i))+0xf8));
  m.w8(this.tail+0x818,1);m.w32(this.scratch,0);
  if(this.config.route==='combined') {
   m.w32(this.scratch+64,this.actor(4));m.w32(this.scratch+68,this.actor(5));m.w32(this.scratch+72,0);
   this.call('lc_attack_combined',[this.scratch+64,this.scratch]);
  } else if(this.config.route==='pedra') {
   m.load(this.scratch+64,Uint8Array.from([0,this.config.visual||101,159]));
   this.call('lc_attack_pedra',[this.scratch+64,this.scratch]);
  } else this.call('lc_attack_'+(this.config.route||'generic'),[this.actor(4),this.scratch]);
  // Construct another real action in the same native stream. It must survive
  // insertion/rescheduling by the first action's later impact callbacks.
  this.followSource=Array.from({length:18},(_,i)=>i).find(i=>i!==4 && m.r16(this.actor(i)+32)>0 && !(m.r32(this.actor(i)+64)&1));
  if(this.followSource!==undefined && !this.config.single) {
   this.configureAttack(160,{...this.config,effect:2,pattern:9,side:3,status:0,after:0,visual:101});
   m.w32(this.actor(this.followSource)+0x7c,160);
   this.call('lc_attack_generic',[this.actor(this.followSource),this.scratch]);
  }
  this.expected=Array.from({length:18},(_,i)=>({hp:m.r16(this.actor(i)+32),flags:m.r32(this.actor(i)+64)}));
  this.commands=this.readCommands();
  // A terminal sentinel beyond the finite controlled presentation schedule;
  // avoid inventing an early round-ending command inside a still-running pose.
  const time=this.commands.reduce((n,x)=>n+x.delay,0)+1024;
  this.call(0x801d98f4,[0x41,time,0,0,0,0,0,0,0]);
  this.generated=Buffer.from(m.ram.slice(m.phys(this.stream+16),m.phys(this.stream+16)+m.r32(this.tail+0x814)));
  for(let i=0;i<18;i++)m.load(this.actor(i),this.before[i]);
  m.w32(this.stream+4,0);m.w32(this.stream+8,0);m.w32(this.tail+0x810,0);m.w8(this.tail+0x823,0);
  // Opaque sprite children are absent in playback; native commands still apply
  // actor HP, flags and states, while child drawing/movement is not emulated.
  for(let i=0;i<18;i++){m.w32(this.actor(i),0);m.w32(this.actor(i)+12,0);}
  return this.commands;
 }
 readCommands() {
  const m=this.m,rows=[],length=m.r32(this.tail+0x814);let off=0,time=0;
  assert(length<=(this.stream===0x80450000?0x7800:0x800),`native command stream overflow (${length})`);
  while(off<length) {
   const p=this.stream+16+off,type=m.r8(p),delay=m.r8(p+2);
   const width=type===22?m.r8(p+3)+4:type===56?m.r8(p+3):m.r8(0x801e5c70+type);
   assert(width>=3&&off+width<=length,`invalid packet ${type}/${width} at ${off}`);
   time+=delay;rows.push({offset:off,type,delay,time,bytes:Array.from(m.ram.slice(m.phys(p),m.phys(p)+width))});off+=width;
  }return rows;
 }
 playback(limit=6000) {
  const m=this.m;this.phase='playback';
  for(this.wall=0;this.wall<limit;this.wall++) {
   const cursor=m.r32(this.tail+0x810);
   if(this.wall<20||this.trace.at(-1)?.cursor!==cursor)this.trace.push({wall:this.wall,cursor,clock:m.r32(this.stream+4),total:m.r32(this.stream+8),pending:m.r8(this.tail+0x824)});
   this.call('lc_attack_playback');
   for(const hit of this.impacts.filter(x=>x.at===this.wall)) {
    this.trace.push({event:'impact',id:hit.id,wall:this.wall});
    const pending=m.r8(this.tail+0x824);this.call('lc_attack_visual_impact',[hit.id,10]);
    if(m.r8(this.tail+0x824)===pending)this.ignoredContacts++;
   }
   if(this.visualEnd&&this.wall>=this.visualEnd)m.w8(this.tail+0x822,0);
   // Completion of the controlled sprite pose, independent of the command clock.
   for(let i=0;i<18;i++)if(m.r32(this.actor(i)+0x74)>1)m.w32(this.actor(i)+0x74,1);
   if(!m.r32(0x801ce8c4)) {
    this.readCommands();
    for(const at of this.guards)for(let j=0;j<32;j++)assert.equal(m.r8(at+j),0xa5,'playback scratch/stack guard');
    for(let i=0;i<18;i++) {
     assert.equal(m.r16(this.actor(i)+32),this.expected[i].hp,`actor ${i} playback HP differs from calculation`);
     assert.equal(m.r32(this.actor(i)+64)&0x1f,this.expected[i].flags&0x1f,`actor ${i} playback status differs from calculation`);
     assert.equal(m.r8(this.actor(i)+0xd0),0,`actor ${i} has unconsumed results`);
    }
    // Retail counts visual targets even for immune/no-op outcomes with no
    // result packet. Such contacts are intentionally ignored by its consumer.
    assert(m.r8(this.tail+0x824)<=this.ignoredContacts,'unaccounted pending impacts at completion');
    return {ticks:this.wall+1,commands:this.commands.length,maxInstructions:this.maxInstructions,presentations:this.presentations};
   }
  }
  throw Error(`playback did not complete within ${limit} ticks`);
 }
}
module.exports={AttackPlaybackMachine};
if(require.main===module) {
 const h=new AttackPlaybackMachine(process.argv[2]);
 try {h.reset(process.argv[3]?JSON.parse(process.argv[3]):{});console.log(h.generate());console.log(h.playback());}
 catch(error){console.error(error);console.error(JSON.stringify(h.trace.slice(-20)));process.exitCode=1;}
}
