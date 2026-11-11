/* Private sound sources. Original libmus still interprets notes, envelopes,
 * modulation and samples. No shared retail sequence/sample is edited. */
typedef unsigned char sp_u8;
typedef unsigned int sp_u32;
extern sp_u8 lc_audio_state[];
#define SPW(p,n) (*(sp_u32 *)((sp_u8 *)(p)+(n)))
#define SPSTATE (lc_audio_state+4112)
#define SPFX ((sp_u8 *)0x8010a760)
#define SPSAMPLES ((sp_u8 *)0x80104710)
void lc_original_edit_sound_frame(void);
void lc_original_edit_sound_flush(void *,int);
void lc_original_edit_sound_decode(void *,int);
static sp_u32 *sp_seed(sp_u32 handle) {
    sp_u32 i,j,n;sp_u8 *c;sp_u32 *p;
    for(i=0;i<128;++i){p=(sp_u32 *)(lc_audio_state+8224+i*8);if(p[0]==handle)return p+1;}
    for(i=0;i<128;++i) {
        p=(sp_u32 *)(lc_audio_state+8224+i*8);n=*(sp_u32 *)0x800b1804;c=(sp_u8 *)*(sp_u32 *)0x800b180c;
        for(j=0;j<n;++j,c+=0x13c)if(p[0] && SPW(c,4) && SPW(c,0x44)==p[0])break;
        if(j==n){p[0]=handle;p[1]=*(sp_u32 *)0x800b1824;return p+1;}
    }
    return (sp_u32 *)0x800b1824; /* More handles than native channels is impossible. */
}
static int sp_magic(sp_u8 *b) { return b && SPW(b,0)==0x4c43534e && SPW(b,4)==0x44303031; }
void lc_audio_submit(sp_u8 *b,int side) {
    sp_u32 n=SPW(SPSTATE,4),i;sp_u8 *q;
    /* Same source requested twice before the audio update is one request,
       as in the native game queue. Distinct sources stay independent. */
    for(i=0;i<n;++i) {
        q=SPSTATE+16+i*16;
        if(SPW(q,0)==(sp_u32)b && SPW(q,8)==(sp_u32)side && SPW(q,4)==SPW(SPSTATE,0))return;
    }
    if(n>=256)return;
    q=SPSTATE+16+n*16;SPW(q,0)=(sp_u32)b;SPW(q,4)=SPW(SPSTATE,0);SPW(q,8)=side;SPW(SPSTATE,4)=n+1;
}
static void sp_init(sp_u8 *b,sp_u8 *fx,sp_u8 *samples) {
    sp_u32 old=*(sp_u32 *)0x800b1838,one=*(sp_u32 *)0x800b183c,ptr=*(sp_u32 *)0x800b1828;
    if(!SPW(b,48)) {
        if(SPW(b,44))SPW(b+SPW(b,44),12)=(sp_u32)(b+SPW(b,44)+64);
        ((void (*)(void *))0x80081f7c)(fx);
        ((void (*)(void *,void *))0x8008202c)(fx,samples);
        SPW(b,48)=1;
    }
    *(sp_u32 *)0x800b1838=old;*(sp_u32 *)0x800b183c=one;*(sp_u32 *)0x800b1828=ptr;
}
static void sp_start(sp_u8 *b,int side) {
    sp_u32 kind=SPW(b,16),i,j,n,handle,flags,priority;sp_u8 *fx,*state,*channel,*row,*q;
    if(kind==2) {
        fx=b+64+128;
        if(!SPW(b,48))((void (*)(void *,sp_u32))0x80081db4)(b+64,0);
        sp_init(b,fx,b+64);
        ((int (*)(void *,int,int,int,int))0x80083b24)(fx,0,128,128,-1);
    } else if(kind==0 || kind==1 || kind==3) {
        fx=b+64;sp_init(b,fx,SPSAMPLES);
        handle=((int (*)(void *,int,int,int,int))0x80083b24)(fx,kind==1?0:SPW(b,side?20:24),128,128,-1);
        if(handle && kind==0)*sp_seed(handle)=*(sp_u32 *)0x800b1824;
        if(kind==1 && handle) {
            state=b+SPW(b,side?28:32);
            *sp_seed(handle)=SPW(state,0x148);
            channel=(sp_u8 *)*(sp_u32 *)0x800b180c;
            n=*(sp_u32 *)0x800b1804;
            for(i=0;i<n;++i,channel+=0x13c) if(SPW(channel,0x44)==handle && SPW(channel,0x78)==(sp_u32)fx) {
                flags=SPW(channel,0);priority=SPW(channel,0x48);
                for(j=0;j<0x13c;++j)channel[j]=state[j];
                SPW(channel,0)=flags;SPW(channel,0x44)=handle;SPW(channel,0x48)=priority;SPW(channel,0x78)=(sp_u32)fx;
                *(unsigned short *)(channel+0xa0)=65535;channel[0xbe]=255;channel[0xcc]=255;SPW(channel,0x138)=0;
            }
        }
    }
    n=SPW(b,36);
    for(i=0;i<n;++i) {
        row=b+SPW(b,40)+i*32;
        if(!SPW(row,28) || SPW(SPSTATE,4)>=256 || SPW(row,side?4:8)==0xffffffff)continue;
        q=SPSTATE+16+SPW(SPSTATE,4)*16;
        SPW(q,0)=(sp_u32)(b+SPW(row,28));SPW(q,4)=SPW(SPSTATE,0)+SPW(row,side?4:8);SPW(q,8)=side;
        ++SPW(SPSTATE,4);
    }
}
void lc_attack_edit_sound_decode(void *arg,int voice) {
    sp_u8 *c=(sp_u8 *)arg,*fx=(sp_u8 *)SPW(c,0x78),*b=fx?fx-64:0;
    sp_u32 old,*seed;
    if(sp_magic(b) && (SPW(b,16)==0 || SPW(b,16)==1)) {
        seed=sp_seed(SPW(c,0x44));old=*(sp_u32 *)0x800b1824;*(sp_u32 *)0x800b1824=*seed;
        lc_original_edit_sound_decode(arg,voice);*seed=*(sp_u32 *)0x800b1824;*(sp_u32 *)0x800b1824=old;
    } else lc_original_edit_sound_decode(arg,voice);
}
void lc_attack_edit_sound_frame(void) {
    sp_u32 i=0,j;sp_u8 *q,*b;int side;
    while(i<SPW(SPSTATE,4)) {
        q=SPSTATE+16+i*16;
        if(SPW(q,4)>SPW(SPSTATE,0)){++i;continue;}
        b=(sp_u8 *)SPW(q,0);side=SPW(q,8);
        --SPW(SPSTATE,4);
        for(j=i;j<SPW(SPSTATE,4);++j) {
            SPW(SPSTATE,16+j*16)=SPW(SPSTATE,32+j*16);
            SPW(SPSTATE,20+j*16)=SPW(SPSTATE,36+j*16);
            SPW(SPSTATE,24+j*16)=SPW(SPSTATE,40+j*16);
        }
        sp_start(b,side);
    }
    lc_original_edit_sound_frame();++SPW(SPSTATE,0);
}
void lc_attack_edit_sound_flush(void *arg,int voice) {
    sp_u8 *c=(sp_u8 *)arg,*fx=(sp_u8 *)SPW(c,0x78),*b=fx?fx-64:0,*row;
    sp_u32 ordinal,key,i,kind;int mute=0;
    if(SPW(c,8) && sp_magic(b)) {
        ordinal=++SPW(c,0x138);kind=SPW(b,16);
        if(kind==1)mute=ordinal>1;
        else if(kind==3) {
            key=SPW(c,0x80)-(sp_u32)fx;
            for(i=0;i<SPW(b,36);++i) {
                row=b+SPW(b,40)+i*32;
                if((SPW(row,12)==key && SPW(row,16)==ordinal) || (SPW(row,20)==key && SPW(row,24)==ordinal))mute=1;
            }
        }
        if(mute)SPW(c,8)=(sp_u32)(b+SPW(b,44));
    }
    lc_original_edit_sound_flush(arg,voice);
}
