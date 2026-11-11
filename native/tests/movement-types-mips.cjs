'use strict';
// Original entry points in an exported ROM. Terrain interpolation/height and
// unrelated save-name fixups are controlled; movement math, priority and the
// movement-cache reconstruction execute as exported MIPS instructions.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const editor=path.resolve(__dirname,'../..');
const {FloatMips}=require(path.join(editor,'native/tests/float-mips.cjs'));
const owners=require(path.join(editor,'native/game/accepted/rom-owners.json')).sources;
const candidate=fs.readFileSync(process.argv[2]),original=fs.readFileSync(process.argv[3]);
for(const r of [candidate,original])if(r.readUInt32BE(0)===0x37804012)r.swap16();
const movementHeader=candidate.subarray(0x27c5700,0x27c5740);
assert.equal(movementHeader.subarray(0,8).toString(),'LCMOVE01');
const movementOffset=movementHeader.readUInt32BE(12),movementRam=movementHeader.readUInt32BE(16),data=candidate.subarray(movementOffset,movementOffset+movementHeader.readUInt32BE(20));
const model=JSON.parse(data.subarray(data.readUInt32BE(8),data.readUInt32BE(8)+data.readUInt32BE(12)));
assert.equal(model.types['8'].name,'Scout');assert.equal(model.types['8'].baseSpeed,60);
const unit=0x80600000,grid=0x80610000,record=0x80620000;
const source=name=>owners.find(s=>s.name.toLowerCase()===name.toLowerCase());
function machine(rom){const m=new FloatMips();m.ram=new Uint8Array(0x800000);m.load(0x80070000,rom.subarray(0x400,0x2f000));m.load(0x8016af80,rom.subarray(0x40e80,0x66e10));m.load(0x801ad5a0,rom.subarray(0x101ce0,0x145210));
 if(rom.subarray(0x5dc80,0x5dc88).toString()==='LC05CLAS'){const at=rom.readUInt32BE(0x5dc8c),size=rom.readUInt32BE(0x5dc90),ram=rom.readUInt32BE(0x5dc98);m.load(ram,rom.subarray(at,at+size));}
 m.load(0x801f36d8,rom.subarray(0x13fa58,0x13fc58));m.set('sp',0x807ff000);return m;}
function step(rom,type,terrain,group,curve,destination=5){const m=machine(rom);m.load(grid,Buffer.alloc(4096,terrain));m.w8(0x800e7ac3,0);m.w32(0x800e7a90,grid);
 for(const[a,v]of[[0x801f0d98,0],[0x801f0d9c,0],[0x801f0da0,64],[0x801f0da4,64]])m.writeFloat(a,v);
 m.w32(unit+0x70,type);m.w32(unit+0x24,1);for(const[a,v]of[[8,1],[16,1],[0x28,destination],[0x30,1]])m.writeFloat(unit+a,v);
 const routine=source(group?'func_0012C788':'func_00125460');let heights=[];
 for(const key of ['func_801A63FC','func_801A66C8'])if(routine.bindings[key])m.trap(routine.bindings[key],c=>{heights.push(key);c.setSingle(0,0);});
 if(curve){m.w32(unit+0xa4,record);m.writeFloat(record+0x18,0.01);m.writeFloat(record+0x1c,0.2);m.trap(routine.bindings.func_0011AB74,c=>{m.writeFloat(c.get('a2'),2);m.writeFloat(c.get('a3'),1);});}
 m.set('a0',0);m.set('a1',unit);m.set('a2',group);m.set('a3',0);m.call(routine.address);
 return{value:m.readFloat(curve?record+0x1c:unit+8),heights};}
