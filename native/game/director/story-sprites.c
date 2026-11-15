/* Authored sprite placement/motion. Native sprite records and pose sweeps stay
 * owned by the game; this adds an independent position trajectory afterwards.
 * Called by the existing scene pose-sweep call in func_002A0680. */
typedef struct {
    u32 owner, record;
    s32 from_x, from_y, to_x, to_y, duration, elapsed, last_x, last_y;
} LcSceneSpriteMotion;
extern LcSceneSpriteMotion lc_scene_sprite_motion[30];
void lc_original_scene_poses(void);

void lc_scene_sprite_cancel(int slot) {
    int i;
    for(i=0;i<30;i++) if(slot<0 || slot==i) {
        lc_scene_sprite_motion[i].record=0;
        lc_scene_sprite_motion[i].duration=0;
    }
}
static u8 *lc_scene_sprite_record(int slot) {
    if(slot<0 || slot>=30 || !D_8022A974) return (u8 *)0;
    return *(u8 **)((u8 *)D_8022A974+0x1ac0+slot*4);
}
void lc_scene_sprite_command(u32 command,u32 value) {
    int slot=command&0xffff;
    u32 kind=command&0xffff0000U;
    u8 *record=lc_scene_sprite_record(slot);
    LcSceneSpriteMotion *motion;
    if(!record) return;
    motion=&lc_scene_sprite_motion[slot];
    if(kind==0x4c500000U) {
        lc_scene_sprite_cancel(slot);
        *(s32 *)(record+0x24)=(s32)value;
    } else if(kind==0x4c510000U) {
        lc_scene_sprite_cancel(slot);
        *(s32 *)(record+0x28)=-(s32)value;
    } else if(kind==0x4c4d0000U) {
        motion->owner=(u32)D_8022A974;motion->record=(u32)record;
        motion->to_x=(s32)value;
        motion->duration=0;
    } else if(kind==0x4c4e0000U && motion->owner==(u32)D_8022A974 && motion->record==(u32)record) {
        motion->to_y=-(s32)value;
    } else if(kind==0x4c540000U && motion->owner==(u32)D_8022A974 && motion->record==(u32)record && value>0 && value<=1800) {
        motion->from_x=motion->last_x=*(s32 *)(record+0x24);
        motion->from_y=motion->last_y=*(s32 *)(record+0x28);
        motion->elapsed=0;motion->duration=value;
    }
}
int lc_scene_sprite_busy(int slot) {
    u8 *record=lc_scene_sprite_record(slot);
    LcSceneSpriteMotion *motion;
    if(!record) return 0;
    motion=&lc_scene_sprite_motion[slot];
    return motion->owner==(u32)D_8022A974 && motion->record==(u32)record && motion->duration>0;
}
void lc_scene_sprites_update(void) {
    int i,x,y;
    u8 *record;
    LcSceneSpriteMotion *motion;
    lc_original_scene_poses();
    if(!D_8022A974 || *((u8 *)D_8022A974+0x1cb1)) return;
    for(i=0;i<30;i++) {
        motion=&lc_scene_sprite_motion[i];
        if(!lc_scene_sprite_busy(i)) continue;
        record=lc_scene_sprite_record(i);
        ++motion->elapsed;
        x=motion->from_x+(motion->to_x-motion->from_x)*motion->elapsed/motion->duration;
        y=motion->from_y+(motion->to_y-motion->from_y)*motion->elapsed/motion->duration;
        *(s32 *)(record+0x24)+=x-motion->last_x;
        *(s32 *)(record+0x28)+=y-motion->last_y;
        motion->last_x=x;motion->last_y=y;
        if(motion->elapsed>=motion->duration)motion->duration=0;
    }
}
