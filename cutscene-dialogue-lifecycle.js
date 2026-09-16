// Shared dialogue construction, ROM archive resolution, and preview-owned cleanup.
window.OB64=window.OB64||{};
(function(OB64){
 'use strict';
 function stop(message,code){var e=new Error(message);e.code=code||'dialogue-lifecycle-input';throw e;}
 var POOL=0x800e82c8,STRIDE=168;
 function Lifecycle(engine,input,rom){
  if(input.kind!=='shared-dialogue-v1'||!engine.payloadStorage)stop('Shared lifecycle requires preview payload storage.');
  var a=input.archiveArena;
  if(!a||!Number.isInteger(a.address)||a.address<0x80200000||a.address%16||!Number.isInteger(a.byteLength)||a.byteLength<16||a.byteLength>65536||a.byteLength%16||a.address+a.byteLength>0x80700000)stop('Archive storage requires a bounded aligned arena.');
  var p=engine.payloadStorage.config;
  if(a.address<p.address+p.byteLength&&p.address<a.address+a.byteLength)stop('Archive and payload arenas must not overlap.');
  engine.machine.region(a.address,a.byteLength,true);
  this.engine=engine;this.machine=engine.machine;this.rom=rom;this.arena={address:a.address,byteLength:a.byteLength};this.used=0;this.archives=new Map();
  var machine=this.machine;
  (OB64.cutsceneDialogueLifecycleTables||[]).forEach(function(table){
   var data=Uint8Array.from(table.hex.match(/../g),x=>parseInt(x,16));
   if(table.rom+data.length>rom.length||data.some((v,i)=>v!==rom[table.rom+i]))stop('Dialogue control table differs from its qualified ROM.','dialogue-table-image');
   for(var i=0;i<data.length;i+=4){var address=table.address+i,existing=machine.regions.find(r=>address<r.address+r.bytes.length&&r.address<address+4);
    if(existing){if(address<existing.address||address+4>existing.address+existing.bytes.length||data.slice(i,i+4).some((v,j)=>v!==machine.get(address+j,1)))stop('Supplied dialogue memory conflicts with a qualified control table.','dialogue-table-memory');}
    else machine.regions.push({address:address,bytes:data.slice(i,i+4),writable:false});
   }
  });
  if(machine.regions.reduce((n,r)=>n+r.bytes.length,0)>131072)stop('Dialogue control tables exceed the bounded native memory.','dialogue-memory-bound');
  this.machine.serviceHelper=this.helper.bind(this);
 }
 Lifecycle.prototype.archive=function(selector){
  if(this.archives.has(selector))return this.archives.get(selector);
  // Retail resource_table_word_cached/materialize use key + 0x594280,
  // followed by a four-byte big-endian extent. Resolve the current ROM table.
  var rom=this.rom,table=0x01a3b7b2+0x594280;
  if(!Number.isInteger(selector)||selector<0||!OB64.lh5Decompress||table+4>rom.length)stop('A supported ROM Serifu selector and LH5 decoder are required.','dialogue-archive-input');
  var view=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),tableBytes=view.getUint32(table);
  if(!tableBytes||tableBytes%4||tableBytes>65536||table+4+tableBytes>rom.length||selector>=tableBytes/4)stop('Serifu selector is outside the current ROM table.','dialogue-archive-input');
  var key=view.getUint32(table+4+selector*4),entry=(key&0x0fffffff)+0x594280,at=entry+4;
  if(!key||key===0xffffffff||key>>>28||at+24>rom.length)stop('Serifu archive key is unsupported.','dialogue-archive-input');
  var resourceBytes=view.getUint32(entry),size=view.getUint32(at+7,true),length=view.getUint32(at+11,true),level=rom[at+20],header=level===2?view.getUint16(at,true):rom[at]+2;
  var method=String.fromCharCode.apply(null,rom.slice(at+2,at+7)),nameLength=rom[at+21],crcAt=level===2?at+21:at+22+nameLength;
  if(![0,2].includes(level)||!['-lh5-','-lh0-'].includes(method)||header<24||header>1024||crcAt+2>at+header||size<1||size>65536||length<1||length>65536||resourceBytes<header+size||resourceBytes>66560||at+resourceBytes>rom.length)stop('Unsupported or malformed Serifu archive.','dialogue-archive-input');
  var i,checksum=0;if(level===0){for(i=at+2;i<at+header;i++)checksum=(checksum+rom[i])&255;
   if(checksum!==rom[at+1])stop('Serifu archive header checksum differs.','dialogue-archive-input');}
  var extent=Math.ceil(length/16)*16;if(this.used+extent>this.arena.byteLength)stop('The declared archive arena is exhausted.','dialogue-archive-exhaustion');
  var packed=rom.slice(at+header,at+header+size),data=method==='-lh0-'?packed:OB64.lh5Decompress(packed,length);
  if(data.length!==length)stop('Serifu archive decoded length differs.','dialogue-archive-input');
  var crc=0;for(i=0;i<data.length;i++){crc^=data[i];for(var bit=0;bit<8;bit++)crc=(crc>>>1)^((crc&1)?0xa001:0);}
  if(crc!==view.getUint16(crcAt,true))stop('Serifu archive data checksum differs.','dialogue-archive-input');
  var address=this.arena.address+this.used,m=this.machine;data.forEach(function(v,j){m.put(address+j,v,1);});
  this.used+=extent;this.archives.set(selector,address);return address;
 };
 Lifecycle.prototype.helper=function(pc){
  var m=this.machine;
  if(pc===0x8007938c){
   if(m.r[4]!==0x01a3b7b2||m.r[6]!==0xfffffffc||m.r[7]!==0)stop('Archive call is outside the supported Serifu lookup contract.','dialogue-archive-input');
   m.r[2]=this.archive(m.r[5]);return true;
  }
  if(pc===0x800712c4){
   var lease=this.engine.payloadStorage.leases.find(function(r){return r.handle===m.r[4];});
   if(!lease)stop('Resource cleanup requires a current preview-owned payload.','dialogue-cleanup-owner');
   this.engine.payloadStorage.release(lease.owner);m.r[2]=0;return true;
  }
  return false;
 };
 Lifecycle.prototype.create=function(words,ownerId,point){
  var m=this.machine,e=this.engine,mode=words[13]|0;
  if(words.length!==14||words[0]!==191||![0,1].includes(mode)||words[11]===0xffffffff)stop('Dialogue constructor mode or portrait source is unsupported.','dialogue-constructor-input');
  var slot=-1;for(var i=0;i<6;i++)if(!(m.get(POOL+i*STRIDE,2)&0x8000)){slot=i;break;}
  if(slot<0)stop('Dialogue resource pool is exhausted.','dialogue-resource-exhaustion');
  if(e.owners.some(function(o){return o&&o.ownerId===ownerId;}))stop('Dialogue constructor owner must be fresh.','dialogue-service-owner');
  if(mode===0&&(!point||point.absentActor!==true&&(!Number.isFinite(point.x)||!Number.isFinite(point.y))))stop('Actor-linked dialogue requires current qualified placement.','dialogue-constructor-placement');
  var g=m.run(0x8019ee58,[words[1]&255,words[2]&65535,words[3]&65535,mode===0?((words[6]<<4)+words[5])&255:0,words[8]&255],[],16384),result;
  do{result=g.next();}while(!result.done);
  if(result.value!==slot)stop('Native constructor did not select the first free resource.','dialogue-registration-input');
  var r=POOL+slot*STRIDE,flags=m.get(r+0x8a,1);
  if(words[9]===1)flags|=0x20;if(words[10]===1)flags|=8;
  var portrait=words[11]|0;if(words[12]===1)portrait=-portrait;
  m.put(r+0x92,portrait,2);
  if(mode===1){flags|=0x40;m.put(r+0x8c,words[5]&255,2);m.put(r+0x94,words[6],2);}
  // Native placement skips a known empty Actor slot and retains constructor bytes.
  else if(point.absentActor!==true){var threshold=((words[6]*14+49)<<16)>>16;m.put(r+0x8c,Math.trunc(point.x),2);m.put(r+0x94,Math.trunc(point.y)-(point.y<threshold?20:45),2);}
  m.put(r+0x8a,flags,1);for(i=0;i<8;i++)m.put(0x8019ee40+i,0,1);
  e.owners[slot]={ownerId:ownerId,payload:null};return {slot:slot,ownerId:ownerId};
 };
 OB64.cutsceneDialogueLifecycle=Lifecycle;
})(window.OB64);
