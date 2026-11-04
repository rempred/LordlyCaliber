#include "game/scenario_item_views.h"

extern u8 *D_80196AF8;
extern u16 D_800E8100;
extern u8 D_801939D4, D_801939D5;
extern u8 D_8019A384[], D_8019A3DC[], D_8019A41C[];
extern void *func_00001330(s32);
extern void func_000016C4(void *);
extern void *func_0002e138(u32);
extern s32 rand(void);
extern u8 func_00045440(s32);
extern u8 func_00045460(s32);
extern u16 func_000c4650(u16);
extern void func_000c4a40(void *,s32);
extern void func_000c54c0(s32);
extern void func_000c6fe8(u16);
extern u16 func_000c8d28(s16 *);
extern void func_000c8f04(s16 *);
extern void func_000ae30c(s32,s32,s32);
extern void func_000b1f00(void);
extern void func_000b1f4c(s32,s32,s32);
extern void func_000b4a70(u32);
extern void func_000b4b08(u32);
extern void func_000b4bb8(void);
extern void func_000b6b70(void),func_000b6ba8(void);
extern void func_000b8a94(void),func_000b8aec(void),func_000b8b60(void),func_000b8d70(void);
extern s32 func_000b8e70(s32);
extern void func_000b9234(s32);
extern void func_000BA2C0(s32,s32);
extern u8 func_000bc464(u16);
extern void func_000bc7e4(s32,s32),func_000bc828(s32,s32);
/* Accepted descriptor 0 entry at runtime address 0x80197B70. */
extern s32 func_00066e10(void *);
#define B(p,o) (*(u8 *)((p)+(o)))
#define H(p,o) (*(u16 *)((p)+(o)))
#define SH(p,o) (*(s16 *)((p)+(o)))
#define W(p,o) (*(s32 *)((p)+(o)))
#define PTR(p,o) (*(void **)((p)+(o)))
void func_000c9850(void)
{
    u8 *state = D_80196AF8;
    s16 *selection = (s16 *)(state + 0x1BDA);
    s32 index, output, active, value, next_state, slot_index;
    /* Item remains shared across selection and consumption states; state 5 has its own item lifetime. */
    u16 item, entry, buttons, action;
    u8 old_entry;
    u8 *base, *other, *squad;
    void **resources;
    ScenarioInventory *record;
    switch (SH(state,0x122)) {
    case 0: {

        u8 *base1, *base2, *base3, *base4, *base5;

        PTR(D_80196AF8,0x1E28) = 0;
        for(slot_index=0;slot_index<7;slot_index++) if(B((u32)D_80196AF8 + (u32)slot_index,0x1E00)==7) break;
        if(slot_index==7) {
            H(D_80196AF8,0x122)=99;
            func_000b4a70(0x40016);
            break;
        }
        B(D_80196AF8,0x1DFE)=3;
        base1=D_80196AF8;
        B((u32)base1 + (u32)B(base1,0x9D),0x1DF6)=selection[0];
        base2=D_80196AF8;
        B((u32)base2 + (u32)B(base2,0x9D),0x1DEE)=selection[1];
        B(D_80196AF8,0x9D)=slot_index;
        resources=func_00001330(0xD0);
        PTR(D_80196AF8,0x1E28)=resources;
        resources[0]=func_0002e138(0x1DE9BC0);
        resources[1]=func_0002e138(0x1DE9FA4);
        base3=D_80196AF8;
        selection[0]=B((u32)base3 + (u32)B(base3,0x9D),0x1DF6);
        selection[1]=B((u32)base3 + (u32)B(base3,0x9D),0x1DEE);
        value=func_000bc464((u16)(B((u32)base3 + (u32)B(base3,0x9D),0x1E00)-1));
        B(D_80196AF8,0x1DED)=value;
        base4=D_80196AF8;
        func_000b4b08(D_80193AC0[H(base4,0x1BEE + selection[0]*2)].field00|0x140000);
        B(D_80196AF8,0x9C)=1;
        base5=D_80196AF8;
        selection[3]=B(base5,0x1DED)-1;
        B(base5,0x1E2C)=1;
        B(D_80196AF8,0x9A)=1;
        { u8 *next = D_80196AF8; H(next,0x122)++; break; }
    }
    case 1: {

        u8 target_mode, group_mode;
        u8 *base, *other, *state, *squad;
        u8 *selection_state;
        s32 entry;
        selection_state=D_80196AF8;
        selection=(s16 *)(selection_state+0x1BDA);
        old_entry=B(selection_state,0x1BEF + selection[0]*2);
        action=func_000c8d28(selection);
        switch (action) {
        case 1: {
            entry=H(D_80196AF8,0x1BEE + SH(selection_state,0x1BDA)*2);
            if(!(func_00045460(D_80193AC0[entry].field00)&1)) func_000b4a70(0x40018);
            else if(D_80193AC0[entry].field03==D_80193AC0[entry].field02) func_000b4a70(0x40017);
            else {
                func_000c54c0(6);
                B(D_80196AF8,0x9A)=2;
                H(selection_state,0x1BE2)=H(selection_state,0x1BDA);
                func_000b6b70();
                item=D_80193AC0[entry].field00;
                slot_index=0;
                func_000c4650(item);
                for(;slot_index<10;slot_index++) {
                    u8 *clear_state=D_80196AF8;
                    W(clear_state,0x24FC + slot_index*4)=0;
                    /* Stage the record address to preserve the legacy local-register allocation. */
                    clear_state+=slot_index*16; B(clear_state,0x1E3F)=0;
                }
                target_mode=func_00045440(item);
                group_mode=2;
                if(target_mode==1) {
                    u8 *single_state, *single_current;
                    next_state=2;
                    single_state=D_80196AF8;
                    B(single_state,0x14D)=1;
                    single_current=D_80196AF8;
                    H(single_state,0x136)=14;
                    H(single_current,0x130)=14;
                    W(single_current,0x10AC)=777;
                    H(single_current,0x10C4)=1;
                    B(single_current,0x10A9)=3;
                    func_000b1f4c(0,0,2);
                } else {
                    u8 *group_state, *group_current;
                    group_state=D_80196AF8;
                    B(group_state,0x14D)=2;
                    group_current=D_80196AF8;
                    next_state=3;
                    H(group_state,0x136)=12;
                    H(group_current,0x130)=12;
                }
                func_000BA2C0(0,next_state);
                func_000b8a94();
                func_000b8d70();
                B(D_80196AF8,0x9C)=0;
                base=D_80196AF8;
                H(base,0x122)++;
                func_000ae30c(10,0,0);
            }
            break;
        }
        case 2:
            func_000c54c0(8);
            H(D_80196AF8,0x122)=99;
            break;
        default: func_000c8f04(selection); break;
        }
        base=D_80196AF8;
        entry=H(base,0x1BEE + selection[0]*2);
        if(entry!=old_entry) func_000b4b08(D_80193AC0[entry].field00|0x140000);
        break;
    }
    case 2: {
        u8 *details_base, *confirm_base, *cancel_base, *other, *state, *squad;
        buttons=D_800E8100;
        if(buttons&0x1000) {
            details_base=D_80196AF8;
            if(B(details_base,0x14D)==1) {
                /* Keep the loaded character narrow: this also preserves the retail compiler frame. */
                s16 selected_character=SH(details_base,0x18A);
                if(selected_character==255) { func_000c54c0(7); break; }
                D_801939D4=selected_character;
                func_000c4a40(details_base+0x610,0); func_000ae30c(13,1,0); break;
            } else {
                entry=H(details_base,0x18C);
                if(entry>=128) { func_000c54c0(7); break; }
                D_801939D5=entry;
                func_000c4a40(details_base+0x610,2); func_000ae30c(13,1,0); break;
            }

        } else if(buttons&0x8000) {
            confirm_base=D_80196AF8;
            if(B(confirm_base,0x14D)==1) {
                s32 confirmed_character=SH(confirm_base,0x18A);
                if(confirmed_character==255) { func_000c54c0(7); break; }
                { u8 *character_flags=confirm_base+0x1872; buttons=character_flags[confirmed_character*2]; }
                if(buttons&0x10) { func_000b4a70(0x4001F); break; }
                if(buttons&0x80) { func_000b4a70(0x4001E); break; }
                func_000c54c0(6);
                B(D_80196AF8,0x99)=1;
                func_000c6fe8(H(D_80196AF8,0x18A));
                B(D_80196AF8,0x609)=0;
                func_000b8aec();
                { u8 *next = D_80196AF8; H(next,0x122)++; break; }
            } else {
                entry=H(confirm_base,0x18C);
                if(entry&0x80) { func_000c54c0(7); break; }
                { u8 *group_flags=confirm_base+0x117D; if(group_flags[entry*0x36]&0x10) { func_000b4a70(0x4001F); break; } }
                func_000c54c0(6);
                B(D_80196AF8,0x9C)=1;
                { u8 *next = D_80196AF8; H(next,0x122)+=2; }
            }
        } else if(buttons&0x4000) {
            func_000c54c0(1);
            func_000bc828(128,2);
            func_000bc7e4(128,2);
            B(D_80196AF8,0x10A9)=0;
            cancel_base=D_80196AF8;
            H(cancel_base,0x10C4)=0;
            B(cancel_base,0x9A)=1;
            func_000b9234(0);
            func_000b6ba8();
            func_000b9234(17);
            B(D_80196AF8,0x9C)=1;
            { u8 *next = D_80196AF8; H(next,0x122)--; }
            func_000ae30c(10,0,0);
        } else func_000b1f00();
        break;

    }
    case 3: {
        s32 result, effect;
        u8 *cancel_next;
        u8 *phase0, *phase1, *phase2, *phase3, *phase4, *phase5, *phase6, *phase7, *other, *state, *squad;
        buttons=D_800E8100;
        if(buttons&0x1000) {
            phase0=D_80196AF8;
            D_801939D4=H(phase0,0x18A);
            func_000c4a40(phase0+0x610,5);
            { func_000ae30c(13,1,0); break; }
        } else if(buttons&0x8000) {
            B(D_80196AF8,0x10A9)=0;
            phase1=D_80196AF8;
            H(phase1,0x10C4)=0;
            D_80193AC0[H(phase1,0x1BEE + selection[0]*2)].field03--;
            func_000bc7e4(128,2);
            phase2=D_80196AF8;
            item=D_80193AC0[H(phase2,0x1BEE + selection[0]*2)].field00;
            result=func_00041098(0,0,item,B(phase2,0x18B));
            phase3=D_80196AF8;
            W(phase3,0x24FC)=result;
            W(phase3,0x2500)=result;
            other=D_8019A41C;
            PTR(phase3,0x1E30)=D_8019A384;
            if(W(phase3,0x24FC)>0) other=D_8019A3DC;
            B(phase3,0x1E3F)=128;
            state=D_80196AF8;
            PTR(phase3,0x1E40)=other;
            B(state,0x1E4F)=128;
            phase4=D_80196AF8;
            B(phase4,0x1E4C)=D_80193AC0[H(phase4,0x1BEE + selection[0]*2)].field00;
            if(item<6) func_000c54c0(19); else if(item>=8) func_000c54c0(21); else func_000c54c0(20);
            phase5=D_80196AF8;
            B(phase5,0xA0)|=128;
            phase6=D_80196AF8;
            H(phase6,0x12E)=10;
            H(phase6,0x122)=5;
        } else if(buttons&0x4000) {
            func_000c54c0(1);
            func_000b8b60();
            B(D_80196AF8,0x99)=0;
            phase7=D_80196AF8;
            B(phase7,0x10A9)=3;
            cancel_next=D_80196AF8;
            W(phase7,0x10AC)=777;
            H(phase7,0x10C4)=1;
            H(cancel_next,0x122)--;
        }
        break;
    }
    case 4: {
        u8 *initial, *members, *effect, *flags, *duration;
        func_000c54c0(19);
        initial=D_80196AF8;
        entry=H(initial,0x1BEE + selection[0]*2);

        item=D_80193AC0[entry].field00;

        D_80193AC0[entry].field03--;
        func_000bc7e4(128,2);
        func_000bc828(128,2);
        output=0;
        for(value=0;value<9;value++) {
            members=D_80196AF8;
            { u8 *group_members=members+H(members,0x18C)*0x36;
            if(B((u32)group_members+(u32)value,0x1180)!=255) {
                if(item==20 && output==0) {
                    s32 random=rand();
                    u8 *result_base=D_80196AF8;
                    s32 result=random%5+1;
                    W(result_base,0x24FC + output*4)=result;
                    W(result_base,0x2500 + output*4)=result;
                } else {
                    s32 result=func_00041098(0,0,item,B(members,0x18B));
                    u8 *result_base=D_80196AF8;
                    W(result_base,0x24FC + output*4)=result;
                    W(result_base,0x2500 + output*4)=result;
                }
                /* Form the complete record base before either store. */
                effect=D_80196AF8 + output*16;
                PTR(effect,0x1E30)=D_8019A384;
                B(effect,0x1E3F)=128;
                output+=2;
            }
        }
        }
        flags=D_80196AF8;
        B(flags,0xA0)|=128;
        duration=D_80196AF8;
        H(duration,0x12E)=10;
        H(duration,0x122)++;
        break;
    }
    case 5: {
        u16 item;
        s32 timer;
        s32 effect_offset;
        u8 *other, *state, *squad;

        active=0;
        slot_index=0;
        effect_offset=0x1E30;
        for(;slot_index<10;slot_index++,effect_offset+=16) if(func_00066e10(D_80196AF8+effect_offset)) active++;
        
        timer=H(D_80196AF8,0x12E);
        if(timer==1) {
            item=D_80193AC0[H(D_80196AF8,0x1BEE + selection[0]*2)].field00;
            if(func_00045440(item)==timer) {
                s32 value;
                if(W(D_80196AF8,0x24FC)<0) value=W(D_80196AF8,0x24FC);
                else { value=0; if(W(D_80196AF8,0x24FC)>0) value=W(D_80196AF8,0x24FC); }
                func_00041098(1,value,item,B(D_80196AF8,0x18B));
            } else if(item==20) {
                s32 value;
                
                squad=D_80196AF8; value=W(squad,0x24FC);
                if(value!=0) {
                    squad+=H(squad,0x18C)*0x36;
                    if(B(squad,0x119B)+value>100) B(squad,0x119B)=100;
                    else B(squad,0x119B)+=value;
                    W(D_80196AF8,0x24FC)=0;
                }
            } else {
                /* Separate the group-use lifetime so the legacy allocator can reuse the timer register. */
                u16 group_item=item;
                output=0;
                for(slot_index=0;slot_index<9;slot_index++) {
                    
                    u8 *member_state=D_80196AF8;
                    u8 *member_group=member_state+H(member_state,0x18C)*0x36;
                    if(B((u32)member_group+(u32)slot_index,0x1180)!=255) {
                        s32 value;
                        if(W(D_80196AF8,0x24FC + output*4)<0) { value=W(D_80196AF8,0x24FC + output*4); goto apply_group_value; }
                        value=W(D_80196AF8,0x24FC + output*4);
                        { s32 positive=0; if(value>0) positive=value; value=positive; }
apply_group_value:
                        
                        { u8 *apply_state=D_80196AF8; apply_state+=H(apply_state,0x18C)*0x36; func_00041098(1,value,group_item,B((u32)apply_state+(u32)slot_index,0x1180)); }
                        output+=2;
                    }
                }
            }
        }
        if(active) B(D_80196AF8,0x1E2D)=1;
        else { B(D_80196AF8,0x1E2D)=0; { u8 *next = D_80196AF8; H(next,0x122)++; break; } }
        break;
    }
    case 6: {
        u8 *base, *other, *state, *squad;
        B(D_80196AF8,0x1E2D)=0;
        base=D_80196AF8;
        H(base,0x12E)=16;
        H(base,0x122)++;
        break;
    }
    case 7: {
        u8 *base, *other, *state, *squad;
        base=D_80196AF8;
        if(H(base,0x12E)==0) H(base,0x122)++;
        break;
    }
    case 8: {
        u16 last_entry;
        u8 remaining;
        u8 *phase0, *phase1, *phase2, *phase3, *phase4, *phase5, *phase6, *phase7, *other, *state, *squad;
        func_000c54c0(1);
        func_000b9234(0);
        B(D_80196AF8,0x10A9)=0;
        phase0=D_80196AF8;
        H(phase0,0x10C4)=0;
        B(phase0,0x9A)=1;
        func_000b6ba8();
        B(D_80196AF8,0x99)=0;
        phase1=D_80196AF8;
        record=&D_80193AC0[H(phase1,0x1BEE + selection[0]*2)];
        if(record->field03==0) {
            record->field00=0;
            { u8 *slots=phase1+0x1E00; value=func_000bc464((u16)(slots[B(phase1,0x9D)]-1)); }
            B(D_80196AF8,0x1DED)=value;
            phase2=D_80196AF8;
            remaining=B(phase2,0x1DED);

            if(remaining==0) {
                B((u32)phase2 + (u32)B(phase2,0x9D),0x1E00)=0;
                do {
                    phase3=D_80196AF8;
                    B(phase3,0x9D)=(B(phase3,0x9D)+1)%B(phase3,0x1DFF);
                    phase4=D_80196AF8;
                } while(B((u32)phase4 + (u32)B(phase4,0x9D),0x1E00)==0);
                selection[0]=B((u32)phase4 + (u32)B(phase4,0x9D),0x1DF6);
                selection[1]=B((u32)phase4 + (u32)B(phase4,0x9D),0x1DEE);
                value=func_000bc464((u16)(B((u32)phase4 + (u32)B(phase4,0x9D),0x1E00)-1));
                B(D_80196AF8,0x1DED)=value;
                phase5=D_80196AF8;
                selection[3]=B(phase5,0x1DED)-1;
                B(phase5,0x1DFF)--;
                H(D_80196AF8,0x122)=99;
            } else {
                u16 first_entry;
                s32 first_index;
                last_entry=remaining-1;
                first_entry=selection[1];
                selection[3]=last_entry;
                first_index=first_entry & 0xFFFF;
                if(first_index+(u16)selection[2]-1>(last_entry & 0xFFFF)) {
                    u16 adjusted;
                    if(first_index==0) adjusted=0; else adjusted=first_entry-1;
                    selection[1]=adjusted;
                }
                if(selection[0]>(u16)selection[3]) selection[0]=(u16)selection[3];
                H(D_80196AF8,0x122)=1;
            }
        } else H(phase1,0x122)=1;
        phase6=D_80196AF8;
        if(SH(phase6,0x122)==1) {
            func_000b4b08(D_80193AC0[H(phase6,0x1BEE + selection[0]*2)].field00|0x140000);
            func_000b9234(17);
        }
        phase7=D_80196AF8;
        B(phase7,0xA0)&=127;
        func_000ae30c(10,0,0);
        break;
    }
    case 99: {
        u8 *base, *other, *state, *squad;
        if(!func_000b8e70(0x400)) {
            func_000b6ba8();
            func_000ae30c(10,0,0);
        } else {
            resources=PTR(D_80196AF8,0x1E28);
            if(resources) {
                func_000016C4(resources[0]);
                func_000016C4(resources[1]);
                func_000016C4(PTR(D_80196AF8,0x1E28));
                PTR(D_80196AF8,0x1E28)=0;
            }
            B(D_80196AF8,0x1E2C)=0;
            func_000b4bb8();
        }
        break;
    }
    }
    return;

}












