'use strict';
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const {Mips}=require('../../tests/lib/mips-interpreter.cjs');
global.window=global;global.OB64={};
for(const file of ['art.js','army-sprites.js'])vm.runInThisContext(fs.readFileSync(path.resolve(__dirname,'../../',file),'utf8'));
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const specs=OB64.armySprites.specs,refs=[[0x2093dc,0x2093e4],[0x2093dc,0x2093ec],[0x20942c,0x209434],[0x20942c,0x209438],[0x2093f4,0x2093fc],[0x2093f4,0x209400]],atlases=[];
for(let atlas=0;atlas<6;++atlas){
 const spec=specs[atlas],site=refs[atlas],key=(rom.readUInt32BE(site[0])&65535)*65536+(rom.readUInt32BE(site[1])&65535);
 const decoded=Buffer.from(OB64.art.readCompressedResource(rom,key).decoded),count=(decoded.length-1024-spec.tailLength)/(spec.width*spec.height),images=[];
 for(let model=0;model<count;++model)for(let palette=0;palette<2;++palette){
  const indices=decoded.subarray(1024+model*spec.width*spec.height,1024+(model+1)*spec.width*spec.height),words=Buffer.alloc(indices.length*2),rgba=Buffer.alloc(indices.length*4);
  for(let pixel=0;pixel<indices.length;++pixel){const word=decoded.readUInt16BE(palette*512+indices[pixel]*2);words.writeUInt16BE(word,pixel*2);rgba.set(OB64.art.rgba5551(word),pixel*4);}
  images.push({model,palette,indices:indices.toString('base64'),words:words.toString('base64'),rgba:rgba.toString('base64')});
 }
 // Execute the actual original branch/delay-slot sequence up to its resource call.
 const machine=new Mips(),delta=0x7ffbcb70;
 machine.load(0x20934c+delta,rom.subarray(0x20934c,0x2094f8));machine.set('s4',atlas%2?0:1);
 machine.call([0x2093d8,0x2093d8,0x209428,0x209428,0x2093f0,0x2093f0][atlas]+delta,0x801ad60c,30);
 if(machine.get('a0')!==key)throw Error('Original side selector did not select the atlas key');
 atlases.push({key,count,decoded:decoded.toString('base64'),images});
}
let legacy=null,classRoutes=null;
if(atlases.every((atlas,i)=>atlas.key===specs[i].resourceKey)){
 const state=OB64.armySprites.initialize(rom);if(!state.supported)throw new Error(state.unavailableReason);
 for(let i=0;i<6;++i)OB64.armySprites.setEdit(state,specs[i].key,0,OB64.armySprites.currentIndices(state,specs[i].key,1));
 OB64.armySprites.setEdit(state,specs[3].key,24,OB64.armySprites.currentIndices(state,specs[2].key,24),{create:true});
 OB64.armySprites.setEdit(state,specs[5].key,25,OB64.armySprites.currentIndices(state,specs[4].key,25),{create:true});
 legacy=OB64.armySprites.collectProject(state);classRoutes=state.classRoutes;
}
fs.writeFileSync(process.argv[3],JSON.stringify({atlases,legacy,classRoutes}));
console.log('Checked six original-MIPS side branches and '+atlases.reduce((n,a)=>n+a.images.length,0)+' palette images');
