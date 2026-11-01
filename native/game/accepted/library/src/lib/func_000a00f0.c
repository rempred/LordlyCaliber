typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern u32 * volatile D_800E9BA0;
extern s32 D_80196A6C;
extern u8 * volatile D_80196AF8;

void func_00087200();
void func_00087b30(s32, s32, s32, s32, s32);
void func_00088b10(s32, s32, s32, s32);

void func_000a00f0(void *unused, u8 *screen, s32 slot)
{
    register s32 x asm("$18");
    register s32 y asm("$16");
    register u32 pipe_sync asm("$17");
    register u32 *commands asm("$2");
    register u8 *state asm("$4");
    register u32 load_block asm("$6");
    register u32 set_tile asm("$8");
    register u32 set_tile_size asm("$9");
    register s32 seven asm("$7");
    register u32 texture asm("$5");

    {
        register u8 *screen_arg asm("$5") = screen;

        if (*(s16 *)(screen_arg + ((slot & 0xFF) * 2) + 0xE0) == 0) {
            return;
        }

        load_block = 0x073FF200;
        set_tile = 0xF5400800;
        set_tile_size = 0x000FC0FC;
        x = *(volatile s32 *)(screen_arg + 0x0C);
        y = *(volatile s32 *)(screen_arg + 0x10);
        commands = D_800E9BA0;
        state = D_80196AF8;
        seven = 7;
        asm("# emits no code; keeps the call constant in $a3 before display-list setup"
            : : "r"(seven));
    }

    {
        register u32 command_word asm("$3");

        pipe_sync = 0xE7000000;

        command_word = 0xDE000000;
        commands[0] = command_word;
        asm volatile(
            "lui %0,0x0600 # load display-list segment/address high half\n"
            "addiu %0,%0,0x47A0 # add display-list address low half"
            : "=r"(command_word));
        commands[1] = command_word;

        command_word = 0xFD500000;
        commands[2] = command_word;
        asm volatile(
            "lw %0,4(%2) # load the class-screen texture base from UI state\n"
            "addiu %1,%3,4 # compute the first icon x coordinate"
            : "=r"(texture), "=r"(state)
            : "1"(state), "r"(x)
            : "memory");

        command_word = 0xF5500000;
        commands[4] = command_word;
        command_word = 0x07000000;
        commands[5] = command_word;
        command_word = 0xE6000000;
        commands[6] = command_word;
        command_word = 0xF3000000;
        commands[8] = command_word;
        command_word = 0xF2000000;
        commands[14] = command_word;
        asm volatile("# emits no code; completes command headers before cursor updates"
                     : : : "memory");
        D_800E9BA0 = commands + 2;
        D_800E9BA0 = commands + 4;
        D_800E9BA0 = commands + 6;
        D_800E9BA0 = commands + 8;
        D_800E9BA0 = commands + 10;
        D_800E9BA0 = commands + 12;
        commands[7] = 0;
        commands[9] = load_block;
        commands[10] = pipe_sync;
        commands[11] = 0;
        commands[12] = set_tile;
        commands[13] = 0;
        commands[15] = set_tile_size;
        D_800E9BA0 = commands + 14;
        asm volatile("# emits no code; completes texture-load commands before the image address"
                     : : : "memory");
        texture += 0x948;
        commands[3] = texture;
        asm volatile("# emits no code; writes the texture address before loading the draw state"
                     : : : "memory");
        load_block = D_80196A6C;
        asm("# emits no code; keeps the draw-state value in $a2 before cursor advancement"
            : : "r"(load_block));
        D_800E9BA0 = commands + 16;
    }

    asm volatile(
        "# Preserve the retail call delay slot.\n"
        ".set noreorder\n"
        "jal func_00088b10 # draw the class-screen icon using $a0/$a2/$a3\n"
        "addiu $5,$16,6 # pass y + 6 in $a1 in the call delay slot\n"
        "# Restore normal assembler scheduling after the fixed call delay slot.\n"
        ".set reorder"
        :
        : "r"(state), "r"(y), "r"(load_block), "r"(seven)
        : "$5", "$31", "memory");
    {
        register u8 *state asm("$2");

        state = D_80196AF8;
        func_00087b30(*(s32 *)(state + 8) + 0x320,
                      *(s32 *)(state + 4) + 0x2828,
                      0x30, 0x10, 2);
    }
    func_00087200(x + 0x3A, y + 7, 0x1A);

    {
        register u32 *commands asm("$3");
        register u32 color asm("$7");
        register s32 second_x asm("$4");

        color = 0x004FC3BC;
        second_x = x + 8;
        asm("# emits no code; materializes the second draw call's $a0 before cursor access"
            : : "r"(second_x));
        commands = D_800E9BA0;
        y -= 6;
        D_800E9BA0 = commands + 2;
        D_800E9BA0 = commands + 4;
        commands[2] = 0xED000000;
        *(volatile u32 *)&commands[0] = pipe_sync;
        *(volatile u32 *)&commands[1] = 0;
        *(volatile u32 *)&commands[3] = color;
        D_800E9BA0 = commands + 6;
        commands[4] = pipe_sync;
        commands[5] = 0;

        func_00087200(second_x, y, 0x1E, color);
    }
    func_00087200(x + 0x20, y, 0x1F);
}
