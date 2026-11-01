typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

extern u32 * volatile D_800E9BA0;
extern u8 * volatile D_80196AF8;
extern u32 D_80196A6C;

void func_00087200(s32, s32, s32);
void func_000873a0(s32, s32, s32, s32);
void func_00087b30(s32, s32, s32, s32, s32);
void func_00088b10(s32, s32, u32, s32);
void func_0008b5e4(s32, s32, s32, u32);
s32 func_000bf9c8(s32, s32);

void func_00099b1c(void *unused, u8 *screen, s32 slot)
{
    register u8 *screen_arg asm("$18") = screen;
    register s32 x asm("$19");
    register s32 y asm("$20");
    register s32 icon_x asm("$17");
    register s32 text_y asm("$16");
    register u32 result asm("$6");

    result = *(s16 *)(screen_arg + ((slot & 0xFF) * 2) + 0xE0);
    if (result == 0) {
        return;
    }

    {
        register u32 *first_commands asm("$5");
        register u32 mode_word asm("$3");
        register u32 header_word asm("$4");

        mode_word = 0xE3000C00;
        header_word = 0xE3001201;
        x = *(volatile s32 *)(screen_arg + 0x0C);
        first_commands = D_800E9BA0;
        y = *(s32 *)(screen_arg + 0x10);
        D_800E9BA0 = first_commands + 2;
        first_commands[0] = 0xFA000000;
        first_commands[1] = result | 0xFFFFFF00;
        D_800E9BA0 = first_commands + 4;
        D_800E9BA0 = first_commands + 6;
        asm volatile(
            "# Preserve the retail render-mode branch and both delay slots.\n"
            ".set noreorder\n"
            "addiu $2,$0,0xFF # materialize the special color selector\n"
            "sw $3,8($5) # write the first other-mode command header\n"
            "sw $0,12($5) # clear the first other-mode command payload\n"
            "sw $4,16($5) # write the second other-mode command header\n"
            "bne $6,$2,1f # choose the ordinary render-mode constants unless color is 0xFF\n"
            "sw $0,20($5) # clear the second payload in the branch delay slot\n"
            "lui $4,0xE200 # load the special render-mode command high half\n"
            "ori $4,$4,0x001C # complete the special render-mode command\n"
            "lui $3,0x0F0A # load the special render-mode value high half\n"
            "j 2f # skip the ordinary render-mode constants\n"
            "ori $3,$3,0x7008 # complete the special value in the jump delay slot\n"
            "1: # ordinary render-mode constant path\n"
            "lui $4,0xE200 # load the ordinary render-mode command high half\n"
            "ori $4,$4,0x001C # complete the ordinary render-mode command\n"
            "lui $3,0x0050 # load the ordinary render-mode value high half\n"
            "ori $3,$3,0x4240 # complete the ordinary render-mode value\n"
            "2: # join after selecting the special or ordinary render-mode constants\n"
            "addiu $2,$5,0x20 # advance the display-list cursor by four commands\n"
            "lui $1,%%hi(D_800E9BA0) # load the display-list cursor global address\n"
            "sw $2,%%lo(D_800E9BA0)($1) # publish the advanced display-list cursor\n"
            "sw $4,24($5) # write the selected render-mode command\n"
            "sw $3,28($5) # write the selected render-mode value\n"
            "# Restore normal assembler scheduling after the render-mode fragment.\n"
            ".set reorder"
            :
            : "r"(mode_word), "r"(header_word), "r"(first_commands), "r"(result)
            : "$2", "$1", "memory");
    }

    {
        register volatile u32 *commands asm("$2");
        register u8 *state asm("$5");
        register u32 first_word asm("$4");
        register u32 set_tile asm("$7");
        register u32 size_word asm("$8");
        u32 command_word;
        u32 final_pipe;

        first_word = 0xFC119623;
        command_word = 0xFF2FFFFF;
        result = 0x073FF200;
        set_tile = 0xF5400000;
        asm volatile(
            "# emit no instruction; keep the set-tile high half live before the volatile cursor loads"
            :
            : "r"(set_tile)
            : "memory");
        commands = D_800E9BA0;
        state = D_80196AF8;
        set_tile |= 0x0800;
        size_word = 0x000F0000;
        asm volatile(
            "# emit no instruction; keep the texture-size high half before the first command store"
            :
            : "r"(size_word)
            : "memory");
        commands[1] = command_word;
        commands[2] = 0xDE000000;
        asm volatile(
            "lui $3,0x0600 # load the segmented icon display-list address high half\n"
            "addiu $3,$3,0x47A0 # add the signed low half exactly as retail\n"
            "sw $3,12(%0) # store the icon display-list address in the pending command"
            :
            : "r"(commands)
            : "$3", "memory");
        commands[0] = first_word;
        commands[4] = 0xFD500000;
        first_word = *(u32 *)(state + 4);
        size_word |= 0xC0FC;
        D_800E9BA0 = commands + 2;
        D_800E9BA0 = commands + 4;
        D_800E9BA0 = commands + 6;
        D_800E9BA0 = commands + 8;
        commands[6] = 0xF5500000;
        commands[7] = 0x07000000;
        D_800E9BA0 = commands + 10;
        commands[8] = 0xE6000000;
        D_800E9BA0 = commands + 12;
        commands[10] = 0xF3000000;
        D_800E9BA0 = commands + 14;
        commands[12] = 0xE7000000;
        D_800E9BA0 = commands + 16;
        D_800E9BA0 = commands + 18;
        commands[9] = 0;
        commands[11] = result;
        commands[13] = 0;
        commands[14] = set_tile;
        commands[15] = 0;
        commands[16] = 0xF2000000;
        commands[17] = size_word;
        first_word += 0x948;
        commands[5] = first_word;
        first_word = state[0x18B];

        if ((state[0x60C] != 0) & (state[0x5E9] != 0xFF)) {
            result = func_000bf9c8(first_word, state[0x5E9]);
        } else {
            result = 0;
        }

        asm volatile(
            "# Preserve the retail palette branch and its two delay slots.\n"
            ".set noreorder\n"
            "lui $2,%%hi(D_80196A6C) # load the address of the palette-selection threshold\n"
            "lw $2,%%lo(D_80196A6C)($2) # read the palette-selection threshold\n"
            "sltu $2,$2,$6 # test whether the computed class/equipment result exceeds the threshold\n"
            "beq $2,$0,1f # choose the low-result palette when the threshold was not exceeded\n"
            "lui $4,0xF500 # begin the shared palette tile command in the branch delay slot\n"
            "ori $4,$4,0x0100 # complete the palette tile command for the high-result path\n"
            "lui $2,%%hi(D_800E9BA0) # load the display-list cursor global address\n"
            "lw $2,%%lo(D_800E9BA0)($2) # read the display-list cursor for the high-result path\n"
            "lui $5,0x0703 # load the palette tile-state high half\n"
            "addiu $3,$2,8 # advance the display-list cursor by one command\n"
            "lui $1,%%hi(D_800E9BA0) # reload the display-list cursor global address\n"
            "sw $3,%%lo(D_800E9BA0)($1) # publish the advanced high-result cursor\n"
            "lui $3,0xFD10 # load the palette texture command\n"
            "sw $3,0($2) # write the high-result palette texture command\n"
            "lui $3,0x0600 # load the high-result palette address high half\n"
            "addiu $3,$3,0x0120 # add the high-result palette address low half\n"
            "j 2f # skip the duplicated low-result command setup\n"
            "ori $5,$5,0xC000 # complete the tile state in the jump delay slot\n"
            "1: # low-result palette setup path\n"
            "ori $4,$4,0x0100 # complete the palette tile command for the low-result path\n"
            "lui $2,%%hi(D_800E9BA0) # load the display-list cursor global address\n"
            "lw $2,%%lo(D_800E9BA0)($2) # read the display-list cursor for the low-result path\n"
            "lui $5,0x0703 # load the low-result palette tile-state high half\n"
            "ori $5,$5,0xC000 # complete the low-result palette tile state\n"
            "addiu $3,$2,8 # advance the display-list cursor by one command\n"
            "lui $1,%%hi(D_800E9BA0) # reload the display-list cursor global address\n"
            "sw $3,%%lo(D_800E9BA0)($1) # publish the advanced low-result cursor\n"
            "lui $3,0xFD10 # load the palette texture command\n"
            "sw $3,0($2) # write the low-result palette texture command\n"
            "lui $3,0x0600 # load the low-result palette address high half\n"
            "addiu $3,$3,0x00C0 # add the low-result palette address low half\n"
            "2: # join after selecting the high- or low-result palette address\n"
            "sw $3,4($2) # write the selected segmented palette address\n"
            "# Restore normal assembler scheduling after the palette fragment.\n"
            ".set reorder"
            : "=r"(commands), "=r"(first_word), "=r"(state)
            : "r"(result)
            : "$1", "$3", "memory");
        D_800E9BA0 = commands + 4;
        commands[2] = 0xE8000000;
        D_800E9BA0 = commands + 6;
        commands[5] = 0x07000000;
        D_800E9BA0 = commands + 8;
        commands[6] = 0xE6000000;
        D_800E9BA0 = commands + 10;
        commands[8] = 0xF0000000;
        D_800E9BA0 = commands + 12;
        final_pipe = 0xE7000000;
        asm volatile(
            "# emit no instruction; materialize the final pipe-sync word after publishing the cursor"
            :
            : "r"(final_pipe)
            : "memory");
        commands[3] = 0;
        commands[4] = first_word;
        commands[7] = 0;
        commands[9] = (u32)state;
        commands[10] = final_pipe;
        commands[11] = 0;
        asm volatile(
            "# emit no instruction; keep all display-list stores before the icon draw call"
            :
            :
            : "memory");

        icon_x = x + 4;
        func_00088b10(icon_x, y + 6, result, -4);
    }
    {
        register u8 *state asm("$2");

        state = D_80196AF8;
        func_00087b30(*(s32 *)(state + 8) + 0x300,
                      *(s32 *)(state + 4) + 0x27E8,
                      0x12, 8, 4);
    }

    {
        register volatile u32 *commands asm("$3");
        register u32 pipe_sync asm("$7");
        register u32 color_word asm("$8");
        register s32 draw_x asm("$4");
        register s32 draw_y asm("$5");
        register s32 draw_count asm("$6");

        color_word = 0x004FC3BC;
        draw_x = x + 7;
        commands = D_800E9BA0;
        text_y = y - 6;
        draw_y = text_y;
        draw_count = 3;
        asm volatile(
            "# emit no instruction; keep the draw-count argument before the pipe-sync constant"
            :
            : "r"(draw_count)
            : "memory");
        pipe_sync = 0xE7000000;
        D_800E9BA0 = commands + 2;
        D_800E9BA0 = commands + 4;
        commands[2] = 0xED000000;
        commands[0] = pipe_sync;
        commands[1] = 0;
        commands[3] = color_word;
        D_800E9BA0 = commands + 6;
        commands[4] = pipe_sync;
        commands[5] = 0;

        func_0008b5e4(draw_x, draw_y, draw_count, pipe_sync);
    }
    func_000873a0(x + 0x0B, text_y, *(s32 *)(screen_arg + 0x14), 0x56);
    {
        register u8 *state asm("$2");
        register s32 draw_x asm("$4");
        register s32 draw_y asm("$5");
        register s32 draw_width asm("$6");
        register s32 draw_height asm("$7");

        state = D_80196AF8;
        draw_width = 0x30;
        asm volatile(
            "# emit no instruction; materialize the width argument before reading the draw coordinates"
            :
            : "r"(draw_width)
            : "memory");
        draw_x = *(s32 *)(state + 8);
        asm volatile(
            "# emit no instruction; preserve the retail x-then-y coordinate load order"
            :
            : "r"(draw_x)
            : "$5", "$7", "memory");
        draw_y = *(s32 *)(state + 4);
        draw_height = 0x10;
        asm volatile(
            "addiu $16,$0,2 # retain the fifth draw argument in s0 for the later status draw"
            : "=r"(text_y));
        asm volatile(
            "# Preserve the retail fifth-argument store and call delay slot.\n"
            ".set noreorder\n"
            "sw $16,16($29) # pass the retained value two as the fifth draw argument\n"
            "addiu $4,$4,0x0320 # offset the destination x coordinate\n"
            "jal func_00087b30 # draw the class-screen status rectangle\n"
            "addiu $5,$5,0x2828 # offset the destination y coordinate in the call delay slot\n"
            "# Restore normal assembler scheduling after the fixed draw-call delay slot.\n"
            ".set reorder"
            :
            : "r"(draw_x), "r"(draw_y), "r"(draw_width),
              "r"(draw_height), "r"(text_y)
            : "$31", "memory");
    }
    func_00087200(x + 0x24, y + 7, 0x1A);
    {
        register u8 *state asm("$2");

        state = D_80196AF8;
        func_00087b30(*(s32 *)(state + 0x64),
                      *(s32 *)(state + 0x64) + 0x2E0,
                      0x30, 0x20, text_y);
    }
    func_00087200(icon_x, y - 7, 0x4B);
}
