/* Item effect programs: bounded forward-only branches and base character effects.
 * Query stores rolled results; apply never rerolls. Battle actors are not accepted. */
#include "game/scenario_item_views.h"
extern u16 D_80190EBC[];
extern u8 D_80191010[];
extern u32 lc_tree_index[];
extern u8 lc_tree_state[];
extern u32 lc_item_visual_index[];
extern u8 lc_item_native_pixels[], lc_item_native_palettes[];
extern s32 rand(void);
extern void osWritebackDCache(void *, s32);
extern s32 lc_base_effect(u8,s32,u16,u8);
extern void func_00044aa4(void *);
typedef struct { s32 amount; u8 op,percent,pad[2]; } Step;
typedef struct { u16 item; u8 count,valid; Step steps[16]; } Plan;
#define PLANS ((Plan *)lc_tree_state)
#define CONTEXT (*(s32 *)(lc_tree_state+30000))
static s32 hp(s32 id){return id<100?D_80193BC0[id].field18:D_80190EBC[id];}
static s32 status(s32 id){return id<100?D_80193BC0[id].field33:D_80191010[id];}
static s32 maximum(s32 id){return id<100?D_80193BC0[id].field16:D_80190EBC[99];}
static s32 limit(s32 id){s32 n,cap;if(id<100)return maximum(id);n=func_00040FF4(0,id);cap=maximum(id)/3*n;if(n==3)cap+=maximum(id)%3;return cap;}
static s32 clamp(s32 v,s32 cap){if(v<0)return 0;if(v>cap)return cap;return v;}
int lc_tree_has(int item){return item>0&&item<45&&lc_tree_index[item]!=0;}
void lc_tree_context(int context){CONTEXT=context;}
/* Copy into the menu's own resource buffers. Keep their ownership/lifetime and
 * every icon beyond the four sparkle frames. Both colour variants receive the
 * selected palette; the two result-icon palettes remain native. Selecting an
 * untouched item restores the original sheet, so custom artwork cannot leak. */
