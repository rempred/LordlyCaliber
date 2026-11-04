'use strict';
// Static cross-feature ownership audit. ROM addresses are normalized z64.
// Authentication/bypass spans count as claims too: disjoint hook words can
// still discard each other's behavior when one replaces the enclosing function.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../game');
const read=name=>JSON.parse(fs.readFileSync(path.join(root,name),'utf8'));
function claims(){
 const result=[];
 const add=(group,name,start,size)=>{
  if(!Number.isSafeInteger(start)||!Number.isSafeInteger(size)||start<0||size<=0||start+size>0x4000000)
   throw Error(`Invalid ROM claim: ${group}:${name}`);
  result.push({group,name,start,end:start+size});
 };
 for(const file of ['tools/high-attack.json','tools/experience.json','tools/chaos.json','art/animation-overrides.json','attacks/effect-targets.json'])
  for(const row of read(file).sites)add(file,row.name||'',row.rom??row.offset,row.size??row.original.length/2);
 for(const feature of read('tools/displays.json').features)
  for(const row of feature.sites)add('display-'+feature.name,row.name,row.rom,row.size);
 for(const file of ['tools/game-rules.json','tools/status-effects.json'])
  for(const [name,row] of Object.entries(read(file)))add(file,name,row.offset,row.size);
 for(const file of ['encounters/custom-owners/catalog.json','classes/save-owners.json','shops/owners.json','scenarios/capacity/owners.json'])
  for(const row of read(file).owners)add(file,row.name||row.symbol||'',row.start,row.size);
 for(const file of ['classes/relocations.json','items/relocations.json','attacks/relocations.json'])
  for(const row of read(file).owners){
   for(const ref of row.references)for(const key of ['high','low'])add(file,row.start.toString(16),row.start+ref[key],4);
   for(const ref of row.calls||[])add(file,row.start.toString(16),row.start+ref.offset,4);
  }
 for(const file of ['classes/text-owners.json','items/text-owners.json'])
  for(const row of read(file).owners)
   for(const key of ['high','low','word'])if(key in row)add(file,row.start.toString(16),row.start+row[key],4);
 for(const row of read('attacks/hooks.json').hooks)
  add('attacks/hooks.json',row.entry,row.rom,row.original?(row.sharedOwnerSize||8):row.size);
 for(const row of read('scenarios/capacity-references.json').owners)
  for(const ref of row.references){
   add('capacity-references',row.start.toString(16),ref.high,4);
   for(const use of ref.uses)add('capacity-references',row.start.toString(16),use.low,4);
  }
 for(const file of ['scenarios/capacity-bounds.json','scenarios/capacity-save-owners.json'])
  for(const row of read(file).owners)
   for(const ref of row.patches)add(file,row.start.toString(16),ref.site,4);
 for(const row of read('scenarios/capacity-extra.json').patches)add('capacity-extra',row.name,row.site,row.end-row.site);
 for(const row of read('scenarios/capacity-predicates.json').patches)add('capacity-predicates',row.name,row.hook,8);
 for(const row of read('campaign/owners.json').owners)add('campaign',row.start.toString(16),row.start,row.end-row.start);
 const affinity=fs.readFileSync(path.resolve(root,'../engine/leader_affinity.h'),'utf8')
  .match(/offset\s*=\s*(0x[\da-f]+),\s*size\s*=\s*(0x[\da-f]+)/i);
 if(!affinity)throw Error('Combat-affinity owner declaration changed; update the ownership reader');
 add('combat-affinity','native eligibility and compiled group lookup',parseInt(affinity[1],16),parseInt(affinity[2],16));
 const flagSlots=fs.readFileSync(path.resolve(root,'../engine/class_data.h'),'utf8')
  .match(/flagGetterSlot\s*=\s*(0x[\da-f]+),\s*flagSetterSlot\s*=\s*(0x[\da-f]+)/i);
 if(!flagSlots)throw Error('Primary custom story flag slot declarations changed');
 for(const slot of flagSlots.slice(1))add('custom-story-flags','primary VM service12',parseInt(slot,16),4);
 const locationText=fs.readFileSync(path.resolve(root,'../engine/location_text.h'),'utf8')
  .match(/directoryWord\s*=\s*(0x[\da-f]+)/i);
 if(!locationText)throw Error('Location description directory declaration changed');
 add('location-descriptions','help resource directory',parseInt(locationText[1],16),4);
 return result;
}
const family=name=>/^(capacity|scenarios\/capacity)/.test(name)?'capacity':name;
function overlaps(rows){
 const found=[];
 const sorted=[...rows].sort((a,b)=>a.start-b.start);
 for(let i=0;i<sorted.length;i++)for(let j=i+1;j<sorted.length&&sorted[j].start<sorted[i].end;j++){
  const a=sorted[i],b=sorted[j];
  // Internal relocation maps already reject conflicting writes. This gate
  // covers the missing boundary between independently implemented features.
  if(family(a.group)===family(b.group))continue;
  found.push({pair:[a.group,b.group].sort().join(' + '),start:Math.max(a.start,b.start),end:Math.min(a.end,b.end),a,b});
 }
 return found;
}
function check(rows,policies){
 const found=overlaps(rows),failures=[];
 for(const hit of found){
  const policy=policies[hit.pair];
  if(!policy||!policy.reason||!policy.ranges.some(([start,end])=>hit.start>=parseInt(start,16)&&hit.end<=parseInt(end,16)))
   failures.push(`${hit.pair} at z64 0x${hit.start.toString(16)}..0x${hit.end.toString(16)} (${hit.a.name}, ${hit.b.name})`);
 }
 if(failures.length)throw Error('Unreviewed runtime sharing. Compose both behaviors and add a combination regression before accepting:\n'+failures.join('\n'));
 return found;
}
function fixedStorage(){
 const rom=[],ram=[],engine=path.resolve(root,'../engine');
 for(const name of fs.readdirSync(engine).filter(n=>n.endsWith('.h'))){
  const text=fs.readFileSync(path.join(engine,name),'utf8');
  for(const match of text.matchAll(/descriptor(?:Rom)?Offset\s*=\s*(0x[\da-f]+),\s*descriptorSize\s*=\s*(\d+)/gi))
   rom.push({group:name,name:'descriptor',start:parseInt(match[1],16),end:parseInt(match[1],16)+Number(match[2])});
 }
 const add=(name,start,size,address)=>{
  rom.push({group:name,name,start,end:start+size});
  if(address>=0x80400000)ram.push({group:name,name,start:address,end:address+size});
 };
 for(const file of ['tools/high-attack.json','tools/chaos.json'])
  for(const row of read(file).sites)if(row.rom>=0x2700000)add(file,row.rom,row.size,row.ram);
 for(const feature of read('tools/displays.json').features)
  for(const row of feature.sites)if(row.rom>=0x2700000)add(feature.name,row.rom,row.size,row.ram);
 const animation=read('art/animation-overrides.json').layout;
 add('animation override',animation.moduleZ64,animation.moduleSize,animation.moduleRam);
 const reserve=read('tools/game-rules.json').reserve;
 add('reserve XP',reserve.offset,reserve.size,reserve.address);
 const promotion=fs.readFileSync(path.join(engine,'expanded_promotion_resource.h'),'utf8').match(/romOffset\s*=\s*(0x[\da-f]+),\s*slotSize\s*=\s*(\d+)/i);
 if(!promotion)throw Error('Expanded promotion storage declaration changed; update the ownership reader');
 add('promotion artwork',parseInt(promotion[1],16),Number(promotion[2]));
 const item=fs.readFileSync(path.join(engine,'class_text.h'),'utf8').match(/itemDescriptorOffset\s*=\s*(0x[\da-f]+)/i);
 if(!item)throw Error('Item description storage declaration changed; update the ownership reader');
 add('item description descriptor',parseInt(item[1],16),64);
 return {rom,ram};
}
function checkStorage(storage){
 for(const [space,rows] of Object.entries(storage)){
  for(const row of rows)
   if(row.start<0||row.end<=row.start||row.end>(space==='ram'?0x80800000:0x2800000))
    throw Error(`${space} allocation exceeds its reserved area: ${row.name}`);
  const sorted=[...rows].sort((a,b)=>a.start-b.start);
  for(let i=1;i<sorted.length;i++)if(sorted[i].start<sorted[i-1].end)
   throw Error(`${space} allocation collision: ${sorted[i-1].group} and ${sorted[i].group}`);
 }
}
module.exports={claims,overlaps,check,fixedStorage,checkStorage};
if(require.main===module){
 const rows=claims(),policies=JSON.parse(fs.readFileSync(path.join(__dirname,'runtime-sharing.json'),'utf8'));
 const hits=check(rows,policies);
 console.log(`${rows.length} catalogued ROM claims checked; ${hits.length} cross-feature intersections have explicit composition contracts.`);
 const storage=fixedStorage();checkStorage(storage);
 console.log(`${storage.rom.length} fixed ROM storage regions and ${storage.ram.length} extension RAM modules are disjoint.`);
 console.log('Scope: native catalogued writes/bypasses and fixed storage. Dynamic sizes, overlay lifetimes, imported legacy arenas and gameplay require exporter/instruction tests too.');
}
