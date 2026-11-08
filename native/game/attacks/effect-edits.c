/* Independent retail visual contexts. No character or attack mechanics read
 * these settings. Native tasks, cache reference counts and asset parsers remain
 * responsible for resource lifetimes. All maps are reset on slot allocation. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed short s16;
extern u8 lc_attack_settings[], lc_effect_state[];
#define W(p,n) (*(u32 *)((u8 *)(p)+(n)))
#define H(p,n) (*(u16 *)((u8 *)(p)+(n)))
#define G(a) (*(u32 *)(a))
#define active W(lc_effect_state,0)
#define task_id(i) W(lc_effect_state,4+(i)*4)
#define graphic_id(i) W(lc_effect_state,1028+(i)*8)
#define graphic_fn(i) W(lc_effect_state,1032+(i)*8)
#define packet_id(i) W(lc_effect_state,3076+(i)*4)
#define screen_id(i) W(lc_effect_state,4100+(i)*4)
#define packet_fn(i) W(lc_effect_state,5124+(i)*4)
#define screen_fn(i) W(lc_effect_state,6148+(i)*4)
#define cache_id(i) W(lc_effect_state,7172+(i)*4)
void lc_original_edit_task(void *, int);
void *lc_original_edit_task_alloc(u32,void *);
void *lc_original_edit_graphic(u32);
void *lc_original_edit_cache(int,int,int);
void *lc_original_edit_cache_alloc(void);
void *lc_original_edit_resource(int,int);
void *lc_original_edit_model(int);
void *lc_original_edit_texture(int);
void *lc_original_edit_material(int);
void *lc_original_edit_geometry(int);
void *lc_original_edit_frame(int);
void *lc_original_edit_strip(int);
void *lc_original_edit_texture_kind(int,int);
void *lc_original_edit_packet(void);
void *lc_original_edit_packet_callback(u32);
void *lc_original_edit_screen(void);
void *lc_original_edit_screen_callback(u32,int);
void lc_original_edit_scene_draw(void);
void lc_original_edit_screen_draw(int);
u32 lc_effect_color(u32,int,int,u32);
static u8 *config(int id) {
    u8 *p;
    if (id < 1 || id > 254) return 0;
    p = (u8 *)W(lc_attack_settings + id * 32,24);
    return p && W(p,0)==0x4c434658 && W(p,4)==0x30303032 ? p : 0;
}
static int index(void *p,u32 base,int stride,int skip) {
    u32 offset = (u32)p - base - skip;
    return offset < (u32)(stride*256) && offset % stride == 0 ? offset / stride : -1;
}
int lc_effect_push(void *p) {
    int id,old=active;
    active=0;
    if(!p) return old; /* A retail effect has no custom resource namespace. */
    for(id=1;id<255;id++) if(config(id)==p) { active=id; break; }
    return old;
}
void lc_effect_pop(int old) { active=old; }
void lc_effect_start(void *p) {
    /* Reserved config words are RAM-only scratch; every submission resets them
     * after its visual delay, before the retail root creates its children. */
    if(W(p,0)==0x4c434658 && W(p,4)==0x30303032) {
        W(p,48)=0; W(p,52)=1;
    }
}
void lc_effect_bind_current(void *p) {
    int at=index((void *)G(0x80220fe8),G(0x80220d84),156,0);
    lc_effect_push(p);
    if(at>=0) task_id(at)=active;
}
void lc_attack_edit_task(void *task,int state) {
    int at=index(task,G(0x80220d84),156,0), old=active;
    if(at>=0) {
        if(state==256) task_id(at)=active;
        active=task_id(at);
    } else active=0;
    lc_original_edit_task(task,state);
    active=old;
}
void *lc_attack_edit_task_alloc(u32 callback,void *parent) {
    /* Native creation invokes state256 inline, bypassing the dispatcher. */
    int owner=active,at;
    void *p=lc_original_edit_task_alloc(callback,parent);
    at=index(p,G(0x80220d84),156,0);
    if(at>=0) task_id(at)=owner;
    active=owner;
    return p;
}
static int graphic_call(int state,void *node) {
    int at=index(node,G(0x80220d6c),148,0), old=active, result=0;
    if(at>=0) {
        active=graphic_id(at);
        result=((int (*)(int,void *))graphic_fn(at))(state,node);
    }
    active=old;
    return result;
}
void *lc_attack_edit_graphic(u32 callback) {
    void *p=lc_original_edit_graphic(callback);
    int at=index(p,G(0x80220d6c),148,0);
    if(at>=0) {
        graphic_id(at)=active; graphic_fn(at)=callback;
        if(active) W(p,4)=(u32)graphic_call;
    }
    return p;
}
static int named(int id) { return active && id>0 && id<65536 ? (active<<16)|id : id; }
void *lc_attack_edit_cache_alloc(void) {
    void *p=lc_original_edit_cache_alloc(); int at=index(p,G(0x80220ca8),20,0);
    if(at>=0) cache_id(at)=active;
    return p;
}
void *lc_attack_edit_cache(int type,int id,int extra) {
    int i,count=G(0x80220ca0); u8 *p=(u8 *)G(0x80220ca8);
    /* Hook the common entry lookup, also used by release-by-ID, rather than
     * only the pointer getter. Native preloaders fill entries inline: preserve
     * their insertion/refcounts, then qualify keys before lookup or release. */
    if(count<=256) for(i=0;i<count;i++,p+=20)
        if(cache_id(i) && H(p,0) && H(p,2)>=3 && H(p,2)<=8 && W(p,4)>0 && W(p,4)<65536)
            W(p,4)|=cache_id(i)<<16;
    return lc_original_edit_cache(type,type>=3 && type<=8 ? named(id) : id,extra);
}
void *lc_attack_edit_model(int id) { return lc_original_edit_model(named(id)); }
void *lc_attack_edit_texture(int id) { return lc_original_edit_texture(named(id)); }
void *lc_attack_edit_material(int id) { return lc_original_edit_material(named(id)); }
void *lc_attack_edit_geometry(int id) { return lc_original_edit_geometry(named(id)); }
void *lc_attack_edit_frame(int id) { return lc_original_edit_frame(named(id)); }
void *lc_attack_edit_strip(int id) { return lc_original_edit_strip(named(id)); }
void *lc_attack_edit_texture_kind(int type,int id) {
    return lc_original_edit_texture_kind(type,type>=3 && type<=8 ? named(id) : id);
}
void *lc_attack_edit_resource(int type,int id) {
    u8 *c, *data, *row;
    int size,i,j,art_hue=0;
    u32 key,art_color=0;
    if(type>=3 && type<=8) id=named(id);
    c=config((u32)id>>16);
    data=lc_original_edit_resource(type,c ? id&65535 : id);
    if(!c || !data || (type!=4 && type!=7 && type!=8 && type!=6)) return data;
    size=((int (*)(void *))0x8007a7e0)((void *)G(0x80220f20));
    if(size<4 || size>1024*1024 || size%4) return data;
    if(type==6) {
        /* Retail model243 (effect94) has seven untextured, precoloured meshes.
         * Its packet preset802202D0 clears lighting and uses SHADE colour.
         * Other geometry can contain normals in these bytes: preserve it. */
        int mesh=id&65535,at,count;
        if(H(c,32) && mesh>=647 && mesh<=653 && size>=8 && W(data,0)>>24==1 && W(data,4)>>24==14) {
            at=W(data,4)&0xffffff; count=(W(data,0)>>12)&255;
            if(at>=8 && count>0 && at+count*16<=size)
                for(i=0;i<count;i++) W(data,at+i*16+12)=lc_effect_color(W(data,at+i*16+12),H(c,32),(s16)H(c,34),W(c,36));
        }
        return data;
    }
    key=(type<<16)|(id&65535);
    for(i=0;i<H(c,46);i++) {
        row=c+96+i*64;
        if(W(row,0)==key && W(row,12)==(u32)size) {
            for(j=0;j<size;j++) data[j]=c[W(row,8)+j];
            art_hue=(s16)H(row,48); art_color=W(row,52);
            break;
        }
    }
    if(H(c,32)) for(i=0;i<size;i+=4)
        W(data,i)=lc_effect_color(W(data,i),H(c,32),(s16)H(c,34),W(c,36));
    if(art_color || art_hue) for(i=0;i<size;i+=4)
        W(data,i)=lc_effect_color(W(data,i),art_color?2:1,art_hue,art_color-1);
    return data;
}
void *lc_attack_edit_packet(void) {
    void *p=lc_original_edit_packet(); int at=index(p,G(0x80220d58),128,4);
    if(at>=0) packet_id(at)=active;
    return p;
}
void *lc_attack_edit_packet_callback(u32 callback) {
    void *p=lc_original_edit_packet_callback(callback); int at=index(p,G(0x80220d58),128,4);
    if(at>=0) packet_id(at)=active;
    return p;
}
void *lc_attack_edit_screen(void) {
    void *p=lc_original_edit_screen(); int at=index(p,G(0x80220da8),120,0);
    if(at>=0) screen_id(at)=active;
    return p;
}
void *lc_attack_edit_screen_callback(u32 callback,int layer) {
    void *p=lc_original_edit_screen_callback(callback,layer); int at=index(p,G(0x80220da8),120,0);
    if(at>=0) screen_id(at)=active;
    return p;
}
static void viewport(u8 *p) {
    u32 *out=(u32 *)G(0x800e9ba0);
    out[0]=0xdc080008; out[1]=(u32)p;
    G(0x800e9ba0)=(u32)(out+2);
}
static int coordinate(int value,int centre,int scale,int offset) {
    return (value-centre)*scale/100+centre+offset*4;
}
static int clamp(int value,int maximum) { return value<0 ? 0 : value>maximum ? maximum : value; }
static void motion(u8 *c,int *x,int *y) {
    int elapsed,t,mode,duration=H(c,20);
    *x=(s16)H(c,42); *y=(s16)H(c,44);
    if(!duration || !W(c,52)) return;
    elapsed=(int)W(c,48)-(int)H(c,22);
    if(elapsed<0) return;
    t=duration<=1 ? 1000 : clamp(elapsed*1000/(duration-1),1000);
    mode=H(c,28);
    if(mode==1) t=t*t/1000;
    else if(mode==2) t=1000-(1000-t)*(1000-t)/1000;
    else if(mode==3) t=t*t*(3000-2*t)/1000000;
    *x+=((s16)H(c,24)-*x)*t/1000;
    *y+=((s16)H(c,26)-*y)*t/1000;
}
static void moving_viewport(u8 *c,int x,int y) {
    u32 *out=(u32 *)G(0x800e9ba0);
    u8 *view=(u8 *)(out+2); int i;
    /* Keep the viewport in this frame's display list, not mutable shared RAM:
     * the RSP can still be reading the previous frame. Branch over its data. */
    out[0]=0xde010000; out[1]=(u32)(out+6);
    for(i=0;i<16;i++) view[i]=c[64+i];
    H(view,8)=640+x*4; H(view,10)=480+y*4;
    out[6]=0xdc080008; out[7]=(u32)view;
    G(0x800e9ba0)=(u32)(out+8);
}
static void appearance(u32 *first,u32 *end,u8 *c) {
    int scale=H(c,40), dx=(s16)H(c,42),dy=(s16)H(c,44),op;
    int left,top,right,bottom,x,y,s,t,ds,dt;
    if(end<first || end-first>65536) return;
    motion(c,&dx,&dy);
    for(;first+1<end;first+=2) {
        op=first[0]>>24;
        if((op==0xfa || op==0xfb) && H(c,32))
            first[1]=lc_effect_color(first[1],H(c,32),(s16)H(c,34),W(c,36));
        if((scale!=100 || dx || dy) && (op==0xe4 || op==0xe5 || op==0xf6)) {
            left=coordinate((first[1]>>12)&4095,640,scale,dx);
            top=coordinate(first[1]&4095,480,scale,dy);
            right=coordinate((first[0]>>12)&4095,640,scale,dx);
            bottom=coordinate(first[0]&4095,480,scale,dy);
            x=clamp(left,1280); y=clamp(top,960);
            first[0]=(first[0]&0xff000000)|(clamp(right,1280)<<12)|clamp(bottom,960);
            first[1]=(first[1]&0xff000000)|(x<<12)|y;
            if(op!=0xf6 && first+5<end && first[2]>>24==0xe1 && first[4]>>24==0xf1) {
                ds=(s16)(first[5]>>16)*100/scale; dt=(s16)first[5]*100/scale;
                s=(s16)(first[3]>>16)+(op==0xe5 ? y-top : x-left)*ds/128;
                t=(s16)first[3]+(op==0xe5 ? x-left : y-top)*dt/128;
                first[3]=((u32)(u16)s<<16)|(u16)t;
                first[5]=((u32)(u16)ds<<16)|(u16)dt;
            }
        }
    }
}
static int draw(void *node,int screen) {
    int at=index(node,G(screen?0x80220da8:0x80220d58),screen?120:128,screen?0:4);
    int old=active, changed, result,dx=0,dy=0;
    u8 *c; u32 fn,*first;
    if(at<0) return 0;
    active=screen?screen_id(at):packet_id(at);
    fn=screen?screen_fn(at):packet_fn(at); c=config(active);
    if(c) motion(c,&dx,&dy);
    changed=c && (H(c,40)!=100 || dx || dy);
    if(changed) {
        if(H(c,20)) moving_viewport(c,dx,dy);
        else viewport(c+64);
    }
    first=(u32 *)G(0x800e9ba0);
    result=((int (*)(void *))fn)(node);
    if(c) appearance(first,(u32 *)G(0x800e9ba0),c);
    if(changed) viewport(c+80);
    active=old;
    return result;
}
static int packet_draw(void *p) { return draw(p,0); }
static int screen_draw(void *p) { return draw(p,1); }
void lc_attack_edit_scene_draw(void) {
    int i,count=G(0x80220d54); u8 *base=(u8 *)G(0x80220d58),*p;
    if(count>256) { lc_original_edit_scene_draw(); return; }
    for(i=0;i<count;i++) {
        p=base+i*128+4;
        if(packet_id(i) && W(p,0)) { packet_fn(i)=W(p,0); W(p,0)=(u32)packet_draw; }
    }
    lc_original_edit_scene_draw();
    for(i=0;i<count;i++) { p=base+i*128+4; if(W(p,0)==(u32)packet_draw) W(p,0)=packet_fn(i); }
}
void lc_attack_edit_screen_draw(int layer) {
    int i,count=G(0x80220da4); u8 *base=(u8 *)G(0x80220da8),*p;
    if(count>256) { lc_original_edit_screen_draw(layer); return; }
    for(i=0;i<count;i++) {
        p=base+i*120;
        if(screen_id(i) && W(p,4)) { screen_fn(i)=W(p,4); W(p,4)=(u32)screen_draw; }
    }
    lc_original_edit_screen_draw(layer);
    for(i=0;i<count;i++) { p=base+i*120; if(W(p,4)==(u32)screen_draw) W(p,4)=screen_fn(i); }
    if(layer==1) for(i=1;i<255;i++) {
        p=config(i);
        if(p && H(p,20) && W(p,52) && W(p,48)<600) W(p,48)++;
    }
}
