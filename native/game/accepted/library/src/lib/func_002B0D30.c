typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;

extern void *D_8018FC14;
extern u8 D_8022A997;
extern u8 *D_8022A974;

s32 func_8009DAF4(s32 resource);
void *func_80071C04(s32 size);
void func_8009DBB8(void *destination, s32 resource);
void func_800712C4(void *pointer);
void *func_8023BDDC(void *resource);

s16 func_002B0D30(s32 selector, s32 count_override)
{
    s32 *group_table;
    s32 packed_size;
    s32 *group;
    s16 member_index;

    group_table = func_80071C04(func_8009DAF4(0x016B3D18));
    func_8009DBB8(group_table, 0x016B3D18);

    selector *= 4;
    selector += (s32)group_table;
    packed_size = func_8009DAF4(*(s32 *)selector);
    group = func_80071C04(packed_size);
    packed_size /= 4;
    func_8009DBB8(group, *(s32 *)selector);
    func_800712C4(group_table);

    if (count_override != 0) {
        packed_size = count_override;
    }

    member_index = 0;
    if (packed_size > 0) {
        do {
            void *preloaded;
            register void **slot asm("$2");

            preloaded = D_8018FC14;
            if ((preloaded != 0) & (member_index == 0)) {
                slot = (void **)((member_index * 4) +
                                  (s32)D_8022A974);
                D_8018FC14 = 0;
                asm(".set noreorder\n.word 0x0808F00C" : : "r"(slot));
                *(void **)((u8 *)slot + 0x1C04) = preloaded;
                asm(".set reorder");
            }
            *(void **)(D_8022A974 + 0x1C04 + (member_index * 4)) =
                func_8023BDDC((void *)group[member_index]);
            member_index++;
            D_8022A997++;
        } while (member_index < packed_size);
    }

    func_800712C4(group);
    return (s16)packed_size;
}
