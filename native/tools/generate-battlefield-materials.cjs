'use strict';
// Generates bounded, static material recipes from the original field helpers.
// Textures, vertices, matrices and map programs are always read from the user's
// ROM at runtime. This file emits command constants and resource references only.
const fs=require('fs'),path=require('path'),crypto=require('crypto'),vm=require('vm');
global.window=global;global.OB64={};vm.runInThisContext(fs.readFileSync(path.join(__dirname,'../../parsers.js'),'utf8'));
const {FloatMips}=require('../tests/float-mips.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
if(rom.readUInt32BE(0)!==0x80371240)throw Error('ROM order');
const recipes={};
function resource(key){const at=key+0x594280;return rom.subarray(at+4,at+4+rom.readUInt32BE(at));}
function unpack(root,selector,mode,index){
 const data=resource(resource(root).readUInt32BE(selector*4));
 if(mode===-10)return Buffer.from(OB64.lzssDecode(data,4,data.readUInt32BE(0)).output);
 let at=0;for(let i=0;i<=index;i++){const size=data.readUInt32LE(at+1),decoded=data.readUInt32LE(at+5);if(i===index)return data[at]===0x80?data.subarray(at+9,at+9+size):Buffer.from(OB64.lh5Decompress(data.subarray(at+9,at+9+size),decoded));at+=size+9;}
}
function capture(name,entry,arg=0,side=0){
 const m=new FloatMips();m.ram=new Uint8Array(0x400000);
 m.load(0x80197b70,rom.subarray(0xe48f0,0xebbb0));
 m.load(0x8019ee50,rom.subarray(0xebbb0,0xf9ff0));
 m.load(0x8018f810,rom.subarray(0x65710,0x65710+69*12));
 const field=0x80300000,dl=0x80380000,context=0x80310000,identity=0x80320000;
 m.w32(0x801aba58,field);m.w32(field+260,0);m.writeFloat(field+348,1);
 m.w32(0x800e9ba0,dl);m.w32(context+8,identity);m.w8(0x800e7ab8,9);
 m.w16(0x801951cc,side);m.set('sp',0x803ffe00);m.set('a0',arg);
 const resources=[];
 const bind=(root,selector,mode,member)=>{
  const value={root,selector,mode,member},index=resources.findIndex(r=>JSON.stringify(r)===JSON.stringify(value));
  if(index>=0)return 0x80200000+index*0x10000;
  resources.push(value);const address=0x80200000+(resources.length-1)*0x10000;const data=unpack(root,selector,mode,member);if(data.length>65536)throw Error("Recipe resource budget");m.load(address,data);return address;
 };
 m.trap(0x8007938c,c=>c.set('v0',bind(c.get('a0'),c.get('a1'),c.get('a2')|0,[-3,-4].includes(c.get('a2')|0)?c.get('a3'):0)));
 m.trap(0x800987e0,c=>{m.load(c.get('a0'),Buffer.alloc(64));for(let i=0;i<4;i++)m.w16(c.get('a0')+i*10,1);});
 m.w32(field+284,bind(0x2093576,4,-3,name==='stone'?2:1));
 try {m.call(entry);} catch(e) {throw Error(name+" "+entry.toString(16)+": "+e.message);}
 const end=m.r32(0x800e9ba0),commands=[];
 if(end<dl||end-dl>4096||(end-dl)%8)throw Error('Unexpected command extent');
 for(let p=dl;p<end;p+=8){
  const a=m.r32(p),b=m.r32(p+4);let low=b;
  if([0xfd,0xda,0xde,0x01].includes(a>>>24)){
   if(b>=0x80200000&&b<0x80200000+resources.length*0x10000)low={resource:(b-0x80200000)>>>16,offset:b&65535};
   else if(b===identity||(b&0x1fffffff)===(identity&0x1fffffff))low={identity:true};
   else if(b>=0x80000000&&(b<0x80197b70||b>=0x801ad290))throw Error('Unresolved pointer '+b.toString(16));
  }
  commands.push([a,low]);
 }
 recipes[name]={entry,resources,commands};
}
for(let i=0;i<11;i++)capture('preset'+i,rom.readUInt32BE(0xf9408+i*4));
for(const [name,side]of[['neutral',0],['ally',2],['enemy',4]])capture(name,0x801a8e64,1,side);
capture('grass',0x801a5184,0x80310000);
capture('snow',0x801a9714);
const source=rom.subarray(0xf4f04,0xf67e0);
const result={schema:1,generator:'native/tools/generate-battlefield-materials.cjs',source:{revision:0,offset:0xf4f04,size:source.length,sha256:crypto.createHash('sha256').update(source).digest('hex')},notes:'Static daylight, zero animation phase. Host projection/lighting are editor approximations. No texture, vertex or matrix bytes are embedded.',recipes};
const out=process.argv[3]||path.join(__dirname,'../game/scenarios/field-materials.json');
fs.writeFileSync(out,JSON.stringify(result,null,2)+'\n');
console.log('Generated',Object.keys(recipes).length,'field material recipes.');
