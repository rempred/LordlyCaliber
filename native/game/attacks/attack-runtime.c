/* Extension rules around the original attack consumers. The original amount
 * kernel retains defense, resistance, Luck, rounding, caps and event flags.
 * Spell-index ranges follow the byte-proven func_0020144C reader.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
extern u8 lc_attack_settings[];
int lc_original_amount(void *, void *, float, int, int, int);
int lc_effective_action(void *);
int lc_amount_override(void *,int);

int lc_attack_spell_index(int action)
{
    int id;
    id = action & 255;
    if (id >= 159) id = lc_attack_settings[id * 32 + 1];
    if ((u8)(id - 17) < 28) return (id + 80) & 255;
    if ((u8)(id + 110) < 8) return (id + 235) & 255;
    if ((u8)(id - 55) < 36) return (id + 201) & 255;
    if ((u8)(id - 91) < 54) return (id + 204) & 255;
    if ((u8)(id + 102) < 3) return (id + 138) & 255;
    return 1;
}

int lc_attack_amount(void *source, void *target, float power,
                     int secondary, int supplied, int flags)
{
    int action, percent, prior, result;
    prior=source?*(u32 *)((u8 *)source+0x7c):0;
    if (secondary == 0 && source != 0) {
        action = lc_effective_action(source);
        percent = *(u16 *)(lc_attack_settings + action * 32 + 6);
        if (percent != 100) power = (power * (float)percent) / 100.0f;
        if(lc_amount_override(source,action)) *(u32 *)((u8 *)source+0x7c)=action;
    }
    result=lc_original_amount(source, target, power, secondary, supplied, flags);
    if(source) *(u32 *)((u8 *)source+0x7c)=prior;
    return result;
}

/* Battle actors and event arguments follow the carried accepted physical and
 * generic executors. Scratch is in the boot-loaded allocation, never an overlay.
 * The game constructs one action at a time. Each wrapper resets the context. */
#define W(p,n) (*(s32 *)((u8 *)(p)+(n)))
#define H(p,n) (*(u16 *)((u8 *)(p)+(n)))
#define B(p,n) (*(u8 *)((u8 *)(p)+(n)))
extern u8 lc_attack_records[], lc_attack_scratch[];
int lc_attack_visual_program(int action,int original){
    int value;
    if(action<1||action>254)return original;
    value=lc_attack_settings[action*32+21];
    return value?value:original;
}
typedef struct {
    int pass, impact, poseCount, poseAt, poses[2];
} PhysicalGroup;
typedef struct {
    void *source;
    s32 action, forced, total, last, initialHP, initialMaxHP, hit, time, synthetic;
    void *target;
    s32 targetHP;
    PhysicalGroup *group;
} AttackContext;
#define CTX ((AttackContext *)lc_attack_scratch)
void *func_0020C478(u32);
int func_0020C2C0(void *);
int func_0020C1B4(void *);
int func_0020D4C8(void *,u32);
int func_0020D72C(void *);
int func_002379AC(u8,void *);
void func_0022B06C(void *,u8 *);
void func_0022A7B8(void *,int,int,int,int,int);
void func_0021D200(int,int,void *);
void func_0021D230(int,int,void *,int);
void func_0021D25C(int,int,void *,int,int);
void func_0021D28C(int,int,void *,int,int,int);
void func_0021D2C0(int,int,void *,int,void *,void *,void *);
void func_0021D334(int,int,void *,int,int,int,int,int,int);
int func_0022257C(void *,int,int *);
u32 rand(void);
int func_00022e60(void);
void func_00239338(void *,void *,void *,void *);
int lc_original_melee(void *,int *);
int lc_original_ranged(void *,int *);
int lc_original_alternate(void *,int *);
int lc_original_anchor(void *);
int lc_original_ranged_anchor(void *);
void lc_original_hit(void *,int,int,int,int,u8,int,int);
void lc_original_targets(void *,u8,void *);
int lc_original_animation(int,int,int,int);
u8 lc_original_resolve(u8 *,int,int,int,u8,u16,u16,u16,u16);
u8 lc_original_spell_action(u8);
extern int (*lc_attack_group_functions[])(void *,int *);
int lc_attack_animation(int,int,int,int);

