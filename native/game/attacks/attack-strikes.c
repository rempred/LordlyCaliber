/* Playback storage for authored repeated spells. Calculation still belongs to
 * the accepted battle producers; each native visual impact consumes one of
 * their result commands through the original battle callback. */
#include "battle-state.h"
typedef unsigned char u8;
typedef unsigned int u32;
extern u8 lc_strike_state[];
void lc_original_visual_impact(int, int);
u32 func_0021C970(u32);
#define SW(p,n) (*(u32 *)((u8 *)(p)+(n)))
#define SB(p,n) (*((u8 *)(p)+(n)))
enum { STRIKE_LIMIT = 18, RESULT_LIMIT = 36 };
typedef struct {
    u32 stream;
    int enabled;
    u8 sources[20], targets[20], flags[20];
    u8 count[20], next[20];
    u32 results[20][RESULT_LIMIT];
    u32 afterStream, after[20];
    int afterCount;
} StrikePlayback;
#define PLAY ((StrikePlayback *)lc_strike_state)
/* Reserved by both exporters and previews. Never change actor record layout. */
typedef char strike_storage_fits[(sizeof(StrikePlayback) <= 4096) ? 1 : -1];
static void lc_deliver_after(u32,int);
static void lc_flush_after(int);

int lc_strike_command(u8 *command) {
    u8 *sources=command+5, *targets=sources, *flags;
    int i,n,repeated;
    while(*targets!=255)++targets;
    ++targets;
    flags=targets;
    while(*flags!=255)++flags;
    ++flags;
    PLAY->enabled=0;
    PLAY->afterStream=*(u32 *)0x801ce8c0;
    PLAY->afterCount=0;
    if(targets[0]==255)return 0;
    repeated=(flags[0]&128)!=0;
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
    return repeated;
}
void lc_strike_end(void) { PLAY->enabled=0; lc_flush_after(1); }
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
    command=(u8 *)(*(u32 *)0x801ce8c0+16+cursor);
    /* Authored drain/recoil have no guaranteed caster contact in the chosen
     * visual. Keep them out of the actor's native impact queue and deliver
     * once after the main contacts (or at their own time for physical actions).
     * Bit6 is private; native result kinds occupy only the low four bits. */
    if(command[4]&64) {
        if(PLAY->afterStream!=*(u32 *)0x801ce8c0) {
            PLAY->afterStream=*(u32 *)0x801ce8c0;PLAY->afterCount=0;
        }
        if(!(command[4]&16) && *(u8 *)lc_battle_field((void *)PLAY->afterStream,0x822)) {
            if(PLAY->afterCount<20)PLAY->after[PLAY->afterCount++]=cursor;
        } else lc_deliver_after(cursor,1);
        return 1;
    }
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
    u8 *actor;
    u32 cursor,offset,length,prior,missing,run,next,savedFlags;
    int reserved=0,statusKind=-1;
    int pending=stream?*(u8 *)lc_battle_field(stream,0x824):0;
    /* A custom secondary command may have overwritten the actor's parameter
     * bytes after its first result was queued. Reload that queued result just
     * as the native consumer already does for a multi-result actor. */
    if(stream && id>=0 && id<20) {
        actor=(u8 *)(*(u32 *)0x801ce8bc+0x1c4+id*0xf8);
        if(SB(actor,0xd0)>0 && SB(actor,0xd0)<=8) {
            offset=SW(actor,0xac+4*SB(actor,0xd0));
            if(offset<*(u32 *)lc_battle_field(stream,0x814) && stream[16+offset]==0x1f) {
                SB(actor,0xd1)=1;
                /* An effect longer than its predicted wait can still deliver
                 * impacts while the completion barrier is active. Leave room
                 * before that barrier for the native presentation commands
                 * inserted at this impact's time, rather than seeking past it. */
                cursor=*(u32 *)lc_battle_field(stream,0x810);
                if(cursor<*(u32 *)lc_battle_field(stream,0x814) &&
                   stream[16+cursor]==0x20 && *(u8 *)lc_battle_field(stream,0x822) &&
                   stream[18+cursor]<255 && SW(stream,4)==stream[18+cursor])
                    ++stream[18+cursor];
                if(pending==1 && PLAY->afterStream==(u32)stream && PLAY->afterCount) {
                    reserved=PLAY->afterCount;
                    *(u8 *)lc_battle_field(stream,0x824)+=reserved;
                }
                if(stream[20+offset]&32) {
                    /* Native secondary-status presentation writer (US Rev 0
                     * battle RAM 801DFB80). Preserve playback state like the
                     * original impact consumer, and use its no-op result kind
                     * for queue/clock completion without a zero-heal popup. */
                    statusKind=stream[20+offset];savedFlags=SW(actor,0x40);
                    SW(actor,0x40)=(savedFlags&~0x3eU)|stream[21+offset];
                    /* Some primary reactions (petrification in particular)
                     * finish after contact. Queue the secondary after that
                     * reaction, not before its delayed state command. */
                    prior=SW(actor,0x94);if(prior<SW(stream,8))prior=SW(stream,8);
                    ((int (*)(void *,int,int))0x801dfb80)(actor,prior,stream[25+offset]);
                    SW(actor,0x40)=savedFlags;stream[20+offset]=5;
                }
            }
        }
    }
    lc_original_visual_impact(id,kind);
    if(statusKind>=0)stream[20+offset]=(u8)statusKind;
    if(reserved && stream==*(u8 **)0x801ce8c0) {
        if(*(u8 *)lc_battle_field(stream,0x824)==reserved)lc_flush_after(0);
        else *(u8 *)lc_battle_field(stream,0x824)-=reserved;
    }
    if(!stream || stream!=*(u8 **)0x801ce8c0 || !pending ||
       *(u8 *)lc_battle_field(stream,0x824))return;
    cursor=*(u32 *)lc_battle_field(stream,0x810);
    length=*(u32 *)lc_battle_field(stream,0x814);
    if(cursor>=length)return;
    /* Clearing a whole run of wait markers also erases completed 255-tick
     * chunks. Restore their elapsed contribution before the current cursor,
     * or a later native absolute-time seek can jump into the next attack. */
    offset=0;run=length;
    while(offset<length) {
        if(stream[16+offset]==0x1e) {
            if(run==length)run=offset;
        } else {
            if(stream[16+offset]==0x20 && stream[19+offset]==*(u8 *)lc_battle_field(stream,0x823))break;
            run=length;
        }
        next=func_0021C970(offset);
        if(next<=offset || next>=length)break;
        offset=next;
    }
    /* Include any ordinary commands before the seek position as well. */
    offset=0;prior=0;
    while(offset<cursor) {
        prior+=stream[18+offset];next=func_0021C970(offset);
        if(next<=offset || next>=length)break;
        offset=next;
    }
    missing=SW(stream,8)-SW(stream,4);
    if(missing>=prior && run<cursor) {
        missing-=prior;
        while(run<cursor && missing && stream[16+run]==0x1e) {
            if(!stream[18+run]) {
                stream[18+run]=(u8)(missing>255?255:missing);
                missing-=stream[18+run];
            }
            next=func_0021C970(run);if(next<=run || next>=length)break;run=next;
        }
    }
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
static void lc_deliver_after(u32 offset,int isolated) {
    u8 *stream=*(u8 **)0x801ce8c0,*command=stream+16+offset,*actor;
    u32 cursor,clock,total,head;
    int pending,queued,reload,kind,id;
    if(offset>=*(u32 *)lc_battle_field(stream,0x814) || command[0]!=0x1f)return;
    id=command[3];if(id>=20)return;
    actor=(u8 *)(*(u32 *)0x801ce8bc+0x1c4+id*0xf8);
    head=SW(actor,0xb0);queued=SB(actor,0xd0);reload=SB(actor,0xd1);
    cursor=*(u32 *)lc_battle_field(stream,0x810);clock=SW(stream,4);total=SW(stream,8);
    pending=*(u8 *)lc_battle_field(stream,0x824);kind=command[4];
    /* An immediate physical result must not release another visual's wait
     * markers. The native consumer decrements this temporary count to one;
     * restore the actual count and clock after its presentation is queued. */
    if(isolated)*(u8 *)lc_battle_field(stream,0x824)=2;
    command[4]&=15;SW(actor,0xb0)=offset;SB(actor,0xd0)=SB(actor,0xd1)=1;
    lc_complete_visual_impact(id,10);
    command[4]=(u8)kind;SW(actor,0xb0)=head;SB(actor,0xd0)=queued;SB(actor,0xd1)=reload;
    if(isolated) {
        *(u8 *)lc_battle_field(stream,0x824)=pending;
        *(u32 *)lc_battle_field(stream,0x810)=cursor;SW(stream,4)=clock;SW(stream,8)=total;
    }
}
static void lc_flush_after(int isolated) {
    int i,n;
    if(PLAY->afterStream!=*(u32 *)0x801ce8c0)return;
    n=PLAY->afterCount;PLAY->afterCount=0;
    for(i=0;i<n;i++)lc_deliver_after(PLAY->after[i],isolated);
}
void lc_attack_visual_impact(int id,int kind) {
    u8 *actor;
    int at,end,resultKind,amount,hp;
    unsigned short originalHP;
    u32 offset,scan,next,time;
    u8 *packet;
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
    originalHP=*(unsigned short *)(actor+0x20);
    /* The original consumer expects reversed pending offsets and indexes by
     * byteD0. Present exactly one entry, so it never reads past actor+0xCC. */
    do {
        offset=PLAY->results[id][at]&0x7fffffffU;
        packet=(u8 *)PLAY->stream+16+offset;
        resultKind=packet[4]&63;amount=(packet[5]<<8)|packet[6];
        SW(actor,0xb0)=offset;
        SB(actor,0xd0)=1; SB(actor,0xd1)=1;
        lc_complete_visual_impact(id,kind);
        if(SB(actor,0xd0))break;
        PLAY->next[id]=++at;
        if(at>=end)break;
        /* The native callback restores pre-impact HP while it queues visual
         * commands. A continuation must nevertheless calculate its reaction
         * from the preceding result: two individually nonlethal losses can
         * be lethal together. Restore the real playback state below. */
        hp=*(unsigned short *)(actor+0x20);
        if(resultKind==0 || resultKind==1 || resultKind==13) {
            hp-=amount;if(hp<0)hp=0;
        } else if(resultKind==2) {
            hp+=amount;if(hp>*(unsigned short *)(actor+0x22))hp=*(unsigned short *)(actor+0x22);
        }
        *(unsigned short *)(actor+0x20)=(unsigned short)hp;
        /* Petrification's native writer does not update ready-time, but emits
         * its state change after the hit and recovery poses. A secondary cure
         * must follow that generated command. */
        if(resultKind==11) {
            scan=0;time=0;
            while(scan<*(u32 *)lc_battle_field((void *)PLAY->stream,0x814)) {
                packet=(u8 *)PLAY->stream+16+scan;time+=packet[2];
                if(packet[0]==0x23 && packet[3]==id && time>SW(actor,0x94))SW(actor,0x94)=time;
                next=func_0021C970(scan);if(next<=scan)break;scan=next;
            }
        }
    } while(1);
    *(unsigned short *)(actor+0x20)=originalHP;
}
