/* Per-effect sound ownership. Retail sound/voice scheduling stays native. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;
extern u8 lc_attack_settings[],lc_effect_state[],lc_audio_state[];
#define W(p,n) (*(u32 *)((u8 *)(p)+(n)))
#define H(p,n) (*(u16 *)((u8 *)(p)+(n)))
#define bypass W(lc_effect_state,8196)
#define clock W(lc_audio_state,0)
#define count W(lc_audio_state,4)
void lc_original_edit_sound(int,int,int);
void lc_original_edit_sound_request(int,int);
void lc_audio_submit(u8 *,int);
u8 *lc_audio_base(u8 *p) {
    return p && W(p,0)==0x4c434658 && W(p,4)==0x30303033 ? p+W(p,12) : p;
}
static u8 *audio(int owner) {
    u8 *p;
    if(owner<1 || owner>254)return 0;
    p=(u8 *)W(lc_attack_settings+owner*32,24);
    return p && W(p,0)==0x4c434658 && W(p,4)==0x30303033 ? p+W(p,16) : 0;
}
static void emit(u8 *row,int actor,int context) {
    u8 *bank;
    int old=bypass;
    bypass=1;
    if(W(row,8)) {
        /* Row +12 is a relocated RAM-only bank pointer prepared by schedule. */
        bank=(u8 *)W(row,12);
        if(W(bank,0)==0x4c43534e && W(bank,4)==0x44303031) {
            int type=actor<20?0:(unsigned char)(actor-20)<27?1:(unsigned char)(actor-47)<40?2:-1;
            int side=1;
            if(type>=0) side=((int (**)(unsigned char))0x80209700)[type](actor)>=0;
            lc_audio_submit(bank,side);bypass=old;return;
        }
        if(!(W(bank,16)&0x80000000)) {
            ((void (*)(void *,u32))0x80081db4)(bank,0);
            ((void (*)(void *))0x80081f7c)(bank+128);
            ((void (*)(void *,void *))0x8008202c)(bank+128,bank);
        }
        ((int (*)(void *,int,int,int,int))0x80083b24)(bank+128,0,128,128,-1);
    } else if((s16)H(row,4)>=0) {
        ((void (*)(int,int,int))0x801f9dfc)(context,actor,H(row,4));
    } else {
        ((void (*)(int,int))0x801c5938)(context,H(row,6));
    }
    bypass=old;
}
/* Sound delays must not be native visual tasks: several retail effects pause
 * their task group, and the retail pool only contains 24 tasks. */
void lc_audio_tick(void) {
    int i,j;u8 *q;
    for(i=0;i<(int)count;) {
        q=lc_audio_state+16+i*16;
        if(W(q,12)>clock){++i;continue;}
        emit((u8 *)W(q,0),W(q,4),W(q,8));
        --count;
        for(j=i;j<(int)count;++j) {
            W(lc_audio_state,16+j*16)=W(lc_audio_state,32+j*16);
            W(lc_audio_state,20+j*16)=W(lc_audio_state,36+j*16);
            W(lc_audio_state,24+j*16)=W(lc_audio_state,40+j*16);
            W(lc_audio_state,28+j*16)=W(lc_audio_state,44+j*16);
        }
    }
    ++clock;
}
void lc_original_edit_audio_init(void);
void lc_original_edit_audio_free(void);
void lc_attack_edit_audio_init(void){clock=0;count=0;bypass=0;lc_original_edit_audio_init();}
void lc_attack_edit_audio_free(void){count=0;lc_original_edit_audio_free();}
static void schedule(u8 *a,int anchor,int actor,int context) {
    int i;u8 *r,*q;
    for(i=0;i<(int)W(a,8);++i) {
        r=a+32+i*16;
        if((s16)H(r,0)!=anchor)continue;
        if(W(r,8))W(r,12)=(u32)(a+W(r,8));
        if(!H(r,2))emit(r,actor,context);
        else if(count<256) {
            q=lc_audio_state+16+count++*16;
            W(q,0)=(u32)r;W(q,4)=actor;W(q,8)=context;W(q,12)=clock+H(r,2);
        }
    }
}
void lc_audio_start(u8 *config,int actor) {
    int i; u8 *a;
    for(i=1;i<255;++i) {
        a=audio(i);
        if(a && lc_audio_base((u8 *)W(lc_attack_settings+i*32,24))==config) {
            schedule(a,-1,actor,0);return;
        }
    }
}
void lc_attack_edit_sound(int context,int actor,int cue) {
    u8 *a=audio(W(lc_effect_state,0));
    if(a && !bypass) {schedule(a,cue,actor,context);return;}
    lc_original_edit_sound(context,actor,cue);
}
void lc_attack_edit_sound_request(int context,int request) {
    if(audio(W(lc_effect_state,0)) && !bypass)return;
    lc_original_edit_sound_request(context,request);
}
