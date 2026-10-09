typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
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
typedef struct { u32 w0, w1; } Gfx;
extern Gfx *D_800E9BA0;
extern Gfx D_801869C8[];
extern struct { s16 left, top, right, bottom; } D_800E7A36;
extern u16 D_800E7A30;
extern u8 D_800E7AC1, D_800E7ABA;
extern struct { u8 unknown[6]; u8 style; } D_8019E198;
extern u8 D_8019E19A;
#define CONTROL D_800E7AC1
#define FLAGS D_800E7ABA
#define EMIT(a,b) { Gfx *command = D_800E9BA0++; command->w0 = (a); command->w1 = (b); }
/* Coordinate expressions retain separate reads of the shared dialogue object. */
#define TEXT_X(x) (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        ((DIALOGUE->style == 0 && DIALOGUE->portraitSide == 0) ? \
            (x)+(DIALOGUE->portrait >= 0 ? 56 : 8) : (x)+8) : (x)+7) : (x))
#define UNTEXT_X(x) (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        ((DIALOGUE->style == 0 && DIALOGUE->portraitSide == 0) ? \
            (x)-(DIALOGUE->portrait >= 0 ? 56 : 8) : (x)-8) : (x)-7) : (x))
/* Parenthesising the columns term retains KMC's x+offset evaluation order. */
#define RIGHT_X(x) (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        (DIALOGUE->style == 0 ? (DIALOGUE->portraitSide ? (x)+(7+DIALOGUE->columns*7) : \
            (x)+((DIALOGUE->portrait >= 0 ? 55 : 7)+DIALOGUE->columns*7)) : (x)+(7+DIALOGUE->columns*7)) : \
        (x)+(6+DIALOGUE->columns*7)) : (x)+(-1+DIALOGUE->columns*7))
#define UNRIGHT_X(x) (!(CONTROL & 4) ? \
    (DIALOGUE->style < 2 ? \
        (DIALOGUE->style == 0 ? (DIALOGUE->portraitSide ? (x)+(8-DIALOGUE->columns*7) : \
            (x)+((DIALOGUE->portrait >= 0 ? -40 : 8)-DIALOGUE->columns*7)) : (x)+(8-DIALOGUE->columns*7)) : \
        (x)+(9-DIALOGUE->columns*7)) : (x)+(16-DIALOGUE->columns*7))
static inline s32 row_spacing(s32 n) { return DIALOGUE->style < 2 ? n*17 : n*16; }
#define LINE_Y(n) (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (n)*17+5 : (n)*16+7) : \
    ((n) * (DIALOGUE->style < 2 ? 17 : 16)))
#define MARKER_Y(n) (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (n)*17+7 : (n)*16+9) : \
    (row_spacing(n)+2))
#define MARKER_BOTTOM(n) (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (n)*17+18 : (n)*16+20) : \
    (row_spacing(n)+13))
static inline s32 bare_marker_source_y(s32 y, s32 n) { return DIALOGUE->style < 2 ? y + (-2-n*17) : y + (-2-n*16); }
#define UNMARKER_Y(y,n) (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (y)+(-7-(n)*17) : (y)+(-9-(n)*16)) : \
    bare_marker_source_y((y),(n)))
#define MARKER_SOURCE_X ({ s32 coordinate_x = TEXT_X(15); UNTEXT_X(coordinate_x); })
#define MARKER_SOURCE_Y(n) ({ s32 coordinate_y = MARKER_BOTTOM(n); UNMARKER_Y(coordinate_y,n); })
#define BOTTOM_Y(y) ((y) + (!(CONTROL & 4) ? 53 : (DIALOGUE->style < 2 ? 48 : 46)))
#define UNBOTTOM_Y(y) ((y) - (!(CONTROL & 4) ? 38 : (DIALOGUE->style < 2 ? 33 : 31)))
#define PORTRAIT_LEFT (DIALOGUE->portraitSide ? DIALOGUE->columns*7+16 : 8)
#define PORTRAIT_RIGHT (DIALOGUE->portraitSide ? DIALOGUE->columns*7+55 : 47)
#define PORTRAIT_LOCAL(x) (DIALOGUE->portraitSide ? (x)+(-16-DIALOGUE->columns*7) : (x)-8)
extern void func_0004c7d4(s32,s32,s32,s32,s32,s32);
extern void func_000e48f0(void);
extern void func_000e495c(s32);
extern void func_000E4C54(u32,s32,s32);
extern void func_000E4BE0(u32,s32,s32);
extern void func_000E4FE4(u32,s32,s32,u32,s32);
extern void func_000E4DC8(u32,s32,s32);
extern void func_0004cb6c(s32,s32,s32,s32,s32,s32,s32);
extern void func_0004c848(s32,s32,s32,s32,s32,s32,s32,s32,s32);
extern void func_000ead04(s32,s32);
extern void func_0004f1e8(void);
extern void func_0004fe04(u8 *);
extern void func_0004ef34(u8 *,s32,s32);
extern void func_00008A74(s32,s32,s32,s32);
extern void func_000e4930(void);

