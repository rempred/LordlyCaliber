/* Editor-owned world connections. Original road flags keep their saved meaning.
 * Added roads are open when both endpoints are visible. The native route walker
 * consumes at most seven new bends per section. Display dots follow the full path. */
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
extern const u8 campaign_roads[];
extern const unsigned int campaign_display_offsets[];
extern const s16 campaign_display_points[];
extern const unsigned int campaign_road_count;
extern const u8 campaign_road_flags[];
extern const u8 campaign_location_icons[];
extern const u8 *campaign_artwork;
extern const s16 campaign_waypoints[];
extern unsigned int campaign_travel_next;

/* A logical connection has at most two seven-bend sections. The native x/y
 * arrays have 16 slots including endpoints and a zero terminator. Page one
 * section at a time rather than overwriting the following world state. */
static void campaign_travel_page(u8 *ctx, int first) {
    const s16 *locations=(const s16 *)0x801a7510;
    s16 *xs=(s16 *)(ctx+0x60),*ys=(s16 *)(ctx+0x80);
    int signed_road=*(s8 *)(ctx+0xe1),road=signed_road<0?-signed_road:signed_road;
    int route=ctx[0xe2],from,to;
    unsigned int cursor=first?0:campaign_travel_next;
    int n,count=1,bends=0,more=0;
    s16 x,y;
    if(road<1 || road>(int)campaign_road_count || route>62) {
        xs[0]=ys[0]=0;campaign_travel_next=14;return;
    }
    from=ctx[0xa1+route];to=ctx[0xa2+route];
    if(from<1 || from>40 || to<1 || to>40) {
        xs[0]=ys[0]=0;campaign_travel_next=14;return;
    }
    if(first){x=locations[(from-1)*4];y=locations[(from-1)*4+1];}
    else {n=ctx[0xe4];if(n>14)n=0;x=xs[n];y=ys[n];}
    for(n=0;n<16;++n)xs[n]=ys[n]=0;
    xs[0]=x;ys[0]=y;
    /* The native state machine rebuilds a road whenever step == 0. A continued
     * section starts at slot 1 so it uses the existing-road path next tick. */
    if(!first){xs[1]=x;ys[1]=y;count=2;}
    while(cursor<14 && bends<7) {
        n=(road-1)*28+(signed_road<0?13-(int)cursor:(int)cursor)*2;
        ++cursor;
        if(!campaign_waypoints[n])continue;
        xs[count]=campaign_waypoints[n];ys[count]=campaign_waypoints[n+1];++count;++bends;
    }
    for(n=(int)cursor;n<14;++n) {
        int at=(road-1)*28+(signed_road<0?13-n:n)*2;
        if(campaign_waypoints[at])more=1;
    }
    if(!more) {
        xs[count]=locations[(to-1)*4];ys[count]=locations[(to-1)*4+1];++count;cursor=14;
    }
    xs[count]=ys[count]=0;
    campaign_travel_next=cursor;ctx[0xe4]=first?0:1;
}
void campaign_prepare_road(void) {
    u8 *ctx=*(u8 **)0x80197af8;
    campaign_travel_page(ctx,1);
}
int campaign_more_road(void) {
    u8 *ctx=*(u8 **)0x80197af8;
    s16 *xs=(s16 *)(ctx+0x60);
    unsigned int current=ctx[0xe4];
    if(current>=15)return 0;
    if(xs[current+1])return 1;
    if(campaign_travel_next>=14)return 0;
    campaign_travel_page(ctx,0);
    return xs[1]!=0;
}
int campaign_location_icon(int id) {
    if(id<1 || id>40)return 0;
    if(campaign_artwork) {
        const unsigned int *records=*(const unsigned int * const *)(campaign_artwork+48);
        return records[campaign_artwork[52+id-1]*3];
    }
    return (campaign_location_icons[(id-1)>>3]>>((id-1)&7))&1;
}
/* Only replace image selection. The original renderer still controls visibility,
 * clipping, location positions, mission badges and graphics state. */
