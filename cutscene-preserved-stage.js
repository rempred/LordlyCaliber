// Preview-owned Stage built from current ROM code and declared party/world choices.
window.OB64=window.OB64||{};
(function(O){
'use strict';
function need(ok,message){if(!ok){const e=new Error(message||'Preview Stage construction requires supported native inputs.');e.code='preserved-stage-input';throw e;}}
function equal(a,b){need(a===b,'Preview Stage data or mapping does not match its qualified input.');}
const hex=b=>Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');
function Stage(rom,input){
input=input||{world:{red:255,green:255,blue:255,scenarioByte:0}};
const arenaLimit=input.previewParty?2097152:524288,workingLimit=arenaLimit+131072;
const environmentSelector=input.environmentSelector===undefined?0:input.environmentSelector;
need(Number.isInteger(environmentSelector)&&environmentSelector>=0&&environmentSelector<80,'Preview Stage requires an environment from the ROM table.');
// Static placements are emitted by the generator from source extents qualified during producer tracing.
const view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={};
for(const c of O.cutscenePreservedStageData.contracts)equal(hex(rom.subarray(c.rom,c.rom+c.hex.length/2)),c.hex);
for(const [a,p,w]of O.cutscenePreservedStageData.words){equal(view.getUint32(p),w);code[a]=w;}
let m=new O.cutsceneDialogue.Machine(code,[{address:0x807fe000,bytes:new Uint8Array(8192),writable:true}]);O.cutsceneImageEcho.installFpu(m);
const ownedRegions=m.regions;
function add(a,n,bytes){need(ownedRegions.reduce((sum,r)=>sum+r.bytes.length,0)+n<=workingLimit,'Preview Stage working memory exceeds its bounded pool.');need(!ownedRegions.some(r=>a<r.address+r.bytes.length&&r.address<a+n),'Preview Stage allocation overlaps existing memory.');ownedRegions.push({address:a,bytes:bytes||new Uint8Array(n),writable:true});}
// Explicit preview choices: isolated resource registries and clear launch flags.
add(0x80196aed,1);add(0x801976d8,0x20);add(0x80197b60,2);add(0x80197b18,8);add(0x801ce8bc,0x1dc4,rom.slice(0x211d4c,0x213b10));add(0x801d0680,0x1c0);add(0x80197774,4);
m.put(0x80196aed,environmentSelector,1);
add(0x801971f0,775);add(0x80193bc0,5600);add(0x80195560,5200);add(0x80190f80,0x16c);add(0x800c47d0,4);m.put(0x800c47d0,1);
add(0x80187c20,165*72,rom.slice(0x5db20,0x5db20+165*72));
add(0x8018c400,256*32,rom.slice(0x62300,0x64300));
add(0x8018f557,3,Uint8Array.of(input.world.red,input.world.green,input.world.blue));add(0x801936a7,1,Uint8Array.of(input.world.scenarioByte));add(0x8020a2d4,4);add(0x80220000,0x2000);

for(const data of [O.cutsceneImageEchoCode,O.cutsceneSharedActorCode]){
 for(const [a,p,w]of data.words)if(a<0x800a0000){equal(view.getUint32(p),w);code[a]=w;}
 for(const row of data.tables){const b=Uint8Array.from(row.hex.match(/../g),x=>parseInt(x,16));equal(hex(rom.slice(row.rom,row.rom+b.length)),row.hex);let start=-1;for(let i=0;i<=b.length;i++){const old=i<b.length&&m.regions.find(r=>row.address+i>=r.address&&row.address+i<r.address+r.bytes.length);if(old)equal(old.bytes[row.address+i-old.address],b[i]);if(i<b.length&&!old){if(start<0)start=i;}else if(start>=0){add(row.address+start,i-start,b.slice(start,i));start=-1;}}}
}
// Current assets occupy a bounded arena: 512 KiB for preserved scenes, 2 MiB for class-eight backgrounds.
// Freed ranges become inaccessible and can be reused.
 // A new playback receives a fresh arena; no Stage memory enters retained frame snapshots.
const leases=[],calls=[];
function allocate(n){if(n===0)return 0;need(Number.isInteger(n)&&n>0&&n<arenaLimit,'Preview Stage allocation of '+n+' bytes exceeds its bounded pool.');const size=Math.ceil(n/16)*16;let a=0x80400000;for(const r of leases.filter(r=>!r.freed).sort((x,y)=>x.address-y.address)){if(a+size<=r.address)break;a=r.address+Math.ceil(r.length/16)*16;}need(a+size<=0x80400000+arenaLimit,'Preview Stage resource pool is exhausted.');add(a,n);leases.push({address:a,length:n});return a;}
function read(a,n){need(Number.isInteger(n)&&n>=0&&n<=arenaLimit,'Preview Stage read exceeds its resource bound.');if(n)m.region(a,n,false);return Uint8Array.from({length:n},(_,i)=>m.get(a+i,1));}
function write(a,b){need(b.length<=arenaLimit,'Preview Stage write exceeds its resource bound.');if(b.length)m.region(a,b.length,true);for(let i=0;i<b.length;i++)m.put(a+i,b[i],1);}
const resource=key=>{const at=key+0x594280;need(Number.isInteger(key)&&key>=0&&at+4<=rom.length,'Preview Stage resource key exceeds the loaded ROM.');const length=view.getUint32(at);need(length>0&&length<=arenaLimit&&at+4+length<=rom.length,'Preview Stage resource exceeds its bounded pool.');return rom.slice(at+4,at+4+length);};
// The native preloader converts a standalone HUFF environment into a 64 RGBA5551
// object before Stage construction (func_000689EC, ROM 0x68B54..0x68BBC).
const envKey=view.getUint32(0x594288+environmentSelector*4),envBytes=resource(envKey);
let environment=0;
if(new DataView(envBytes.buffer,envBytes.byteOffset,envBytes.byteLength).getUint32(0)===0x4855fe00){
 const image=O.cutsceneNjpg.parseEmbedded(envBytes);
 environment=allocate(8+image.width*image.height*2);m.put(environment,0x36340002);m.put(environment+4,image.width,2);m.put(environment+6,image.height,2);
 for(let i=0;i<image.width*image.height;i++){const c=image.rgba;m.put(environment+8+i*2,((c[i*4]>>3)<<11)|((c[i*4+1]>>3)<<6)|((c[i*4+2]>>3)<<1)|1,2);}
 m.put(0x80197774,environment);
}else if((envBytes[0]===0x42&&envBytes[1]===0x35)||(envBytes[0]===0x36&&envBytes[1]===0x34)){
 let expanded=envBytes;
 if(envBytes[0]===0x42&&envBytes[2]===3){
  // Expand embedded HUFF records while retaining each B5 rectangle and origin.
  // Stage geometry uses these records; the renderer keeps the original decoded art.
  const parsed=O.cutsceneAssets.parseBg2(envBytes),size=8+parsed.records.reduce((n,r)=>n+16+Math.ceil(r.width/4)*8*r.height,0);
  expanded=new Uint8Array(size);expanded.set(envBytes.subarray(0,8));expanded[2]=1;const out=new DataView(expanded.buffer);let at=8;
  for(const r of parsed.records){const stride=Math.ceil(r.width/4)*8;out.setInt16(at,r.x);out.setInt16(at+2,r.y);out.setUint16(at+4,r.width);out.setUint16(at+6,r.height);out.setUint32(at+8,stride*r.height);for(let y=0;y<r.height;y++)for(let x=0;x<r.width;x++){const i=(y*r.width+x)*4,c=r.rgba;out.setUint16(at+16+y*stride+x*2,((c[i]>>3)<<11)|((c[i+1]>>3)<<6)|((c[i+2]>>3)<<1)|(c[i+3]>=128?1:0));}at+=16+stride*r.height;}
 }
 environment=allocate(expanded.length);write(environment,expanded);m.put(0x80197774,environment);
}
// Compressed environments use the native Stage loader's resource-four fallback.
m.serviceHelper=function(pc){const a=m.r[4],b=m.r[5],c=m.r[6];if(calls.length<4096)calls.push({pc,args:[a,b,c],ra:m.r[31]});
 if(pc===0x80070f30||pc===0x80071c04)m.r[2]=allocate(a);
 else if(pc===0x8016ff5c)m.r[2]=O.cutsceneRuntime.nativeActor.classFamilyMatch(a,b)?1:0;
 else if(pc===0x800712c4){need(a===0||leases.some(r=>r.address===a&&!r.freed));if(a){leases.find(r=>r.address===a&&!r.freed).freed=true;const index=ownedRegions.findIndex(r=>r.address===a);need(index>=0);ownedRegions.splice(index,1);}m.r[2]=0;}
 else if(pc===0x80093380){write(a,new Uint8Array(b));m.r[2]=a;}
 else if(pc===0x8009c970){write(a,new Uint8Array(c).fill(b));m.r[2]=a;}
 else if(pc===0x8009dd38||pc===0x8009df48){if(!a)m.r[2]=0;else{const bytes=O.cutsceneCodec.decodeCustomLz(resource(a),{requireExact:false,maxOutput:arenaLimit}).bytes,target=allocate(bytes.length);write(target,bytes);m.r[2]=target;}}
 else if(pc===0x80093060){write(b,read(a,c));m.r[2]=b;}
 else if(pc===0x80080998){write(a,read(b,c));m.r[2]=a;}
 else if(pc===0x8007acb0){const source=read(m.get(a+8),m.get(a+12)),v=new DataView(source.buffer),header=source[20]===2?v.getUint16(0,true):source[0]+2,size=v.getUint32(7,true),length=v.getUint32(11,true);equal(String.fromCharCode(...source.slice(2,7)),'-lh5-');need(length>0&&length<=arenaLimit&&header>=22&&header+size<=source.length,'Preview Stage LH5 extent exceeds its resource bound.');const bytes=O.lh5Decompress(source.slice(header,header+size),length),target=allocate(bytes.length);write(target,bytes);m.put(b+8,target);m.put(b+12,bytes.length);m.r[2]=0;}
 else if(pc===0x8007a7e0)m.r[2]=m.get(a);
 else if(pc===0x8007a110){const lease=leases.find(r=>r.address===b&&!r.freed);need(lease);const bytes=O.cutsceneCodec.decodeCustomLz(read(b,lease.length),{requireExact:false,maxOutput:arenaLimit}).bytes;write(a,bytes);m.r[2]=bytes.length;}
 else if(pc===0x8009daf4)m.r[2]=a?resource(a).length:0;
 else if(pc===0x8009dbb8){const bytes=resource(b);write(a,bytes);m.r[2]=a;}
 else {m.lastUnhandledHelper=pc;return false;}
 return true;
};
const alignedStep=m.step.bind(m);m.step=function(pc){const w=this.code[pc],op=w>>>26;if(![34,38,42,46].includes(op))return alignedStep(pc);const rs=w>>>21&31,rt=w>>>16&31,a=(this.r[rs]+(w<<16>>16))>>>0,k=a&3,base=a-k;if(op===34||op===42){for(let j=k;j<4;j++){const shift=24-8*(j-k),mask=(255<<shift)>>>0;if(op===34)this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;else this.put(base+j,(this.r[rt]>>>shift)&255,1);}}else for(let j=0;j<=k;j++){const shift=8*(k-j),mask=(255<<shift)>>>0;if(op===38)this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;else this.put(base+j,(this.r[rt]>>>shift)&255,1);}this.r[0]=0;this.steps++;return {target:null,annul:false};};

 let active=true;const boundary={},nativeStep=m.step.bind(m);let reached=false;
 if(environment&&envBytes[0]===0x42&&envBytes[1]===0x35){run(0x801b7a7c,[environment]);environment=m.r[2];m.put(0x80197774,environment);}
 // Execute the fixed owner through shared construction and subsystem initialization.
 // Stop before its final battle callback; this preview owner does not start a battle.
 m.step=function(pc){if(pc===0x801afe18){reached=true;throw boundary;}return nativeStep(pc);};
 const party=input.previewClasses||[81,2];need(party.length>0&&party.length<=5,'Preview unit supports up to five members.');
 for(const [index,primary]of party.entries()){const member=index+1,formation=[4,1,3,5,7][index],p=0x80193bc0+member*56;need(Number.isInteger(primary)&&primary>0&&primary<165,'Preview member class exceeds the ROM table.');m.put(p+0x11,primary,1);m.put(p+0x12,rom[0x5db20+primary*72+57]||primary,1);run(0x8016f11c,[p,1],100000);if(index===0)m.put(p+0x33,m.get(p+0x33,1)|2,1);const name=primary>=81&&primary<=83?'Magnus':O.className?O.className(primary):'Member '+member;for(let i=0;i<16;i++)m.put(p+i,i<name.length?name.charCodeAt(i):0,1);m.put(0x801971f0+member+1,member,1);m.put(0x801971f0+member+6,formation,1);}
 try{run(0x801afc0c,[],200000);}catch(e){if(e!==boundary)throw e;}finally{m.step=nativeStep;}
 need(reached,'Native preview Stage construction did not reach the shared-owner boundary before the battle callback.');
 run(0x801c58f0,[]);
 // Unit 30 supplies the deployed members used by opcode 0x96 and 0xA6.
 // They are declared preview characters, built from current ROM class data.
 for(let i=0;i<party.length;i++){const member=i+1;write(0x80195560+member*52,read(0x80193bc0+member*56,52));m.put(0x801971f0+30*25+2+i,member,1);m.put(0x801971f0+30*25+7+i,[4,1,3,5,7][i],1);}
 if(input.previewDeployed)run(0x8023d7a8,[]);
 function run(pc,args,limit){need(active,'The preview Stage has been released.');const g=m.run(pc,args,[],limit||200000);try{while(!g.next().done){}}catch(error){if(error.code==='dialogue-unqualified-helper')error.message+=' Native Stage helper 0x'+m.lastUnhandledHelper.toString(16)+'.';throw error;}}
 const service=m.serviceHelper;this.service=function(pc,target){const before=m;m=target;try{return service(pc);}finally{m=before;}};
 this.close=()=>{active=false;};this.machine=m;this.rom=rom;this.leases=leases;this.calls=calls;this.root=m.get(0x801ce8bc);this.read=read;this.write=write;this.run=run;this.environment=environment;this.live=true;this.arenaLimitBytes=arenaLimit;this.workingLimitBytes=workingLimit;this.heightCache=new Map();
 this.rows=()=>Array.from({length:20},(_,i)=>hex(read(this.root+0x1c4+i*248,248)));
 this.byteLength=()=>m.regions.reduce((n,r)=>n+r.bytes.length,0);
 }
 Stage.prototype.dispose=function(){if(!this.live)return;this.close();this.live=false;this.machine.regions.length=0;this.leases.length=0;this.calls.length=0;this.heightCache.clear();};
 Stage.prototype.attach=function(l){
  const m=this.machine,lm=l.machine;this.launch=l;
  // Both machines access the same computed presentation context; the class-two callback preserves it.
  // The Stage pool is separate from the existing 128 KiB Director/dialogue pool.
  // Only current native construction owns it. Snapshots retain published Actor values.
  const context=m.region(0x80220e70,30,false),launchContext=lm.regions.find(r=>r.address===0x80220e70);launchContext.bytes=context.bytes.subarray(0x80220e70-context.address,0x80220e70-context.address+30);
  const removed=new Set([0x80187c20,0x8018c400,0x801ce8bc,0x801ce8fc,0x801d06fc]);
  lm.regions=lm.regions.filter(r=>!removed.has(r.address));
  const stageRegion=m.region.bind(m),launchRegion=lm.region.bind(lm);
  m.region=function(a,n,w){const own=this.regions.find(r=>a>=r.address&&a+n<=r.address+r.bytes.length);return own?stageRegion(a,n,w):launchRegion(a,n,w);};
  lm.region=function(a,n,w){const own=this.regions.find(r=>a>=r.address&&a+n<=r.address+r.bytes.length);return own?launchRegion(a,n,w):stageRegion(a,n,w);};
  for(const [pc,word]of Object.entries(lm.code))m.code[pc]=word;
  for(const row of O.cutscenePreservedStageData.words)lm.code[row[0]]=row[2];
  // The existing Director body wrapper owns its logical ROM pose registration service.
  // Stage construction uses the complete native cache function in its separate machine.
  delete lm.code[0x801c41c8];
  const launchHelper=lm.serviceHelper,stage=this;lm.serviceHelper=function(pc){return launchHelper(pc)||stage.service(pc,lm);};
  m.regions.push({address:0x8022ac80,bytes:Uint8Array.of(l.input.previewParty?(l.input.terminalClass===1?253:246):l.input.terminalClass===7?247:250),writable:false});
  lm.put(0x8022a981,l.input.preservedStage?(l.input.terminalClass===7?2:1):0,1);
 };
 Stage.prototype.materialize=function(words,records){
  need(this.live,'The preview Stage has been released.');
  const poseCalls=new Set();
  const l=this.launch,m=this.machine,root=m.get(0x8022a974),original=m.serviceHelper;
  for(const r of records){const at=m.get(root+24+r.slot*4);need(at,'Current Actor allocation is missing.');this.write(at,r.bytes);}
  m.serviceHelper=function(pc){
   if(pc===0x80070f30&&m.r[4]===336){need(Array.from({length:28},(_,i)=>m.get(root+24+i*4)).some(p=>!p),'Director Actor capacity is exhausted.');m.r[2]=l.allocate(336);return true;}
   if(pc===0x801c41c8){O.cutsceneRomStart.preparePose(l,m.get(m.r[4]),m.get(m.r[5]),m.get(m.r[7]),m.get(m.r[6]),m.get(m.get(m.r[29]+16)));return true;}
   if(pc===0x8022e9e8){poseCalls.add(m.r[4]);return true;} // The shared runtime evaluates each requested immediate pose after publication.
   if(original(pc))return true;need(false,'Preview Stage reached native helper 0x'+pc.toString(16)+'.');
  };
  try{this.run(0x8023c3dc,[words[1],words[2],(words[0]&0x7fffffff)===0xab?1:0]);}finally{m.serviceHelper=original;}
  return Array.from({length:28},(_,slot)=>{const at=m.get(root+24+slot*4);if(!at)return null;const ordinal=m.get(at+0x147,1),row=this.root+0x1c4+ordinal*248;need(ordinal<20||ordinal===255,'Roster Actor has no native source row.');if(ordinal<20){this.run(0x8016fa34,[0x36,0x38,0x3a,0x3c].map(off=>m.get(row+off,2)));O.cutsceneRomStart.preparePose(l,m.get(at+0xe8),m.get(at+0xec),m.get(at+0x146,1),m.get(at+0x13a,2),m.r[2]&65535);}return {slot,bytes:this.read(at,336),poseRequested:poseCalls.has(at)};}).filter(Boolean);
 };
 Stage.prototype.height=function(x,z){need(this.live,'The preview Stage has been released.');need(Number.isFinite(x)&&Number.isFinite(z),'Terrain query requires finite coordinates.');x=Math.trunc(x)<<16>>16;z=Math.trunc(z)<<16>>16;const key=x+','+z;if(this.heightCache.has(key))return this.heightCache.get(key);this.run(0x801bc35c,[x,z]);const b=new DataView(new ArrayBuffer(4));b.setUint32(0,this.machine.f[0]);const height=b.getFloat32(0);if(this.heightCache.size>=4096)this.heightCache.clear();this.heightCache.set(key,height);return height;};
 Stage.prototype.sharedRequest=function(actor,record){
  const m=this.machine,ordinal=actor.sourceRowOrdinal;need(Number.isInteger(ordinal)&&ordinal>=0&&ordinal<20,'Shared pose request requires its current source row.');
  const row=this.root+0x1c4+ordinal*248,child=m.get(row+actor.linkedOrdinal*4);if(!child)return {suppressed:true};
  const context=record.opcode===18?'A':'B',at=context==='A'?0x801d06c4:0x801d06c8;m.put(at,0xffffffff);
  this.run(0x8022e72c,[child+0x44,m.get(row+0x48),m.get(row+0x4c),record.operands[0],record.operands[1],record.opcode]);
  const request=m.get(at)|0;return request<0?{suppressed:true}:{context,request};
 };
 Stage.prototype.resetRoster=function(){this.run(0x8023d7a8,[]);return Array.from({length:5},(_,i)=>this.machine.get(0x801971f0+30*25+2+i,1));};
 Stage.prototype.actorProfile=function(actor){
  if(!actor)return -1;const ordinal=actor.sourceRowOrdinal;if(!Number.isInteger(ordinal)||ordinal<0||ordinal>=20)return -1;
  const m=this.machine,row=this.root+0x1c4+ordinal*248,art=m.get(row+0x48);if(!art)return -1;
  // func_002AC700 reads the current source row, character Gender and ROM story overrides.
  const deployed=!!(m.get(row+0x40)&256),member=m.get(row+0xf6,1),gender=m.get((deployed?0x80195560:0x80193bc0)+member*(deployed?52:56)+0x14,1);
  for(let i=0;i<46;i+=2){const kind=this.rom[0x2868f1+i];if(kind===255)break;if(kind===art)return this.rom[0x2868f0+i];}
  return 3*gender+Math.floor(Math.min(m.get(row+0x34,1),98)/33);
 };
 Stage.prototype.actorName=function(ordinal){
  if(!Number.isInteger(ordinal)||ordinal<0||ordinal>=20)return null;
  const m=this.machine,row=this.root+0x1c4+ordinal*248;if(!m.get(row+0x48))return null;
  const deployed=!!(m.get(row+0x40)&256),at=(deployed?0x80195560:0x80193bc0)+m.get(row+0xf6,1)*(deployed?52:56);
  let name='';for(let i=0;i<16;i++){const c=m.get(at+i,1);if(!c)break;name+=String.fromCharCode(c);}return name;
 };
 O.cutscenePreservedStage=Stage;
})(window.OB64);
