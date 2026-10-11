typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
extern struct {
    u16 width, height;
    u8 unknown04[0x5C];
    u8 command, textIndex, flags, preset;
    u8 startXHi, startXLo, targetXHi, unknown67, targetXLo, control;
    u8 portraitHi, portraitLo, startYHi, startYLo, archiveHi, archiveLo;
} D_800E7A58;
typedef struct {
    void (*callback0)(void), (*callback1)(void);
    void (*callback2)(s32);
    u16 stateBytes, unknown0E;
    u32 field10;
} DialogueCallbacks;
typedef struct {
    u16 flags;
    u8 field02, field03, speed, unknown05;
    s16 left, top, right, bottom;
    u16 field0E, field10, field12;
} DialogueHeader;
extern struct { DialogueHeader header; DialogueCallbacks callbacks; } D_800E7A30;
extern u8 D_800E91D0[];
extern char D_8019E1F8[], D_8019E20C[];
extern u8 D_800E7A32;
extern u8 D_800E7A33;

typedef struct DialogueState {
    s16 field00;
    u16 field02;
    u8 *text;
    u8 *cursor;
    u8 unknown0C[8];
    s16 portrait;
    u16 field16;
    u16 field18;
    u16 columns;
    u16 field1C;
    u16 rows;
    u16 width;
    u16 height;
    union { s32 packed; struct { u16 x, y; } xy; } start;
    u16 targetX, targetY;
    u16 currentX, currentY;
    s16 pointerX, pointerY;
    u16 field34, textOffset, steps;
    u8 pointerSide, pointerEdge, field3C, portraitSide;
    u8 unknown3E[4];
    u8 field42, field43, field44, field45, field46, style;
    u8 field48, portraitAlpha, portraitVariant, field4B;
    u8 unknown4C[3];
    u8 field4F, field50, field51, field52, field53, firstLine, field55, alpha;
    u8 lineFlags[0x61];
    u16 lineOffsets[0x60];
    u8 textBuffer[0x300];
} DialogueState;

extern DialogueState *D_8019E194;
#define DIALOGUE D_8019E194
#define FLAGS D_800E7A58.flags
#define CONTROL D_800E7A58.control

/* These repeated expressions follow the retail branch trees; they deliberately
 * reload the shared object when used again after stores. */
#define BELOW_LEFT(x) (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        ((DIALOGUE->style == 0 && DIALOGUE->portraitSide == 0) ? \
            (DIALOGUE->portrait >= 0 ? (x) < 56 : (x) < 8) : (x) < 8) : (x) < 7) : ((x) < 0))
#define RIGHT_EDGE (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        (DIALOGUE->style == 0 ? \
            (DIALOGUE->portraitSide ? DIALOGUE->columns * 7 - 1 : \
                DIALOGUE->columns * 7 + (DIALOGUE->portrait >= 0 ? 47 : -1)) : \
            DIALOGUE->columns * 7 - 1) : DIALOGUE->columns * 7 - 2) : DIALOGUE->columns * 7 - 9)
#define LEFT_EDGE (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        (DIALOGUE->style == 0 ? (DIALOGUE->portraitSide ? 8 : (DIALOGUE->portrait >= 0 ? 56 : 8)) : 8) : 7) : 0)

extern void func_000E659C(void);
extern void func_000E65DC(void);
extern void func_000E6620(s32);
extern void memset_00023780(void *, s32);
extern u8 *boot_command_stream_dispatch(u32, s32, s32, s32);
extern void func_000ea930(void);
extern u16 func_000e98bc(u8 *, s32);
extern u16 func_000e9528(u8 *);
extern void func_00023940(char *, ...);

