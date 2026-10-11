'use strict';
// Independent reference: existing browser decoder and delta encoder, current ROM inputs.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),crypto=require('node:crypto');
global.window=global;global.OB64={};for(const f of ['art.js','animation-art.js'])vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../',f),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const A=OB64.art,B=OB64.animationArt,hash=b=>crypto.createHash('sha256').update(Buffer.from(b)).digest('hex'),b64=b=>Buffer.from(b||[]).toString('base64');
if(process.argv[4]){
 const plan=JSON.parse(fs.readFileSync(process.argv[4]));
 for(const row of plan){const descriptor=A.readResource(rom,row.descriptor).stored,key=A.readU32(descriptor,(row.art+4)*4),bytes=A.readCompressedResource(rom,key).decoded;if(hash(bytes)!==row.hash)throw Error('Reopened browser resource differs at '+row.id);}
 fs.writeFileSync(process.argv[3],JSON.stringify({verified:plan.length}));process.exit(0);
}
const directory=A.readResource(rom,0x3b6cd0).stored,seen=new Set(),samples=[],resources=new Map(),counts={bindings:0,descriptors:0},selected=new Set();
let ordinary=0,large=0,direct=0,indexed8=0,embedded=0;
for(let h=0;h<directory.length/4;h++){
 const dk=A.readU32(directory,h*4);if(!dk||seen.has(dk))continue;seen.add(dk);counts.descriptors++;
 const d=A.readResource(rom,dk).stored;
 for(let art=0;art<d.length/4-4;art++){
  const key=A.readU32(d,(art+4)*4);if(!key)continue;counts.bindings++;
  if(!resources.has(key)){const bytes=A.readCompressedResource(rom,key).decoded;resources.set(key,A.readU16(bytes,0)===0x5554?B.parseSpriteObject(bytes,key):null);}
  const s=resources.get(key);if(!s||selected.has(key))continue;
  const valid=s.children.filter(c=>c.firstOffset!==null&&(s.secondFormat!==0||c.secondOffset!==null)).map(c=>c.ordinal);if(!valid.length)continue;
  const indexed=s.firstFormat===1&&s.secondFormat===0;
  let choose=false;
  if(indexed&&ordinary<24){ordinary++;choose=true;}
  else if(indexed&&s.childCount>=16&&large<6){large++;choose=true;}
  else if(s.firstFormat===2&&direct<4){direct++;choose=true;}
  else if(s.firstFormat===1&&s.secondFormat===1&&indexed8<4){indexed8++;choose=true;}
  else if(indexed&&s.children.some(c=>c.lookupOffset!==null)&&embedded<4){embedded++;choose=true;}
  if(!choose)continue;selected.add(key);
  const child=valid[0],lanes=B.materializeChildLanes(s,child),color=[],alpha=[];
  const bytes=s.firstFormat===1?1:s.firstFormat===2?2:4;
  for(let y=0;y<s.height;y++)for(let x=0;x<s.width;x++){
   for(let c=0;c<bytes;c++)color.push(lanes.first[y*s.firstStride+x*bytes+c]);
   if(lanes.second){const v=lanes.second[y*s.secondStride+(s.secondFormat?x:x>>1)];alpha.push(s.secondFormat?v:(x&1?v&15:v>>4));}
  }
  color[0]=(color[0]+7)&255;if(alpha.length)alpha[0]=(alpha[0]+3)&(s.secondFormat?255:15);
  let rebuilt=null;
  if(indexed){const source={key:'test',resourceKey:key,sprite:s,editable:true,formatKind:'indexed-ci8',editableChildOrdinals:valid,originalChildren:{}};rebuilt=B.buildDecoded(source,{[child]:{indices:new Uint8Array(color),intensity:new Uint8Array(alpha)}});}
  const children=s.children.map((_,i)=>{const l=B.materializeChildLanes(s,i);return {color:hash(l.first||[]),alpha:hash(l.second||[]),palette:hash(l.lookup||[])};});
  samples.push({id:dk.toString(16).padStart(8,'0')+':'+art,descriptor:dk,art,key,width:s.width,height:s.height,child,color:b64(color),alpha:b64(alpha),decoded:hash(s.decoded),rebuilt:rebuilt?hash(rebuilt):null,children});
 }
}
fs.writeFileSync(process.argv[3],JSON.stringify({counts,samples}));console.log('Browser sampled '+samples.length+' resources from '+counts.bindings+' descriptor bindings');
