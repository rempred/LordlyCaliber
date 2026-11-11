/* Authored from US Rev 0 ROM 00133700/00133944. Original eligibility, cycle
 * thresholds, item bonuses and native recovery/gain services are retained.
 * Per-type rates replace the hard-coded Sky/ground and rest increments. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define F(p,t,o) (*(t *)((u8 *)(p)+(o)))
extern int lc_movement_rule(unsigned int, unsigned int);
extern u8 lc_cycle_sites[],lc_cycle_groups[],lc_cycle_hour[];
extern void *lc_cycle_units[];
extern int lc_cycle_pause;
extern u16 lc_cycle_fatigue_period,lc_cycle_rest_period;
extern int lc_cycle_has_item(void *,int),lc_cycle_town_rest(void *),lc_cycle_group(void *);
extern int lc_cycle_gain(void *),lc_cycle_recover(void *);

static int rest_allowed(void *unit) {
    u32 flags=F(unit,u32,0); int town=F(unit,int,0x74);
    if (!(flags&1) || (flags&0x20000)) return 1;
    if ((!(flags&0x40000) && town!=-1) || F(unit,u8,4)==30) {
        void *site=lc_cycle_sites+town*36;
        u16 sf=F(site,u16,0x1c);
        if (((flags&8) && (sf&2)) || (sf&4) || lc_cycle_town_rest(site)) return 1;
    }
    if ((flags&0x40040)==0x40) {
        int group=lc_cycle_group(unit);
        void *leader=lc_cycle_units[lc_cycle_groups[group*11+2]];
        if (!(F(leader,u32,0)&0x40000)) {
            int site=F(leader,int,0x74);
            if (site==-1) return 1;
            return (F(lc_cycle_sites+site*36,u16,0x1c)&((flags&8)?2:4))!=0;
        }
    }
    return 0;
}
static int night(void) {return (unsigned int)(*lc_cycle_hour-6)>=12;}
static int gain_step(void *unit,int amount,int period,int result) {
    u16 value=(u16)(F(unit,u16,0xb6)+amount);
    F(unit,u16,0xb6)=value;
    if (value>period) {
        int got;
        F(unit,u16,0xb6)=(u16)(value-period);
        got=(u8)lc_cycle_gain(unit); if(got>result)result=got;
    }
    return result;
}
static int recover_step(void *unit,int amount,int period,int result) {
    u16 value=F(unit,u16,0xb6);
    if(value<amount) {
        int got;F(unit,u16,0xb6)=(u16)period;
        got=(u8)lc_cycle_recover(unit);if(got>result)result=got;
    } else F(unit,u16,0xb6)=(u16)(value-amount);
    return result;
}
int func_00133944(void *unit) {
    u32 flags=F(unit,u32,0);int type,step,result=-1,period;
    if (!(flags&1) || (flags&0x20000) || lc_cycle_pause ||
        !(flags&0x40000) || F(unit,u8,4)==30) return -1;
    type=F(unit,int,0x70);step=lc_movement_rule(type,(flags&0x40)?7:6);
    if(lc_cycle_has_item(unit,26))step>>=1;
    /* These retained immediate operands carry Tools' global cycle settings. */
    period=lc_cycle_fatigue_period-1;
    result=gain_step(unit,step,period,result);
    if(night())result=gain_step(unit,lc_movement_rule(type,8),period,result);
    return result;
}
int func_00133700(void *unit) {
    int type,step,result=-1,period;
    if(!rest_allowed(unit))return -1;
    type=F(unit,int,0x70);step=lc_movement_rule(type,9);
    if(lc_cycle_has_item(unit,27))step*=2;
    period=lc_cycle_rest_period;
    result=recover_step(unit,step,period,result);
    if(night())result=recover_step(unit,lc_movement_rule(type,10),period,result);
    return result;
}
