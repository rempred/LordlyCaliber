typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 type;
    s32 value;
} MusCommand;
void func_00014338(s32 handle, s32 a, s32 b);
s32 func_00015560(s32 fx);
void func_00012594(MusCommand *cmd) {
    switch (cmd->type) {
        case 0:
            func_00014338(cmd->value, -2, 1);
            break;
        case 1:
            func_00014338(cmd->value, -2, 0);
            break;
        case 2:
            func_00015560(cmd->value);
            break;
    }
}
void func_00014338(s32 handle, s32 and, s32 or);
s32 func_00015560(s32 fx);
