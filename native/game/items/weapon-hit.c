/* Adapted from accepted battle/func_0022A964.c (physical nonlethal hit).
 * The caller supplies a valid battle actor. Combine pure status predicates and
 * share timing/pose paths to fit six effects in the original 0x498-byte owner.
 * Weapon B23: 02 Sleep, 04 Paralysis, 08 Petrification, 20 Poison,
 * 40 Power Up, 80 Power Down. Existing ailments keep priority for combinations.
 * The retail secondary dispatcher owns eligibility and attribute updates.
 * Events 27/29/2A replay Poison/Power Up/Power Down on the struck actor.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

s32 func_0020C2C0(void *);
static __inline__ int func_0020C2F4(void *record) { return (*(u32 *)((u8 *)record + 0x40) & 8) != 0; }
static __inline__ int func_0020C310(void *record) { return (*(u32 *)((u8 *)record + 0x40) & 4) != 0; }
static __inline__ int func_0020C32C(void *record) { return (*(u32 *)((u8 *)record + 0x40) & 2) != 0; }
static __inline__ int func_0020C348(void *record) { return (*(u32 *)((u8 *)record + 0x40) & 16) != 0; }
void func_0022B06C(void *, u8 *);
void *func_0020C478(unsigned int index);
void func_0021D200(s32, s32, void *);
void func_0021D230(s32, s32, void *, s32);
void func_0021D25C(s32, s32, void *, s32, s32);
void func_0021D28C(s32, s32, void *, s32, s32, s32);
s32 func_002224F4(void);
s32 func_0022257C(void *, s32, s32 *);
s32 func_00239AA4(void *, u16);
s32 func_0002CBCC(void);

void func_0022A964(void *record, s32 initial_time, s32 mode, s32 parameter, u8 flags) {
    s32 event_code;
    u32 sample0;
    u32 sample1;
    s32 next_time;
    s32 scan_result;
    s32 index;
    s32 time;
    s32 mode_bits;
    s32 scan_mask;
    u32 field30;
    void *entry;
    time = initial_time;
    if (func_0020C32C(record) == 0) {
        event_code = 0x12;
        if (mode != 0) {
            event_code = 0xD;
        }
        func_0021D230(event_code, time, record, 0xFF);
        time += func_0022257C((u8 *)*(void **)record + 0x44, ((0 - (mode == 0)) & 0x1A) | 0x12, 0);
    } else {
        func_0021D200(0x2C, time, record);
    }
    if (mode != 0 && (*(u32 *)((u8 *)record + 0x40) & 0xE) == 4) {
        field30 = *(u8 *)((u8 *)record + 0x30);
        sample0 = func_0002CBCC();
        sample1 = func_0002CBCC();
        if (field30 >= (u32) ((((sample0 << 0x12) & 0x0C000000) | (sample1 << 0xF) | func_0002CBCC()) % 100)) {
            func_0021D230(0x26, time, record, 0xFF);
            *(s32 *)((u8 *)record + 0x40) = (*(s32 *)((u8 *)record + 0x40) & ~4);
            time += func_0022257C((u8 *)*(void **)record + 0x44, 0x23, 0);
            if (func_0020C348(record) != 0) {
                func_0021D230(0x27, time, record, 0xFF);
            }
        }
    }
    mode_bits = flags & 0xE;
    if (mode_bits != 0) {
        if (flags & 8) {
            if (func_0020C32C(record) == 0) {
                func_0021D230(0x23, time, record, 0xFF);
                time += func_0022257C((u8 *)*(void **)record + 0x44, 0x26, 0);
                time += 0x16;
                func_0021D200(0x2C, time, record);
                *(s32 *)((u8 *)record + 0x40) = ((*(s32 *)((u8 *)record + 0x40) & ~0x1C) | 2);
                if (*(s32 *)((u8 *)record + 0x94) < time) {
                    *(s32 *)((u8 *)record + 0x94) = time;
                }
                scan_mask = func_002224F4();
                *(u16 *)((u8 *)*(void **)0x801CE8BC + 0x606A) = scan_mask;
                if (scan_mask & 0xFFFF) {
                    index = 0;
                    do {
                        entry = func_0020C478(index);
                        scan_result = func_0020C2C0(entry);
                        index += 1;
                        if (scan_result == 0) {
                            *(s32 *)((u8 *)entry + 0x6C) = *(s32 *)((u8 *)entry + 0x70);
                        }
                    } while (index < 0x14);
                    if (func_00239AA4(record, *(u16 *)((u8 *)*(void **)0x801CE8BC + 0x606A)) != 0) {
                        func_0021D25C(0x1D, time, record, 0xFF, 0x24);
                        next_time = time + func_0022257C((u8 *)*(void **)record + 0x44, 0x24, 0);
                        if (*(s32 *)((u8 *)record + 0x94) < next_time) {
                            *(s32 *)((u8 *)record + 0x94) = next_time;
                        }
                    }
                }
            } else {
                goto default_path;
            }
        } else if (flags & 4) {
            if (((*(u32 *)((u8 *)record + 0x40) & 0xA) == 0)) {
                func_0021D28C(0x1A, time, record, 0xFF, time + 0x64, parameter & 0xFF);
                time = time + func_0022257C((u8 *)*(void **)record + 0x44, 0x20, 0);
                *(s32 *)((u8 *)record + 0x40) = ((*(s32 *)((u8 *)record + 0x40) & ~0x14) | 8);
                goto finish;
            } else {
                goto default_path;
            }
        } else if ((flags & 2) && ((*(u32 *)((u8 *)record + 0x40) & 0xE) == 0)) {
            func_0021D230(0x25, time, record, 0xFF);
            time = time + func_0022257C((u8 *)*(void **)record + 0x44, 0x22, 0);
            *(s32 *)((u8 *)record + 0x40) = ((*(s32 *)((u8 *)record + 0x40) & ~0x10) | 4);
            goto finish;
        } else {
            goto default_path;
        }
    } else {
        u8 effect = (flags & 0x20) ? 10 : (flags & 0x40) ? 5 : (flags & 0x80) ? 6 : 0;
        if (effect) {
            func_0022B06C(record, &effect);
            if (effect)
                func_0021D230(effect == 10 ? 0x27 : effect + 0x24, time, record, 0xFF);
        }
default_path:
        if (func_0020C32C(record) == 0) {
            s32 pose = func_0020C2F4(record) ? 0x20 : func_0020C310(record) ? 0x22 : 0;
            if (pose) {
                func_0021D25C(0x1D, time, record, 0xFF, pose);
                time += func_0022257C((u8 *)*(void **)record + 0x44, pose, 0);
            }
        }
finish:
        if (*(s32 *)((u8 *)record + 0x94) < time) {
            *(s32 *)((u8 *)record + 0x94) = time;
        }
    }
}
