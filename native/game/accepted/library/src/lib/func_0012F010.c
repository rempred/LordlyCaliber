typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

typedef struct { u8 bytes[25]; } SourceRecord25;
extern SourceRecord25 g_func_001957D0_source_records[];
extern u16 D_8019532C[];
extern u16 D_80190EBC[];
extern u8 D_80195480[];
extern u8 D_80191010[];
extern u16 D_80195578[][26];
extern u16 D_80193BD8[][28];
extern u8 D_80195593[][52];
extern u8 D_80193BF3[][56];

s32 func_0012F010(s32 source_index)
{
    s32 first_group = source_index < 30;
    u8 *source = g_func_001957D0_source_records[source_index].bytes + 2;
    s32 slot;
    for (slot = 0; slot < 5; slot++) {
        s32 member = source[slot];
        s32 flags;
        if (member == 0) {
            continue;
        }
        if (member >= 100) {
            if (!first_group) {
                if (D_8019532C[member] == 0) {
                    continue;
                }
                flags = D_80195480[member];
            } else {
                if (D_80190EBC[member] == 0) {
                    continue;
                }
                flags = D_80191010[member];
            }
        } else {
            if (!first_group) {
                if (D_80195578[member][0] == 0) {
                    continue;
                }
                flags = D_80195593[member][0];
            } else {
                if (D_80193BD8[member][0] == 0) {
                    continue;
                }
                flags = D_80193BF3[member][0];
            }
        }
        if (!(flags & 4)) {
            return 0;
        }
    }
    return 1;
}
