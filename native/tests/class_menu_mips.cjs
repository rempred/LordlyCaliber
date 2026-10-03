'use strict';

// Execute independently assembled old and newly compiled C menu owners with
// identical inputs. No ROM, emulator, or game installation is needed.
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const {Mips} = require('../../tests/lib/mips-interpreter.cjs');
const native = path.resolve(__dirname, '..');
const tools = process.env.LORDLY_N64_TOOLCHAIN || path.join(native, '.local/n64-toolchain');
const pins = JSON.parse(fs.readFileSync(path.join(native, 'rebuild/toolchain.json')));
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
for (const [file, expected] of Object.entries(pins.tools))
  assert.equal(hash(fs.readFileSync(path.join(tools, file))), expected, file);
fs.mkdirSync(path.join(native, 'build'), {recursive: true});
const work = fs.mkdtempSync(path.join(native, 'build/class-menu-'));
const run = (exe, args) => cp.execFileSync(path.join(tools, exe), args, {cwd: work, encoding: 'utf8'});
const menuAddress = 0x801D27E8, lookupAddress = 0x801CF2F0, checkAddress = 0x801D2358;
const source = fs.readFileSync(path.join(native, 'game/promotion/family-menu.c'), 'utf8');
const config = fs.readFileSync(path.join(native, 'game/promotion/class_menu_config.h'), 'utf8');
function extract(name, address, section = '.text', bindings = '') {
  fs.writeFileSync(path.join(work, name + '.ld'), `${bindings}\nFAMILY_LOOKUP = 0x${lookupAddress.toString(16)}; CHECK_CHANGE = 0x${checkAddress.toString(16)};\nSECTIONS { ${section} 0x${address.toString(16)} : { *(${section}) } }\n`);
  run('mips-kmc-elf-as.exe', ['-G', '0', '-mips3', '-mabi=32', '-force-n64align', '-EB', '-o', name+'.o', name+'.s']);
  run('mips-kmc-elf-ld.exe', ['-EB', '-m', 'elf32bmip', '-T', name+'.ld', '-o', name+'.elf', name+'.o']);
  run('mips-kmc-elf-objcopy.exe', ['-O', 'binary', ...['.text','.data','.bss','.rdata','.reginfo','.pdr','.mdebug','.comment','.lit4','.lit8'].filter(x=>x!==section).map(x=>'--remove-section='+x), name+'.elf', name+'.bin']);
  return fs.readFileSync(path.join(work, name+'.bin'));
}
function compile(name, configuration, code = source, address = menuAddress, bindings = '') {
  fs.writeFileSync(path.join(work, 'class_menu_config.h'), configuration);
  fs.writeFileSync(path.join(work, name+'.c'), code);
  run('cpp.exe', ['-P', '-undef', '-nostdinc', '-D__mips=3', '-D__mips__', '-D_MIPSEB', '-D__MIPSEB__', name+'.c', name+'.i']);
  run('cc1.exe', ['-quiet','-O2','-meb','-mips3','-mgp32','-mfp32','-G','0','-fno-PIC','-mno-abicalls','-fno-builtin','-funsigned-char',name+'.i','-o',name+'.s']);
  return extract(name, address, '.text', bindings);
}
function assembly(name, filename, address) {
  fs.writeFileSync(path.join(work, name+'.s'), '.set noat\n.set noreorder\n.section .owner,"ax"\n.balign 4\n' + fs.readFileSync(path.join(native, 'game/promotion', filename), 'utf8'));
  return extract(name, address, '.owner');
}
const maskAddress=0x801CF320;
const maskSource=fs.readFileSync(path.join(native,'game/promotion/compatibility-mask.c'),'utf8');
function fitMask(name,bytes) {
  const elf=fs.readFileSync(path.join(work,name+'.elf')),sections=elf.readUInt32BE(32),count=elf.readUInt16BE(48);
  let definitions=0;
  for(let i=0;i<count;i++) {
    const h=sections+i*40;if(elf.readUInt32BE(h+4)!==2)continue;
    const at=elf.readUInt32BE(h+16),end=at+elf.readUInt32BE(h+20);
    for(let s=at;s<end;s+=16)if((elf[s+12]&15)===2) {
      const begin=elf.readUInt32BE(s+4),size=elf.readUInt32BE(s+8);
      assert(begin>=maskAddress&&size>0&&begin+size<=maskAddress+44,'entire function including delay slot fits');definitions++;
    }
  }
  assert(definitions>0);assert(bytes.subarray(44).every(x=>x===0),'only linker alignment is removed');
  const result=Buffer.alloc(44);bytes.copy(result,0,0,44);return result;
}
const maskCode=fitMask('compatibility_mask',compile('compatibility_mask',config,maskSource,maskAddress));
const maskWide=fitMask('compatibility_mask_wide',compile('compatibility_mask_wide',config.replace('LC_CLASS_COUNT 81','LC_CLASS_COUNT 256').replace('LC_MASK_RESOURCE_OFFSET -1','LC_MASK_RESOURCE_OFFSET 3072'),maskSource,maskAddress));
assert(maskCode.length<=44);assert(maskWide.length<=44);
const maskPadded=Buffer.alloc(44);maskCode.copy(maskPadded);
const maskManifest=JSON.parse(fs.readFileSync(path.join(native,'game/promotion/owners.json'))).owners.find(x=>x.start===0xBBD80);
assert.equal(hash(maskPadded),maskManifest.cSourceSha256);
function maskLookup(code,count,id) {
  const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);cpu.set('sp',0x80060000);cpu.set('a0',id);
  cpu.load(maskAddress,code);cpu.w32(0x80196AF8,0x80280000);cpu.w32(0x80280080,0x80300000);
  for(let i=0;i<count;i++)cpu.w8(count===81?0x801EF2CD+i*2:0x80300C00+i,(i*73+19)&255);
  // All table input and unrelated RAM must be unchanged after the pure lookup.
  cpu.w8=cpu.w16=cpu.w32=()=>{throw Error('lookup changed RAM');};cpu.call(maskAddress);
  return cpu.get('v0');
}
let maskCases=0;
for(let id=0;id<256;id++)for(const highBits of [0,256,0x7FFF0000,0xFFFF0000]) {
  assert.equal(maskLookup(maskCode,81,(id+highBits)>>>0),id<81?(id*73+19)&255:0);
  assert.equal(maskLookup(maskWide,256,(id+highBits)>>>0),(id*73+19)&255);maskCases+=2;
}
if(process.env.LORDLY_TEST_ROM) {
  const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  assert.deepEqual(maskPadded,rom.subarray(0xBBD80,0xBBDAC),'accepted C compatibility getter');
}
console.log(JSON.stringify({maskCases,maskBytes:maskCode.length,maskWideBytes:maskWide.length,maskSha256:hash(maskPadded)}));
const renderBlock = 0x801B82FC, renderAddress = renderBlock + 16, renderEnd = 0x801B8478;
const renderBindings = 'lc_army_owner = 0x80196AF8; lc_class_coordinates = 0x801C6B28; lc_class_matrix = 0x80098C20;';
const renderSource = fs.readFileSync(path.join(native, 'game/promotion/class-render-rows.c'), 'utf8');
const renderCode = compile('render_rows', config, renderSource, renderAddress, renderBindings);
const renderExpanded = compile('render_rows_expanded', config.replace('LC_CLASS_COUNT 81', 'LC_CLASS_COUNT 256').replace('LC_DISABLED_FLAG 128', 'LC_DISABLED_FLAG 32768').replace('LC_LIST_OFFSET 0x1C0E', 'LC_LIST_OFFSET 0x2528'), renderSource, renderAddress, renderBindings);
assert(renderCode.length <= renderEnd-renderAddress, 'retail render body extent');
assert(renderExpanded.length <= renderEnd-renderAddress, 'expanded render body extent');
fs.writeFileSync(path.join(work, 'render_entry.s'), '.set noat\n.set noreorder\n.section .owner,"ax"\n' + fs.readFileSync(path.join(native, 'game/promotion/class-render-entry.s'), 'utf8'));
const renderEntry = extract('render_entry',renderBlock,'.owner',`CLASS_RENDER_ROWS = 0x${renderAddress.toString(16)}; CLASS_RENDER_CONTINUE = 0x${renderEnd.toString(16)};`);
assert.equal(renderEntry.length, 16);
function renderImage(code) {
  const block=Buffer.alloc(renderEnd-renderBlock);
  renderEntry.copy(block);code.copy(block,16);
  return block;
}
const styleAddress=0x801B8C08,styleEnd=styleAddress+76;
function compileStyle(wide) {
  const name='render_style_'+wide;
  fs.writeFileSync(path.join(work,name+'.s'),'.set noat\n.set noreorder\n.section .owner,"ax"\nWIDE_CLASSES = '+wide+'\n'+fs.readFileSync(path.join(native,'game/promotion/class-render-style.s'),'utf8'));
  const code=extract(name,styleAddress,'.owner','CLASS_DRAW_STYLE = 0x8019B5C4; DRAW_CLASS_SPRITE = 0x801BA900;');
  assert.equal(code.length,76);return code;
}
const styleRetail=compileStyle(0),styleExpanded=compileStyle(1);
function renderStyle(code,id,disabled,selected,wide) {
  const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);
  const data=0x80280000,row=data+0x32C;
  cpu.load(styleAddress,code);cpu.load(data,Buffer.alloc(0x1000,0xA6));cpu.w32(0x80196AF8,data);
  cpu.w16(data+0x5E8,selected);cpu.set('s0',row);cpu.set('t0',data);
  if(wide)cpu.w16(row,id|(disabled?0x8000:0));else cpu.w8(row,id|(disabled?0x80:0));
  const before=Buffer.from(cpu.ram.slice(cpu.phys(data),cpu.phys(data)+0x1000)),calls=[];
  for(const [address,name] of [[0x8019B5C4,'style'],[0x801BA900,'sprite']])cpu.trap(address,c=>{
    calls.push([name,c.get('a0')]);
    for(const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(reg,0xCCCCCCCC);
  });
  cpu.call(styleAddress,styleEnd);
  const after=Buffer.from(cpu.ram.slice(cpu.phys(data),cpu.phys(data)+0x1000));before[0x5E4]=after[0x5E4];
  assert.deepEqual(before,after,'style wrote outside its output field');
  return calls;
}
let styleCases=0;
for(const wide of [false,true])for(let id=0;id<(wide?256:128);id++)for(const disabled of [false,true])for(const selected of [id,(id+1)&255]) {
  assert.deepEqual(renderStyle(wide?styleExpanded:styleRetail,id,disabled,selected,wide),[['style',id===selected?(disabled?5:0):(disabled?6:4)],['sprite',id]]);
  styleCases++;
}
function renderRows(code, base, entries, flags, initialRows, selected, expanded=false) {
  const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);
  const renderOwner=0x80280000, matrices=0x80300000, listOffset=expanded?0x2528:0x1C0E;
  cpu.load(base,code);cpu.load(renderOwner,Buffer.alloc(0x2800,0xA6));
  cpu.w32(0x80196AF8,renderOwner);cpu.w16(renderOwner+0x130,flags);
  cpu.w16(renderOwner+0x1BFC,entries.length);cpu.w16(renderOwner+0x5EA,selected);
  cpu.w32(renderOwner+0x5C8,initialRows);cpu.set('sp',0x80060000);cpu.set('s6',matrices);
  entries.forEach((entry,i)=>cpu.w16(renderOwner+listOffset+i*2,entry));
  cpu.w16(renderOwner+listOffset+entries.length*2,0);
  const before=Buffer.from(cpu.ram.slice(cpu.phys(renderOwner),cpu.phys(renderOwner)+0x2800));
  const calls=[];
  const clobber=c=>{for(const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(reg,0xCCCCCCCC);};
  cpu.trap(0x801C6B28,c=>{
    const index=c.get('a2');assert(index<entries.length);
    calls.push(['coordinates',index]);
    c.w32(c.get('a0'),0x40000000|index);c.w32(c.get('a1'),0x42000000|index);clobber(c);
  });
  cpu.trap(0x80098C20,c=>{
    const address=c.get('a0');assert(address>=matrices+0x1B10&&address<matrices+0x1B10+167*64);
    calls.push(['matrix',address,c.get('a1'),c.get('a2'),c.get('a3')]);clobber(c);
  });
  cpu.call(renderBlock,renderEnd);
  assert.equal(cpu.get('s6'),matrices);assert.equal(cpu.get('sp'),0x80060000);
  const after=Buffer.from(cpu.ram.slice(cpu.phys(renderOwner),cpu.phys(renderOwner)+0x2800));
  after.copy(before,0x32C,0x32C,0x5CC);
  assert.deepEqual(after,before,'render wrote outside rows/count');
  return {rows:after.subarray(0x32C,0x5C8), count:cpu.r32(renderOwner+0x5C8),calls};
}
let renderComparisons=0,renderExpandedCases=0,renderOwnerHash;
if(process.env.LORDLY_TEST_ROM) {
  const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  const original=rom.subarray(0xA4A00,0xA71C8);
  assert.deepEqual(styleRetail,rom.subarray(0xA5668,0xA56B4),'retail class draw block');
  const rebuilt=Buffer.from(original);renderImage(renderCode).copy(rebuilt,0x35C);
  renderOwnerHash={original:hash(original),rebuilt:hash(rebuilt)};
  const manifest=JSON.parse(fs.readFileSync(path.join(native,'game/promotion/owners.json'))).owners.find(x=>x.start===0xA4A00);
  assert.equal(renderOwnerHash.original,manifest.retailSha256);
  assert.equal(renderOwnerHash.rebuilt,manifest.cSourceSha256,'render export owner identity');
  for(const count of [0,1,4,20,21,22,40,80,81])for(const flags of [0,0x100,0x200,0x300])for(const initialRows of [0,3])for(const terminated of [false,true]) {
    const entries=Array.from({length:count},(_,i)=>(i+1)%81|(i%2?128:0));
    if(terminated&&count)entries[count>>1]=0;
    const args=[entries,flags,initialRows,0];
    assert.deepEqual(renderRows(rebuilt,0x801B7FA0,...args),renderRows(original,0x801B7FA0,...args),JSON.stringify({count,flags,initialRows,terminated}));
    renderComparisons++;
  }
}
const expandedRender=renderImage(renderExpanded);
for(const mode of [0x100,0x200,0x300])for(const count of [1,21,81,82,165,256])for(let selected=0;selected<count;selected++) {
  const entries=Array.from({length:count},(_,i)=>i|0x8000);
  const result=renderRows(expandedRender,renderBlock,entries,mode,0,selected,true);
  const first=count<=81?0:Math.max(0,Math.floor((selected-40)/4)*4),end=Math.min(count,first+81);
  assert.equal(result.count,end-first);assert(selected>=first&&selected<end);
  const rows=Buffer.alloc(167*4,0xA6),calls=[];
  for(let i=first;i<end;i++) {
    rows.writeUInt16BE(entries[i],(i-first)*4);rows.writeUInt16BE(1,(i-first)*4+2);
    calls.push(['coordinates',i],['matrix',0x80301B10+(i-first)*64,0x40000000|i,0x42000000|i,0]);
  }
  assert.deepEqual(result.rows,rows);assert.deepEqual(result.calls,calls);renderExpandedCases++;
}
for(const initialRows of [166,167,168]) {
  const result=renderRows(expandedRender,renderBlock,[0x8080,0x80A5,0x80FF],0x200,initialRows,0,true);
  assert.equal(result.count,Math.max(initialRows,167));
  assert.equal(result.calls.length,initialRows===166?2:0);renderExpandedCases++;
}
console.log(JSON.stringify({renderComparisons,renderExpandedCases,styleCases,renderBytes:renderCode.length,renderExpandedBytes:renderExpanded.length,renderOwnerHash}));
const previous = assembly('previous', 'family-menu.s', menuAddress);
assert.equal(hash(previous), 'dd8d2826376f6c4831f249a6e3e0f6f211c0b9f24fdda393188dae6f8686b298');
const lookup = assembly('lookup', 'family-lookup.s', lookupAddress);
const compiled = compile('menu', config);
assert(compiled.length <= 528, `C menu exceeds the existing function extent (${compiled.length})`);
const padded = Buffer.alloc(528); compiled.copy(padded);
const menuOwner = JSON.parse(fs.readFileSync(path.join(native, 'game/promotion/owners.json'))).owners.find(x => x.start === 0xbf248);
assert.equal(hash(padded), menuOwner.cSourceSha256, 'export owner identity');
const expanded = compile('expanded', config.replace('LC_CLASS_COUNT 81', 'LC_CLASS_COUNT 256').replace('LC_LIST_CAPACITY 82', 'LC_LIST_CAPACITY 257').replace('LC_DISABLED_FLAG 128', 'LC_DISABLED_FLAG 32768').replace('LC_LIST_OFFSET 0x1C0E', 'LC_LIST_OFFSET 0x2528').replace('LC_DISCOVERY_ADDRESS 0x80196A76U', 'LC_DISCOVERY_ADDRESS 0x80308000U'));
const owner = 0x80260000, resource = 0x80300000;
const categoriesAddress=0x801CF65C;
const categoriesSource=fs.readFileSync(path.join(native,'game/promotion/class-categories.c'),'utf8');
const categoriesCode=compile('class_categories',config,categoriesSource,categoriesAddress,'lc_army_owner = 0x80196AF8;');
const categoriesWide=compile('class_categories_wide',config.replace('LC_CLASS_COUNT 81','LC_CLASS_COUNT 256').replace('LC_MASK_RESOURCE_OFFSET -1','LC_MASK_RESOURCE_OFFSET 3072').replace('LC_DISCOVERY_ADDRESS 0x80196A76U','LC_DISCOVERY_ADDRESS 0x80308000U'),categoriesSource,categoriesAddress,'lc_army_owner = 0x80196AF8;');
assert(categoriesCode.length<=412);assert(categoriesWide.length<=412);
const categoriesPadded=Buffer.alloc(412);categoriesCode.copy(categoriesPadded);
const categoryManifest=JSON.parse(fs.readFileSync(path.join(native,'game/promotion/owners.json'))).owners.find(x=>x.start===0xBC0BC);
assert.equal(hash(categoriesPadded),categoryManifest.cSourceSha256);
function categoryList(code,masks,discovery) {
  const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);cpu.set('sp',0x80060000);
  cpu.load(categoriesAddress,code);cpu.load(owner,Buffer.alloc(0x2800,0xA6));
  cpu.w32(0x80196AF8,owner);cpu.w32(owner+0x80,resource);
  const wide=masks.length===256;
  cpu.load(wide?0x80308000:0x80196A76,discovery);cpu.load(0x801EF370,Buffer.from([1,2,4,8,16,32,64,128]));
  masks.forEach((value,id)=>{
    if(wide)cpu.w8(resource+3072+id,value);else cpu.w16(0x801EF2CC+id*2,((id%11)<<8)|value);
  });
  if(!wide)cpu.w16(0x801EF36E,65535);
  const before=Buffer.from(cpu.ram.slice(cpu.phys(owner),cpu.phys(owner)+0x2800));
  cpu.trap(0x80093380,c=>c.load(c.get('a0'),Buffer.alloc(c.get('a1'))));
  cpu.call(categoriesAddress);
  const count=cpu.get('v0');assert(count<=8);
  const after=Buffer.from(cpu.ram.slice(cpu.phys(owner),cpu.phys(owner)+0x2800));
  after.copy(before,0x1E00,0x1E00,0x1E00+count+1);
  assert.deepEqual(after,before,'category list changed unrelated owner data');
  return {count,categories:[...after.subarray(0x1E00,0x1E00+count+1)]};
}
let categoryComparisons=0,categoryOriginalHash;
if(process.env.LORDLY_TEST_ROM) {
  const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  const original=rom.subarray(0xBC0BC,0xBC258);categoryOriginalHash=hash(original);
  for(let mask=0;mask<256;mask++)for(const found of [0,1,0xA5,255]) {
    const args=[Array(81).fill(mask),Buffer.alloc(11,found)];
    assert.deepEqual(categoryList(categoriesCode,...args),categoryList(original,...args),`category mask=${mask}, discovery=${found}`);categoryComparisons++;
  }
  for(let seed=0;seed<32;seed++) {
    const args=[Array.from({length:81},(_,i)=>(i*23+seed*7)&255),Buffer.alloc(11,seed*17&255)];
    assert.deepEqual(categoryList(categoriesCode,...args),categoryList(original,...args));categoryComparisons++;
  }
}
const categoryMasks=Array(256).fill(0);categoryMasks[165]=128;categoryMasks[249]=3;categoryMasks[255]=64;
assert.deepEqual(categoryList(categoriesWide,categoryMasks,Buffer.alloc(32,255)),{count:4,categories:[1,2,7,8,9]});
assert.deepEqual(categoryList(categoriesWide,categoryMasks,Buffer.alloc(32)),{count:0,categories:[9]});
categoryMasks[249]=5;
assert.deepEqual(categoryList(categoriesWide,categoryMasks,Buffer.alloc(32,255)),{count:3,categories:[1,7,8,9]});
console.log(JSON.stringify({categoryComparisons,categoriesBytes:categoriesCode.length,categoriesWideBytes:categoriesWide.length,categoryOriginalHash,categoriesHash:hash(categoriesPadded)}));
const equipmentAddress=0x801D2B7C;
const equipmentSource=fs.readFileSync(path.join(native,'game/promotion/equipment-class-menu.c'),'utf8');
const equipmentBindings='lc_army_owner = 0x80196AF8; lc_goth = 0x80196A6C; lc_equipment_summary = 0x801D29F8; lc_equipment_cost = 0x801D2EE0; lc_equipment_full = 0x801D2E2C;';
const equipmentCode=compile('equipment_class_menu',config,equipmentSource,equipmentAddress,equipmentBindings);
const equipmentWide=compile('equipment_class_menu_wide',config.replace('LC_CLASS_COUNT 81','LC_CLASS_COUNT 256').replace('LC_LIST_CAPACITY 82','LC_LIST_CAPACITY 257').replace('LC_DISABLED_FLAG 128','LC_DISABLED_FLAG 32768').replace('LC_LIST_OFFSET 0x1C0E','LC_LIST_OFFSET 0x2528').replace('LC_DISCOVERY_ADDRESS 0x80196A76U','LC_DISCOVERY_ADDRESS 0x80308000U').replace('LC_FLAGS_RESOURCE_OFFSET -1','LC_FLAGS_RESOURCE_OFFSET 3328'),equipmentSource,equipmentAddress,equipmentBindings);
assert(equipmentCode.length<=332);assert(equipmentWide.length<=332);
const equipmentPadded=Buffer.alloc(332);equipmentCode.copy(equipmentPadded);
const equipmentManifest=JSON.parse(fs.readFileSync(path.join(native,'game/promotion/owners.json'))).owners.find(x=>x.start===0xBF5DC);
assert.equal(hash(equipmentPadded),equipmentManifest.cSourceSha256);
function equipmentList(code,flags,discovery,money,cost,full) {
  const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);cpu.set('sp',0x80060000);
  const wide=flags.length===256,listOffset=wide?0x2528:0x1C0E;
  cpu.load(equipmentAddress,code);cpu.load(owner,Buffer.alloc(0x2800,0xA6));
  cpu.w32(0x80196AF8,owner);cpu.w32(owner+0x80,resource);cpu.w32(0x80196A6C,money);
  cpu.load(wide?0x80308000:0x80196A76,discovery);
  cpu.load(0x801EF370,Buffer.from([1,2,4,8,16,32,64,128]));
  flags.forEach((value,id)=> {
    if(wide)cpu.w8(resource+3328+id,value);
    else cpu.w16(0x801EF2CC+id*2,((value|(id%11))<<8)|((id*13)&255));
  });
  if(!wide)cpu.w16(0x801EF36E,65535);
  const before=Buffer.from(cpu.ram.slice(cpu.phys(owner),cpu.phys(owner)+0x2800));
  const calls=[],clobber=c=>{for(const reg of ['v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(reg,0xCCCCCCCC);};
  cpu.trap(0x801D29F8,c=>{calls.push(['summary']);clobber(c);});
  cpu.trap(0x801D2EE0,c=>{
    const id=c.get('a0');calls.push(['cost',id]);
    const count=calls.filter(x=>x[0]==='cost').length-1;
    assert.equal(c.r16(owner+listOffset+count*2),id,'row stored before cost callback');
    clobber(c);c.set('v0',cost(id));
  });
  cpu.trap(0x801D2E2C,c=>{const id=c.get('a0');calls.push(['full',id]);clobber(c);c.set('v0',full(id));});
  cpu.call(equipmentAddress);
  const count=cpu.r16(owner+0x1BFC);assert(count<=flags.length);
  assert.equal(cpu.r16(owner+listOffset+count*2),0);
  const entries=Array.from({length:count},(_,i)=>cpu.r16(owner+listOffset+i*2));
  const after=Buffer.from(cpu.ram.slice(cpu.phys(owner),cpu.phys(owner)+0x2800));
  after.copy(before,0x1BFC,0x1BFC,0x1BFE);after.copy(before,listOffset,listOffset,listOffset+(count+1)*2);
  assert.deepEqual(after,before,'equipment list wrote outside its allocation/count');
  return {entries,calls};
}
let equipmentComparisons=0,equipmentOriginalHash;
if(process.env.LORDLY_TEST_ROM) {
  const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  const original=rom.subarray(0xBF5DC,0xBF728);equipmentOriginalHash=hash(original);
  for(const pattern of [0,0x80,0xF0])for(const found of [0,0xA5,255])for(const money of [0,100,0xFFFFFFFF])for(const capacity of [0,0x100,0x101]) {
    const flags=Array.from({length:81},(_,i)=>pattern|(i%3===0?0:0x80));
    const args=[flags,Buffer.alloc(11,found),money,id=>id%7===0?0xFFFFFFFF:id*7,id=>id%2?capacity:0];
    assert.deepEqual(equipmentList(equipmentCode,...args),equipmentList(original,...args));equipmentComparisons++;
  }
}
const equipmentFlags=Array.from({length:256},(_,i)=>i>=128?0xD0:0);
const equipmentActual=equipmentList(equipmentWide,equipmentFlags,Buffer.alloc(32,255),200,id=>id,id=>id%3===0?0x101:0x100);
const equipmentExpected={entries:[],calls:[['summary']]};
for(let id=128;id<256;id++) {
  equipmentExpected.entries.push(id|((id>200||id%3===0)?0x8000:0));
  equipmentExpected.calls.push(['cost',id]);if(id<=200)equipmentExpected.calls.push(['full',id]);
}
assert.deepEqual(equipmentActual,equipmentExpected);
assert.deepEqual(equipmentList(equipmentWide,Array(256).fill(0),Buffer.alloc(32,255),0,()=>{throw Error('empty list cost');},()=>{throw Error('empty list capacity');}),{entries:[],calls:[['summary']]});
const fullEquipment=equipmentList(equipmentWide,[0,...Array(255).fill(0x80)],Buffer.alloc(32,255),0,()=>0,()=>0);
assert.deepEqual(fullEquipment.entries,Array.from({length:255},(_,i)=>i+1));
console.log(JSON.stringify({equipmentComparisons,equipmentBytes:equipmentCode.length,equipmentWideBytes:equipmentWide.length,equipmentOriginalHash,equipmentHash:hash(equipmentPadded)}));
function machine(code, count, listOffset, selected, families, discovery, eligible) {
  const cpu = new Mips();
  cpu.ram = new Uint8Array(0x400000);
  cpu.load(menuAddress, code); cpu.load(lookupAddress, lookup);
  cpu.set('sp', 0x80060000);
  cpu.load(owner, Buffer.alloc(0x2800, 0xA6));
  cpu.w32(0x80196AF8, owner); cpu.w32(owner+0x80, resource);
  cpu.w16(owner+0x5E8, selected); cpu.w8(owner+0x18B, 73);
  families.forEach((value, id) => cpu.w32(resource+648+id*4, value));
  cpu.load(count > 81 ? 0x80308000 : 0x80196A76, discovery);
  cpu.load(0x801EF370, Buffer.from([1,2,4,8,16,32,64,128]));
  const calls = [];
  cpu.trap(checkAddress, c => {
    assert.equal(c.get('a0'), 73);
    const target = c.get('a1'); assert(target < count);
    calls.push(target);
    c.set('v0', eligible(target));
    for (const reg of ['v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9']) c.set(reg, 0xCCCCCCCC);
  });
  const before = Buffer.from(cpu.ram.subarray(owner & 0x1FFFFFFF, (owner & 0x1FFFFFFF)+0x2800));
  cpu.call(menuAddress);
  const size = cpu.r16(owner+0x1BFC);
  assert(size <= count);
  assert.equal(cpu.r16(owner+listOffset+size*2), 0, 'terminator');
  const entries = Array.from({length: size}, (_, i) => cpu.r16(owner+listOffset+i*2));
  const after = Buffer.from(cpu.ram.subarray(owner & 0x1FFFFFFF, (owner & 0x1FFFFFFF)+0x2800));
  // Only the reported count and the produced list may change in the Army owner.
  after.copy(before, 0x1BFC, 0x1BFC, 0x1BFE);
  after.copy(before, listOffset, listOffset, listOffset+(size+1)*2);
  assert.deepEqual(after, before, 'write outside list/count');
  return {entries, calls};
}
let comparisons = 0;
for (let behavior = 0; behavior <= 10; behavior++) {
  const families = Array.from({length:81}, (_, id) => id%3 === 0 ? (behavior<<16)|0x1234 : (5<<16)|0x4321);
  families[0] = 0;
  if (!behavior) families[3] = 0;
  for (const selected of [0,3,6,38,43,80,81,127,128,164,255]) {
    for (const pattern of [0,0xFF,0xA5]) {
      const discovery = Buffer.alloc(11, pattern);
      for (const eligibility of [() => 0, () => 1, id => id%2]) {
        const old = machine(previous, 81, 0x1C0E, selected, families, discovery, eligibility);
        const next = machine(compiled, 81, 0x1C0E, selected, families, discovery, eligibility);
        assert.deepEqual(next, old, JSON.stringify({behavior, selected, pattern}));
        comparisons++;
      }
    }
  }
}
// Independent explicit expectations cover the widened format, full list bound,
// upper IDs, the selected-row disable flag, and discovery across byte boundaries.
const families = Array(256).fill((5<<16)|16);
families[0] = 0;
const full = machine(expanded, 256, 0x2528, 165, families, Buffer.alloc(32, 255), () => 1);
assert.deepEqual(full.entries, Array.from({length:255}, (_, i) => i+1 === 165 ? 0x80A5 : i+1));
const sparse = Buffer.alloc(32);
for (const id of [1,127,128,165,254,255]) sparse[id>>3] |= 1 << (id&7);
const partial = machine(expanded, 256, 0x2528, 128, families, sparse, id => id%2);
assert.deepEqual(partial.entries, [1,127,0x8080,165,0x80FE,255]);
// All 256 IDs can be members; one extra halfword is reserved for termination.
families[0] = (5<<16)|16;
assert.equal(machine(expanded, 256, 0x2528, 255, families, Buffer.alloc(32,255), () => 1).entries.length, 256);
const selectionAddress = 0x801DC740;
const selectionSource = fs.readFileSync(path.join(native, 'game/promotion/class-selection.c'), 'utf8');
const selectionBindings = 'lc_army_owner = 0x80196AF8; lc_input_buttons = 0x800E8100; lc_repeat_buttons = 0x800E8700; lc_menu_sound = 0x801D8A60; lc_menu_update = 0x801CFC24; lc_menu_equipment = 0x801D2B7C;';
const selection = compile('selection', config, selectionSource, selectionAddress, selectionBindings);
assert(selection.length <= 388, 'selection owner extent');
const selectionPadded = Buffer.alloc(388); selection.copy(selectionPadded);
const selectionOwner = JSON.parse(fs.readFileSync(path.join(native, 'game/promotion/owners.json'))).owners.find(x=>x.start===0xc91a0);
assert.equal(hash(selectionPadded), selectionOwner.cSourceSha256, 'selection export owner identity');
const selectionExpanded = compile('selection_expanded', config.replace('LC_DISABLED_FLAG 128', 'LC_DISABLED_FLAG 32768').replace('LC_LIST_OFFSET 0x1C0E', 'LC_LIST_OFFSET 0x2528'), selectionSource, selectionAddress, selectionBindings);
function select(code, entries, selected, buttons, repeat, flags, listOffset) {
  const cpu = new Mips(); cpu.ram = new Uint8Array(0x400000);
  cpu.load(selectionAddress, code); cpu.set('sp', 0x80060000);
  cpu.w32(0x80196AF8, owner);
  cpu.w16(owner+0x5EA, selected); cpu.w16(owner+0x5E8, 23);
  cpu.w16(owner+0x1BFC, entries.length); cpu.w16(owner+0x130, flags);
  cpu.w16(0x800E8100, buttons); cpu.w16(0x800E8700, repeat);
  entries.forEach((entry, i) => cpu.w16(owner+listOffset+i*2, entry));
  const calls = [];
  for (const [address, name] of [[0x801D8A60,'sound'],[0x801CFC24,'update'],[0x801D2B7C,'equipment']]) {
    cpu.trap(address, c => {
      if (name === 'sound') assert.equal(c.get('a0'), 1);
      calls.push(name);
      for (const reg of ['v0','v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9']) c.set(reg, 0xCCCCCCCC);
    });
  }
  cpu.call(selectionAddress);
  return {result:cpu.get('v0'), selected:cpu.r16(owner+0x5EA), classId:cpu.r16(owner+0x5E8), calls};
}
let selectionComparisons = 0;
if (process.env.LORDLY_TEST_ROM) {
  const rom = fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if (rom.readUInt32BE(0) === 0x37804012) rom.swap16();
  assert.equal(hash(rom), '571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  const original = rom.subarray(0xC91A0,0xC9324);
  for (const count of [1,2,3,4,5,8,17,81]) {
    const entries = Array.from({length:count}, (_,i) => ((i*17)%81) | (i%2 ? 128 : 0));
    for (let selected=0; selected<count; selected++)
      for (const [buttons,repeat] of [[0,0],[0,0x800],[0,0x400],[0,0x200],[0,0x100],[0,0xF00],[0x1000,0xF00],[0x8000,0xF00],[0x4000,0xF00],[0xD000,0xF00]])
        for (const flags of [0,0x200]) {
          const args = [entries,selected,buttons,repeat,flags,0x1C0E];
          assert.deepEqual(select(selection,...args), select(original,...args), JSON.stringify({count,selected,buttons,repeat,flags}));
          selectionComparisons++;
        }
  }
}
const wideEntries = Array.from({length:256}, (_,i) => i | (i%2 ? 0x8000 : 0));
for (const [start, repeat, target] of [[126,0x400,127],[129,0x800,128],[161,0x100,165],[250,0x100,254],[251,0x100,255],[255,0x400,255]]) {
  const result = select(selectionExpanded,wideEntries,start,0,repeat,0,0x2528);
  assert.equal(result.selected,target);
  assert.equal(result.classId,start===target ? 23 : target);
}
assert.deepEqual(select(selectionExpanded,[],0,0,0x400,0,0x2528), {result:0,selected:0,classId:23,calls:[]});
const discoveryAddress=0x801D734C, eligibilityAddress=0x801D6EA0;
const discoverySource=fs.readFileSync(path.join(native,'game/promotion/class-discovery.c'),'utf8');
const discoveryBindings='lc_roster = 0x80193BF8; lc_named_class_map = 0x801EF3B8; lc_class_eligible = 0x801D6EA0;';
const discoveryCode=compile('discovery',config,discoverySource,discoveryAddress,discoveryBindings);
assert(discoveryCode.length<=420,'discovery owner extent');
const discoveryPadded=Buffer.alloc(420);discoveryCode.copy(discoveryPadded);
const discoveryOwner=JSON.parse(fs.readFileSync(path.join(native,'game/promotion/owners.json'))).owners.find(x=>x.start===0xc3dac);
assert.equal(hash(discoveryPadded),discoveryOwner.cSourceSha256,'discovery export owner identity');
const discoveryExpanded=compile('discovery_expanded',config.replace('LC_CLASS_COUNT 81','LC_CLASS_COUNT 256').replace('LC_RECORD_COUNT 165','LC_RECORD_COUNT 256').replace('LC_DISCOVERY_SCAN_COUNT 82','LC_DISCOVERY_SCAN_COUNT 256').replace('LC_DISCOVERY_ADDRESS 0x80196A76U','LC_DISCOVERY_ADDRESS 0x80308000U'),discoverySource,discoveryAddress,discoveryBindings);
function discover(code, count, roster, aliases, initial, eligible) {
  const cpu=new Mips();cpu.ram=new Uint8Array(0x400000);cpu.set('sp',0x80060000);
  cpu.load(discoveryAddress,code);cpu.load(0x80193BF8,roster);
  cpu.load(0x801EF3B8,aliases);cpu.load(0x801EF3B0,Buffer.from([1,2,4,8,16,32,64,128]));
  // Original class discovery uses this sentinel to end the eligible-class walk.
  cpu.load(0x801EF2CC,Buffer.alloc(164));cpu.w16(0x801EF2CC+162,65535);
  const flags=count===81?0x80196A76:0x80308000;
  cpu.load(flags-8,Buffer.alloc(initial.length+16,0xA6));cpu.load(flags,initial);
  const calls=[];
  cpu.trap(eligibilityAddress,c=> {
    const actor=c.get('a0'),target=c.get('a1');
    assert(actor>=1 && actor<99);assert(target<=count);
    assert(target<256);calls.push([actor,target]);
    const answer=eligible(actor,target);
    for(const reg of ['v1','a0','a1','a2','a3','t0','t1','t2','t3','t4','t5','t6','t7','t8','t9'])c.set(reg,0xCCCCCCCC);
    c.set('v0',answer);
  });
  cpu.call(discoveryAddress,0x80000100,3000000);
  assert.deepEqual(Buffer.from(cpu.ram.slice(cpu.phys(flags-8),cpu.phys(flags))),Buffer.alloc(8,0xA6));
  assert.deepEqual(Buffer.from(cpu.ram.slice(cpu.phys(flags+initial.length),cpu.phys(flags+initial.length+8))),Buffer.alloc(8,0xA6));
  assert.deepEqual(Buffer.from(cpu.ram.slice(cpu.phys(0x80193BF8),cpu.phys(0x80193BF8)+roster.length)),roster);
  return {flags:Buffer.from(cpu.ram.slice(cpu.phys(flags),cpu.phys(flags)+initial.length)),calls};
}
let discoveryComparisons=0;
if(process.env.LORDLY_TEST_ROM) {
  const rom=fs.readFileSync(process.env.LORDLY_TEST_ROM);
  if(rom.readUInt32BE(0)===0x37804012)rom.swap16();
  assert.equal(hash(rom),'571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a');
  const original=rom.subarray(0xC3DAC,0xC3F50);
  assert.equal(hash(original),discoveryOwner.retailSha256,'original discovery owner identity');
  const aliases=rom.subarray(0xDBE18,0xDBE18+165);
  for(let seed=0;seed<16;seed++)for(const fill of [0,0xA5,255])for(const rule of [0,1,2]) {
    const roster=Buffer.alloc(99*56);
    for(let actor=0;actor<99;actor++) {
      roster[actor*56+17]=(actor*13+seed*17)%165;
      roster[actor*56+18]=(actor*19+seed*7)%165;
    }
    const eligible=(actor,target)=>rule===0?0:rule===1?(actor===(target*7)%98+1?1:0):(target%3===0?0x101:0);
    const args=[81,roster,aliases,Buffer.alloc(11,fill),eligible];
    const actual=discover(discoveryCode,...args),expected=discover(original,...args);
    assert.deepEqual(actual.flags,expected.flags,JSON.stringify({seed,fill,rule}));
    assert.equal(JSON.stringify(actual.calls),JSON.stringify(expected.calls),`discovery callback order: ${seed}/${fill}/${rule}`);
    discoveryComparisons++;
  }
}
const roster=Buffer.alloc(99*56),aliases=Buffer.alloc(256),initialFlags=Buffer.alloc(32);
for(const [actor,primary,secondary] of [[1,165,165],[2,200,200],[99,255,255]]) {
  roster[(actor-1)*56+17]=primary;roster[(actor-1)*56+18]=secondary;
}
aliases[200]=201;
const discovered=discover(discoveryExpanded,256,roster,aliases,initialFlags,(actor,target)=>actor===98&&target===254?1:0);
const expectedFlags=Buffer.alloc(32);for(const id of [165,201,254,255])expectedFlags[id>>3]|=1<<(id&7);
assert.deepEqual(discovered.flags,expectedFlags);
assert(discovered.calls.every(([actor,target])=>actor<99&&target<256));
assert.deepEqual(discover(discoveryExpanded,256,Buffer.alloc(99*56),aliases,Buffer.alloc(32,255),()=>{throw Error('already discovered');}).calls,[]);
console.log(JSON.stringify({comparisons, expandedCases:3, compiledBytes:compiled.length, ownerSha256:hash(padded), selectionComparisons, selectionExpandedCases:7, selectionBytes:selection.length, selectionSha256:hash(selectionPadded), discoveryComparisons, discoveryExpandedCases:2, discoveryBytes:discoveryCode.length, discoverySha256:hash(discoveryPadded), work}, null, 2));
