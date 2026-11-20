typedef unsigned char u8;
typedef signed int s32;
extern u8 osViModeNtscLan1[];
extern u8 osViModeMpalLan1[];
extern u8 D_800AB9B0[];
extern u8 D_800ABA50[];
void boot_mode_message_select(s32 mode);
void osViSetMode(void *m);
extern s32 osTvType;
void boot_mode_message_select(s32 mode) {
    switch (osTvType) {
        case 1:
            switch (mode) {
                case 2:
                    osViSetMode(&osViModeNtscLan1);
                    break;
                case 9:
                    osViSetMode(D_800AB9B0);
                    break;
            }
            break;
        case 2:
            switch (mode) {
                case 2:
                    osViSetMode(&osViModeMpalLan1);
                    break;
                case 9:
                    osViSetMode(D_800ABA50);
                    break;
            }
            break;
    }
}