/* The accepted physical executors keep their own per-target calculations.
 * Grouped adaptations suppress only repeated attacker/projectile events and
 * translate their local clocks to the first target's impact. */
int lc_group_primary(void) { return !CTX->group || CTX->group->pass==0; }
int lc_group_impact(int time) {
    PhysicalGroup *g=CTX->group;
    if(!g) return 0;
    if(!g->pass) { g->impact=time; return 0; }
    return g->impact-time;
}
int lc_group_animation(int source,int owner,int action,int mode) {
    PhysicalGroup *g=CTX->group;
    int n,value;
    if(!g) return lc_attack_animation(source,owner,action,mode);
    n=g->poseAt++;
    if(g->pass && n<g->poseCount) return g->poses[n];
    value=lc_attack_animation(source,owner,action,mode);
    if(!g->pass && n<2) {g->poses[n]=value;g->poseCount=n+1;}
    return value;
}

int lc_effective_action(void *source) { return CTX->source==source?CTX->action:(W(source,0x7c)&255); }
int lc_amount_override(void *source,int action) {
    int donor;
    u8 *data,*original;
    if(action==(W(source,0x7c)&255)) return 0;
    donor=lc_attack_settings[action*32]; data=lc_attack_records+action*16+4;
    original=(u8 *)0x8018aa80+donor*16;
    return data[1]!=original[1] || data[4]!=original[4];
}
u8 *lc_rules(void *source) { return lc_attack_settings + lc_effective_action(source)*32; }
int lc_behavior(int id) { return lc_attack_settings[(id&255)*32]; }
int lc_animation_id(int id) { return lc_attack_settings[(id&255)*32+1]; }
/* Event 11 uses its spell byte only for the displayed action name. Retail
 * visual indices end below 159; new actions use their own ID as a name token.
 * Event 22 still receives the actual visual index, independently of the name. */
int lc_attack_name_token(void *source,int visual) {
    int action=lc_effective_action(source);
    return action>=159 && action<=254?action:visual;
}
u8 lc_attack_spell_action(u8 token) {
    return token>=159 && token<=254?token:lc_original_spell_action(token);
}
int lc_template(int id) { return (id>=45 && id<=54) || id==145; }
int lc_attack_template_choice(int action,int resolved) {
    u8 *map;
    int i,count;
    if(action<1 || action>254) return resolved;
    map=*(u8 **)(lc_attack_settings+action*32+28);
    if(!map) return resolved;
    count=H(map,8);
    for(i=0;i<count;i++) if(H(map,16+i*4)==resolved) return H(map,18+i*4);
    return resolved;
}
int lc_attack_resolved_kind(int action) { return lc_attack_records[(action&255)*16+4]; }

/* An explicit class/action selector wins. New actions otherwise inherit the
 * donor's class-specific route, including its alternate raw mode. */
int lc_attack_animation(int source,int owner,int action,int mode) {
    int pose;
    pose=lc_original_animation(source,owner,action,mode);
    if ((pose&255)==0x28 && action>=159)
        pose=lc_original_animation(source,owner,lc_animation_id(action),mode);
    return pose;
}
u8 lc_attack_resolve(u8 *out,int source,int owner,int action,u8 element,
                     u16 a,u16 b,u16 c,u16 d) {
    u8 resolved, result;
    int donor,mapped;
    donor=lc_behavior(action);
    if ((action<159 || !lc_template(donor)) && !W(lc_attack_settings+action*32,28))
        return lc_original_resolve(out,source,owner,action,element,a,b,c,d);
    resolved=action>=159?donor:action;
    result=lc_original_resolve(&resolved,source,owner,resolved,element,a,b,c,d);
    mapped=lc_attack_template_choice(action,resolved);
    if(mapped!=resolved) result=lc_attack_resolved_kind(mapped);
    if (out) *out=resolved==donor && mapped==resolved ? action : mapped;
    return result;
}

