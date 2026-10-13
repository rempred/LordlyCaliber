'use strict';
// Execute each guarded assembly adapter against its original instruction block.
// The oracle changes only the one member-kind operation for tagged enemies;
// player instructions, branch decisions/delay slots and displaced work remain.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto');
const assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
const native=path.resolve(__dirname,'..'),root=path.resolve(native,'../..');
const tools=path.join(native,'.local/n64-toolchain'),work=fs.mkdtempSync(path.join(native,'build/capacity-predicates-'));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const pins=JSON.parse(fs.readFileSync(path.join(native,'rebuild/toolchain.json')));
for(const [n,h] of Object.entries(pins.tools))assert.equal(hash(fs.readFileSync(path.join(tools,n))),h);
const catalog=JSON.parse(fs.readFileSync(path.join(native,'game/scenarios/capacity-predicates.json')));
const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM||path.join(root,'Ogre Battle 64 - Person of Lordly Caliber (U) [!].v64'));
if(rom.readUInt32BE(0)===0x37804012)rom.swap16();assert.equal(hash(rom),catalog.inputRomSha256);
const types=0x80300000,entryBase=0x80280000;
let source='.text\n',bindings={lc_enemy_types:types},entry=entryBase;
for(const row of catalog.patches){
 const p=row.owner;assert.equal(hash(rom.subarray(p.start,p.start+p.size)),p.retailSha256);
 row.expected.forEach((w,i)=>assert.equal(rom.readUInt32BE(row.hook+i*4),w));
 row.entry=entry;entry+=0x200;
 let s=row.source;
 for(const label of [...s.matchAll(/^([a-z_][a-z_0-9]*):/gm)].map(m=>m[1]).filter(n=>n!==row.name))s=s.replace(new RegExp('\\b'+label+'\\b','g'),row.name+'_'+label);
 for(const [name,value] of Object.entries(row.bindings)){const n=row.name+'_'+name;bindings[n]=value;s=s.replaceAll(name,n);}
 source+=`.org 0x${(row.entry-entryBase).toString(16)}\n`+s;
}
fs.writeFileSync(path.join(work,'predicates.s'),source);
fs.writeFileSync(path.join(work,'predicates.ld'),Object.entries(bindings).map(([n,a])=>`${n} = ${typeof a==='number'?'0x'+a.toString(16):a.entry+' + '+a.offset};`).join('\n')+`\nSECTIONS { .text 0x${entryBase.toString(16)} : { *(.text) } }\n`);
const run=(n,args)=>cp.execFileSync(path.join(tools,n),args,{cwd:work,encoding:'utf8'});
run('mips-kmc-elf-as.exe',['-G','0','-mips3','-mabi=32','-force-n64align','-EB','-o','predicates.o','predicates.s']);
run('mips-kmc-elf-ld.exe',['-EB','-T','predicates.ld','-o','predicates.elf','predicates.o']);
run('mips-kmc-elf-objcopy.exe',['-O','binary',...['.data','.bss','.reginfo','.pdr','.mdebug','.comment'].map(n=>'--remove-section='+n),'predicates.elf','predicates.bin']);
const code=fs.readFileSync(path.join(work,'predicates.bin'));
function cpu(){const m=new FloatMips();m.ram=new Uint8Array(0x400000);return m;}
function setContext(m,c,enemy){
 if(c.kind==='enemy')return;
 if(c.kind==='enemyBase'){m.r[c.register]=enemy?0x801953f0:c.playerBase;return;}
 let value=c.kind==='source'?enemy?c.limit||30:29:c.kind==='playerFlag'?+!enemy:enemy?c.mask||1:0;
 if(c.kind==='eitherEnemy')value=enemy?30:29;
 let a=c.address===undefined?0x80310000:c.address;
 if(c.loads?.length){
  if(c.address===undefined)m.r[c.register]=a;
  c.loads.forEach(([type,off],i)=>{
   if(i===c.loads.length-1){if(type==='byte')m.w8(a+off,value);else m.w32(a+off,value);}
   else{m.w32(a+off,0x80310100+i*256);a=0x80310100+i*256;}
  });
 }else if(c.kind==='eitherEnemy'){m.r[c.register]=0;m.w8(c.address,value);}
 else m.r[c.register]=value;
}
function isEnemy(m,c){
 if(c.kind==='enemy')return true;
 if(c.kind==='enemyBase')return m.r[c.register]!==c.playerBase;
 if(c.kind==='eitherEnemy')return !!m.r[c.register]||m.r8(c.address)>=30;
 let v=c.address===undefined?m.r[c.register]:c.address;
 for(const [type,off] of c.loads||[])v=type==='byte'?m.r8(v+off):m.r32(v+off);
 if(c.mask)v&=c.mask;
 return c.kind==='source'?v>=(c.limit||30):c.kind==='playerFlag'?!v:!!v;
}
function execute(m,row,oracle){
 let pc=row.address,next=pc+4,count=0;
 const ordinaryExec=m.exec;
 if(oracle)m.exec=function(at,after){
  const r=catalog.patches.find(r=>at===r.address+r.site-r.hook);
  if(r&&isEnemy(this,r.context)){
   const w=rom.readUInt32BE(r.site),member=this.r[w>>>21&31];
   this.r[w>>>16&31]=this.r8(types+member)!==2?1:0;
   return [after,after+4];
  }
  return ordinaryExec.call(this,at,after);
 };
 const covered=at=>catalog.patches.some(r=>r.address<=at&&at<r.address+r.expected.length*4)||(!oracle&&entryBase<=at&&at<entryBase+code.length);
 while(covered(pc)){
  assert(++count<400,row.name+' did not exit');
  [pc,next]=m.exec(pc,next);
  m.r[0]=0;
 }
 m.exec=ordinaryExec;return pc;
}
let checks=0,playerChecks=0,enemyChecks=0;const exits={};
for(const row of catalog.patches)for(const enemy of [false,true])for(const handle of [0,1,99,100,127,128,219,220,254,255])for(const tag of [1,2])for(const branchMode of [0,1]){
 if(!enemy&&row.context.kind==='enemy')continue;
 const a=cpu(),b=cpu();
 a.load(entryBase,code);
 for(const r of catalog.patches)r.expected.forEach((w,i)=>a.w32(r.address+i*4,w));
 for(let r=1;r<32;r++)a.r[r]=0x80320000;
 a.r[29]=0x803f0000;
 const pred=rom.readUInt32BE(row.site),memberReg=pred>>>21&31;
 setContext(a,row.context,enemy);a.r[memberReg]=handle;a.w8(types+handle,tag);
 // Vary the decision before a delay-slot predicate. Its original result register
 // is still live at that branch, so it must not be overwritten early.
 if(row.kind==='precedingBranch'){
  const w=row.expected[0],rs=w>>>21&31,rt=w>>>16&31;
  if(rt===0&&rs!==(memberReg))a.r[rs]=branchMode;
  else if(rt!==0&&rt!==memberReg)a.r[rt]=branchMode?a.r[rs]:a.r[rs]+1;
 }
 b.r.set(a.r);b.ram.set(a.ram);
 for(const r of catalog.patches){b.w32(r.address,(0x08000000|((r.entry>>>2)&0x03ffffff))>>>0);for(let i=1;i<r.expected.length;i++)b.w32(r.address+i*4,0);}
 const expectedExit=execute(a,row,true),actualExit=execute(b,row,false);
 assert.equal(actualExit,expectedExit,row.name+' branch');
 for(let r=2;r<32;r++)assert.equal(b.r[r],a.r[r],`${row.name} register ${r} enemy=${enemy} handle=${handle} tag=${tag} branch=${branchMode}`);
 for(const r of catalog.patches)r.expected.forEach((w,i)=>b.w32(r.address+i*4,w));
 assert.deepEqual(b.ram,a.ram,row.name+' memory');
 exits[row.name]??=new Set();exits[row.name].add(actualExit);
 checks++;if(enemy)enemyChecks++;else playerChecks++;
}
const report={scope:'Private MIPS instruction blocks; original branch/delay/displaced work with independent enemy-tag oracle. Not full game execution.',predicates:catalog.patches.length,checks,playerChecks,enemyChecks,
 branchExits:Object.fromEntries(Object.entries(exits).map(([n,v])=>[n,[...v]]))};
fs.writeFileSync(path.join(work,'report.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({work,predicates:report.predicates,checks,playerChecks,enemyChecks}));
