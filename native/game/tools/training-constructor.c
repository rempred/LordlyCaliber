/* Adapted from accepted func_0023A5EC. Retains the source library, player-level
 * limit, Soldier pool, leader flags and mode transition. LS templates select
 * a separate 35-byte specification for each follower, like scenario squads.
 * Owns only the original 0x23a5ec..0x23b048 function allocation.
 */
#ifdef LC_ENEMY_CAPACITY
#include "enemy-capacity.h"
extern unsigned short lc_enemy_soldier_max_hp;
#endif
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u32 a,b; u8 *data; u32 size; } Archive;
#define BYTE(a) (*(volatile u8 *)(a))
#define HALF(a) (*(volatile u16 *)(a))
extern void *load_resource(void *,u32);
extern u32 resource_size(u32);
extern void free_resource(void *);
extern void load_archive(Archive *,Archive *,int);
extern void copy_bytes(void *,const void *,int);
extern void clear_bytes(void *,int);
extern void construct_member(u8 *,u32,int,u32,u32);
extern void prepare_battle(void);
extern int set_mode(int);
extern void release_battle(void);

void training_constructor(void) {
    u8 record[175];
    Archive first,second;
    u32 *link,key;
    int id,level=99,maximum=0,i,g,p,count=0,extended,n,chosen;
    u8 *spec,*group,*unit,*out=(u8 *)0x801974de,*member;
    const u8 selector=BYTE(0x80193672);
#ifdef LC_ENEMY_CAPACITY
    out=lc_capacity_sources[30];
#endif
    if(BYTE(0x801976e8)==30) return;
    id=(BYTE(0x80193670)<<8)|BYTE(0x80193671);
    if(HALF(0x80197b60))id=HALF(0x80197b60)&32767;
    first=second=*(Archive *)0x801d16b0;
    link=load_resource(0,0x021ae74c);key=*link;free_resource(link);
    first.size=resource_size(key);first.data=load_resource(0,key);second.b=0;
    load_archive(&first,&second,1);free_resource(first.data);
    spec=second.data+id*35;copy_bytes(record,spec,35);
    extended=record[25]=='L'&&record[26]=='S';
    if(extended){
        n=1;
        for(i=13;i<=15;++i)n+=record[i]!=0;
        for(i=22;i<=24;++i)n+=record[i]!=0;
        if(n<2||n>5){free_resource(second.data);return;}
        copy_bytes(record+35,spec+35,(n-1)*35);
    }
    free_resource(second.data);
    BYTE(0x801976e8)=30;
    unit=(u8 *)0x801971f0+BYTE(0x801976dc)*25;
#ifdef LC_ENEMY_CAPACITY
    unit=lc_capacity_sources[BYTE(0x801976dc)];
#endif
    for(i=0;i<5;++i)if(unit[i+2]&&unit[i+2]<100){
        int candidate=BYTE(0x80193bd3+unit[i+2]*56);
        if(candidate>maximum)maximum=candidate;
    }
    if(maximum<level)level=maximum;
#ifdef LC_ENEMY_CAPACITY
    lc_enemy_reset();
#else
    clear_bytes((void *)0x80195560,0x141c);
#endif
    clear_bytes((void *)0x801953f0,0x16c);
    BYTE(0x801953f0)=BYTE(0x801953f1)=120;
#ifdef LC_ENEMY_CAPACITY
    construct_member(lc_enemy_characters[0],1,level,0,0);
    lc_enemy_soldier_max_hp=*(u16 *)(lc_enemy_characters[0]+22);
#else
    construct_member((u8 *)0x80195560,1,level,0,0);
    HALF(0x801953f2)=HALF(0x80195576);
#endif
    clear_bytes(out,25);
    for(g=0;g<3;++g){
        group=record+(g==0?0:g==1?7:16);
        if(g && !group[0])continue;
        for(p=0;p<(g?3:1);++p){
            if(g&&!group[6+p])continue;
            spec=extended?record+count*35:group;
#ifdef LC_ENEMY_CAPACITY
            chosen=lc_enemy_allocate(spec[0]==1?LC_MEMBER_SOLDIER:LC_MEMBER_ORDINARY);
            if(!chosen)return;
            if(spec[0]==1){
                lc_enemy_soldier_hp[chosen]=lc_enemy_soldier_max_hp;
                lc_enemy_soldier_state[chosen]=1;
            }else{
                member=lc_enemy_characters[chosen];
                construct_member(member,spec[0],level+(s8)spec[1],(spec[2]<<8)|spec[3],(spec[4]<<8)|spec[5]);
            }
#else
            if(spec[0]==1){
                chosen=0;while(chosen<120&&(BYTE(0x801954e4+chosen)&1))++chosen;
                HALF(0x801953f4+chosen*2)=HALF(0x801953f2);
                BYTE(0x801954e4+chosen)=1;chosen+=100;
            }else{
                chosen=1;while(BYTE(0x80195571+chosen*52))++chosen;
                member=(u8 *)0x80195560+chosen*52;
                construct_member(member,spec[0],level+(s8)spec[1],(spec[2]<<8)|spec[3],(spec[4]<<8)|spec[5]);
            }
#endif
            out[count+2]=chosen;out[count+7]=group[6+p]-1;
            if(!count){
#ifdef LC_ENEMY_CAPACITY
                lc_enemy_characters[(u8)chosen][51]=2;
#else
                BYTE(0x80195593+(u8)chosen*52)=2;
#endif
                out[1]=0x81;out[12]=selector;
            }
            ++count;
        }
    }
    if(BYTE(0x801976d8)&8){BYTE(0x801936a8)=0x44;prepare_battle();set_mode(8);release_battle();}
}
