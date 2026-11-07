/* Playback storage for authored repeated spells. Calculation still belongs to
 * the accepted battle producers; each native visual impact consumes one of
 * their result commands through the original battle callback. */
#include "battle-state.h"
typedef unsigned char u8;
typedef unsigned int u32;
extern u8 lc_strike_state[];
void lc_original_visual_impact(int, int);
#define SW(p,n) (*(u32 *)((u8 *)(p)+(n)))
#define SB(p,n) (*((u8 *)(p)+(n)))
enum { STRIKE_LIMIT = 18, RESULT_LIMIT = 36 };
typedef struct {
    u32 stream;
    int enabled;
    u8 sources[20], targets[20], flags[20];
    u8 count[20], next[20];
    u32 results[20][RESULT_LIMIT];
} StrikePlayback;
#define PLAY ((StrikePlayback *)lc_strike_state)
/* Reserved by both exporters and previews. Never change actor record layout. */
typedef char strike_storage_fits[(sizeof(StrikePlayback) <= 4096) ? 1 : -1];

int lc_strike_command(u8 *command) {
    u8 *sources=command+5, *targets=sources, *flags;
    int i,n;
    while(*targets!=255)++targets;
    ++targets;
    flags=targets;
    while(*flags!=255)++flags;
    ++flags;
    PLAY->enabled=0;
    if(targets[0]==255 || !(flags[0]&128))return 0;
    PLAY->stream=*(u32 *)0x801ce8c0;
    PLAY->enabled=1;
    for(i=0;i<20;i++) {
        PLAY->sources[i]=PLAY->targets[i]=PLAY->flags[i]=255;
        PLAY->count[i]=PLAY->next[i]=0;
    }
    for(i=0;i<19 && sources[i]!=255;i++)PLAY->sources[i]=sources[i];
    for(n=0;n<STRIKE_LIMIT && targets[n]!=255;n++) {
        PLAY->targets[n]=targets[n]; PLAY->flags[n]=flags[n];
    }
    return 1;
}
void lc_strike_end(void) { PLAY->enabled=0; }
int lc_strike_round_mode(void) {
    /* Keep the authenticated round-mode call in the original owner as the
     * compatibility indirection. Custom encounter export can replace that
     * call; the battle entry hook leaves the rest of the owner intact. */
    u32 call=*(u32 *)0x801ddfb0;
    return ((int (*)(void))(0x80000000U|((call&0x03ffffffU)<<2)))();
}
u8 *lc_strike_sources(void) { return PLAY->sources; }
u8 *lc_strike_targets(void) { return PLAY->targets; }
u8 *lc_strike_flags(void) { return PLAY->flags; }
/* Preview submission has no battle command stream, but uses the same driver. */
void lc_strike_visual_list(u8 *targets) {
    u32 at=SW(lc_strike_state,4112)&3;
    SW(lc_strike_state,4096+at*4)=(u32)targets;
    SW(lc_strike_state,4112)=at+1;
}
int lc_strike_is_list(u8 *targets) {
    int i;
    for(i=0;i<4;i++)if(SW(lc_strike_state,4096+i*4)==(u32)targets)return 1;
    return 0;
}
int lc_strike_result(void *actor,u32 cursor) {
    u32 base=*(u32 *)0x801ce8bc+0x1c4;
    u32 index=((u32)actor-base)/0xf8;
    u8 *command;
    int i;
    if(!PLAY->enabled || PLAY->stream!=*(u32 *)0x801ce8c0 || index>=20)return 0;
    for(i=0;i<STRIKE_LIMIT && PLAY->targets[i]!=255;i++)if(PLAY->targets[i]==index)break;
    if(i==STRIKE_LIMIT || PLAY->targets[i]==255)return 0;
    command=(u8 *)(PLAY->stream+16+cursor);
    /* At most18 hits plus secondary/attacker results. Preserve order, including
     * multiple results referring to the same actor; no deduplication. */
    if(PLAY->count[index]<RESULT_LIMIT)
        PLAY->results[index][PLAY->count[index]++]=cursor | ((command[4]&128)?0x80000000U:0);
    command[4]&=127;
    return 1;
}
static void lc_complete_visual_impact(int id,int kind) {
    u8 *stream=*(u8 **)0x801ce8c0;
    u32 cursor;
    int pending=stream?*(u8 *)lc_battle_field(stream,0x824):0;
    lc_original_visual_impact(id,kind);
    if(!stream || stream!=*(u8 **)0x801ce8c0 || !pending ||
       *(u8 *)lc_battle_field(stream,0x824))return;
    cursor=*(u32 *)lc_battle_field(stream,0x810);
    if(cursor>=*(u32 *)lc_battle_field(stream,0x814))return;
    /* The native final contact seeks to the current playback time, then clears
     * this group's wait delays. If contact arrives partway through the wait,
     * its elapsed clock can no longer equal the new zero delay. Retain the
     * elapsed portion, as the native special-visual path does. This releases
     * the wait on the next tick without desynchronizing future absolute-time
     * seeks. The separate visual-active barrier still owns effect completion. */
    if((stream[16+cursor]==0x1e || stream[16+cursor]==0x20) &&
       stream[18+cursor]==0 && SW(stream,4)<256)
        stream[18+cursor]=(u8)SW(stream,4);
}
void lc_attack_visual_impact(int id,int kind) {
    u8 *actor;
    int at,end;
    if(id<0 || id>=20 || !PLAY->enabled || PLAY->stream!=*(u32 *)0x801ce8c0) {
        lc_complete_visual_impact(id,kind); return;
    }
    actor=(u8 *)(*(u32 *)0x801ce8bc+0x1c4+id*0xf8);
    at=PLAY->next[id];
    if(!PLAY->count[id]) { lc_complete_visual_impact(id,kind); return; }
    if(at>=PLAY->count[id])return;
    end=at+1;
    while(end<PLAY->count[id] && (PLAY->results[id][end]&0x80000000U))++end;
    /* Reserve continuation notifications before the first result, so the final
     * hit cannot release the battle's completion barrier prematurely. */
    (*(u8 *)lc_battle_field((void *)PLAY->stream,0x824))+=end-at-1;
    /* The original consumer expects reversed pending offsets and indexes by
     * byteD0. Present exactly one entry, so it never reads past actor+0xCC. */
    do {
        SW(actor,0xb0)=PLAY->results[id][at]&0x7fffffffU;
        SB(actor,0xd0)=1; SB(actor,0xd1)=1;
        lc_complete_visual_impact(id,kind);
        if(SB(actor,0xd0))return;
        PLAY->next[id]=++at;
        if(at>=end)break;
    } while(1);
}
