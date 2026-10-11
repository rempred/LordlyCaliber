'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),{Mips}=require('../../tests/lib/mips-interpreter.cjs');
const rom=fs.readFileSync(process.argv[2]);if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
const m=new Mips();m.ram=new Uint8Array(0x400000);
m.load(0x80232ee8,rom.subarray(0x2a2718,0x2a2964));
m.load(0x802332a0,rom.subarray(0x2a2ad0,0x2a2db0));
const root=0x80300000,owners=0x80310000,keys=0x80312000,images=0x80313000,ages=0x80314000,layout=0x80315000,palettes=0x80316000,flags=0x80317000,input=0x80318000,actor=0x80319000,buffer=0x80320000,converted=0x80330000,sp=0x803ffe00;
let global=0,decodes=[],conversions=0;
m.trap(0x8009daf4,c=>c.set('v0',32));m.trap(0x80071c04,c=>c.set('v0',buffer));
m.trap(0x8009dbb8,()=>{});m.trap(0x8007a7e0,c=>c.set('v0',32));
m.trap(0x8007a110,c=>{m.load(c.get('a0'),Buffer.alloc(32));m.w16(c.get('a0'),0x5554);m.w8(c.get('a0')+2,1);});
m.trap(0x800712c4,()=>{});m.trap(0x80093380,c=>m.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
m.trap(0x801c2304,c=>c.set('v0',global));
m.trap(0x801c15e0,c=>{const flag=c.r32(c.get('sp')+16);decodes.push(flag);c.set('v0',converted);});
m.trap(0x801bfba0,()=>conversions++);
function owner(){m.w32(owners,30);m.w32(owners+8,keys);m.w32(owners+12,2);m.w32(owners+16,1);m.w32(owners+28,images);m.w32(owners+32,ages);m.w32(owners+36,layout);m.w32(owners+52,flags);m.w32(owners+56,palettes);}
function setup(focus,selector){m.load(root,Buffer.alloc(0x1d00));m.load(owners,Buffer.alloc(40*60));m.load(images,Buffer.alloc(16));m.load(ages,Buffer.alloc(16));m.w32(0x8022a974,root);m.w32(0x802395b0,owners);owner();m.w32(input,0);m.w32(keys+16,0x1234);m.w8(flags,0);m.set('sp',sp);m.w32(sp+16,2);if(focus!==null){m.w32(root+0x1a48,root+0x1c00);m.w16(root+0x1c04,focus);}if(selector!==null){m.w32(root+0x1ca4,root+0x1c10);m.w8(root+0x1c10,selector);}decodes=[];conversions=0;}
function touch(slot){m.set('a0',30);m.set('a1',input);m.set('a2',0);m.set('a3',slot);m.w32(sp+16,2);m.call(0x80232ee8);assert.equal(m.get('v0'),converted);assert.equal(m.r32(ages),0);assert.equal(m.get('sp'),sp);}
const decisions=[];
for(const focus of [null,-1,0,1,27])for(const slot of [0,1,27])for(const selector of [null,2,3,4])for(const processing of [0,1,2,3]){
 setup(focus,selector);global=processing;touch(slot);assert.equal(decodes.length,1);const flag=decodes[0];global^=1;touch(slot);assert.equal(decodes.length,1);decisions.push({focus,slot,selector,processing,flag});
}
setup(-1,null);global=0;touch(0);const sequence=[];
function state(action){sequence.push({action,present:!!m.r32(images),age:m.r32(ages),decodes:decodes.slice(),conversions});}
state('normal');m.call(0x80233398);state('convert');global=1;touch(0);state('hit');
for(let i=0;i<3;i++){m.call(0x802332a0);state('age');}
touch(0);state('miss');
m.load(actor,Buffer.alloc(336));m.w32(actor+0xe8,30);m.w8(actor+0x146,2);m.w32(root+0x18,actor);m.w16(root+0x1c04,0);m.call(0x80233448);assert.equal(m.r32(owners),0);assert.equal(m.r32(images),0);owner();state('focus');touch(0);state('focused miss');
m.w16(root+0x1c04,1);touch(0);state('prior focus hit');
fs.writeFileSync(process.argv[3],JSON.stringify({decisions,sequence}));
console.log(`${decisions.length} original image-loader flag and cache-hit cases; original conversion, three-draw expiry, focus refresh and retained prior-focus sequence; decompression, allocation and pixel conversion controlled.`);
