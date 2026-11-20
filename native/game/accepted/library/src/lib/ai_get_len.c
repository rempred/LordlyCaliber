typedef unsigned int u32;
typedef volatile u32 vu32;
u32 ai_get_len(void) {
    return (*(vu32 *)((u32)((0x04500000 + 0x04)) | 0xA0000000));
}
