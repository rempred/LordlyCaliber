// Shared help panel and opt-in preset-3 option menu. Native code computes ownership, layout and animation.
window.OB64=window.OB64||{};
(function(O){
 'use strict';
 const ROOT=0x80390000,CTX=0x80392000,MANAGER=0x80392900,ARENA=0x80393000,SIZE=8192;
 function fail(message,code){const e=new Error(message);e.code=code||'map-menu-input';throw e;}
 function hex(b){return Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');}
 function bytes(h){return Uint8Array.from(h.match(/../g)||[],x=>parseInt(x,16));}
 function Menu(rom,input){
  if(!input||input.kind!=='native-map-menu-v1'||input.initialEntitiesEmpty!==true||input.displayMode!==0||!(rom instanceof Uint8Array)||!O.cutsceneMapMenuCode)fail('Map menus require qualified ROM code and an explicitly empty entity list in display mode zero.');
  this.rom=rom;this.leases=[];this.audio=[];this.optionsEnabled=input.optionController==='declared-action-direction-v1';if(input.optionController!==undefined&&!this.optionsEnabled)fail('Unknown option-menu controller contract.');
  const d=O.cutsceneMapMenuCode,v=new DataView(rom.buffer,rom.byteOffset,rom.byteLength),code={};
  d.words.forEach(r=>{if(r[1]+4>rom.length||v.getUint32(r[1])!==r[2])fail('Menu code differs from the qualified ROM.','map-menu-image');code[r[0]]=r[2];});
  d.references.forEach(r=>{if(r[0]+4>rom.length||v.getUint32(r[0])!==r[1])fail('Menu drawing or scheduling differs from the qualified ROM.','map-menu-image');});
  const regions=[{address:ROOT+0x19b8,bytes:new Uint8Array(56),writable:true},{address:CTX+0x834,bytes:new Uint8Array(4),writable:true},{address:MANAGER,bytes:new Uint8Array(216),writable:true},{address:ARENA,bytes:new Uint8Array(SIZE),writable:true},{address:0x807fe800,bytes:new Uint8Array(3072),writable:true}];
  for(const [address,n]of [[0x8018fdc0,4],[0x8022a970,8],[0x800c4b28,2],[0x800e8100,2],[0x800e8700,2],[0x80187020,1]])regions.push({address,bytes:new Uint8Array(n),writable:true});
  d.tables.forEach(r=>{const b=bytes(r.hex);if(r.rom+b.length>rom.length||b.some((x,i)=>x!==rom[r.rom+i]))fail('Menu table differs from the qualified ROM.','map-menu-image');regions.push({address:r.address,bytes:b,writable:false});});
  this.machine=new O.cutsceneDialogue.Machine(code,regions);O.cutsceneImageEcho.installFpu(this.machine);this.machine.serviceHelper=this.helper.bind(this);
  const m=this.machine;m.put(0x8018fdc0,MANAGER);m.put(0x8022a974,ROOT);m.put(0x8022a970,CTX);
  // Native manager initialization sets count/list/lock to zero. Drawing matrices
  // and texture resources are outside this bounded product panel renderer.
 }
 Menu.prototype.read=function(a,n){this.machine.region(a,n,false);return Uint8Array.from({length:n},(_,i)=>this.machine.get(a+i,1));};
 Menu.prototype.write=function(a,b){this.machine.region(a,b.length,true);b.forEach((v,i)=>this.machine.put(a+i,v,1));};
 Menu.prototype.invoke=function(pc,args){const g=this.machine.run(pc,args||[],[],131072);let r;do{r=g.next();}while(!r.done);return r.value;};
 Menu.prototype.allocate=function(n){if(!Number.isInteger(n)||n<1||n>SIZE)fail('Menu allocation exceeds its arena.','map-menu-exhaustion');const size=(n+15)&~15;let a=ARENA;for(const r of this.leases){if(a+size<=r.address)break;a=r.address+r.size;}if(a+size>ARENA+SIZE)fail('Menu arena is exhausted.','map-menu-exhaustion');this.leases.push({address:a,size});this.leases.sort((x,y)=>x.address-y.address);this.write(a,new Uint8Array(size));return a;};
 Menu.prototype.resource=O.cutsceneDirectorLaunch.prototype.resource;
 Menu.prototype.helper=function(pc){const m=this.machine,a=m.r[4],b=m.r[5];
  if(pc===0x80070f30||pc===0x80071288||pc===0x80071c04)m.r[2]=this.allocate(a);
  else if(pc===0x800712c4){if(a){const i=this.leases.findIndex(r=>r.address===a);if(i<0)fail('Menu release targets an unowned allocation.','map-menu-owner');this.leases.splice(i,1);}m.r[2]=0;}
  else if(pc===0x80093380){m.region(a,b,true);this.write(a,new Uint8Array(b));m.r[2]=a;}
  else if(pc===0x80080998){this.write(a,this.read(b,m.r[6]));m.r[2]=a;}
  else if(pc===0x80093060){this.write(b,this.read(a,m.r[6]));m.r[2]=b;}
  else if(pc===0x8009daf4)m.r[2]=this.resource(a).length;
  else if(pc===0x8009dbb8){const data=this.resource(b),target=a||this.allocate(data.length);this.write(target,data);const r=this.leases.find(r=>r.address===target);if(!r)fail('Menu resource requires its owned allocation.','map-menu-owner');r.contentLength=data.length;m.r[2]=target;}
  else if(pc===0x8007a7e0)m.r[2]=m.get(a);
  else if(pc===0x8007a110){const r=this.leases.find(r=>r.address===b);if(!r)fail('Menu compressed resource lacks ownership.','map-menu-owner');const data=O.cutsceneCodec.decodeCustomLz(this.read(b,r.contentLength||r.size),{requireExact:false,maxOutput:SIZE}).bytes;this.write(a,data);m.r[2]=data.length;}
  else if(pc===0x800ea604){this.audio.push({kind:'menu-audio-request',program:b,playback:'request-only'});m.r[2]=0;}
  else return false;return true;
 };
 Menu.prototype.create=function(slot,preset,width,height){if(!Number.isInteger(slot)||slot<0||slot>=14||!(preset===33||([1,3].includes(preset)&&this.optionsEnabled))||![width,height].every(n=>Number.isInteger(n)&&n>=1&&n<=2048))fail('Map menu requires a supported preset, valid owner slot, and current image dimensions.');const m=this.machine;m.put(CTX+0x834,width,2);m.put(CTX+0x836,height,2);this.invoke(0x80236920,[slot,preset]);return this.owner(slot);};
 Menu.prototype.advance=function(mask,direction){if(this.optionsEnabled&&(!Number.isInteger(direction)||direction<0||direction>65535))fail('Option menu requires a declared direction mask.');if(!Number.isInteger(mask)||mask<0||mask>65535)fail('Menu update requires a declared current controller mask.');this.audio=[];this.machine.put(0x800c4b28,mask,2);this.machine.put(0x800e8100,mask,2);this.machine.put(0x800e8700,direction||0,2);this.invoke(0x80237620);return this.audio.slice();};
 Menu.prototype.query=function(slot){if(!Number.isInteger(slot)||slot<0||slot>=14)fail('Menu query slot is outside the owner pool.');return this.invoke(0x802276f8,[slot])|0;};
 Menu.prototype.release=function(slot){if(slot!==-1&&(!Number.isInteger(slot)||slot<0||slot>=14))fail('Menu release slot is outside the owner pool.');this.invoke(0x80227778,[slot]);};
 Menu.prototype.owner=function(slot){const m=this.machine,a=m.get(ROOT+0x19b8+slot*4);return a?{slot,address:a,entity:m.get(a+12),status:this.query(slot),recordHex:hex(this.read(a,24))}:null;};
 Menu.prototype.snapshot=function(){const m=this.machine,entities=[];let a=m.get(MANAGER+4);while(a){if(entities.length>=64||entities.some(e=>e.address===a))fail('Menu entity list exceeds its bound.','map-menu-owner');const p=m.get(a+0xd0);entities.push({address:a,payload:p,recordHex:hex(this.read(a,0xe8)),payloadHex:p?hex(this.read(p,0x604)):null,drawCallback:m.get(a+8),selection:(m.get(a+0x22,1)<<24)>>24,alpha:m.get(a+0x18,2),animation:m.get(a+0x28,2),bounds:[0x2c,0x30,0x34,0x38].map(o=>m.get(a+o)|0)});a=m.get(a+4);}return {owners:Array.from({length:14},(_,i)=>this.owner(i)),entities,leases:this.leases.map(r=>({address:r.address,size:r.size}))};};
 Menu.lines=function(entity){
  if(!entity.alpha)return [];
  if(entity.drawCallback===0x8017eae8){const table=O.cutsceneMapMenuCode.tables.find(t=>t.address===0x8018fe10),p=bytes(table.hex);return [0,4].map((at,i)=>{let end=at;while(end<p.length&&p[end])end++;return {hex:hex(p.slice(at,end)),x:entity.bounds[0]+21,y:entity.bounds[1]+(i?14:2)};});}
  if(!entity.payloadHex)return [];
  const p=bytes(entity.payloadHex),v=new DataView(p.buffer),lines=[];
  if(p.length!==0x604)fail('Menu drawing requires its complete current payload.');
  for(let i=0;i<p[0x5f4];i++){const index=p[0x5f1]+i;if(index>=p[0x5f5])continue;if(index>=48)fail('Menu line index exceeds its pointer table.','map-menu-text');const at=v.getUint32(0x530+index*4)-entity.payload;if(at<0||at>=0x500)fail('Menu line pointer is outside its text storage.','map-menu-text');let end=at;while(end<0x500&&p[end])end++;if(end===0x500)fail('Menu text has no bounded terminator.','map-menu-text');lines.push({hex:hex(p.slice(at,end)),x:entity.bounds[0]+(entity.drawCallback===0x8017ec84?24:v.getInt16(0x5f8)+6),y:entity.bounds[1]+i*p[0x5f6]+(entity.drawCallback===0x8017ec84?4:v.getInt16(0x5fa)+5)});}
  return lines;
 };
 Menu.cursor=function(entity){if(!entity.alpha)return null;if(entity.drawCallback===0x8017eae8)return {x:entity.bounds[0]+1,y:entity.bounds[1]+(entity.selection&1)*14+4};if(entity.drawCallback!==0x8017ec84||!entity.payloadHex)return null;const p=bytes(entity.payloadHex);return {x:entity.bounds[0]+3,y:entity.bounds[1]+entity.selection*p[0x5f6]+6};};
 O.cutsceneMapMenu=Menu;
})(window.OB64);

