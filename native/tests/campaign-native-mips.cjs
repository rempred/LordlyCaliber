'use strict';
// Execute the retail event interpreter and world-map picker. UI rendering and
// full mission transitions are outside this instruction-level regression.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const {FloatMips}=require('./float-mips.cjs');
function read(file){const b=fs.readFileSync(file);if(b.readUInt32BE(0)===0x37804012)b.swap16();assert.equal(b.readUInt32BE(0),0x80371240);return b;}
const image=read(process.argv[2]),original=read(process.argv[3]||process.argv[2]);
const parser=fs.readFileSync(path.resolve(__dirname,'../../parsers.js'),'utf8');
const begin=parser.indexOf('OB64.lzssDecode = function'),end=parser.indexOf('\n};',begin)+3,codec={OB64:{}};
vm.runInNewContext(parser.slice(begin,end),codec);
function row(b,id){const at=0x594280+b.readUInt32BE(0x9398ec+id*4);return Buffer.from(codec.OB64.lzssDecode(b,at+8,b.readUInt32BE(at+4)).output);}
function machine(b){
 const m=new FloatMips();
 // Resident event VM/data (ROM-to-RAM delta0x8012a100), then world overlay
 // (delta0x80121070). These are separate qualified mappings, not early boot.
 m.load(0x80163910,b.subarray(0x39810,0x66e10));
 m.load(0x80197b70,b.subarray(0x76b00,0x87200));
 const header=b.lastIndexOf('LCCAMP01');
 if(header>=0)m.load(0x8019a7a0,b.subarray(b.readUInt32BE(header+12),b.readUInt32BE(header+12)+b.readUInt32BE(header+16)));
 m.set('sp',0x807ff000);return m;
}
function run(m,program,start){m.load(0x80700000,program);m.w16(0x80197b04,start/2);m.set('a0',0x80700000);m.call(0x80170954,0x80000100,1000000);assert.equal(m.get('sp'),0x807ff000);return m.get('v0');}
function flag(m,id,value){const at=0x80193686+(id>>3),mask=1<<(id&7);m.w8(at,(m.r8(at)&~mask)|(value?mask:0));}
const world=row(image,0),stock=row(original,0);
const newNodes=[];for(let id=1;id<=40;id++)if(!original.readInt16BE(0x864a0+(id-1)*8)&&image.readInt16BE(0x864a0+(id-1)*8))newNodes.push(id);
assert.ok(newNodes.length,'Supply the freshly exported Campaign test ROM');
for(const id of newNodes){
 const m=machine(image),program=row(image,4),table=program.readUInt16BE(10),handler=program.readUInt16BE(table);
 assert.equal(run(m,program,handler),0);
 assert.equal((m.r8(0x80196a86+((id-1)>>2))>>(((id-1)&3)*2))&3,2,'native mission handler unlocks node');
 assert.equal((m.r8(0x80196a93+((id-1)>>3))>>((id-1)&7))&1,1,'native mission handler marks active mission');
 const x=image.readInt16BE(0x864a0+(id-1)*8),y=image.readInt16BE(0x864a0+(id-1)*8+2),cursor=0x80740000;
 m.w32(0x80197af8,cursor);m.w32(cursor+64,x+32);m.w32(cursor+68,y+32);m.w16(cursor+82,0);m.w16(cursor+84,0);
 m.call(0x8019dcd8,0x80000100,100000);assert.equal(m.get('v0'),id-1,'retail map picker selects the created node');
 assert.equal(run(m,world,world.readUInt16BE(id*2)),2);assert.equal(m.r8(0x801936a8),5,'native world entry selects the configured event');
}
for(const value of [false,true]){
 const edited=machine(image),baseline=machine(original);flag(edited,250,value);flag(baseline,250,value);
 assert.equal(run(edited,world,world.readUInt16BE(12)),2);
 if(value)assert.equal(edited.r8(0x801936a8),5,'flag ON takes the custom branch');
 else {assert.equal(run(baseline,stock,stock.readUInt16BE(12)),2);assert.equal(edited.r8(0x801936a8),baseline.r8(0x801936a8),'flag OFF follows the unchanged native entry rule');}
}
console.log('PASS: exported native VM unlock/marker writes, retail picking of new nodes, custom event selection, and original-rule fallback. No full gameplay/rendering claim.');

