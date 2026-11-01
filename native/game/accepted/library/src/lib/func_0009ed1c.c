typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

extern u32 * volatile D_800E9BA0;
extern u8 * volatile D_80196AF8;

void func_0004ef34(void *, s32, s32, u32 *);
void func_00087200(s32, s32, s32);
void func_00087b30();
void func_00088304(s32, s32, u16, s32);

void func_0009ed1c(void *unused, u8 *screen, s32 slot)
{
    register u8 *screen_arg asm("$20") = screen;
    register s32 x asm("$19");
    register s32 y asm("$21");
    register s32 text_y asm("$16");
    register u32 pipe_sync asm("$17");
    register s32 two asm("$18");
    register s32 color asm("$6");

    color = *(s16 *)(screen_arg + ((slot & 0xFF) * 2) + 0xE0);
    if (color == 0) {
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
        first_commands[1] = color | 0xFFFFFF00;
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
            : "r"(mode_word), "r"(header_word), "r"(first_commands), "r"(color)
            : "$2", "$1", "memory");
    }

    {
        register u32 *image_commands asm("$7");
        register u8 *image_state asm("$3");
        register u32 temp_a0 asm("$4");
        register u32 temp_v0 asm("$2");

        temp_a0 = 0xFC119623;
        image_commands = D_800E9BA0;
        image_state = D_80196AF8;
        temp_v0 = 0xFF2FFFFF;
        image_commands[0] = temp_a0;
        image_commands[1] = temp_v0;
        temp_v0 = *(u16 *)(image_state + 0x5E8);
        temp_a0 = *(u32 *)(0x80187C14 + temp_v0 * 0x48);
        {
            register s32 image_x asm("$5") = x + 4;

            asm volatile(
                "# Preserve the retail image-call cursor publication and delay slot.\n"
                ".set noreorder\n"
                "addiu $7,$7,8 # advance the image command pointer passed in $a3\n"
                "lui $1,%%hi(D_800E9BA0) # load the display-list cursor global address\n"
                "sw $7,%%lo(D_800E9BA0)($1) # publish the command pointer passed in $a3\n"
                "jal func_0004ef34 # draw the selected class-screen image\n"
                "addiu $6,$21,4 # pass y + 4 in $a2 in the call delay slot\n"
                "# Restore normal assembler scheduling after the image-call delay slot.\n"
                ".set reorder"
                : "=r"(image_commands)
                : "0"(image_commands), "r"(temp_a0), "r"(image_x)
                : "$1", "$6", "$31", "memory");
        }
    }

    {
        register u32 load_block asm("$9") = 0x073FF200;
        register u32 set_tile asm("$10") = 0xF5400800;
        register u32 set_tile_size asm("$11") = 0x000FC0FC;
        register volatile u32 *draw_commands asm("$2");
        register u8 *draw_state asm("$8");
        register s32 draw_resource asm("$4");
        register u32 command_word asm("$3");
        register u32 command_header asm("$7");
        register s32 texture_base asm("$5");
        register s32 width asm("$6");

        draw_commands = D_800E9BA0;
        draw_state = D_80196AF8;
        asm volatile(
            "# emit no instruction; keep the UI-state load before the draw-resource constant"
            :
            : "r"(draw_state)
            : "memory");
        asm volatile(
            "lui %0,0x0600 # load the rectangle resource segment high half\n"
            "addiu %0,%0,0x0180 # add the signed resource offset exactly as retail"
            : "=r"(draw_resource));
        command_header = 0xDE000000;
        asm volatile(
            "lui %0,0x0600 # load the first segmented display-list address high half\n"
            "addiu %0,%0,0x47A0 # add the signed low half exactly as retail"
            : "=r"(command_word));
        width = 0x50;
        pipe_sync = 0xE7000000;
        draw_commands[1] = command_word;
        command_word = 0xFD500000;
        draw_commands[0] = command_header;
        draw_commands[2] = command_word;
        texture_base = *(s32 *)(draw_state + 4);
        two = 2;
        command_word = 0xF5500000;
        draw_commands[4] = command_word;
        command_word = 0x07000000;
        draw_commands[5] = command_word;
        command_word = 0xE6000000;
        draw_commands[6] = command_word;
        command_word = 0xF3000000;
        draw_commands[8] = command_word;
        command_word = 0xF2000000;
        draw_commands[14] = command_word;
        asm volatile(
            "lui %0,0x0600 # load the second segmented display-list address high half\n"
            "addiu %0,%0,0x47F0 # add the signed low half exactly as retail"
            : "=r"(command_word));
        draw_commands[7] = 0;
        draw_commands[9] = load_block;
        draw_commands[10] = pipe_sync;
        draw_commands[11] = 0;
        draw_commands[12] = set_tile;
        draw_commands[13] = 0;
        draw_commands[15] = set_tile_size;
        draw_commands[16] = command_header;
        draw_commands[17] = command_word;
        texture_base += 0x948;
        draw_commands[3] = texture_base;
        asm volatile(
            "sw $18,16($sp) # pass the constant two as the fifth rectangle-draw argument"
            :
            : "r"(two)
            : "memory");
        texture_base = *(s32 *)(draw_state + 0x58);
        asm volatile(
            "# emit no instruction; keep the rectangle y/source argument before cursor advancement"
            :
            : "r"(texture_base)
            : "memory");
        D_800E9BA0 = draw_commands + 2;
        D_800E9BA0 = draw_commands + 4;
        D_800E9BA0 = draw_commands + 6;
        D_800E9BA0 = draw_commands + 8;
        D_800E9BA0 = draw_commands + 10;
        D_800E9BA0 = draw_commands + 12;
        D_800E9BA0 = draw_commands + 14;
        D_800E9BA0 = draw_commands + 16;
        asm volatile(
            "# emit no instruction; publish the penultimate cursor before computing the final one"
            :
            : "r"(draw_commands)
            : "memory");
        D_800E9BA0 = draw_commands + 18;
        asm volatile(
            "# Preserve the rectangle-draw call and retail delay slot.\n"
            ".set noreorder\n"
            "jal func_00087b30 # draw the prepared class-screen texture rectangle\n"
            "addiu $7,$0,0x11 # pass height 0x11 in $a3 in the call delay slot\n"
            "# Restore normal assembler scheduling after the texture-rectangle call.\n"
            ".set reorder"
            :
            : "r"(draw_resource), "r"(texture_base), "r"(width)
            : "$7", "$31", "memory");
    }

    {
        register u8 *text_state asm("$2") = D_80196AF8;
        register s32 text_x asm("$4") = x + 0x65;

        asm volatile(
            "# emit no instruction; keep the first label x coordinate before y + 7"
            :
            : "r"(text_x)
            : "memory");
        text_y = y + 7;
        func_00088304(text_x, text_y, *(u16 *)(text_state + 0x1BF6), -2);
    }
    func_00087200(x + 0x75, text_y, 0x19);
    {
        register u8 *text_state asm("$2") = D_80196AF8;

        func_00088304(x + 0x7E, text_y, *(u16 *)(text_state + 0x1BFA), -2);
    }

    {
        register u8 *rectangle_state asm("$2") = D_80196AF8;

        func_00087b30(*(s32 *)(rectangle_state + 8) + 0x300,
                      *(s32 *)(rectangle_state + 4) + 0x27E8,
                      0x10, 8, 4);
    }

    {
        register u32 *rectangle_commands asm("$3") = D_800E9BA0;
        register u32 state_or_x asm("$4") = (u32)D_80196AF8;
        register u32 rectangle_color asm("$2") = 0x004FC3BC;
        register s32 rectangle_y asm("$5");
        register s32 rectangle_width asm("$6");
        register s32 rectangle_height asm("$7");

        text_y = 0xED000000;
        rectangle_commands[0] = pipe_sync;
        rectangle_commands[1] = 0;
        rectangle_commands[2] = (u32)text_y;
        rectangle_commands[3] = rectangle_color;
        rectangle_commands[4] = pipe_sync;
        rectangle_commands[5] = 0;
        asm volatile(
            "# emit no instruction; complete the six command stores before preparing call arguments"
            :
            :
            : "memory");
        rectangle_y = *(s32 *)(state_or_x + 0x64);
        rectangle_width = 0x30;
        asm volatile(
            "sw $18,16($sp) # pass the constant two as the fifth rectangle-draw argument"
            :
            : "r"(two)
            : "memory");
        state_or_x = *(s32 *)(state_or_x + 0x64);
        rectangle_height = 0x20;
        D_800E9BA0 = rectangle_commands + 2;
        D_800E9BA0 = rectangle_commands + 4;
        D_800E9BA0 = rectangle_commands + 6;
        asm volatile(
            "# Preserve the second rectangle-draw call delay slot.\n"
            ".set noreorder\n"
            "jal func_00087b30 # draw the status rectangle from the prepared arguments\n"
            "addiu $5,$5,0x2E0 # add the rectangle y/source offset in the call delay slot\n"
            "# Restore normal assembler scheduling after the status-rectangle call.\n"
            ".set reorder"
            :
            : "r"(state_or_x), "r"(rectangle_y), "r"(rectangle_width), "r"(rectangle_height)
            : "$5", "$31", "memory");
    }

    {
        register u32 *clip_base asm("$2") = D_800E9BA0;
        register u32 *clip_pointer asm("$3");
        register u32 *clip_output asm("$7");
        register s32 clip_x_value asm("$4");
        register u32 clip_value asm("$2");

        clip_base[0] = pipe_sync;
        clip_base[1] = 0;
        clip_x_value = *(s32 *)(screen_arg + 0x0C);
        clip_pointer = clip_base + 2;
        D_800E9BA0 = clip_pointer;
        D_800E9BA0 = clip_base + 4;
        asm volatile(
            "# preserve the retail negative-X branch instead of KMC's branch-likely contraction\n"
            "# Preserve the signed-X branch and its explicit delay slot.\n"
            ".set noreorder\n"
            "bltz $4,1f # select the literal left clip edge when screen x is negative\n"
            "move $7,$3 # retain the first scissor command pointer in the branch delay slot\n"
            "# let the assembler reproduce the retail FPU hazard nops\n"
            "# Allow scheduling within the nonnegative-X floating-point conversion.\n"
            ".set reorder\n"
            "lui $1,0x4080 # load float 4.0 high bits for nonnegative screen x\n"
            "mtc1 $1,$f2 # move float 4.0 into the multiplier register\n"
            "mtc1 $4,$f0 # move the signed screen x coordinate into the FPU\n"
            "cvt.s.w $f0,$f0 # convert screen x from signed integer to float\n"
            ".word 0x46020002 # encode retail mul.s $f0,$f0,$f2 to scale X by four subpixels\n"
            ".word 0x4600008D # encode retail trunc.w.s $f2,$f0 without the old assembler's incompatible macro\n"
            "mfc1 $2,$f2 # move the truncated coordinate into $v0\n"
            "andi $2,$2,0x0FFF # retain the 12-bit scissor coordinate\n"
            "sll $2,$2,12 # place the X coordinate in the command word\n"
            "# preserve the retail join jump and its delay slot\n"
            "# Preserve the conversion result sequence and path-join delay slot.\n"
            ".set noreorder\n"
            "j 2f # join the negative and nonnegative X paths\n"
            "or $2,$2,$16 # merge the ED command header in the jump delay slot\n"
            "1: # negative-X path using a literal zero left edge\n"
            "lui $2,0xED00 # form the literal zero-left-edge scissor command\n"
            "2: # join after forming the left-edge scissor command\n"
            "sw $2,0($7) # write the completed X scissor command through the saved pointer\n"
            "# Restore normal assembler scheduling after the X scissor fragment.\n"
            ".set reorder"
            : "=r"(clip_output)
            : "r"(clip_x_value), "r"(clip_pointer), "r"(text_y)
            : "$1", "$2", "$f0", "$f2", "memory");

        asm volatile(
            "# preserve the retail branch-likely layout for the special bottom clip edge\n"
            "# Preserve the special-bottom branch-likely and its taken-only load.\n"
            ".set noreorder\n"
            "lw $5,20($20) # load the screen Y/bottom coordinate into $a1\n"
            "addiu $2,$0,0x140 # materialize the special 320-pixel bottom edge\n"
            "beql $5,$2,3f # branch to the preloaded special command only when Y is 320\n"
            "lui $2,0x004F # preload the special command high half in the likely delay slot\n"
            "# let the assembler reproduce the retail FPU hazard nops\n"
            "# Allow scheduling within the ordinary-bottom floating-point conversion.\n"
            ".set reorder\n"
            "lui $1,0x4080 # load float 4.0 high bits for an ordinary bottom coordinate\n"
            "mtc1 $1,$f2 # move float 4.0 into the multiplier register\n"
            "mtc1 $5,$f0 # move the signed bottom coordinate into the FPU\n"
            "cvt.s.w $f0,$f0 # convert the bottom coordinate from signed integer to float\n"
            ".word 0x46020002 # encode retail mul.s $f0,$f0,$f2 to scale Y by four subpixels\n"
            ".word 0x4600008D # encode retail trunc.w.s $f2,$f0 without the old assembler's incompatible macro\n"
            "mfc1 $2,$f2 # move the truncated bottom coordinate into $v0\n"
            "andi $2,$2,0x0FFF # retain the 12-bit bottom scissor coordinate\n"
            "sll $2,$2,12 # place the Y coordinate in the command word\n"
            "# preserve the retail join jump and its delay slot\n"
            "# Preserve the ordinary-bottom join and its completion delay slot.\n"
            ".set noreorder\n"
            "j 4f # skip the special bottom-edge completion\n"
            "ori $2,$2,0x03BC # merge the ordinary right/bottom edge in the delay slot\n"
            "3: # special 320-pixel bottom-edge command completion\n"
            "ori $2,$2,0xC3BC # complete the special 320-pixel bottom-edge command\n"
            "4: # join after forming the bottom-edge scissor command\n"
            "# Restore normal assembler scheduling after the Y scissor fragment.\n"
            ".set reorder"
            : "=r"(clip_value)
            : "r"(screen_arg), "r"(clip_output)
            : "$1", "$5", "$f0", "$f2", "memory");

        asm volatile(
            "# Preserve the first post-scissor label call and its display-list schedule.\n"
            ".set noreorder\n"
            "addiu $4,$19,8 # compute the first label x coordinate from the saved screen x\n"
            "lui $3,%%hi(D_800E9BA0) # load the display-list cursor global address\n"
            "lw $3,%%lo(D_800E9BA0)($3) # read the cursor for the final pipe-sync command\n"
            "addiu $16,$21,-6 # compute and retain the shared label y coordinate\n"
            "move $5,$16 # pass the shared label y coordinate in $a1\n"
            "addiu $6,$0,0x22 # pass the first label identifier in $a2\n"
            "sw $2,4($7) # write the completed bottom scissor word through the saved pointer\n"
            "addiu $2,$3,8 # advance the display-list cursor by one command\n"
            "lui $1,%%hi(D_800E9BA0) # reload the display-list cursor global address\n"
            "sw $2,%%lo(D_800E9BA0)($1) # publish the advanced cursor\n"
            "lui $2,0xE700 # materialize the final pipe-sync command word\n"
            "sw $2,0($3) # write the pipe-sync command header\n"
            "jal func_00087200 # draw the first post-scissor label\n"
            "sw $0,4($3) # clear the pipe-sync payload in the call delay slot\n"
            "# Restore normal assembler scheduling after the final label call.\n"
            ".set reorder"
            : "=r"(text_y)
            : "r"(x), "r"(y), "r"(clip_output), "r"(clip_value)
            : "$1", "$3", "$4", "$5", "$6", "$31", "memory");
        {
            register s32 second_label_x asm("$4");

            asm volatile(
                "addiu $4,$19,0x65 # compute the second post-scissor label x coordinate without cross-call CSE"
                : "=r"(second_label_x)
                : "r"(x));
            func_00087200(second_label_x, text_y, 0x28);
        }
        func_00087200(x + 0x7C, text_y, 0x2B);
        func_00087200(x + 0x60, y + 0x15, 0x47);
    }
}
