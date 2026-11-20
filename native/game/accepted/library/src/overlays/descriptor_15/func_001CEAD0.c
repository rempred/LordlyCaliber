typedef unsigned char u8;
typedef signed int s32;
void *func_00001330(s32 size);
void *func_00001330(s32 size);
void func_000016C4(void *p);
typedef struct {
    void *name;
    s32 index;
    u8 *buf;
    s32 size;
} Arc16_1CE070;
extern Arc16_1CE070 D_801B99A0;
s32 D_8009DAF4(s32 id);
void *D_8009DBB8(void *dst, s32 id);
s32 func_0000ABE0(void *p);
void func_0000A510(void *dst, void *src);
void boot_resource_archive_load_many(void *arc, void *reqs, s32 n);
void *func_001CEAD0(s32 id) {
    s32 size = D_8009DAF4(id);
    u8 *raw = func_00001330(size);
    void *out;
    D_8009DBB8(raw, id);
    if (raw[2] == '-' && raw[3] == 'l' && raw[4] == 'h') {
        Arc16_1CE070 arc = D_801B99A0;
        Arc16_1CE070 req = D_801B99A0;
        arc.buf = raw;
        arc.size = size;
        boot_resource_archive_load_many(&arc, &req, 1);
        out = req.buf;
    } else {
        out = func_00001330(func_0000ABE0(raw));
        func_0000A510(out, raw);
    }
    func_000016C4(raw);
    return out;
}