void lc_attack_begin(void *source) {
    CTX->source=source; CTX->action=W(source,0x7c)&255; CTX->forced=-1;
    CTX->total=0; CTX->last=0; CTX->initialHP=H(source,0x20);
    CTX->initialMaxHP=H(source,0x22); CTX->hit=0; CTX->time=W(source,0x94);
    CTX->synthetic=0; CTX->target=0; CTX->targetHP=0; CTX->group=0;
}
void lc_attack_group_begin(void *source,int action) { lc_attack_begin(source); CTX->action=action&255; }
void lc_attack_pedra_begin(void *source) { lc_attack_begin(source); CTX->synthetic=1; }
/* Return front/middle/rear-major cell in the target's own formation. */
int lc_attack_cell(void *actor) {
    int x,y;
    x=W(actor,0x54); y=W(actor,0x58);
    if (W(actor,0x40)&0x100) y=2-y;
    else { x=2-x; y-=6; }
    if (x<0 || x>2 || y<0 || y>2) return -1;
    return y*3+x;
}
int lc_attack_target_list(void *source,int *ids,int anchor) {
    static const int aroundX[9]={0,1,-1,0,0,1,-1,-1,1};
    static const int aroundY[9]={0,0,0,1,-1,1,1,-1,-1};
    u8 *rules;
    void *actor,*selected;
    int i,n,pattern,side,cell,anchorCell,mask,same,main;
    int candidates[9],count,j,strikes,pick,originalAnchor,centerX,centerY,x,y;
    rules=lc_rules(source); pattern=rules[2]; side=rules[3];
    if(pattern==9) side=3;
    main=lc_attack_records[lc_effective_action(source)*16+7];
    if (!side) side=(main==2 || main==5 || main==6 || main==11)?2:1;
    originalAnchor=anchor;
    selected=anchor>=0 && anchor<20?func_0020C478(anchor):0;
    /* Native damage targeting may have chosen an opponent even when the
     * authored rule targets allies or the user. Anchor on the requested side
     * before applying row/column filters, or a valid action can become empty. */
    if(side==3) selected=source;
    else {
        same=selected?((W(selected,0x40)^W(source,0x40))&0x100)==0:0;
        if(!selected || func_0020C2C0(selected) || lc_attack_cell(selected)<0 ||
           (side==1 && same) || (side==2 && !same)) {
            selected=0;
            for(i=0;i<20;i++) {
                actor=func_0020C478(i);
                if(!actor || func_0020C2C0(actor) || lc_attack_cell(actor)<0) continue;
                same=((W(actor,0x40)^W(source,0x40))&0x100)==0;
                if((side==1 && same) || (side==2 && !same)) continue;
                selected=actor; anchor=i; break;
            }
        }
    }
    anchorCell=selected?lc_attack_cell(selected):4;
    if (anchorCell<0) anchorCell=4;
    centerX=selected?W(selected,0x54):1;
    centerY=selected?W(selected,0x58):1;
    /* Native spells can anchor on a coordinate rather than an actor, including
     * an empty center. Retain an in-formation coordinate on the requested side. */
    if(pattern==11 && side!=3 && originalAnchor>=20 && originalAnchor<87) {
        i=originalAnchor<47?originalAnchor-20:originalAnchor-47;
        j=originalAnchor<47?3:4;x=i%j;y=i/j;
        same=((W(source,0x40)&0x100)!=0)==(y<3);
        if(x<3 && (y<3 || (y>=6 && y<9)) &&
           ((side==1 && !same) || (side==2 && same))) {
            centerX=x;centerY=y;
            anchorCell=y<3?(2-y)*3+x:(y-6)*3+2-x;
        }
    }
    mask=H(rules,4); n=0;
    if (!pattern) pattern=1;
    for(i=0;i<20 && n<9;i++) {
        actor=func_0020C478(i);
        if (!actor || func_0020C2C0(actor)) continue;
        same=((W(actor,0x40)^W(source,0x40))&0x100)==0;
        if ((side==1 && same) || (side==2 && !same) || (side==3 && actor!=source)) continue;
        cell=lc_attack_cell(actor);
        if (cell<0) continue;
        if (pattern==9 && actor!=source) continue;
        if (pattern>=3 && pattern<=5 && cell/3!=pattern-3) continue;
        if (pattern==6 && cell%3!=anchorCell%3) continue;
        if (pattern==7 && cell/3!=anchorCell/3) continue;
        if (pattern==8 && cell/3!=anchorCell/3 && cell%3!=anchorCell%3) continue;
        if ((pattern==10 || pattern==11) && (cell/3<anchorCell/3-1 || cell/3>anchorCell/3+1 ||
                           cell%3<anchorCell%3-1 || cell%3>anchorCell%3+1)) continue;
        if (pattern==12 && !(mask & (1<<cell))) continue;
        ids[n++]=i;
    }
    if(pattern==1 && n) {
        for(i=0;i<n;i++) if(ids[i]==anchor) { ids[0]=anchor; break; }
        n=1;
    }
    if(pattern==11 && n) {
        /* Retail patterns 9/10 (00237E2C) enumerate these world-grid offsets,
         * then draw with replacement: first eligible actor weight 40, others
         * weight 15. Preserve that order and RNG, including mirrored sides.
         * The adapted executors separate up to 18 strikes from nine actors. */
        count=0;
        for(i=0;i<9;i++) for(j=0;j<n;j++) {
            actor=func_0020C478(ids[j]);
            if(W(actor,0x54)==centerX+aroundX[i] && W(actor,0x58)==centerY+aroundY[i]) {
                candidates[count++]=ids[j];break;
            }
        }
        if(!count) return 0;
        strikes=rules[22]?rules[22]:6;
        if(strikes>18) strikes=18;
        for(i=0;i<strikes;i++) {
            pick=func_00022e60()%(40+(count-1)*15);
            ids[i]=candidates[pick<40?0:1+(pick-40)/15];
        }
        n=strikes;
    }
    return n;
}
/* Owned spell executors use a 240-byte packet with 20 storage slots. Native
 * services retain their original 128-byte/10-slot ABI; bridge at each call.
 * Offsets: IDs16, hit chances96, primary136, selectors156, amounts176,
 * secondary216. The editable strike limit is18, leaving terminator headroom. */
