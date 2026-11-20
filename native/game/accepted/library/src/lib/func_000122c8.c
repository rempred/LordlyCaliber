typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 type;
    s32 value;
} MusCommand;
void func_000122c8(s32 handle);
s32 func_00012614(MusCommand *cmd);
void func_000122c8(s32 handle) {
    MusCommand cmd;
    cmd.type = 1;
    cmd.value = handle;
    func_00012614(&cmd);
}
s32 func_00012614(MusCommand *cmd);
