// Preview-owned Stage built from current ROM code and declared party/world choices.
window.OB64=window.OB64||{};
(function(O){
'use strict';
function need(ok,message){if(!ok){const e=new Error(message||'Preview Stage construction requires supported native inputs.');e.code='preserved-stage-input';throw e;}}
function equal(a,b){need(a===b,'Preview Stage data or mapping does not match its qualified input.');}
const hex=b=>Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');
function Stage(rom,input){
input=input||{world:{red:255,green:255,blue:255,scenarioByte:0}};
// Static placements are emitted by the generator from source extents qualified during producer tracing.
const view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={};
for(const c of O.cutscenePreservedStageData.contracts)equal(hex(rom.subarray(c.rom,c.rom+c.hex.length/2)),c.hex);
for(const [a,p,w]of O.cutscenePreservedStageData.words){equal(view.getUint32(p),w);code[a]=w;}
let m=new O.cutsceneDialogue.Machine(code,[{address:0x807fe000,bytes:new Uint8Array(8192),writable:true}]);O.cutsceneImageEcho.installFpu(m);
const ownedRegions=m.regions;
function add(a,n,bytes){need(ownedRegions.reduce((sum,r)=>sum+r.bytes.length,0)+n<=655360,'Preview Stage working memory exceeds 640 KiB.');need(!ownedRegions.some(r=>a<r.address+r.bytes.length&&r.address<a+n));ownedRegions.push({address:a,bytes:bytes||new Uint8Array(n),writable:true});}
// Explicit preview choices: isolated resource registries, environment 0, clear launch flags.
add(0x80196aed,1);add(0x801976d8,0x20);add(0x80197b60,2);add(0x80197b18,8);add(0x801ce8bc,0x1dc4,rom.slice(0x211d4c,0x213b10));add(0x801d0680,0x1c0);add(0x80197774,4);
add(0x801971f0,750);add(0x80193bc0,5600);add(0x80190f80,0x16c);add(0x800c47d0,4);m.put(0x800c47d0,1);
add(0x80187c20,165*72,rom.slice(0x5db20,0x5db20+165*72));
add(0x8018c400,256*32,rom.slice(0x62300,0x64300));
add(0x8018f557,3,Uint8Array.of(input.world.red,input.world.green,input.world.blue));add(0x801936a7,1,Uint8Array.of(input.world.scenarioByte));add(0x8020a2d4,4);add(0x80220000,0x2000);

for(const data of [O.cutsceneImageEchoCode,O.cutsceneSharedActorCode]){
 for(const [a,p,w]of data.words)if(a<0x800a0000){equal(view.getUint32(p),w);code[a]=w;}
 for(const row of data.tables){const b=Uint8Array.from(row.hex.match(/../g),x=>parseInt(x,16));equal(hex(rom.slice(row.rom,row.rom+b.length)),row.hex);let start=-1;for(let i=0;i<=b.length;i++){const old=i<b.length&&m.regions.find(r=>row.address+i>=r.address&&row.address+i<r.address+r.bytes.length);if(old)equal(old.bytes[row.address+i-old.address],b[i]);if(i<b.length&&!old){if(start<0)start=i;}else if(start>=0){add(row.address+start,i-start,b.slice(start,i));start=-1;}}}
}
// Current ROM assets occupy a separate 512 KiB monotonic arena. Freed ranges become inaccessible.
 // A new playback receives a fresh arena; no Stage memory enters retained frame snapshots.
let next=0x80400000;const leases=[],calls=[];
function allocate(n){if(n===0)return 0;need(Number.isInteger(n)&&n>0&&n<0x80000);need(next+Math.ceil(n/16)*16<=0x80480000,'Preview Stage resource pool exceeds 512 KiB.');const a=next;next+=Math.ceil(n/16)*16;add(a,n);leases.push({address:a,length:n});return a;}
function read(a,n){need(Number.isInteger(n)&&n>=0&&n<=524288,'Preview Stage read exceeds its resource bound.');if(n)m.region(a,n,false);return Uint8Array.from({length:n},(_,i)=>m.get(a+i,1));}
function write(a,b){need(b.length<=524288,'Preview Stage write exceeds its resource bound.');if(b.length)m.region(a,b.length,true);for(let i=0;i<b.length;i++)m.put(a+i,b[i],1);}
const resource=key=>O.cutsceneRomStart.resource(rom,key);
// The native preloader converts a standalone HUFF environment into a 64 RGBA5551
// object before Stage construction (func_000689EC, ROM 0x68B54..0x68BBC).
const envKey=view.getUint32(0x594288),image=O.cutsceneNjpg.parseEmbedded(resource(envKey));
const environment=allocate(8+image.width*image.height*2);m.put(environment,0x36340002);m.put(environment+4,image.width,2);m.put(environment+6,image.height,2);
for(let i=0;i<image.width*image.height;i++){const c=image.rgba;m.put(environment+8+i*2,((c[i*4]>>3)<<11)|((c[i*4+1]>>3)<<6)|((c[i*4+2]>>3)<<1)|1,2);}
m.put(0x80197774,environment);
m.serviceHelper=function(pc){const a=m.r[4],b=m.r[5],c=m.r[6];if(calls.length<4096)calls.push({pc,args:[a,b,c],ra:m.r[31]});
 if(pc===0x80070f30||pc===0x80071c04)m.r[2]=allocate(a);
 else if(pc===0x800712c4){need(a===0||leases.some(r=>r.address===a&&!r.freed));if(a){leases.find(r=>r.address===a).freed=true;const index=ownedRegions.findIndex(r=>r.address===a);need(index>=0);ownedRegions.splice(index,1);}m.r[2]=0;}
 else if(pc===0x80093380){write(a,new Uint8Array(b));m.r[2]=a;}
 else if(pc===0x8009c970){write(a,new Uint8Array(c).fill(b));m.r[2]=a;}
 else if(pc===0x8009dd38||pc===0x8009df48){if(!a)m.r[2]=0;else{const bytes=O.cutsceneCodec.decodeCustomLz(resource(a),{requireExact:false,maxOutput:0x80000}).bytes,target=allocate(bytes.length);write(target,bytes);m.r[2]=target;}}
 else if(pc===0x80093060){write(b,read(a,c));m.r[2]=b;}
 else if(pc===0x80080998){write(a,read(b,c));m.r[2]=a;}
 else if(pc===0x8007acb0){const source=read(m.get(a+8),m.get(a+12)),v=new DataView(source.buffer),header=source[20]===2?v.getUint16(0,true):source[0]+2,size=v.getUint32(7,true),length=v.getUint32(11,true);equal(String.fromCharCode(...source.slice(2,7)),'-lh5-');need(length>0&&length<=524288&&header>=22&&header+size<=source.length,'Preview Stage LH5 extent exceeds its resource bound.');const bytes=O.lh5Decompress(source.slice(header,header+size),length),target=allocate(bytes.length);write(target,bytes);m.put(b+8,target);m.put(b+12,bytes.length);m.r[2]=0;}
 else if(pc===0x8007a7e0)m.r[2]=m.get(a);
 else if(pc===0x8007a110){const lease=leases.find(r=>r.address===b);need(lease);const bytes=O.cutsceneCodec.decodeCustomLz(read(b,lease.length),{requireExact:false,maxOutput:0x80000}).bytes;write(a,bytes);m.r[2]=bytes.length;}
 else if(pc===0x8009daf4)m.r[2]=resource(a).length;
 else if(pc===0x8009dbb8){const bytes=resource(b);write(a,bytes);m.r[2]=a;}
 else return false;
 return true;
};
const alignedStep=m.step.bind(m);m.step=function(pc){const w=this.code[pc],op=w>>>26;if(![34,38,42,46].includes(op))return alignedStep(pc);const rs=w>>>21&31,rt=w>>>16&31,a=(this.r[rs]+(w<<16>>16))>>>0,k=a&3,base=a-k;if(op===34||op===42){for(let j=k;j<4;j++){const shift=24-8*(j-k),mask=(255<<shift)>>>0;if(op===34)this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;else this.put(base+j,(this.r[rt]>>>shift)&255,1);}}else for(let j=0;j<=k;j++){const shift=8*(k-j),mask=(255<<shift)>>>0;if(op===38)this.r[rt]=((this.r[rt]&~mask)|(this.get(base+j,1)<<shift))>>>0;else this.put(base+j,(this.r[rt]>>>shift)&255,1);}this.r[0]=0;this.steps++;return {target:null,annul:false};};

 let active=true;const boundary={},nativeStep=m.step.bind(m);let reached=false;
 // Execute the fixed owner through shared construction and subsystem initialization.
 // Stop before its final battle callback; this preview owner does not start a battle.
 m.step=function(pc){if(pc===0x801afe18){reached=true;throw boundary;}return nativeStep(pc);};
 for(const [member,primary,formation] of [[1,81,4],[2,2,1]]){const p=0x80193bc0+member*56;m.put(p+0x11,primary,1);m.put(p+0x12,rom[0x5db20+primary*72+57]||primary,1);run(0x8016f11c,[p,1],100000);m.put(0x801971f0+member+1,member,1);m.put(0x801971f0+member+6,formation,1);}
 try{run(0x801afc0c,[],200000);}catch(e){if(e!==boundary)throw e;}finally{m.step=nativeStep;}
 need(reached,'Native preview Stage construction did not reach the shared-owner boundary before the battle callback.');
 run(0x801c58f0,[]);
 function run(pc,args,limit){need(active,'The preview Stage has been released.');const g=m.run(pc,args,[],limit||200000);while(!g.next().done){}}
 const service=m.serviceHelper;this.service=function(pc,target){const before=m;m=target;try{return service(pc);}finally{m=before;}};
 this.close=()=>{active=false;};this.machine=m;this.leases=leases;this.calls=calls;this.root=m.get(0x801ce8bc);this.read=read;this.write=write;this.run=run;this.environment=environment;this.live=true;
 this.rows=()=>Array.from({length:20},(_,i)=>hex(read(this.root+0x1c4+i*248,248)));
 this.byteLength=()=>m.regions.reduce((n,r)=>n+r.bytes.length,0);
 }
 Stage.prototype.dispose=function(){if(!this.live)return;this.close();this.live=false;this.machine.regions.length=0;this.leases.length=0;this.calls.length=0;};
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
  m.regions.push({address:0x8022ac80,bytes:Uint8Array.of(250),writable:false});
  lm.put(0x8022a981,1,1);
 };
 Stage.prototype.materialize=function(words,records){
  need(this.live,'The preview Stage has been released.');
  need(records.length===0,'Preserved Stage roster construction currently requires an empty Director Actor namespace.');
  const poseCalls=new Set();
  const l=this.launch,m=this.machine,root=m.get(0x8022a974),original=m.serviceHelper;
  for(const r of records){const at=m.get(root+24+r.slot*4);need(at,'Current Actor allocation is missing.');this.write(at,r.bytes);}
  m.serviceHelper=function(pc){
   if(pc===0x80070f30&&m.r[4]===336){m.r[2]=l.allocate(336);return true;}
   if(pc===0x801c41c8){O.cutsceneRomStart.preparePose(l,m.get(m.r[4]),m.get(m.r[5]),m.get(m.r[7]),m.get(m.r[6]),m.get(m.get(m.r[29]+16)));return true;}
   if(pc===0x8022e9e8){poseCalls.add(m.r[4]);return true;} // The shared runtime evaluates each requested immediate pose after publication.
   if(original(pc))return true;need(false,'Preview Stage reached native helper 0x'+pc.toString(16)+'.');
  };
  try{this.run(0x8023c3dc,[words[1],words[2],(words[0]&0x7fffffff)===0xab?1:0]);}finally{m.serviceHelper=original;}
  return Array.from({length:28},(_,slot)=>{const at=m.get(root+24+slot*4);if(!at)return null;const ordinal=m.get(at+0x147,1),row=this.root+0x1c4+ordinal*248;need(ordinal<20,'Roster Actor has no native source row.');this.run(0x8016fa34,[0x36,0x38,0x3a,0x3c].map(off=>m.get(row+off,2)));O.cutsceneRomStart.preparePose(l,m.get(at+0xe8),m.get(at+0xec),m.get(at+0x146,1),m.get(at+0x13a,2),m.r[2]&65535);return {slot,bytes:this.read(at,336),poseRequested:poseCalls.has(at)};}).filter(Boolean);
 };
 Stage.prototype.height=function(x,z){need(Number.isFinite(x)&&Number.isFinite(z),'Terrain query requires finite coordinates.');this.run(0x801bc35c,[Math.trunc(x)<<16>>16,Math.trunc(z)<<16>>16]);const b=new DataView(new ArrayBuffer(4));b.setUint32(0,this.machine.f[0]);return b.getFloat32(0);};
 Stage.prototype.sharedRequest=function(actor,record){
  const m=this.machine,ordinal=actor.sourceRowOrdinal;need(Number.isInteger(ordinal)&&ordinal>=0&&ordinal<20,'Shared pose request requires its current source row.');
  const row=this.root+0x1c4+ordinal*248,child=m.get(row+actor.linkedOrdinal*4);if(!child)return {suppressed:true};
  const context=record.opcode===18?'A':'B',at=context==='A'?0x801d06c4:0x801d06c8;m.put(at,0xffffffff);
  this.run(0x8022e72c,[child+0x44,m.get(row+0x48),m.get(row+0x4c),record.operands[0],record.operands[1],record.opcode]);
  const request=m.get(at)|0;return request<0?{suppressed:true}:{context,request};
 };
 O.cutscenePreservedStage=Stage;
})(window.OB64);
