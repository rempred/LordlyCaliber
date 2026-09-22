// Shared mode-zero Director initialization from the current ROM and explicit caller state.
window.OB64=window.OB64||{};
(function(OB64){
 'use strict';
 function stop(message,code){var e=new Error(message);e.code=code||'director-launch-input';throw e;}
 function hex(bytes){return Array.from(bytes,b=>b.toString(16).padStart(2,'0')).join('');}
 function bytes(text){if(typeof text!=='string'||text.length%2||!/^[0-9a-f]+$/i.test(text))stop('Launch memory requires hexadecimal bytes.');return Uint8Array.from(text.match(/../g),x=>parseInt(x,16));}
 function Launch(input,rom){
  if(!input||!((['mode-zero-director-v1','rom-mode-zero-director-v1'].includes(input.kind)&&input.sceneMode===0)||(input.kind==='rom-mode-two-director-v1'&&input.sceneMode===2))||!Number.isInteger(input.selector)||input.selector<0||!input.world||!input.arena||!OB64.cutsceneDirectorLaunchCode||!(rom instanceof Uint8Array))stop('Director launch requires a supported mode, a ROM selector, caller world state, and a preview arena.');
  if(!Number.isInteger(input.actorPresentationWord)||input.actorPresentationWord<0||input.actorPresentationWord>0xffffffff)stop('Actor construction requires the current caller presentation word.');
  var w=input.world;for(var k of ['mapKind','scenarioByte','red','green','blue'])if(!Number.isInteger(w[k])||w[k]<0||w[k]>255)stop('World inputs require unsigned bytes.');
  if(!Number.isInteger(w.eventState)||w.eventState<0||w.eventState>0xffffffff)stop('World event state requires an unsigned word.');
  if(w.alternateContextPointer!==undefined&&(!Number.isInteger(w.alternateContextPointer)||w.alternateContextPointer<0||w.alternateContextPointer>0xffffffff))stop('Alternate context requires an unsigned caller pointer.');
  var a=input.arena;if(!Number.isInteger(a.address)||a.address<0x80200000||a.address%16||!Number.isInteger(a.byteLength)||a.byteLength<16||a.byteLength>65536||a.byteLength%16||a.address+a.byteLength>0x80700000)stop('Director launch requires a bounded aligned preview arena.');
  var camera=bytes(input.cameraBaseHex);if(camera.length!==144)stop('Director launch requires both current camera banks and their preserved fields.');
  var cameraView=new DataView(camera.buffer);for(var bank of [0,88])for(var ci=0;ci<14;ci++)if(!Number.isFinite(cameraView.getFloat32(bank+ci*4)))stop('Caller camera fields must be finite.');
  this.input=input;this.rom=rom;this.leases=[];this.parserReached=false;this.initialized=false;this.allocations=[];
  var v=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={},data=OB64.cutsceneDirectorLaunchCode;
  data.words.forEach(function(r){if(r[1]+4>rom.length||v.getUint32(r[1])!==r[2])stop('Director initialization code differs from its qualified ROM.','director-launch-image');code[r[0]]=r[2];});
  var regions=[{address:a.address,bytes:new Uint8Array(a.byteLength),writable:true},{address:0x807fe000,bytes:new Uint8Array(8192),writable:true},
   {address:0x800e7a30,bytes:new Uint8Array(168),writable:true},{address:0x800e91d0,bytes:new Uint8Array(92),writable:true},
   {address:0x80220e70,bytes:new Uint8Array(30),writable:true},{address:0x8022a720,bytes:new Uint8Array(0x278),writable:true},
   {address:0x802395b0,bytes:new Uint8Array(4),writable:true},{address:0x801976da,bytes:Uint8Array.of(w.mapKind),writable:false},
   {address:0x801936a7,bytes:Uint8Array.of(w.scenarioByte),writable:false},{address:0x8018f557,bytes:Uint8Array.of(w.red,w.green,w.blue),writable:false},
   {address:0x801ceab0,bytes:Uint8Array.of(w.eventState>>>24,w.eventState>>>16,w.eventState>>>8,w.eventState),writable:false},
   {address:0x8018fc19,bytes:Uint8Array.of(input.sceneMode),writable:false}];
  data.tables.forEach(function(r){var b=bytes(r.hex);if(r.rom+b.length>rom.length||b.some((x,i)=>x!==rom[r.rom+i]))stop('World-context dispatch table differs from its qualified ROM.','director-launch-image');regions.push({address:r.address,bytes:b,writable:false});});
  if(input.sceneMode===2)OB64.cutsceneRomStart.installCode(code,regions,rom);
  var table=this.resource(0x019a8804);if(table.length%4||input.selector>=table.length/4)stop('Director selector exceeds the current ROM directory.','director-launch-selector');
  this.resourceKey=new DataView(table.buffer,table.byteOffset,table.byteLength).getUint32(input.selector*4);
  var decoded=OB64.cutsceneCodec.decodeCustomLz(this.resource(this.resourceKey),{requireExact:false,allowZeroPadding:true,maxOutput:65536}).bytes;
  if(decoded.length%4)stop('Director stream is not word aligned.','director-launch-stream');
  var substitutions=input.operandTranslations||{},seen=new Set(),dv=new DataView(decoded.buffer,decoded.byteOffset,decoded.byteLength);
  for(var i=0;i<decoded.length;i+=4){var word=dv.getUint32(i);if((word&0xffffff00)===0x08880000){var index=word&255,value=substitutions[index];if(!Number.isInteger(value)||value<0||value>65535)stop('Director operand translation '+index+' is missing.','director-launch-translation');if(!seen.has(index)){regions.push({address:0x80196f60+index*2,bytes:Uint8Array.of(value>>>8,value),writable:false});seen.add(index);}}}
  this.decodedLength=decoded.length;this.machine=new OB64.cutsceneDialogue.Machine(code,regions);this.machine.f=new Uint32Array(32);
  this.write(0x8022a720,camera);this.machine.put(0x8022a994,input.selector,2);
  // This initializer uses only FP register transfers and memory operations. No FP arithmetic is approximated here.
  var m=this.machine,step=m.step.bind(m);m.step=function(pc){var word=this.code[pc],op=word>>>26,rs=word>>>21&31,rt=word>>>16&31,fs=word>>>11&31,si=word<<16>>16,at=(this.r[rs]+si)>>>0;
   if(op===17&&rs===4)this.f[fs]=this.r[rt];
   else if(op===49)this.f[rt]=this.get(at);
   else if(op===57)this.put(at,this.f[rt]);
   else if(op===53){this.f[rt+1]=this.get(at);this.f[rt]=this.get(at+4);}
   else if(op===61){this.put(at,this.f[rt+1]);this.put(at+4,this.f[rt]);}
   else if(op===8){var sum=(this.r[rs]|0)+si;if(sum< -2147483648||sum>2147483647)stop('Native launch signed addition overflow.','director-launch-instruction');this.r[rt]=sum;}
   else return step(pc);this.r[0]=0;this.steps++;return {target:null,annul:false};};
  m.serviceHelper=this.helper.bind(this);
  if(input.sceneMode===2)OB64.cutsceneRomStart.attachLaunch(this);
  this.initializeCallback=input.preservedStage===true?0x80226324:input.sceneMode===2?0x802260f0:0x80225a1c;
  if(input.preservedStage===true){if(!OB64.cutscenePreservedStage)stop('Preserved Stage native services are unavailable.','preserved-stage-contract');if(input.sceneMode!==2||input.environmentSelector!==0||input.world.mapKind!==0||input.world.eventState!==0||![0xff000002,0xff000007].includes(dv.getUint32(decoded.length-4)))stop('Preserved Stage preview requires terminal class two or seven and its declared environment-zero world.','preserved-stage-contract');this.stage=new OB64.cutscenePreservedStage(rom,input);this.stage.attach(this);}
  if(input.previewParty===true){if(!OB64.cutscenePreservedStage)stop('Preview party services are unavailable.','preview-party-contract');if(input.preservedStage||input.sceneMode!==2||![0,24].includes(input.world.mapKind)||input.world.eventState!==0||![0xff000001,0xff000008].includes(dv.getUint32(decoded.length-4)))stop('Preview party startup requires terminal class one or eight and its declared world.','preview-party-contract');this.stage=new OB64.cutscenePreservedStage(rom,input);this.stage.attach(this);}
 }
 Launch.prototype.resource=function(key){var p=(key&0x0fffffff)+0x594280,r=this.rom;if(!Number.isInteger(key)||key<=0||key>0xffffffff||key>>>28||p+4>r.length)stop('Launch resource key is invalid.','director-launch-resource');var n=new DataView(r.buffer,r.byteOffset,r.byteLength).getUint32(p);if(n<1||n>65536||p+4+n>r.length)stop('Launch resource extent is invalid or exceeds the supported bound.','director-launch-resource');return r.slice(p+4,p+4+n);};
 Launch.prototype.write=function(a,b){for(var i=0;i<b.length;i++)this.machine.put(a+i,b[i],1);};
 Launch.prototype.read=function(a,n){return Uint8Array.from({length:n},(_,i)=>this.machine.get(a+i,1));};
 Launch.prototype.allocate=function(n){if(!Number.isInteger(n)||n<1||n>65536)stop('Launch allocation size is unsupported.','director-launch-allocation');var size=Math.ceil(n/16)*16,c=this.input.arena,at=c.address;for(var r of this.leases){if(at+size<=r.address)break;at=r.address+r.size;}if(at+size>c.address+c.byteLength)stop('Director launch arena is exhausted.','director-launch-exhaustion');this.leases.push({address:at,size:size});this.leases.sort((a,b)=>a.address-b.address);this.allocations.push({address:at,size:n});return at;};
 Launch.prototype.helper=function(pc){var m=this.machine,a=m.r[4],b=m.r[5];
  if(pc===0x80093380){m.region(a,b,true);for(var i=0;i<b;i++)m.put(a+i,0,1);m.r[2]=a;}
  else if(pc===0x80070f30)m.r[2]=this.allocate(a);
  else if(pc===0x800712c4){if(this.stage&&(a===0||this.stage.leases.some(r=>r.address===a)))return this.stage.service(pc,m);var at=this.leases.findIndex(r=>r.address===a);if(at<0)stop('Launch free targets an unowned allocation.','director-launch-owner');this.leases.splice(at,1);m.r[2]=0;}
  else if(pc===0x8009daf4)m.r[2]=this.resource(a).length;
  else if(pc===0x8009dbb8){var data=this.resource(b),target=a||this.allocate(data.length);this.write(target,data);this.leases.find(r=>r.address===target).contentLength=data.length;if(data.length&1)m.put(target+data.length,this.rom[(b&0x0fffffff)+0x594284+data.length],1);m.r[2]=target;}
  else if(pc===0x8007a7e0)m.r[2]=m.get(a);
  else if(pc===0x8007a110){var lease=this.leases.find(r=>r.address===b);if(!lease)stop('Custom-LZ input lacks a current launch allocation.','director-launch-owner');var decoded=OB64.cutsceneCodec.decodeCustomLz(this.read(b,lease.contentLength||lease.size),{requireExact:false,allowZeroPadding:true,maxOutput:65536}).bytes;this.write(a,decoded);m.r[2]=decoded.length;}
  else if(pc===0x802282b8){this.parserReached=true;m.r[2]=0;}
  else return false;return true;
 };
 Launch.prototype.initialize=function*(record){if(this.initialized)stop('Director launch cannot initialize twice.');if(record.length!==168)stop('Director resource record has the wrong size.');this.write(0x800e7a30,record);yield* this.machine.run(this.initializeCallback,[],[],262144);if(!this.parserReached)stop('Director initialization did not reach its parser.','director-launch-parser');this.initialized=true;};
 Launch.prototype.snapshot=function(){var m=this.machine,root=m.get(0x8022a974),stream=m.get(0x8022a958);return {resourceKey:this.resourceKey,selector:this.input.selector,rootAddress:root,rootHex:hex(this.read(root,0x1cb8)),recordHex:hex(this.read(0x800e7a30,168)),contextHex:hex(this.read(0x80220e70,30)),cameraHex:hex(this.read(0x8022a720,144)),streamAddress:stream,streamHex:hex(this.read(stream,this.decodedLength)),allocations:this.leases.map(r=>({address:r.address,hex:hex(this.read(r.address,r.size))})),parserReached:this.parserReached};};

 Launch.prototype.installMemory=function(m,address,data){
  var region=m.regions.find(r=>r.address<=address&&r.address+r.bytes.length>=address+data.length);
  if(region){if(!region.writable)stop('Computed launch state overlaps read-only memory.','director-launch-memory');region.bytes.set(data,address-region.address);return;}
  if(m.regions.some(r=>address<r.address+r.bytes.length&&r.address<address+data.length))stop('Computed launch memory overlaps a partial allocation.','director-launch-memory');
  if(m.regions.reduce((n,r)=>n+r.bytes.length,0)+this.machine.regions.reduce((n,r)=>n+r.bytes.length,0)+data.length>131072)stop('Computed launch memory exceeds the shared allocation ceiling.','director-launch-memory');
  m.regions.push({address:address,bytes:new Uint8Array(data),writable:true});
 };
 Launch.installAudioQueue=function(engine,rom,onRequest){
  var m=engine.machine,view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),rows=OB64.cutsceneDirectorLaunchCode.words.filter(r=>r[0]>=0x800ea9bc&&r[0]<0x800eac24);
  if(rows.length!==154)stop('Native audio queue code is incomplete.','director-launch-audio');
  rows.forEach(r=>{if(view.getUint32(r[1])!==r[2])stop('Audio queue code differs from its qualified ROM.','director-launch-audio');m.code[r[0]]=r[2];});
  for(var i=0;i<128;i++)m.get(0x800eb8f0+i,1);
  var step=m.step.bind(m);m.step=function(pc){if(pc===0x800ea9bc&&onRequest)onRequest({kind:'native-audio-request',mode:this.r[4]&255,category:this.r[5]&255,program:this.r[6]&65535,priority:this.r[7]&65535,flags:this.get(this.r[29]+16)&255,playback:'queue-only'});return step(pc);};
 };
 Launch.prototype.attachResources=function(engine){
  if(this.stage){const v=new DataView(this.rom.buffer,this.rom.byteOffset,this.rom.byteLength);OB64.cutsceneSharedActorCode.words.filter(r=>r[0]>=0x800ea604&&r[0]<0x800ea9bc).forEach(r=>{if(v.getUint32(r[1])!==r[2])stop('Shared audio dispatcher differs from its qualified ROM.');engine.machine.code[r[0]]=r[2];});}
  this.engine=engine;this.colorSerial=0;this.colorAddress=0;var self=this,m=engine.machine,old=m.serviceHelper;
  var arena=this.input.arena;
  if(m.regions.some(r=>arena.address<r.address+r.bytes.length&&r.address<arena.address+arena.byteLength))stop('Launch arena overlaps supplied resource memory.','director-launch-memory');
  if(m.regions.reduce((n,r)=>n+r.bytes.length,0)+this.machine.regions.reduce((n,r)=>n+r.bytes.length,0)>131072)stop('Combined launch and resource memory exceeds 128 KiB.','director-launch-memory');
  OB64.cutsceneDirectorLaunchCode.words.forEach(function(r){m.code[r[0]]=r[2];});
  var table=OB64.cutsceneDirectorLaunchCode.tables.find(r=>r.address===0x8022ab80);this.installMemory(m,table.address,bytes(table.hex));
  this.installMemory(m,0x8018fc10,new Uint8Array(4));
  this.installMemory(m,0x8022a720,this.read(0x8022a720,144));
  if(this.input.audioQueueHex!==undefined){
   var queue=bytes(this.input.audioQueueHex);if(queue.length!==128)stop('Native audio requests require the current sixteen-entry queue.','director-launch-audio');
   this.installMemory(m,0x800eb8f0,queue);
   var step=m.step.bind(m);m.step=function(pc){if(pc===0x800ea9bc&&self.audioRequest)self.audioRequest({kind:'native-audio-request',mode:this.r[4]&255,category:this.r[5]&255,program:this.r[6]&65535,priority:this.r[7]&65535,flags:this.get(this.r[29]+16)&255,playback:'queue-only'});return step(pc);};
  }else{for(var pc=0x800ea9bc;pc<0x800eac24;pc+=4)delete m.code[pc];}
  m.serviceHelper=function(pc){
   if(pc===0x80070f30&&self.colorCall){if(m.r[4]!==12)stop('Color allocation has an unexpected size.','director-launch-color');self.colorAddress=self.allocate(12);self.installMemory(m,self.colorAddress,new Uint8Array(16));m.r[2]=self.colorAddress;return true;}
   if(pc===0x800712c4&&self.colorAddress&&m.r[4]===self.colorAddress){var i=self.leases.findIndex(r=>r.address===self.colorAddress);if(i<0)stop('Color allocation ownership is missing.','director-launch-owner');self.leases.splice(i,1);self.colorAddress=0;m.r[2]=0;return true;}
   if(pc===0x80093380){m.region(m.r[4],m.r[5],true);for(var i=0;i<m.r[5];i++)m.put(m.r[4]+i,0,1);m.r[2]=m.r[4];return true;}
   return old&&old(pc);
  };
 };
 Launch.prototype.createColor=function(words){
  var e=this.engine,m=e.machine,before=e.owners.map(Boolean);this.colorCall=true;
  try{var g=m.run(0x802278dc,words.slice(1),[],8192),r;do{r=g.next();}while(!r.done);}finally{this.colorCall=false;}
  for(var slot=0;slot<6;slot++)if((m.get(0x800e82c8+slot*168,2)&0x8000)&&!before[slot]){if(m.get(0x800e82c8+slot*168+16)!==0x8022643c)stop('Color construction registered an unexpected resource.','director-launch-color');e.owners[slot]={ownerId:'color:'+this.colorSerial++,payload:null};}
  return this.color();
 };
 Launch.prototype.color=function(){var m=this.engine.machine,a=m.get(0x8018fc10);if(!a)return null;var b=new DataView(this.readEngine(a,12).buffer);return {remaining:b.getInt16(0),duration:b.getInt16(2),red:b.getUint8(4),green:b.getUint8(5),blue:b.getUint8(6),ownershipFlag:b.getUint8(7),alpha:b.getUint8(8),targetAlpha:b.getUint8(9),startAlpha:b.getUint8(10),native:true,ownerId:'computed-color'};};
 Launch.prototype.readEngine=function(a,n){return Uint8Array.from({length:n},(_,i)=>this.engine.machine.get(a+i,1));};
 Launch.prototype.releaseColor=function(){var m=this.engine.machine,a=m.get(0x8018fc10);if(a)m.put(a+7,1,1);};
 OB64.cutsceneDirectorLaunch=Launch;
})(window.OB64);