void func_000E5968(s16 windowId)
{
    u8 *archive = 0;
    s32 preset;
    s32 value;
    s32 left;
    s16 nearLeft, nearRight, outsideLeft;
    u16 width, currentX;
    DialogueState *geometry;

    DialogueCallbacks *callbacks = &D_800E7A30.callbacks;

    D_800E7A32 |= 9;
    D_800E7A33 |= 1;
    D_800E7A30.header.flags |= 0x200;
    callbacks->callback0 = func_000E659C;
    callbacks->callback1 = func_000E65DC;
    callbacks->callback2 = func_000E6620;
    callbacks->stateBytes = 0x478;
    callbacks->field10 = 0;
    D_800E7A30.header.speed = 8;
    /* KMC 2.7.2 needs this block boundary to retain the callback/header
     * address base and the archive's initial lifetime. The byte cannot have
     * bit 8 set: the clear executes exactly once. Its taken exit survives CSE,
     * then combine removes the test and unreachable store. Keep the nonempty
     * tail; an empty tail or do/while (0) loses that compiler context. */
    for (;;) {
        memset_00023780(D_800E91D0, 0x478);
        if (!(D_800E7A32 & 0x100))
            break;
        D_800E7A32 = 0;
    }
    DIALOGUE->field00 = windowId;
    switch (D_800E7A58.command & 0xC0) {
    case 0:
        archive = boot_command_stream_dispatch(0x01A3B7B2,
            (D_800E7A58.archiveHi << 8) | D_800E7A58.archiveLo, -4, 0);
        break;
    case 0x40:
        break;
    case 0x80:
        archive = boot_command_stream_dispatch(0x021B8BA4,
            (D_800E7A58.archiveHi << 8) | D_800E7A58.archiveLo, -4, 0);
        break;
    }
    {
        DialogueState *state = DIALOGUE;
        u8 *text = archive + ((u32 *)archive)[
            ((D_800E7A58.command & 0x3F) << 8) | D_800E7A58.textIndex];
        state->portrait = -1;
        state->portraitAlpha = 255;
        state->cursor = state->text = text;
    }
    func_000ea930();
    if (DIALOGUE->style == 1) {
        DIALOGUE->columns = 36;
        DIALOGUE->rows = 3;
    } else {
        if (DIALOGUE->columns == 0)
            DIALOGUE->columns = (u32)func_000e98bc(DIALOGUE->text, 0) / 7U + 1;
        if (DIALOGUE->style == 0)
            DIALOGUE->rows = 3;
        else if (DIALOGUE->rows == 0)
            DIALOGUE->rows = func_000e9528(DIALOGUE->text);
    }
    if (DIALOGUE->columns >= 37) DIALOGUE->columns = 36;
    if ((D_800E7A58.portraitHi | D_800E7A58.portraitLo) != 0) {
        DIALOGUE->portrait = (D_800E7A58.portraitHi << 8) | D_800E7A58.portraitLo;
        if (DIALOGUE->portrait < 0) {
            DIALOGUE->portrait = -DIALOGUE->portrait;
            DIALOGUE->portraitVariant = 1;
        } else {
            DIALOGUE->portraitVariant = 0;
        }
    }
    FLAGS &= ~0x10;
    D_800E7A58.width = DIALOGUE->width = !(CONTROL & 4) ?
        (DIALOGUE->style < 2 ? (DIALOGUE->style == 0 ?
            DIALOGUE->columns * 7 + (DIALOGUE->portrait >= 0 ? 67 : 19) :
            DIALOGUE->columns * 7 + 19) : DIALOGUE->columns * 7 + 14) : DIALOGUE->columns * 7;
    D_800E7A58.height = DIALOGUE->height = !(CONTROL & 4) ?
        (DIALOGUE->style < 2 ? 61 : ({
            s32 gaps = DIALOGUE->rows - 1;
            s32 pixels;
            /* Retain the shared spacing factor for KMC's late folding. */
            gaps *= DIALOGUE->style < 2 ? 3 : 2;
            pixels = DIALOGUE->rows * 14 + 14;
            gaps + pixels;
        })) :
        (DIALOGUE->rows * 14 + (DIALOGUE->rows - 1) * (DIALOGUE->style < 2 ? 3 : 2));
    if (FLAGS & 4) DIALOGUE->portraitSide = 1;
    if (DIALOGUE->portrait >= 999) {
        func_00023940(D_8019E1F8, DIALOGUE->portrait);
        DIALOGUE->portrait = -1;
    }
    if (!(CONTROL & 8)) {
        if (!(FLAGS & 0x40)) {
            preset = D_800E7A58.preset >> 4;
            DIALOGUE->targetY = preset * 14 + ((s16)DIALOGUE->height / 2 + 19);
            preset = D_800E7A58.preset & 15;
            if (preset >= 8) func_00023940(D_8019E20C, preset);
            if (preset < 4)
                DIALOGUE->targetX = preset * 22 + ((s16)DIALOGUE->width / 2 + 28);
            else
                DIALOGUE->targetX = ((preset - 4) * 22 - (DIALOGUE->width - 211)) + ((s16)DIALOGUE->width / 2 + 16);
        } else {
            DIALOGUE->targetX = (s16)DIALOGUE->width / 2 + (D_800E7A58.startXLo | (D_800E7A58.startXHi << 8));
            DIALOGUE->targetY = (s16)DIALOGUE->height / 2 + (D_800E7A58.startYLo | (D_800E7A58.startYHi << 8));
        }
    } else {
        DIALOGUE->targetX = D_800E7A58.targetXLo | (D_800E7A58.targetXHi << 8);
        DIALOGUE->targetY = D_800E7A58.preset;
    }
    if (FLAGS & 0x80) DIALOGUE->targetX = 160;
    DIALOGUE->start.xy.x = D_800E7A58.startXLo | (D_800E7A58.startXHi << 8);
    DIALOGUE->start.xy.y = D_800E7A58.startYLo | (D_800E7A58.startYHi << 8);
    if (DIALOGUE->start.packed == 0 || DIALOGUE->style != 0) {
        DIALOGUE->start.xy.x = DIALOGUE->targetX;
        DIALOGUE->start.xy.y = DIALOGUE->targetY;
    }
    DIALOGUE->currentX = DIALOGUE->start.xy.x;
    DIALOGUE->currentY = DIALOGUE->start.xy.y;
    DIALOGUE->steps = ({
        s32 speed = D_800E7A30.header.speed - 2;
        speed > 0 ? speed : 1;
    });
    if (((s16)DIALOGUE->targetX - (s16)DIALOGUE->currentX > 0 ?
         (s16)DIALOGUE->targetX - (s16)DIALOGUE->currentX : (s16)DIALOGUE->currentX - (s16)DIALOGUE->targetX) >=
        ((s16)DIALOGUE->targetY - (s16)DIALOGUE->currentY > 0 ?
         (s16)DIALOGUE->targetY - (s16)DIALOGUE->currentY : (s16)DIALOGUE->currentY - (s16)DIALOGUE->targetY)) {
        value = (s16)DIALOGUE->targetX - (s16)DIALOGUE->currentX;
        DIALOGUE->steps = (value > 0 ? value : (s16)DIALOGUE->currentX - (s16)DIALOGUE->targetX) / DIALOGUE->steps;
    } else {
        value = (s16)DIALOGUE->targetY - (s16)DIALOGUE->currentY;
        DIALOGUE->steps = (value > 0 ? value : (s16)DIALOGUE->currentY - (s16)DIALOGUE->targetY) / DIALOGUE->steps;
    }
    if (DIALOGUE->steps == 0) DIALOGUE->steps = 1;
    geometry = DIALOGUE;
    width = geometry->width;
    {
        s32 halfWidth = (s16)width / 2;
        left = geometry->targetX - halfWidth;
    }
    currentX = geometry->currentX;
    {
        s32 innerLeft = left + 20;
        nearLeft = currentX - innerLeft;
    }
    {
        s32 innerRight = width + left - 20;
        nearRight = innerRight - currentX;
    }
    {
        s32 outerLeft = left - 12;
        outsideLeft = currentX - outerLeft;
    }
    {
        s16 initialX = nearLeft;
        if (DIALOGUE->portraitSide == 0) {
            DialogueState *state = DIALOGUE;
            s32 x;
            state->pointerSide = 0;
            state->pointerX = initialX;
            x = DIALOGUE->pointerX;
            if (BELOW_LEFT(x) && nearLeft <= nearRight) {
                state = DIALOGUE;
                state->pointerX = outsideLeft;
                state->pointerSide = 1;
            }
        } else {
            DialogueState *state = DIALOGUE;
            s32 x;
            state->pointerSide = 1;
            state->pointerX = outsideLeft;
            x = DIALOGUE->pointerX;
            if (RIGHT_EDGE < x && nearLeft >= nearRight) {
                DialogueState *corrected = DIALOGUE;
                corrected->pointerX = initialX;
                corrected->pointerSide = 0;
            }
        }
    }
    {
        s32 x = DIALOGUE->pointerX;
        if (BELOW_LEFT(x)) DIALOGUE->pointerX = LEFT_EDGE;
    }
    {
        s32 x = DIALOGUE->pointerX;
        if (RIGHT_EDGE < x) {
            DialogueState *state = DIALOGUE;
            state->pointerX = !(CONTROL & 4) ?
                (state->style < 2 ?
                    ((state->style == 0 && state->portraitSide == 0) ?
                        state->columns * 7 + (state->portrait >= 0 ? 47 : -1) :
                        state->columns * 7 - 1) : state->columns * 7 - 2) : state->columns * 7 - 9;
        }
    }
    if (!(FLAGS & 8)) {
        DialogueState *state = DIALOGUE;
        state->pointerEdge = 0;
        state->pointerY = state->height - 6;
    } else {
        DIALOGUE->pointerY = -11;
        DIALOGUE->pointerEdge = 1;
    }
}
