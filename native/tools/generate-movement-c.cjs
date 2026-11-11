'use strict';
// Authenticated accepted C, with movement policy/data substitutions only.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {digest}=require('./decomp-evidence.cjs');
const root=path.resolve(__dirname,'..'),catalog=require('../game/accepted/rom-owners.json');
const out=path.join(root,'game/movement');fs.mkdirSync(out,{recursive:true});
const ids=['00043ad8','000bd154','00042dc4','00040f88','00101DDC','00105CC0','00106CE0','00121F38','00124520','00125460','00126D24','00127D30','001284C4','0012B440','0012C788','0012D170','0012D6FC','00146094','001960A8','0021D7F0','0021E50C'];
const owners=[];
for(const id of ids){
 const g=catalog.sources.find(s=>s.name.toLowerCase()===('func_'+id).toLowerCase());assert(g,id);
 let s=fs.readFileSync(path.join(root,g.source),'utf8');assert.equal(digest(Buffer.from(s)),g.sourceSha256);s=s.replace(/\r\n/g,'\n');
 const variants=[{s,capacity:false}];
 const capacityFile=path.join(root,'game/scenarios/capacity',`func_${id}.c`);
 if(fs.existsSync(capacityFile))variants.push({s:fs.readFileSync(capacityFile,'utf8').replace(/\r\n/g,'\n'),capacity:true});
 for(const variant of variants){s=variant.s;
  // Only map-unit movement comparisons; retain the actual type for cost rows.
  s=s.replace(/((?:FIELD|WAKE_FIELD)\(unit, s32(?: \*)?,\s*0x70\))\s*(==|!=)\s*([17])\b/g,'lc_movement_kind($1) $2 $3');
  s=s.replace(/(\(\*\(s32 \*\)\(\(s8 \*\)\((?:temp_s1_34|temp_s1_882|cursor1)\) \+ \(0x70\)\)\))\s*(==|!=)\s*([17])\b/g,'lc_movement_kind($1) $2 $3');
  if(id==='00146094')s=s.replace('((*(int *)((signed char *)(arg0) + 0x70)) == 1)','(lc_movement_kind(*(int *)((signed char *)(arg0) + 0x70)) == 1)');
  if(id==='00126D24')s=s.replaceAll('if (type == 1)','if (lc_movement_kind(type) == 1)').replaceAll('if (type == 7)','if (lc_movement_kind(type) == 7)');
  if(id==='00124520')s=s.replaceAll('if (kind == 1)','if (lc_movement_kind(kind) == 1)');
  if(id==='00106CE0')s=s.replaceAll('unit->field_70 == 7','lc_movement_kind(unit->field_70) == 7');
  if(id==='00105CC0')s=s.replace('((*(u8 *)((s8 *)(((func_0012E968(temp_s1_34) * 0xB) + ((u32)D_801969BA))) + (7))) != 7)', '(lc_movement_kind((*(u8 *)((s8 *)(((func_0012E968(temp_s1_34) * 0xB) + ((u32)D_801969BA))) + (7)))) != 7)');
  if(id==='00043ad8')s=s.replace('u8 func_00043ad8(', 'u8 lc_movement_raw(').replace('(ClassEntry *)0x80187C18','(ClassEntry *)lc_movement_class_records');
  if(id==='00043ad8')s='extern unsigned char lc_movement_class_records[];\n'+s+'\nu8 func_00043ad8(int a, int b) { return lc_movement_kind(lc_movement_raw(a,b)); }\n';
  if(id==='00042dc4')s='extern void lc_movement_refresh_saved(void);\n'+s.replace('    func_8016CC2C(D_80196A28);','    func_8016CC2C(D_80196A28);\n    lc_movement_refresh_saved();');
  if(id==='00040f88')s=s.replace('order_index < 7','order_index < LC_MOVE_COUNT');
  if(id==='00101DDC')s=s.replace('memcpy(D_801EB318, D_801F36D8, 0x200);','memcpy(D_801EB318, D_801F36D8, LC_MOVE_ROWS * 64);').replaceAll('index = 7; index >= 0','index = LC_MOVE_ROWS - 1; index >= 0');
  if(id==='00125460')s=s.replaceAll('(35.0f - value)','lc_movement_factor(FIELD(unit, s32, 0x70), value, 0)');
  if(id==='0012C788')s=s.replaceAll('        if (30.0f < amount)\n            amount = 30.0f;\n','').replaceAll('(35.0f - amount)','lc_movement_factor(FIELD(unit, s32, 0x70), amount, mode == 0)');
  if(id==='00125460')s=s.replace('            dx *= factor;','            if (factor > 1.0f) factor = 1.0f;\n            dx *= factor;');
  if(id==='0012C788')s=s.replace('            dx *= step;','            if (step > 1.0f) step = 1.0f;\n            dx *= step;');
  if(id==='001284C4')s=s.replace('(19.0f - query)','lc_movement_factor(UNIT_FIELD(u32, 0x70), query + 16.0f, 0)');
  if(id==='00126D24')s=s.replace('    if (lc_movement_kind(type) == 7)\n        return D_801F3758;\n','');
  // Split the old coupled kind tests by the behavior each consumer actually needs.
  let search=0;
  while((search=s.indexOf('lc_movement_kind(',search))>=0){
   const begin=search+'lc_movement_kind('.length;let end=begin,depth=1;
   for(;end<s.length && depth;end++){if(s[end]==='(')depth++;if(s[end]===')')depth--;}
   const match=s.slice(end).match(/^\s*(==|!=)\s*([17])\b/);
   if(!match){search=end;continue;}
   const arg=s.slice(begin,end-1),immobile=match[2]==='7';
   const field=immobile?0:id==='00124520'?2:id==='00126D24'&&arg==='type'?4:1;
   const negate=immobile?match[1]==='==':match[1]==='!=';
   const replacement=(negate?'!':'')+`lc_movement_rule(${arg}, ${field})`;
   s=s.slice(0,search)+replacement+s.slice(end+match[0].length);search+=replacement.length;
  }
  if(id==='0021D7F0'||id==='0021E50C'){
   assert(s.includes('selector = state->actor->ownerContext;'));
   s=s.replace('selector = state->actor->ownerContext;','selector = lc_movement_profile_selector(lc_movement_raw(state->actor->sourceArt, state->actor->ownerContext), state->actor->ownerContext);');
  }
  const source=`game/movement/${g.name}${variant.capacity?'-capacity':''}.c`;
  s='/* Generated by native/tools/generate-movement-c.cjs from accepted C. */\n#include "movement_config.h"\nextern int lc_movement_kind(unsigned int);\nextern int lc_movement_rule(unsigned int, unsigned int);\nextern int lc_movement_profile_selector(unsigned int, int);\nextern unsigned char lc_movement_raw(int, int);\nextern float lc_movement_factor(unsigned int, float, int);\n'+s;
  s=s.trimEnd()+'\n';
  fs.writeFileSync(path.join(root,source),s);
  owners.push({name:g.name,source,sha256:digest(Buffer.from(s)).toLowerCase(),capacity:variant.capacity,
    originalSource:g.source,originalSha256:g.sourceSha256,bindings:variant.capacity ? {...g.bindings,...require("../game/scenarios/capacity/owners.json").owners.find(o=>o.adaptedSource.endsWith(`func_${id}.c`)).symbols} : g.bindings,headers:g.headers,
    parts:g.parts.filter(p=>p.code).map(p=>({start:p.start,size:p.size,address:p.address,sha256:p.sha256.toLowerCase()}))});
 }
}
fs.writeFileSync(path.join(out,'owners.json'),JSON.stringify({generator:'native/tools/generate-movement-c.cjs',owners},null,2)+'\n');
console.log(`Generated ${owners.length} movement source variants`);
