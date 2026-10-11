#ifndef GAME_COMBAT_DRAW_COMMANDS_H
#define GAME_COMBAT_DRAW_COMMANDS_H

typedef unsigned int CombatDrawWord;
typedef struct CombatDrawCommand {
    CombatDrawWord first;
    CombatDrawWord second;
} CombatDrawCommand;

extern CombatDrawCommand *D_800E9BA0;

#define COMBAT_DRAW_FIELD(value, shift, width) \
    (((CombatDrawWord)(value) & ((1u << (width)) - 1)) << (shift))

/* Use as a statement inside braces. The plain compound block is deliberate:
 * this compiler retains loop notes for do/while wrappers, changing scheduling.
 */
#define COMBAT_DRAW_PAIR(a, b) { \
    CombatDrawCommand *command = D_800E9BA0++; \
    command->first = (a); \
    command->second = (b); \
}

void func_001FCB28(void *image, int type, int format, int width, int height,
                   int x0, int y0, int x1, int y1, unsigned int palette,
                   unsigned int cms, unsigned int cmt,
                   unsigned int masks, unsigned int maskt,
                   unsigned int shifts, unsigned int shiftt);

void func_001FD56C(void *image, unsigned int textureMemory, unsigned int tile,
                   int type, int format, int width, int height,
                   int x0, int y0, int x1, int y1, unsigned int palette,
                   unsigned int cms, unsigned int cmt,
                   unsigned int masks, unsigned int maskt,
                   unsigned int shifts, unsigned int shiftt);

#endif
