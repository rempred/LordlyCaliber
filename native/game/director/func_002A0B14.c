typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef double f64;

extern u8 *D_8022A974;

void *func_00070F30(s32 bytes);
f32 func_001BC35C(s16 x, s16 y);

void func_002A0B14(s32 slot, f32 start_x, f32 start_y, f32 target_x,
                   f32 target_y, s32 duration, s32 scaled_duration)
{
    f32 distance_squared;
    f32 current_x;
    f32 current_y;
    f32 distance;
    f32 duration_float;
    s32 step_count;
    s32 slot_offset;
    s32 scene_state;
    s32 x_is_default;
    s32 y_is_default;
    u8 *actor;
    u8 *motion;
    s16 reached;

    actor = *(u8 **)(D_8022A974 + slot * 4 + 0x18);
    if (actor == 0) {
        goto block_done;
    }
    x_is_default = start_x == -1.0f;
block_compare_y:
    y_is_default = start_y == -1.0f;
block_choose_start:
    if (!(x_is_default & y_is_default)) {
        goto block_explicit_start;
    }
    current_x = *(f32 *)(actor + 0x11C);
    current_y = *(f32 *)(actor + 0x124);
    goto block_duration;

block_explicit_start:
    current_x = start_x;
    current_y = start_y;
    *(f32 *)(actor + 0x11C) = current_x;
    *(f32 *)(actor + 0x124) = current_y;

block_duration:
    if (duration == 1) {
        goto block_immediate;
    }
    goto block_tolerance_x;

block_immediate:
    scene_state = *(u8 *)0x8018FC19;
    *(f32 *)(actor + 0x11C) = target_x;
    *(f32 *)(actor + 0x124) = target_y;
    if (scene_state != 2) {
        goto block_done;
    }
    if ((*(u8 *)(actor + 0x145) & 1) == 0) {
        goto block_done;
    }
    *(f32 *)(actor + 0x120) = func_001BC35C(
        (s16)(s32)*(f32 *)(actor + 0x11C),
        (s16)(s32)target_y);
    goto block_done;

block_tolerance_x:
    {
    f64 current_double;
    f64 target_double;
    f64 tolerance;

    reached = 0;
    current_double = (f64)current_x;
    target_double = (f64)target_x;
    tolerance = *(f64 *)0x80239318;
    if (!(current_double < target_double + tolerance)) {
        goto block_tolerance_y;
    }
    if (target_double - tolerance < current_double) {
        *(f32 *)(actor + 0x11C) = target_x;
        asm("");
        reached = 1;
        tolerance = *(f64 *)0x80239320;
    } else {
        goto block_tolerance_y;
    }

block_tolerance_y:
    target_double = (f64)target_y;
    current_double = (f64)current_y;
    if (!(current_double < target_double + tolerance)) {
        goto block_after_tolerance;
    }
    if (!(target_double - tolerance < current_double)) {
        goto block_after_tolerance;
    }
    *(f32 *)(actor + 0x124) = target_y;
    reached += 2;
    }

block_after_tolerance:
    slot_offset = slot * 4;
    if (reached == 3) {
        goto block_done;
    }
    motion = *(u8 **)(slot_offset + (u32)D_8022A974 + 0xF8);
    if (motion != 0) {
        goto block_motion_ready;
    }
    motion = func_00070F30(0x10);
    *(u8 **)(slot_offset + (u32)D_8022A974 + 0xF8) = motion;

block_motion_ready:
    if (scaled_duration != 0) {
        goto block_scaled_motion;
    }
    goto block_fixed_motion;

block_scaled_motion:
    distance_squared = (target_x - current_x) * (target_x - current_x)
                     + (target_y - current_y) * (target_y - current_y);
    if (distance_squared == 0.0f) {
        goto block_zero_distance;
    }
    distance = __builtin_sqrtf(distance_squared);

block_nonzero_distance:
    distance_squared = (f32)((*(f64 *)0x80239328 / (f64)scaled_duration)
                           * (f64)distance);
    step_count = (s32)distance_squared;
    *(s16 *)(motion + 0xC) = (s16)step_count;
    *(f32 *)(motion + 0) = (target_x - current_x) / (f32)(s16)step_count;
    *(f32 *)(motion + 8) = (target_y - current_y) / (f32)(s16)step_count;
    goto block_clear_motion_flag;

block_zero_distance:
    *(f32 *)(motion + 0) = 0.0f;
    *(f32 *)(motion + 8) = 0.0f;
    *(s16 *)(motion + 0xC) = 1;
    goto block_done;

block_fixed_motion:
    duration_float = (f32)duration;
    *(s16 *)(motion + 0xC) = (s16)duration;
    *(f32 *)(motion + 0) = (target_x - current_x) / duration_float;
    *(f32 *)(motion + 8) = (target_y - current_y) / duration_float;

block_clear_motion_flag:
    *(s8 *)(motion + 0xE) = 0;

block_done:
    return;
}