const descriptor=image.lastIndexOf('LCCAMP01');assert.ok(descriptor>=0);
const overlay=image.readUInt32BE(descriptor+12),size=image.readUInt32BE(descriptor+16),count=image.readUInt32BE(descriptor+20);
const roadAddress=image.readUInt32BE(descriptor+24),dotAddress=image.readUInt32BE(descriptor+28);
const bendCapacity=image.readUInt32BE(descriptor+8)>=4?14:7,roadStride=bendCapacity*4;
assert.equal(count,42);
// Execute the actual changed loader, with DMA and cache services recorded.
{
 const m=machine(image),calls=[];
 m.trap(0x800900c0,()=>calls.push(['instruction-cache',m.get('a0'),m.get('a1')]));
 m.trap(0x80090010,()=>calls.push(['data-cache',m.get('a0'),m.get('a1')]));
 m.trap(0x8009da50,()=>{calls.push(['dma',m.get('a0'),m.get('a1'),m.get('a2')]);m.load(m.get('a1'),image.subarray(m.get('a0'),m.get('a0')+m.get('a2')));});
 m.trap(0x80073164,()=>{});m.trap(0x8019a7a0,()=>{});
 m.call(0x8017b5ec);
 assert.deepEqual(calls,[['instruction-cache',0x8019a7a0,size],['data-cache',0x8019a7a0,size],['dma',overlay,0x8019a7a0,size]]);
 assert.ok(0x8019a7a0+size<=0x801f1510,'extended world overlay remains inside its native reservation');
}
function state(m,id,value){const at=0x80196a86+((id-1)>>2),shift=((id-1)&3)*2;m.w8(at,(m.r8(at)&~(3<<shift))|(value<<shift));}
function route(m,from,to){m.set('a0',from);m.set('a1',to);m.set('a2',0x80750000);m.load(0x80750000,Buffer.alloc(48,0xcc));m.call(0x8019e4c0,0x80000100,2000000);let out=[];for(let i=0;i<42;i++){const v=m.r8(0x80750000+i);if(!v)return out;out.push(v);}throw Error('route lacks terminator');}
const native=machine(original),edited=machine(image),placed=[];
for(let id=2;id<=40;++id)if(original.readInt16BE(0x864a0+(id-1)*8)){placed.push(id);state(native,id,1);state(edited,id,1);}
native.load(0x80196a81,Buffer.alloc(5,255));edited.load(0x80196a81,Buffer.alloc(5,255));
let comparisons=0;
for(let seed=0;seed<4;seed++){
 for(const id of placed){const value=seed&&((id+seed)%7===0)?2:1;state(native,id,value);state(edited,id,value);}
 for(const id of placed)if(id!==6){assert.deepEqual(route(edited,6,id),route(native,6,id),`native route preserved to ${id}, pattern ${seed}`);comparisons++;}
}
const created=newNodes[0];state(edited,created,2);state(edited,6,1);state(edited,8,1);
assert.deepEqual(route(edited,6,created),[6,created]);assert.deepEqual(route(edited,created,6),[created,6]);assert.deepEqual(route(edited,created,8),[created,8]);
for(const [from,to,value] of [[6,created,41],[created,6,-41],[created,8,42]]){edited.set('a0',from);edited.set('a1',to);edited.call(0x8019ec78);assert.equal(edited.get('v0')|0,value);}
state(edited,created,0);edited.set('a0',41);edited.call(0x8019e9d0);assert.equal(edited.get('v0'),0);state(edited,created,2);
// Run the native travel waypoint builder in both directions. This is the
// consumer that animates travel along dots rather than merely drawing a line.
for(const [from,to,signedRoad] of [[6,created,41],[created,6,-41]]){
 const m=machine(image),ctx=0x80740000;
 m.w32(0x80197af8,ctx);m.w8(ctx+0xa1,from);m.w8(ctx+0xa2,to);m.w8(ctx+0xe1,signedRoad);m.set('a0',ctx);m.set('t0',1);
 m.call(0x8019c2a0,0x8019c434,100000);
 const points=[];for(let n=0;n<16;++n){const x=m.r16(ctx+0x60+n*2),y=m.r16(ctx+0x80+n*2);if(!x)break;points.push([x,y]);}
 const dots=[];for(let n=0;n<bendCapacity;++n){const x=m.r16(dotAddress+40*roadStride+n*4),y=m.r16(dotAddress+40*roadStride+n*4+2);if(x)dots.push([x,y]);}
 if(signedRoad<0)dots.reverse();
 const position=id=>[image.readInt16BE(0x864a0+(id-1)*8),image.readInt16BE(0x864a2+(id-1)*8)];
 assert.deepEqual(points,[position(from),...dots,position(to)],'native travel follows the newly exported connection');
}
const displayHeader=image.lastIndexOf('LCCAMP01'),displayVersion=image.readUInt32BE(displayHeader+8);
const displayDirectory=displayVersion>=2?image.readUInt32BE(displayHeader+64):0,displayPoints=displayVersion>=2?image.readUInt32BE(displayHeader+68):0;
function renderedPoints(m,roads){
 const points=[];
 for(let r=0;r<roads;r++){
  if(displayDirectory){for(let n=m.r32(displayDirectory+r*4);n<m.r32(displayDirectory+(r+1)*4);n++)points.push([m.r16(displayPoints+n*4),m.r16(displayPoints+n*4+2),5]);}
  else for(let n=0;n<7;n++){const x=m.r16(dotAddress+r*28+n*4),y=m.r16(dotAddress+r*28+n*4+2);if(x)points.push([x,y,5]);}
 }
 return points;
}
// Capture draws from the real native road renderer plus the extension.
{
 const context=0x80740000;edited.w32(0x80197af8,context);edited.w32(context+4,0x80600000);
 const draws=[];edited.trap(0x8019f560,()=>{});edited.trap(0x8019f81c,()=>draws.push([edited.get('a0'),edited.get('a1'),edited.get('a2')]));
 const oldCall=0x0c000000|((0x801a347c>>>2)&0x3ffffff);let entry=0;
 for(let at=0x79730;at<0x85820;at+=4)if(original.readUInt32BE(at)===oldCall)entry=((edited.r32(at+0x80121070)&0x3ffffff)<<2|0x80000000)>>>0;
 assert.ok(entry);edited.set('a0',0);edited.set('a1',0);edited.call(entry,0x80000100,2000000);
 const expected=renderedPoints(edited,count);
 assert.deepEqual(draws,expected,'original and added road dots use the exported positions');
}
console.log(`PASS: native loader DMA/cache bounds; ${comparisons} original path comparisons; added connections in both directions; unlock filtering; native and extension route-dot drawing. Full-console gameplay remains untested.`);
// Exercise the highest signed road ID in the exported allocation. Controlled
// table entries isolate the new consumer's capacity from story unlock scripts.
{
 const m=machine(image),ctx=0x80740000;
 m.w32(roadAddress-16,96);
 for(let id=1;id<=40;++id)state(m,id,1);
 m.load(0x80196a81,Buffer.alloc(5,255));
 const used=new Set();for(let r=0;r<42;r++)used.add([m.r8(roadAddress+r*2),m.r8(roadAddress+r*2+1)].sort((a,b)=>a-b).join(','));
 let r=42;
 for(let a=1;a<=40&&r<96;a++)for(let b=a+1;b<=40&&r<96;b++)if(!used.has([a,b].join(','))){
  m.w8(roadAddress+r*2,a);m.w8(roadAddress+r*2+1,b);
  for(let n=0;n<7;n++){m.w16(dotAddress+r*28+n*4,100+n*10);m.w16(dotAddress+r*28+n*4+2,100+n*5);}
  if(displayDirectory){const begin=m.r32(displayDirectory+r*4);for(let n=0;n<7;n++){m.w16(displayPoints+(begin+n)*4,100+n*10);m.w16(displayPoints+(begin+n)*4+2,100+n*5);}m.w32(displayDirectory+(r+1)*4,begin+7);}
  r++;
 }
 assert.equal(r,96);
 const from=m.r8(roadAddress+190),to=m.r8(roadAddress+191);
 for(const [a,b,expected] of [[from,to,96],[to,from,-96]]){m.set('a0',a);m.set('a1',b);m.call(0x8019ec78);assert.equal(m.get('v0')|0,expected);assert.deepEqual(route(m,a,b),[a,b]);}
 m.w32(0x80197af8,ctx);m.w32(ctx+4,0x80600000);m.trap(0x8019f560,()=>{});
 let drawCount=0;m.trap(0x8019f81c,()=>drawCount++);
 const oldCall=0x0c000000|((0x801a347c>>>2)&0x3ffffff);let entry=0;
 for(let at=0x79730;at<0x85820;at+=4)if(original.readUInt32BE(at)===oldCall)entry=((m.r32(at+0x80121070)&0x3ffffff)<<2|0x80000000)>>>0;
 m.set('a0',0);m.set('a1',0);m.call(entry,0x80000100,2000000);
 const expectedDots=renderedPoints(m,96).length;
 assert.equal(drawCount,expectedDots);
 console.log('PASS: connection96 lookup in both directions, pathfinding and all96 connection drawing loops.');
}
