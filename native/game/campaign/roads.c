/* Editor-owned world connections. Original road flags keep their saved meaning.
 * Added roads are open when both endpoints are visible. The native route walker
 * consumes seven ordered bends. Display dots are spaced along the same path. */
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
extern const u8 campaign_roads[];
extern const unsigned int campaign_display_offsets[];
extern const s16 campaign_display_points[];
extern const unsigned int campaign_road_count;
extern const u8 campaign_location_icons[];
int campaign_location_icon(int id) {
    if(id<1 || id>40)return 0;
    return (campaign_location_icons[(id-1)>>3]>>((id-1)&7))&1;
}
static int node_state(int id) {
    const u8 *states=(const u8 *)0x80196a86;
    return id>0 && id<=40 ? (states[(id-1)>>2]>>(((id-1)&3)*2))&3 : 0;
}
int campaign_road_open(int id) {
    const u8 *flags=(const u8 *)0x80196a81;
    if(id<1 || id>(int)campaign_road_count)return 0;
    if(!campaign_roads[(id-1)*2] || !campaign_roads[(id-1)*2+1])return 0;
    if(id<=40)return (flags[(id-1)>>3]>>((id-1)&7))&1;
    return node_state(campaign_roads[(id-1)*2]) && node_state(campaign_roads[(id-1)*2+1]);
}
int campaign_find_road(int from,int to) {
    int r;
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
    void (*original)(int,int)=(void (*)(int,int))0x801a347c;
    void (*texture)(const void*,const void*,int,int,int)=(void (*)(const void*,const void*,int,int,int))0x8019f560;
    void (*dot)(int,int,int)=(void (*)(int,int,int))0x8019f81c;
    const u8 *context=*(const u8 **)0x80197af8;
    const u8 *art=*(const u8 **)(context+4);
    int r;
    unsigned int n;
    original(x,y);
    texture(art+0xa0,art+0x4c0,16,6,0);
    for(r=0;r<(int)campaign_road_count;++r)if(campaign_road_open(r+1))
        for(n=campaign_display_offsets[r];n<campaign_display_offsets[r+1];++n)
            dot(x+campaign_display_points[n*2],y+campaign_display_points[n*2+1],5);
}