let cases=0;
for(const type of [1,2,3,4,5,6])for(const terrain of [1,3,5,7])for(const group of [0,1,5])for(const curve of [false,true]){
 assert.deepEqual(step(candidate,type,terrain,group,curve),step(original,type,terrain,group,curve),`retail ${type}/${terrain}/${group}/${curve}`);cases++;
}
for(const [terrain,cost]of[[1,1],[5,4],[7,8]])for(const group of[0,1,3,5])for(const curve of[false,true]){
 const factor=60-(group?cost*2*10/(11-group):cost*2),expected=curve?0.2+0.01*factor/100:1+(3/256)*factor/100;
 const actual=step(candidate,8,terrain,group,curve);assert.ok(Math.abs(actual.value-expected)<2e-7,`Scout ${terrain}/${group}/${curve}: ${actual.value}/${expected}`);cases++;
}
for(const curve of[false,true]){assert.deepEqual(step(candidate,9,5,0,curve),step(original,1,5,0,curve),'Copied Sky retains flying height');cases++;}
for(const group of[0,1,5]){const actual=step(candidate,10,1,group,false,1.015);assert.ok(Math.abs(actual.value-1.015)<1e-6,'Fast type must stop at the waypoint');cases++;}
// Movement availability checks the physical kind, including custom immobile IDs.
for(const type of[2,7,11]){const m=machine(candidate);m.w32(0x801f367c,1);m.w32(0x801f0cb0,unit);m.w32(unit,2);m.w8(unit+4,30);m.w32(unit+0x70,type);m.w8(0x8018f481,1);m.w32(0x801f0de0,0);m.call(0x801ad5a0+0x10756c-0x101ce0);assert.equal(m.r32(unit)&0x40000,type===2?0x40000:0);cases++;}
// Flying types bypass ground-region restrictions when seeking supplies.
for(const type of[1,9]){const m=machine(candidate),r=source('func_00124520');m.w32(0x801f1070,1);m.w32(0x801f361c,1);m.w8(0x8018f481,0x27);m.w16(0x801951cc,4);m.w8(0x800e7ac3,0);m.w32(0x800e7a90,grid);m.load(grid,Buffer.alloc(4096,1));m.w32(unit+0x70,type);m.writeFloat(0x801f0d98,0);m.writeFloat(0x801f0d9c,0);m.writeFloat(0x801f0da0,64);m.writeFloat(0x801f0da4,64);let groundQueries=0;
 m.trap(r.bindings.func_0012EA80,c=>{m.writeFloat(c.get('a1'),1);m.writeFloat(c.get('a1')+4,0);m.writeFloat(c.get('a1')+8,1);});m.trap(r.bindings.func_0013A558,c=>{groundQueries++;m.w8(c.get('a3'),1);});m.set('a0',unit);m.set('a1',record);m.call(0x801ad5a0+0x12469c-0x101ce0);assert.equal(m.get('v0'),0);assert.equal(groundQueries,0);cases++;}
const m=machine(candidate);[8,2,1,0].forEach((v,i)=>m.set('a'+i,v));m.w32(m.get('sp')+16,0);m.call(0x8016b088);assert.equal(m.get('v0'),8);cases++;
// Battle callers receive the original physical behavior, Army aggregation the custom ID.
m.set('a0',5);m.set('a1',5);m.call(0x8016dbd8);assert.equal(m.get('v0'),2);cases++;
const army=source('func_000bd154'),heap=0x80670000;m.load(army.address,candidate.subarray(0xbd154,0xbd268));m.w32(0x80196af8,heap);m.load(heap+0x117c,Buffer.alloc(54,255));m.w8(heap+0x117c+4,1);m.w8(0x80193bc0+56+17,5);m.w8(0x80193bc0+56+18,5);m.set('a0',0);m.call(army.address);assert.equal(m.get('v0'),8);cases++;
// The normal save load callback reconstructs the derived 3-bit movement caches.
const loader=source('func_00042dc4');let decoded=false;
for(const[key,address]of Object.entries(loader.bindings))if(/^func_|boot_bitstream/.test(key))m.trap(address,c=>c.set('v0',0x80680000));
m.trap(loader.bindings.boot_bitstream_descriptor_decode,()=>{decoded=true;m.w8(0x801971f0+25+1,1);m.w8(0x801971f0+25+2,1);m.w8(0x801971f0+25+24,0);m.w8(0x801969b8+1,1);m.load(0x801969b8+2,Buffer.from([1,255,255,255,255]));m.w8(0x801969b8+9,0);});
m.set('a0',0x80690000);m.call(loader.address);assert.ok(decoded);assert.equal(m.r8(0x801971f0+25+24),8);assert.equal(m.r8(0x801969b8+9),8);cases++;
console.log(`PASS ${cases} exported movement cases: retail parity, Scout terrain/group speeds, copied Sky, mixed priority, battle/Army lookup separation and normal-load cache restoration.`);