void campaign_icon_texture(int index,int state) {
    const unsigned int *records=*(const unsigned int * const *)(campaign_artwork+48);
    const unsigned int *entry=records+campaign_artwork[52+index]*3;
    const u8 *ctx=*(const u8 * const *)0x80197af8;
    unsigned int tick=*(const unsigned int *)(ctx+0x108),frame=0;
    int height=16;
    const u8 *art;
    void (*texture)(const void*,const void*,int,int,int)=(void (*)(const void*,const void*,int,int,int))0x8019f560;
    if(!entry[0]) {
        if(state==1)frame=(index+tick/6)%3;
        else {
            const u8 *phases=(const u8 *)0x801a8224;
            unsigned int phase=tick%90;
            frame=phase<4?phases[phase]:0;height=13;
        }
    }
    art=(const u8 *)entry[state==1?1:2]+frame*160;
    texture(art,art+32,16,height,0);
}
static int node_state(int id) {
    const u8 *states=(const u8 *)0x80196a86;
    return id>0 && id<=40 ? (states[(id-1)>>2]>>(((id-1)&3)*2))&3 : 0;
}
int campaign_road_open(int id) {
    const u8 *flags=(const u8 *)0x80196a81;
    int flag;
    if(id<1 || id>(int)campaign_road_count)return 0;
    if(!campaign_roads[(id-1)*2] || !campaign_roads[(id-1)*2+1])return 0;
    flag=campaign_road_flags[id-1];
    if(flag)return (flags[(flag-1)>>3]>>((flag-1)&7))&1;
    return node_state(campaign_roads[(id-1)*2]) && node_state(campaign_roads[(id-1)*2+1]);
}
void campaign_set_road(int id,int open) {
    u8 *flags=(u8 *)0x80196a81;
    int flag,mask;
    if(id<1 || id>(int)campaign_road_count)return;
    flag=campaign_road_flags[id-1];
    if(flag<1 || flag>40)return;
    mask=1<<((flag-1)&7);
    flags[(flag-1)>>3]=(flags[(flag-1)>>3]&~mask)|(open?mask:0);
}
int campaign_find_road(int from,int to) {
    int r;
    if(from<1 || from>40 || to<1 || to>40 || from==to)return 0;
    for(r=0;r<(int)campaign_road_count;++r) {
        if(campaign_roads[r*2]==from && campaign_roads[r*2+1]==to)return r+1;
        if(campaign_roads[r*2]==to && campaign_roads[r*2+1]==from)return -r-1;
    }
    return 0;
}
void campaign_find_path(int from,int to,u8 *out) {
    int distance[41],previous[41],seen[41],reverse[41];
    int i,current,r,next,cost,best,n;
    *out=0;
    if(from<1 || from>40 || to<1 || to>40 || !node_state(from) || !node_state(to))return;
    if(from==to){out[0]=(u8)from;out[1]=0;return;}
    for(i=0;i<=40;++i){distance[i]=255;previous[i]=0;seen[i]=0;}
    distance[from]=1;
    for(;;) {
        current=0;best=255;
        for(i=1;i<=40;++i)if(!seen[i] && distance[i]<best){current=i;best=distance[i];}
        if(!current)break;
        seen[current]=1;
        for(r=0;r<(int)campaign_road_count;++r) {
            next=campaign_roads[r*2]==current ? campaign_roads[r*2+1] : campaign_roads[r*2+1]==current ? campaign_roads[r*2] : 0;
            if(!next || !node_state(next) || !campaign_road_open(r+1))continue;
            cost=((current!=from && current!=to && node_state(current)==2) ||
                  (next!=from && next!=to && node_state(next)==2))?40:1;
            if(distance[current]+cost<distance[next]) {distance[next]=distance[current]+cost;previous[next]=current;}
        }
    }
    if(distance[to]>=255)return;
    n=0;current=to;
    while(current && n<41){reverse[n++]=current;if(current==from)break;current=previous[current];}
    if(!current || n>40)return;
    while(n)*out++=(u8)reverse[--n];
    *out=0;
}
void campaign_draw_roads(int x,int y) {
    void (*texture)(const void*,const void*,int,int,int)=(void (*)(const void*,const void*,int,int,int))0x8019f560;
    void (*dot)(int,int,int)=(void (*)(int,int,int))0x8019f81c;
    const u8 *context=*(const u8 **)0x80197af8;
    const u8 *art=*(const u8 **)(context+4);
    int r,active,phase,reverse,opening;
    unsigned int n,begin,end,cut;
    active=context[0x316]?context[0x312]:0;
    phase=(s8)context[0x314];if(phase<0)phase=0;if(phase>6)phase=6;
    reverse=context[0x313]!=0;opening=context[0x315]!=0;
    texture(art+0xa0,art+0x4c0,16,6,0);
    for(r=0;r<(int)campaign_road_count;++r) {
        if(!campaign_roads[r*2] || !campaign_roads[r*2+1])continue;
        begin=campaign_display_offsets[r];end=campaign_display_offsets[r+1];
        if(active==r+1) {
            /* Native reveal ticks use seven phases. Apply those phases to the
             * full resampled shape, without indexing the old seven-dot table. */
            cut=begin+((end-begin)*(unsigned int)(phase+1)+6)/7;
            if(reverse && opening)cut=begin+((end-begin)*(unsigned int)phase)/7;
            if(opening!=reverse)end=cut;else begin=cut;
        }else if(!campaign_road_open(r+1))continue;
        for(n=begin;n<end;++n)
            dot(x+campaign_display_points[n*2],y+campaign_display_points[n*2+1],5);
    }
}