/* Draw the current dialogue record through every retail rendering phase.
 * Coordinate expressions preserve the compiler's staged arithmetic and
 * separate reads of shared state; field meanings remain conservative. */
void func_000E6D90(void)
{
    s32 portraitAlpha;
    s32 line;

    func_0004c7d4((s16)DIALOGUE->width, (s16)DIALOGUE->height,
        D_800E7A36.left, D_800E7A36.top, D_800E7A36.right, D_800E7A36.bottom);
    func_000e48f0();
    if (!(CONTROL & 4)) {
        if (DIALOGUE->style < 2) {
            if (DIALOGUE->alpha == 255) func_000e495c(8);
            else {
                func_000e495c(2);
                EMIT(0xFA000000, DIALOGUE->alpha | 0xFFFFFF00);
                EMIT(0xFCFF97FF, 0xFF2CFE7F);
            }
            func_000E4C54(0x02093576, 0, 1);
            func_0004cb6c(8, 0, (s16)DIALOGUE->width - 12, 60, 0, 0, 0);
            func_000E4BE0(0x02093576, 0, 0);
            func_0004cb6c(0, 0, 7, 60, 0, 0, 0);
            func_000E4BE0(0x02093576, 0, 2);
            func_0004cb6c((s16)DIALOGUE->width - 11, 0, (s16)DIALOGUE->width - 1, 60, 0, 0, 0);
            if (DIALOGUE->alpha != 255) func_000e48f0();
            if (DIALOGUE->field50 & 1) {
                func_000e495c(8);
                func_000E4BE0(0x02093576, 0, 6);
                func_0004cb6c((s16)DIALOGUE->width - 11, 4, (s16)DIALOGUE->width - 4, 11, 0, 0, 0);
            }
            if (DIALOGUE->field50 & 2) {
                func_000e495c(8);
                func_000E4BE0(0x02093576, 0, 6);
                func_0004cb6c((s16)DIALOGUE->width - 11, (s16)DIALOGUE->height - 15,
                    (s16)DIALOGUE->width - 4, (s16)DIALOGUE->height - 8, 8, 0, 0);
            }
        }
    } else {
        EMIT(0xDE000000, (u32)D_801869C8);
        EMIT(0xD9000000, 0);
        EMIT(0xFCFFFFFF, 0xFFFDF6FB);
        EMIT(0xE200001C, 0x00504240);
        EMIT(0xFA000000, 0);
        EMIT(0xE7000000, 0);
        EMIT(((((D_800E7A36.right*4)&0xFFF)<<12) | 0xE4000000) | ((D_800E7A36.bottom*4)&0xFFF),
            (((D_800E7A36.left*4)&0xFFF)<<12) | ((D_800E7A36.top*4)&0xFFF));
        EMIT(0xE1000000, 0);
        EMIT(0xF1000000, 0);
        EMIT(0xE7000000, 0);
        func_000e48f0();
    }
    if (DIALOGUE->portrait >= 0) {
        s32 left, top, right, bottom, coordinate, sourceX, sourceY;
        portraitAlpha = ((DIALOGUE->alpha + 1) * DIALOGUE->portraitAlpha) >> 8;
        if ((u32)portraitAlpha < 255) {
            func_000e495c(2);
            EMIT(0xFCFFFFFF, 0xFFFCF67B);
            EMIT(0xFA000000, portraitAlpha & 255);
        } else func_000e495c(0);
        func_000ead04(DIALOGUE->portrait, DIALOGUE->portraitVariant);
        if (FLAGS & 0x20) {
            left = PORTRAIT_LEFT;
            right = PORTRAIT_RIGHT;
            coordinate = PORTRAIT_RIGHT;
            sourceX = PORTRAIT_LOCAL(coordinate);
            func_0004c848(left, 5, right, 52, sourceX, 0, 0, 47, 0);
        } else func_0004cb6c(PORTRAIT_LEFT, 5, PORTRAIT_RIGHT, 52, 0, 0, 0);
        if ((u32)portraitAlpha < 255) {
            EMIT(0xE7000000, 0);
            EMIT(0xFCFFFFFF, 0xFFFCF279);
        }
    }
    if (!(D_800E7A30 & 0x1000) && DIALOGUE->textBuffer[0] != 0) {
        s32 x, y;
        u8 *text;
        func_000e495c(40);
        func_0004f1e8();
        D_8019E19A = DIALOGUE->field4B < 10 ? DIALOGUE->field4B + '0' : '0';
        D_8019E198.style = DIALOGUE->style < 2 ? '3' : '2';
        func_0004fe04((u8 *)&D_8019E198);
        if (DIALOGUE->field4F == 0) {
            text = DIALOGUE->textBuffer + DIALOGUE->textOffset;
            {
                /* Keep raw and adjusted coordinates separate for KMC scheduling. */
                s32 rawX = D_800E7A36.left;
                x = TEXT_X(rawX);
            }
            y = D_800E7A36.top;
            if (!(CONTROL & 4)) y += DIALOGUE->style < 2 ? 5 : 7;
        } else {
            text = DIALOGUE->textBuffer + DIALOGUE->lineOffsets[DIALOGUE->firstLine];
            x = D_800E7A36.left;
            x = TEXT_X(x);
            y = D_800E7A36.top;
            if (!(CONTROL & 4)) y += DIALOGUE->style < 2 ? 5 : 7;
        }
        func_0004ef34(text, x, y);
    }
    if (DIALOGUE->field42 != 0 && DIALOGUE->field4F == 0) {
        s32 left, top, right, bottom, coordinate, sourceX, sourceY;
        func_000e495c(8);
        func_000E4FE4(0x02093576, 0, 4, DIALOGUE->field43 >> 6, 16);
        if (DIALOGUE->field42 == 1) {
            left = RIGHT_X(-15);
            top = !(CONTROL & 4) ? 38 : (DIALOGUE->style < 2 ? 33 : 31);
            right = RIGHT_X(0);
            bottom = BOTTOM_Y(0);
        } else if (DIALOGUE->field42 == 2) {
            s32 baseX, inputX, screenX, inputY, screenY;
            coordinate = DIALOGUE->field18 * 7;
            left = TEXT_X(coordinate);
            top = LINE_Y(DIALOGUE->field1C);
            baseX = DIALOGUE->field18 * 7;
            inputX = TEXT_X(baseX);
            screenX = RIGHT_X(inputX);
            right = UNRIGHT_X(screenX);
            inputY = LINE_Y(DIALOGUE->field1C);
            screenY = BOTTOM_Y(inputY);
            bottom = UNBOTTOM_Y(screenY);
        } else {
            s32 baseX, inputX, screenX, inputY, screenY;
            left = TEXT_X(0);
            top = LINE_Y(DIALOGUE->field1C + 1);
            inputX = TEXT_X(0);
            screenX = RIGHT_X(inputX);
            right = UNRIGHT_X(screenX);
            inputY = LINE_Y(DIALOGUE->field1C + 1);
            screenY = BOTTOM_Y(inputY);
            bottom = UNBOTTOM_Y(screenY);
        }
        func_0004cb6c(left, top, right, bottom, 0, 0, 0);
    }
    if (DIALOGUE->field44 != 0 && DIALOGUE->field4F == 0) {
        s32 left, top, right, bottom, coordinate, sourceX, sourceY;
        func_000e495c(8);
        func_000E4BE0(0x02093576, 0, 5);
        /* Nine arguments: both source coordinates are on the caller's stack. */
        left = TEXT_X(0);
        top = MARKER_Y(DIALOGUE->field44+DIALOGUE->field46);
        right = TEXT_X(15);
        bottom = MARKER_BOTTOM(DIALOGUE->field44+DIALOGUE->field46);
        func_0004c848(left, top, right, bottom, MARKER_SOURCE_X, 0, 0, MARKER_SOURCE_Y(DIALOGUE->field44+DIALOGUE->field46), 0);
    }
    if (DIALOGUE->field4F != 0) {
        for (line = DIALOGUE->firstLine; line < DIALOGUE->firstLine + DIALOGUE->rows; line++) {
            if (DIALOGUE->lineFlags[line] != 0) {
                s32 left, top, right, bottom, coordinate, sourceX, sourceY;
                func_000e495c(8);
                func_000E4BE0(0x02093576, 0, 5);
                left = TEXT_X(0);
                top = MARKER_Y(line-DIALOGUE->firstLine);
                right = TEXT_X(15);
                bottom = MARKER_BOTTOM(line-DIALOGUE->firstLine);
                func_0004c848(left, top, right, bottom, MARKER_SOURCE_X, 0, 0, MARKER_SOURCE_Y(line-DIALOGUE->firstLine), 0);
            }
        }
    }
    if (DIALOGUE->style == 0 && !(CONTROL & 4)) {
        s32 x, y;
        func_00008A74(0, 0, 320, 240);
        if (DIALOGUE->alpha == 255) func_000e495c(8);
        else {
            func_000e495c(2);
            EMIT(0xFA000000, DIALOGUE->alpha | 0xFFFFFF00);
            EMIT(0xFCFF97FF, 0xFF2CFE7F);
        }
        func_000E4DC8(0x02093576, 0, 3);
        x = DIALOGUE->pointerX; y = DIALOGUE->pointerY;
        func_0004c848(x, y, x+7, y+12,
            DIALOGUE->pointerSide*8, DIALOGUE->pointerEdge*19,
            DIALOGUE->pointerSide*8+7, DIALOGUE->pointerEdge*19+12, 0);
        func_00008A74(D_800E7A36.left, D_800E7A36.top, D_800E7A36.right, D_800E7A36.bottom);
    }
    func_000e4930();
}