void lc_attack_expand_packet(void *packet) {
    u32 saved[32];
    int i,n;
    for(i=0;i<128;i++) B(saved,i)=B(packet,i);
    for(i=0;i<240;i++) B(packet,i)=0;
    for(i=0;i<16;i++) B(packet,i)=B(saved,i);
    n=W(saved,12);if(n<0)n=0;if(n>10)n=10;W(packet,12)=n;
    for(i=0;i<n;i++) {
        W(packet,16+i*4)=W(saved,16+i*4);
        H(packet,96+i*2)=H(saved,56+i*2);
        B(packet,136+i)=B(saved,76+i);B(packet,156+i)=B(saved,86+i);
        H(packet,176+i*2)=H(saved,96+i*2);B(packet,216+i)=B(saved,116+i);
    }
}
void lc_attack_calculate(void *packet,void *source,void *second,void *third) {
    u32 native[32];
    int start,i,j,n,count;
    count=W(packet,12);
    for(start=0;start<count;start+=9) {
        for(i=0;i<128;i++) B(native,i)=0;
        for(i=0;i<16;i++) B(native,i)=B(packet,i);
        n=count-start;if(n>9)n=9;W(native,12)=n;
        for(i=0;i<n;i++) {
            j=start+i;W(native,16+i*4)=W(packet,16+j*4);
            H(native,56+i*2)=H(packet,96+j*2);
            B(native,76+i)=B(packet,136+j);B(native,86+i)=B(packet,156+j);
            H(native,96+i*2)=H(packet,176+j*2);B(native,116+i)=B(packet,216+j);
        }
        func_00239338(native,source,second,third);
        for(i=0;i<n;i++) {
            j=start+i;H(packet,96+j*2)=H(native,56+i*2);
            B(packet,136+j)=B(native,76+i);B(packet,156+j)=B(native,86+i);
            H(packet,176+j*2)=H(native,96+i*2);B(packet,216+j)=B(native,116+i);
        }
    }
}
void lc_attack_retarget_packet(void *packet,int action,void *source,int expanded) {
    u8 *rules,*data;
    int i,n,ids[18],anchor;
    rules=lc_attack_settings+action*32;
    if (!rules[2] && !rules[3]) return;
    /* Native anchor preference is retained when it is on the chosen side. */
    anchor=W(source,0x68);
    n=lc_attack_target_list(source,ids,anchor);
    if(!expanded && n>9)n=9;
    for(i=0;i<(expanded?240:128);i++) B(packet,i)=0;
    data=lc_attack_records+action*16+6;
    for(i=0;i<8;i++) B(packet,i)=data[i];
    B(packet,8)=10; W(packet,12)=n;
    for(i=0;i<n;i++) W(packet,16+i*4)=ids[i];
    W(source,0x68)=n?ids[0]:-1;
}
void lc_attack_retarget(void *packet,int action,void *source) {
    lc_attack_retarget_packet(packet,action,source,1);
}
void lc_attack_targets(void *packet,u8 action,void *source) {
    lc_original_targets(packet,action,source);
    /* Preserve the public native-packet ABI for any unadapted caller. */
    lc_attack_retarget_packet(packet,action,source,0);
}
void lc_attack_expanded_targets(void *packet,u8 action,void *source) {
    lc_original_targets(packet,action,source);
    lc_attack_expand_packet(packet);
    lc_attack_retarget(packet,action,source);
}
void lc_attack_visual_targets(int action,int *ids,int *flags) {
    int i,j,n;
    /* Effects still receive actors, not an oversized repeated-hit list. Keep
     * the retail <=9 path unchanged, and retain each actor's last nonzero flag. */
    if(lc_attack_settings[action*32+2]!=11 || lc_attack_settings[action*32+22]<=9)return;
    n=0;
    for(i=0;i<19 && ids[i]>=0;i++) {
        for(j=0;j<n;j++)if(ids[j]==ids[i])break;
        if(j==n) {ids[n]=ids[i];flags[n++]=flags[i];}
        else if(flags[i]) flags[j]=flags[i];
    }
    ids[n]=-1;flags[n]=-1;
}
int lc_attack_anchor(void *source) {
    if(CTX->source==source && CTX->forced>=0) return CTX->forced;
    return lc_original_anchor(source);
}
int lc_attack_ranged_anchor(void *source) {
    if(CTX->source==source && CTX->forced>=0) return CTX->forced;
    return lc_original_ranged_anchor(source);
}
int lc_roll(int chance) {
    u32 a,b,c;
    if(chance<=0) return 0;
    if(chance>=100) return 1;
    a=rand(); b=rand(); c=rand();
    return ((((a<<18)&0x0c000000)|(b<<15)|c)%100)<(u32)chance;
}
/* Undead slaying uses the native lethal-result marker, rather than a new
 * ailment or instant death. Playback retains scripted/training safeguards. */
