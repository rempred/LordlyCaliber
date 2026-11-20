typedef signed int s32;
void func_000016C4(void *p);
s32 D_8009DAF4(s32 id);
void *resource_alloc_tree_scan(s32 size);
void *D_8009DBB8(void *dst, s32 id);
s32 func_0000ABE0(void *p);
void func_0000A510(void *dst, void *src);
s32 func_000492a0(s32 i) {
    s32 *buf;
    s32 *out;
    buf = resource_alloc_tree_scan(D_8009DAF4(0x19A8804));
    D_8009DBB8(buf, 0x19A8804);
    i *= 4;
    i += (s32)buf;
    i = *(s32 *)i;
    func_000016C4(buf);
    buf = resource_alloc_tree_scan(D_8009DAF4(i));
    D_8009DBB8(buf, i);
    i = func_0000ABE0(buf);
    out = resource_alloc_tree_scan(i);
    func_0000A510(out, buf);
    i = out[i / 4 - 1];
    func_000016C4(buf);
    func_000016C4(out);
    if ((i & 0xFF000000) == 0xFF000000) {
        return i & 0xFF;
    }
    return 0;
}