void lc_item_visual_select(int item, u8 *pixels, u8 *palettes) {
    const u8 *custom = 0;
    int i;
    if (!pixels || !palettes) return;
    if (item > 0 && item < 45) custom = (const u8 *)lc_item_visual_index[item];
    for (i = 0; i < 2048; i++) pixels[i] = custom ? custom[i] : lc_item_native_pixels[i];
    for (i = 0; i < 128; i++) palettes[i] = lc_item_native_palettes[i];
    if (custom) for (i = 0; i < 32; i++) palettes[i] = palettes[96 + i] = custom[2048 + i];
    osWritebackDCache(pixels, 2048);
    osWritebackDCache(palettes, 128);
}
static int retail(int item,int id,int context){
    ScenarioCharacter *c=&D_80193BC0[id];
    if(item<=3)return hp(id)>0&&hp(id)<limit(id)&&(context==0||!(status(id)&4));
    if(item==6)return hp(id)>0&&(status(id)&4)!=0;
    if(item==7)return id<100&&hp(id)==0;
    if(id>=100)return context==1&&(item==4||item==5);
    if(item==9)return c->field13<99;
    if(item==10)return c->field16<999;
    if(item>=11&&item<=16)return c->fields1C[item-11]<999;
    if(item==17)return c->field1B<100;
    if(item==18)return c->field1B>0;
    return 1;
}
static int predicate(int op,int value,int id,int context){
    ScenarioCharacter *c=&D_80193BC0[id];
    switch(op){
    case 0:return retail(value,id,context);
    case 1:return hp(id)>0;case 2:return hp(id)==0;case 3:return hp(id)<limit(id);
    case 4:return (status(id)&4)!=0;case 5:return id<100;case 6:return id>=100;
    case 7:return hp(id)*100<maximum(id)*value;
    case 8:return id<100&&c->field12==value;
    case 9:return id<100&&c->field32>=value;
    case 10:return id<100&&c->field1B>=value;
    case 11:return id<100&&c->field28>=value;
    case 12:return context==0;case 13:return context==1;
    }return 0;
}
static int possible(int op,int lo,int hi,int id){
    ScenarioCharacter *c=&D_80193BC0[id];s32 v,cap;
    if(op==1)return hp(id)>0&&hp(id)<limit(id)&&hi>0;
    if(op==3)return (status(id)&4)!=0;
    if(id>=100)return 0;
    if(op==2)return c->field32>0&&hi>0;
    if(op==4)return hp(id)==0;
    if(op==5)return c->field13<99;
    cap=op>=13?100:999;
    if(op==6)v=c->field16;else if(op<=12)v=c->fields1C[op-7];else if(op==13)v=c->field1B;else v=c->field28;
    return (lo<0&&v>0)||(hi>0&&v<cap);
}
/* Conditions are checked against the activation state; actions apply in order. */
int lc_tree_prepare(int item,int id,int context,int query){
    const u8 *code;const u8 *n;int pc=0,steps=0,count=0,op,lo,hi,value,first=0,shown=0;
    Plan *plan;
    if(id<=0||id>=220||!lc_tree_has(item))return 0;
    plan=&PLANS[id];if(query){plan->item=item;plan->count=0;plan->valid=1;}
    code=(const u8 *)lc_tree_index[item];
    while(steps++<140){
        n=code+pc*16;op=n[0];if(!op)break;
        if(op==127){pc=*(const u16 *)(n+4);continue;}
        if(op>=128){value=predicate(op-128,*(const u16 *)(n+2),id,context);if(n[1])value=!value;pc=value?pc+1:*(const u16 *)(n+4);continue;}
        lo=*(const s16 *)(n+2);hi=*(const s16 *)(n+4);pc++;
        if(!query){if(possible(op,lo,hi,id))return 1;continue;}
        if(id>=100&&op!=1&&op!=3)continue;
        if(count>=16){plan->valid=0;return 0;}
        value=lo;if(hi!=lo)value=lo+(u32)rand()%(hi-lo+1);
        if(op==5&&lo==hi)rand(); /* Retail level-up preparation advances RNG. */
        plan->steps[count].op=op;plan->steps[count].percent=n[1];plan->steps[count].amount=value;
        if(!shown&&possible(op,lo,hi,id)) {
            first=value;shown=1;
            if((op==1||op==4)&&n[1])first=maximum(id)*value/100;
            if(op==1)first=clamp(first,limit(id)-hp(id));
            else if(op==2)first=clamp(first,D_80193BC0[id].field32);
            else if(op==3)first=0;
            else if(op==4){first=clamp(first,maximum(id));if(first<1)first=1;}
            else if(op==5)first=clamp(first,99-D_80193BC0[id].field13);
        }
        count++;
    }
    if(query)plan->count=count;
    return query?first:0;
}
int lc_tree_allowed(int item,int id,int context){return lc_tree_prepare(item,id,context,0);}
static void apply_step(int id,Step *s){
    ScenarioCharacter *c=&D_80193BC0[id];int n=s->amount,op=s->op,value,i;
    if(op==1){if(hp(id)==0)return;if(s->percent)n=maximum(id)*n/100;value=clamp(hp(id)+n,limit(id));if(id<100)c->field18=value;else D_80190EBC[id]=value;return;}
    if(op==3){if(id<100)c->field33&=~4;else D_80191010[id]&=~4;return;}
    if(id>=100)return;
    if(op==2){c->field32=clamp(c->field32-n,255);return;}
    if(op==4){if(c->field18==0){value=s->percent?c->field16*n/100:n;if(value<1)value=1;c->field18=clamp(value,c->field16);c->field33&=~4;}return;}
    if(op==5){for(i=0;i<n&&c->field13<99;i++)func_00044aa4(c);return;}
    if(op==6){c->field16=clamp(c->field16+n,999);return;}
    if(op<=12){c->fields1C[op-7]=clamp(c->fields1C[op-7]+n,999);return;}
    if(op==13)c->field1B=clamp(c->field1B+n,100);else if(op==14)c->field28=clamp(c->field28+n,100);
}
s32 lc_tree_effect(u8 apply,s32 value,u16 item,u8 id){
    Plan *p;int i;
    if(!lc_tree_has(item))return lc_base_effect(apply,value,item,id);
    if(id==0||id>=220)return 0;
    if(!apply)return lc_tree_prepare(item,id,CONTEXT,1);
    p=&PLANS[id];if(!p->valid||p->item!=item)return 0;p->valid=0;
    for(i=0;i<p->count;i++)apply_step(id,&p->steps[i]);return value;
}