void lc_attack_target_begin(void *target) {
    CTX->target=target; CTX->targetHP=H(target,0x20);
}
void lc_attack_result_event(int event,int time,void *target,int kind,
                            int high,int low,int parameter,int group,int extra) {
    if(event==0x1f && kind==0 && CTX->source && lc_rules(CTX->source)[8]==7) {
        extra=0;
        if(CTX->target==target && CTX->targetHP>0 && H(target,0x20)==0 &&
           func_0020C1B4(target) && lc_roll(lc_rules(CTX->source)[10])) extra=7;
    }
    func_0021D334(event,time,target,kind,high,low,parameter,group,extra);
}
/* Replay the same ailment events used by the native secondary-effect path.
 * No recovery timer is introduced; game status rules still own recovery. */
int lc_attack_status(void *target,int effect,int time) {
    u8 applied;
    int flags,pose,duration,amount;
    if(!target || !effect || func_0020C2C0(target)) return 0;
    if(effect==16) {
        amount=H(target,0x20);
        func_0022A7B8(target,0,time,amount,0,0);
        func_0021D334(0x1f,time,target,0,amount>>8,amount&255,0,B(*(void **)0x801ce8c0,0x823),0);
        return 1;
    }
    if(!func_002379AC(effect,target)) return 0;
    flags=W(target,0x40); applied=effect;
    func_0022B06C(target,&applied);
    if(!applied) return 0;
    pose=0; duration=0;
    if(effect==3) { func_0021D230(0x23,time,target,255); pose=0x26; }
    if(effect==4) { func_0021D230(0x25,time,target,255); pose=0x22; }
    if(effect==8) { func_0021D28C(0x1a,time,target,255,time+100,0); pose=0x20; }
    if(effect==10) func_0021D230(0x27,time,target,255);
    if(effect==5 || effect==6) func_0021D230(effect+0x24,time,target,255);
    if(effect==11) {
        func_0021D230(0x2b,time,target,255);
        if(flags&2) { func_0021D230(0x24,time,target,255); pose=0x27; }
        else if(flags&8) { func_0021D230(0x1b,time,target,255); pose=0x21; }
        else if(flags&4) { func_0021D230(0x26,time,target,255); pose=0x23; }
        else if(flags&16) func_0021D230(0x28,time,target,255);
    }
    if(pose) duration=func_0022257C((u8 *)*(void **)target+0x44,pose,0);
    if(effect==3) duration+=22;
    if(effect==3 || effect==4 || effect==8 || (effect==11 && pose))
        func_0021D200(0x2c,time+duration,target);
    if(W(target,0x94)<time+duration) W(target,0x94)=time+duration;
    return 1;
}
void lc_attack_record(void *source,void *target,int amount,int before,int hit,int time) {
    u8 *rules;
    int lost,effect;
    if(CTX->source!=source) return;
    rules=lc_rules(source); lost=before-H(target,0x20);
    if(lost>0) CTX->total+=lost;
    CTX->last=amount; CTX->hit=1;
    if(CTX->time<time) CTX->time=time;
    effect=rules[8];
    if(effect==255 || !effect || effect==7 || (rules[9]==1 && !hit) ||
       (rules[9]==2 && (!hit || H(target,0x20)!=0))) return;
    if(lc_roll(rules[10])) lc_attack_status(target,effect,time);
}
void lc_attack_finish(void *source,int time) {
    u8 *rules;
    int basis,amount,kind;
    rules=lc_rules(source); kind=rules[11];
    if(CTX->synthetic || (kind!=1 && kind!=2)) { CTX->source=0; return; }
    basis=rules[12]==1?CTX->last:rules[12]==2?CTX->initialHP:
          rules[12]==3?CTX->initialMaxHP:CTX->total;
    amount=(basis*H(rules,14))/100;
    if(amount>9999) amount=9999;
    if(amount>0 && !func_0020C2C0(source)) {
        if(kind==1) {
            amount=func_0020D4C8(source,amount); H(source,0x20)+=amount;
            func_0021D334(0x1f,time,source,2,amount>>8,amount&255,0,B(*(void **)0x801ce8c0,0x823),0);
        } else {
            func_0022A7B8(source,0,time,amount,0,0);
            func_0021D334(0x1f,time,source,0,amount>>8,amount&255,0,B(*(void **)0x801ce8c0,0x823),0);
        }
    }
    CTX->source=0;
}
void lc_attack_hit(void *target,int offset,int time,int amount,int parameter,u8 flags,int mode,int adjust) {
    void *source;
    int effect,before,calculated;
    source=CTX->source;
    if(!source) { lc_original_hit(target,offset,time,amount,parameter,flags,mode,adjust); return; }
    before=H(target,0x20); calculated=amount; effect=lc_attack_records[CTX->action*16+7];
    if(effect==1) {
        if(lc_rules(source)[8]==7 && before>0 && amount>=before && func_0020C1B4(target) &&
           lc_roll(lc_rules(source)[10])) flags|=0x10;
        lc_original_hit(target,offset,time,amount,parameter,flags,mode,adjust);
    }
    else if(effect==2) {
        amount=func_0020D4C8(target,amount); H(target,0x20)+=amount;
        func_0021D334(0x1f,time,target,2,amount>>8,amount&255,parameter,B(*(void **)0x801ce8c0,0x823),0);
    } else if(effect) lc_attack_status(target,effect,time);
    lc_attack_record(source,target,calculated,before,1,time);
}
int lc_attack_physical(void *source,int *time,int route) {
    int ids[18],n,i,result,count,anchor,hit;
    int shared,start,latest,ready,primaryTarget,primaryTiming[3],j;
    int nearest,distance,bestDistance,dx,dy;
    PhysicalGroup group;
    int visualIds[20],visualFlags[20],visualSource[2],visualCount,visualSlot;
    void *target;
    u8 *rules;
    rules=lc_rules(source);
    lc_attack_begin(source);
    count=W(source,0x6c); result=0;visualCount=0;visualSlot=rules[21];
    if(rules[2] || rules[3]) {
        anchor=route==1?lc_original_ranged_anchor(source):lc_original_anchor(source);
        n=lc_attack_target_list(source,ids,anchor);
    } else { ids[0]=-1; n=1; }
    shared=rules[13]==1 && n>1;
    if(shared && route!=1) {
        /* Actor IDs are roster order, not formation order. The shared melee
         * movement/strike must approach the closest affected actor. Use the
         * common battle-grid coordinates (already mirrored for each side).
         * Keep the native anchor on a distance tie and retain every victim. */
        nearest=0;bestDistance=0x7fffffff;
        for(i=0;i<n;i++) {
            target=func_0020C478(ids[i]);
            dx=W(target,0x54)-W(source,0x54);
            dy=W(target,0x58)-W(source,0x58);
            distance=dx*dx+dy*dy;
            if(distance<bestDistance || (distance==bestDistance && ids[i]==anchor)) {
                nearest=i;bestDistance=distance;
            }
        }
        j=ids[nearest];
        for(i=nearest;i>0;i--) ids[i]=ids[i-1];
        ids[0]=j;
    }
    start=*time;latest=*time;ready=W(source,0x94);primaryTarget=W(source,0x68);
    if(shared) {
        if(start<ready) start=ready;
        for(i=0;i<n;i++) {
            target=func_0020C478(ids[i]);
            if(target && start<W(target,0x94)) start=W(target,0x94);
        }
        group.pass=0;group.impact=start;group.poseCount=0;group.poseAt=0;
        CTX->group=&group;
    }
    for(i=0;i<n;i++) {
        CTX->forced=ids[i]; CTX->hit=0;
        W(source,0x6c)=count;
        if(shared) {
            group.pass=i;group.poseAt=0;W(source,0x94)=start;*time=start;
            result|=lc_attack_group_functions[route](source,time);
            if(!i) {
                primaryTarget=W(source,0x68);
                for(j=0;j<3;j++) primaryTiming[j]=W(source,0x84+j*4);
            }
            if(ready<W(source,0x94)) ready=W(source,0x94);
            if(latest<*time) latest=*time;
        }
        else if(route==0) result|=lc_original_melee(source,time);
        else if(route==1) result|=lc_original_ranged(source,time);
        else result|=lc_original_alternate(source,time);
        hit=CTX->hit;
        target=func_0020C478(W(source,0x68));
        if(target&&visualSlot&&visualCount<18){visualIds[visualCount]=func_0020D72C(target);visualFlags[visualCount++]=0;}
        if(!hit && target) lc_attack_record(source,target,0,H(target,0x20),0,shared?group.impact:W(target,0x94));
        if(shared && target && latest<W(target,0x94)) latest=W(target,0x94);
    }
    if(shared) {
        W(source,0x68)=primaryTarget;
        for(j=0;j<3;j++) W(source,0x84+j*4)=primaryTiming[j];
        W(source,0x94)=ready>latest?ready:latest;*time=latest;CTX->group=0;
    }
    W(source,0x6c)=count+1;
    if(visualSlot&&visualCount){
        visualSource[0]=func_0020D72C(source);visualSource[1]=-1;
        visualIds[visualCount]=-1;visualFlags[visualCount]=-1;
        lc_attack_visual_targets(CTX->action,visualIds,visualFlags);
        func_0021D2C0(0x16,shared?group.impact:(*time>0?*time-1:0),0,visualSlot,visualSource,visualIds,visualFlags);
    }
    lc_attack_finish(source,*time);
    return result;
}
int lc_attack_melee(void *source,int *time) { return lc_attack_physical(source,time,0); }
int lc_attack_ranged(void *source,int *time) { return lc_attack_physical(source,time,1); }
int lc_attack_alternate(void *source,int *time) { return lc_attack_physical(source,time,2); }
