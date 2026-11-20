typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
void bcopy(const void *src, void *dst, s32 n);
void *resource_alloc_mode1_wrapper(s32 size);
void boot_state_slot_payload_alloc_copy(u8 kind, void *src, u16 n, u8 **out) {
    u8 *p;
    if (n == 0) {
        *out = ((void *)0);
        return;
    }
    if (kind & 1) {
        return;
    }
    p = resource_alloc_mode1_wrapper(n + 6);
    *out = p;
    p[0] = kind;
    *(u16 *)(p + 2) = n;
    *(u16 *)(p + 4) = n;
    bcopy(src, p + 6, n);
}
