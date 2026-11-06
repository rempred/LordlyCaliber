'use strict';
// Maintained adaptations of accepted complete item owners; originals stay intact.
const fs=require('fs'),path=require('path'),crypto=require('crypto');
const root=path.resolve(__dirname,'..'),dir=path.join(root,'game/consumables');
const digest=b=>crypto.createHash('sha256').update(b).digest('hex');
const sources=JSON.parse(fs.readFileSync(path.join(root,'game/accepted/rom-owners.json'))).sources;
const names=['func_000c4650','func_000c9850','func_001AD8CC','func_001AD0E4','func_001A9290'];
const api='\nextern int lc_tree_has(int);\nextern void lc_tree_context(int);\nextern int lc_tree_allowed(int,int,int);\nextern int lc_tree_prepare(int,int,int,int);\nextern void lc_item_visual_select(int,void *,void *);\n';
const owners=[];
for(const name of names){
 const owner=sources.find(s=>s.targets.some(t=>t.toLowerCase()===name.toLowerCase()));if(!owner)throw Error(name);
 const input=fs.readFileSync(path.join(root,owner.source));if(digest(input)!==owner.sourceSha256.toLowerCase())throw Error('Changed accepted input '+name);
 let s=input.toString().replace(/\r\n/g,'\n');const replace=(a,b)=>{if(!s.includes(a)||s.indexOf(a)!==s.lastIndexOf(a))throw Error('Ambiguous adaptation '+name+' '+a);s=s.replace(a,b);};
 if(name==='func_000c4650'){
  replace('    if (func_00045440(item) == 1) {',`    lc_tree_context(0);
    if (lc_tree_has(item)) {
        if (func_00045440(item)==1) {
            for(index=0;index<220;index++) {
                u8 *f=D_80196AF8+0x1872+index*2;
                if(index && !(*f&0x10) && (index>=100||D_80193BC0[index].field12) && lc_tree_allowed(item,index,0)) {*f&=0x7f;available++;} else *f|=0x80;
            }
        } else {
            for(index=0;index<30;index++) { u8 *g=D_80196AF8+0x117c+index*0x36; int j,ok=0;
                if(g[3]==255)continue;
                for(j=0;j<9;j++)if(g[4+j]!=255&&lc_tree_allowed(item,g[4+j],0))ok=1;
                if(ok&&!(g[1]&0x10)){g[1]&=0x7f;available++;}else g[1]|=0x80;
            }
        }
        return available;
    }
    if (func_00045440(item) == 1) {`);
 }
 if(name==='func_000c9850'){
 replace('result=func_00041098(0,0,item,B(phase2,0x18B));',`resources=PTR(phase2,0x1E28);
            lc_item_visual_select(item,resources[0],resources[1]);
            result=func_00041098(0,0,item,B(phase2,0x18B));`);
 replace('item=D_80193AC0[entry].field00;\n\n        D_80193AC0',`item=D_80193AC0[entry].field00;
        resources=PTR(D_80196AF8,0x1E28);
        lc_item_visual_select(item,resources[0],resources[1]);

        D_80193AC0`);
 replace('s32 result=func_00041098(0,0,item,B(members,0x18B));\n                    u8 *result_base=D_80196AF8;',`s32 result;
                    u8 *result_base;
                    if(lc_tree_has(item)) {
                        if(func_00045440(item)==1) result=output?0:lc_tree_prepare(item,B(members,0x18B),0,1);
                        else result=lc_tree_prepare(item,B((u32)group_members+(u32)value,0x1180),0,1);
                    } else result=func_00041098(0,0,item,B(members,0x18B));
                    result_base=D_80196AF8;`);
 }
 if(name==='func_001AD8CC'){
  s='extern unsigned char func_00045440(int);\n'+s;
  s=s.replaceAll('func_001AD8CC','lc_native_map_item_allowed');
  s+=`\nu8 func_001AD8CC(u16 item,u8 member,s32 group) {
    lc_tree_context(1);
    if(lc_tree_has(item)) {
        int i;if(func_00045440(item)==2){for(i=0;i<5;i++)if(lc_tree_allowed(item,D_801971F2[group].members[i],1))return 1;return 0;}
        return lc_tree_allowed(item,member,1);
    }
    return lc_native_map_item_allowed(item,member,group);
}\n`;
 }
 if(name==='func_001AD0E4'){
  s='extern void *func_000453e0(int);\n'+s;
  replace('    u32 stat_member, masked_member;\n    switch (item) {',`    u32 stat_member, masked_member;
    if(lc_tree_has(item)) {
        func_802027DC();
        func_80179034(func_000453e0(item),x+10,y+0x2e);
        current=member<100?D_80193BC0[member].field18:*(u16 *)(0x80190EBCU+member*2);
        maximum=member<100?D_80193BC0[member].field16:*(u16 *)0x80190F82;
        func_802028A4(x+6,y+0x50,current,maximum);
        return;
    }
    switch (item) {`);
 }
 if(name==='func_001A9290'){
  s='extern unsigned char *D_80214A6C, *D_80214EAC;\n'+s;
  replace('if (result == 1) {\n                if (D_8019EE40 == 0)', `if (result == 1) {
                lc_item_visual_select(*item,D_80214A6C,D_80214EAC);
                if (D_8019EE40 == 0)`);
  replace('D_8021E364 = func_00041098(0, 0, item_value, member);\n                        if (item_value < 4)',`D_8021E364 = 0;
                        if(lc_tree_has(item_value)) {
                            if(func_00045440(item_value)==2) {
                                for(i=0;i<5;i++) { int id=D_801971F2[D_8021E3B4].members[i];
                                    int amount=lc_tree_prepare(item_value,id,1,1);
                                    func_8021DC08(item_value,D_8021E3B4,amount,i,id?i:255);
                                }
                            } else { D_8021E364=lc_tree_prepare(item_value,member,1,1);func_8021DC08(item_value,D_8021E3B4,D_8021E364,0,*(u8 *)0x8021E3AB); }
                        } else if ((D_8021E364 = func_00041098(0,0,item_value,member)), item_value < 4)`);
  replace('if (func_00045440(*item) == 2) {',`if (lc_tree_has(*item)) {
                    u8 leader=D_801971F2[D_8021E3B4].members[0];
                    u16 beforeHp=leader<100?D_80193BC0[leader].field18:D_80190EBC[leader];
                    u8 beforeStone=(leader<100?D_80193BC0[leader].field33:POOL_FLAG(leader))&4;
                    if(func_00045440(*item)==2){for(i=0;i<5;i++)func_00041098(1,0,*item,D_801971F2[D_8021E3B4].members[i]);}
                    else func_00041098(1,0,*item,*target);
                    if((beforeHp==0)!=((leader<100?D_80193BC0[leader].field18:D_80190EBC[leader])==0) || beforeStone!=((leader<100?D_80193BC0[leader].field33:POOL_FLAG(leader))&4)) {
                    unit=D_801F0CB0[D_801F3658];
                    func_801B2D2C(unit);
                    unit->field84=-1;unit->field80=-1;unit->field92=0;unit->field91=0;unit->fieldB8=0;
                    unit->field00&=0xFF7FFFFF;
                    unit->field4C=unit->field08;unit->field50=unit->field0C;unit->field54=unit->field10;unit->field64=unit->field14;unit->field00&=0xFDFFFFFF;
                    }
                } else if (func_00045440(*item) == 2) {`);
  // Native cure/revival performs an early commit; custom plans commit only once.
  replace('if (D_8021E3BE == 12 && (u32)(*item - 6) < 2)', 'if (D_8021E3BE == 12 && !lc_tree_has(*item) && (u32)(*item - 6) < 2)');
 }
 s=`/* Generated by native/tools/generate-consumable-tree-sources.cjs from ${owner.source}; SHA256 ${digest(input)}. */\n`+api+s;
 const file='tree-'+name.toLowerCase()+'.c';fs.writeFileSync(path.join(dir,file),s);
 owners.push({...owner,source:'consumables/'+file,sourceSha256:digest(s),originalSource:owner.source,originalSourceSha256:digest(input)});
}
fs.writeFileSync(path.join(dir,'tree-owners.json'),JSON.stringify({generator:'native/tools/generate-consumable-tree-sources.cjs',owners},null,2)+'\n');
console.log('Generated five complete item owner adaptations.');
