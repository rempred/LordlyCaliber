/* Portable adaptation of accepted Decomp 8012b84a src/lib/func_000E6D90.c.
 * The game routine emits rendering services through an explicit sink. Decode
 * native big-endian fields instead of aliasing them with host structures. */
#include "dialogue_draw_plan.h"
typedef uint8_t u8;
typedef int16_t s16;
typedef int32_t s32;
typedef uint32_t u32;
typedef struct {
    s16 portrait, pointerX, pointerY;
    uint16_t columns, rows, width, height, field18, field1C, textOffset;
    u8 pointerSide, pointerEdge, portraitSide, field42, field43, field44, field46, style,
        portraitAlpha, portraitVariant, field4B, field4F, field50, firstLine, alpha;
    const u8 *textBuffer, *lineFlags;
    uint16_t lineOffsets[96];
} DrawView;
static uint16_t half(const u8 *p) {
    return (uint16_t)((unsigned)p[0] * 256 + p[1]);
}
#define DIALOGUE (&view)
#define CONTROL window[0x91]
#define FLAGS window[0x8a]
#define D_800E7A30 half(window)
#define D_800E7A36 bounds
#define D_801869C8 0x801869c8U
#define CALL(kind, ...)                                                                            \
    do {                                                                                           \
        const int32_t arguments[] = {__VA_ARGS__};                                                 \
        sink(user, kind, arguments, (int)(sizeof(arguments) / sizeof(arguments[0])));              \
    } while (0)
#define EMPTY(kind) sink(user, kind, 0, 0)
#define EMIT(a, b) CALL(LORDLY_DDRAW_RAW, (s32)(a), (s32)(b))
#define func_0004c7d4(...) CALL(LORDLY_DDRAW_TRANSFORM, __VA_ARGS__)
#define func_000e48f0() EMPTY(LORDLY_DDRAW_RESET)
#define func_000e495c(...) CALL(LORDLY_DDRAW_STYLE, __VA_ARGS__)
#define func_000E4C54(...) CALL(LORDLY_DDRAW_REPEAT_FRAME, __VA_ARGS__)
#define func_000E4BE0(...) CALL(LORDLY_DDRAW_FRAME, __VA_ARGS__)
#define func_000E4FE4(...) CALL(LORDLY_DDRAW_SLICE, __VA_ARGS__)
#define func_000E4DC8(...) CALL(LORDLY_DDRAW_POINTER_ATLAS, __VA_ARGS__)
#define func_0004cb6c(...) CALL(LORDLY_DDRAW_RECTANGLE, __VA_ARGS__)
#define func_0004c848(...) CALL(LORDLY_DDRAW_TEXTURE_RECTANGLE, __VA_ARGS__)
#define func_000ead04(...) CALL(LORDLY_DDRAW_PORTRAIT, __VA_ARGS__)
#define func_0004f1e8() EMPTY(LORDLY_DDRAW_TEXT_RESET)
#define func_0004fe04(ignored) CALL(LORDLY_DDRAW_TEXT_FORMAT, format.style, textColor)
#define func_0004ef34(text, x, y) CALL(LORDLY_DDRAW_TEXT, (s32)((text) - payload), x, y)
#define func_00008A74(...) CALL(LORDLY_DDRAW_SCISSOR, __VA_ARGS__)
#define func_000e4930() EMPTY(LORDLY_DDRAW_FINISH)
#define D_8019E198 format
#define D_8019E19A textColor
/* Coordinate expressions retain separate reads of the shared dialogue object. */
#define TEXT_X(x)                                                                                  \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2 ? ((DIALOGUE->style == 0 && DIALOGUE->portraitSide == 0)           \
                                       ? (x) + (DIALOGUE->portrait >= 0 ? 56 : 8)                  \
                                       : (x) + 8)                                                  \
                                : (x) + 7)                                                         \
         : (x))
#define UNTEXT_X(x)                                                                                \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2 ? ((DIALOGUE->style == 0 && DIALOGUE->portraitSide == 0)           \
                                       ? (x) - (DIALOGUE->portrait >= 0 ? 56 : 8)                  \
                                       : (x) - 8)                                                  \
                                : (x) - 7)                                                         \
         : (x))
/* Parenthesising the columns term retains KMC's x+offset evaluation order. */
#define RIGHT_X(x)                                                                                 \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2                                                                    \
                ? (DIALOGUE->style == 0                                                            \
                       ? (DIALOGUE->portraitSide ? (x) + (7 + DIALOGUE->columns * 7)               \
                                                 : (x) + ((DIALOGUE->portrait >= 0 ? 55 : 7) +     \
                                                          DIALOGUE->columns * 7))                  \
                       : (x) + (7 + DIALOGUE->columns * 7))                                        \
                : (x) + (6 + DIALOGUE->columns * 7))                                               \
         : (x) + (-1 + DIALOGUE->columns * 7))
#define UNRIGHT_X(x)                                                                               \
    (!(CONTROL & 4)                                                                                \
         ? (DIALOGUE->style < 2                                                                    \
                ? (DIALOGUE->style == 0                                                            \
                       ? (DIALOGUE->portraitSide ? (x) + (8 - DIALOGUE->columns * 7)               \
                                                 : (x) + ((DIALOGUE->portrait >= 0 ? -40 : 8) -    \
                                                          DIALOGUE->columns * 7))                  \
                       : (x) + (8 - DIALOGUE->columns * 7))                                        \
                : (x) + (9 - DIALOGUE->columns * 7))                                               \
         : (x) + (16 - DIALOGUE->columns * 7))
#define row_spacing(n) (DIALOGUE->style < 2 ? (n) * 17 : (n) * 16)
#define LINE_Y(n)                                                                                  \
    (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (n) * 17 + 5 : (n) * 16 + 7)                          \
                    : ((n) * (DIALOGUE->style < 2 ? 17 : 16)))
#define MARKER_Y(n)                                                                                \
    (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (n) * 17 + 7 : (n) * 16 + 9) : (row_spacing(n) + 2))
#define MARKER_BOTTOM(n)                                                                           \
    (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (n) * 17 + 18 : (n) * 16 + 20) : (row_spacing(n) + 13))
#define bare_marker_source_y(y, n)                                                                 \
    (DIALOGUE->style < 2 ? (y) + (-2 - (n) * 17) : (y) + (-2 - (n) * 16))
#define UNMARKER_Y(y, n)                                                                           \
    (!(CONTROL & 4) ? (DIALOGUE->style < 2 ? (y) + (-7 - (n) * 17) : (y) + (-9 - (n) * 16))        \
                    : bare_marker_source_y((y), (n)))
#define MARKER_SOURCE_X 15    /* TEXT_X followed by UNTEXT_X cancels the offset. */
#define MARKER_SOURCE_Y(n) 11 /* MARKER_BOTTOM followed by UNMARKER_Y cancels row/origin. */
#define BOTTOM_Y(y) ((y) + (!(CONTROL & 4) ? 53 : (DIALOGUE->style < 2 ? 48 : 46)))
#define UNBOTTOM_Y(y) ((y) - (!(CONTROL & 4) ? 38 : (DIALOGUE->style < 2 ? 33 : 31)))
#define PORTRAIT_LEFT (DIALOGUE->portraitSide ? DIALOGUE->columns * 7 + 16 : 8)
#define PORTRAIT_RIGHT (DIALOGUE->portraitSide ? DIALOGUE->columns * 7 + 55 : 47)
#define PORTRAIT_LOCAL(x) (DIALOGUE->portraitSide ? (x) + (-16 - DIALOGUE->columns * 7) : (x) - 8)
/* Draw the current dialogue record through every retail rendering phase.
 * Coordinate expressions preserve the compiler's staged arithmetic and
 * separate reads of shared state; field meanings remain conservative. */
int lordly_dialogue_draw_plan(const u8 window[168], const u8 payload[1144],
                              LordlyDialogueDrawSink sink, void *user) {
    s32 portraitAlpha;
    s32 line;
    DrawView view;
    struct {
        s16 left, top, right, bottom;
    } bounds;
    struct {
        u8 style;
    } format;
    u8 textColor;
    int index;
    if (!window || !payload || !sink)
        return -1;
    bounds.left = (s16)half(window + 6);
    bounds.top = (s16)half(window + 8);
    bounds.right = (s16)half(window + 10);
    bounds.bottom = (s16)half(window + 12);
    view.portrait = half(payload + 0x14);
    view.pointerX = half(payload + 0x30);
    view.pointerY = half(payload + 0x32);
    view.columns = half(payload + 0x1a);
    view.rows = half(payload + 0x1e);
    view.width = half(payload + 0x20);
    view.height = half(payload + 0x22);
    view.field18 = half(payload + 0x18);
    view.field1C = half(payload + 0x1c);
    view.textOffset = half(payload + 0x36);
    view.pointerSide = payload[0x3a];
    view.pointerEdge = payload[0x3b];
    view.portraitSide = payload[0x3d];
    view.field42 = payload[0x42];
    view.field43 = payload[0x43];
    view.field44 = payload[0x44];
    view.field46 = payload[0x46];
    view.style = payload[0x47];
    view.portraitAlpha = payload[0x49];
    view.portraitVariant = payload[0x4a];
    view.field4B = payload[0x4b];
    view.field4F = payload[0x4f];
    view.field50 = payload[0x50];
    view.firstLine = payload[0x54];
    view.alpha = payload[0x56];
    view.textBuffer = payload + 0x178;
    view.lineFlags = payload + 0x57;
    for (index = 0; index < 96; ++index)
        view.lineOffsets[index] = half(payload + 0xb8 + index * 2);
    if (view.field4F && ((unsigned)view.firstLine + view.rows > 96 || view.firstLine >= 96))
        return -1;
    if (view.textOffset >= 768 || (view.field4F && view.lineOffsets[view.firstLine] >= 768))
        return -1;

    func_0004c7d4((s16)DIALOGUE->width, (s16)DIALOGUE->height, D_800E7A36.left, D_800E7A36.top,
                  D_800E7A36.right, D_800E7A36.bottom);
    func_000e48f0();
    if (!(CONTROL & 4)) {
        if (DIALOGUE->style < 2) {
            if (DIALOGUE->alpha == 255)
                func_000e495c(8);
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
            if (DIALOGUE->alpha != 255)
                func_000e48f0();
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
        EMIT(((((D_800E7A36.right * 4) & 0xFFF) << 12) | 0xE4000000) |
                 ((D_800E7A36.bottom * 4) & 0xFFF),
             (((D_800E7A36.left * 4) & 0xFFF) << 12) | ((D_800E7A36.top * 4) & 0xFFF));
        EMIT(0xE1000000, 0);
        EMIT(0xF1000000, 0);
        EMIT(0xE7000000, 0);
        func_000e48f0();
    }
    if (DIALOGUE->portrait >= 0) {
        s32 left, right, coordinate, sourceX;
        portraitAlpha = ((DIALOGUE->alpha + 1) * DIALOGUE->portraitAlpha) >> 8;
        if ((u32)portraitAlpha < 255) {
            func_000e495c(2);
            EMIT(0xFCFFFFFF, 0xFFFCF67B);
            EMIT(0xFA000000, portraitAlpha & 255);
        } else
            func_000e495c(0);
        func_000ead04(DIALOGUE->portrait, DIALOGUE->portraitVariant);
        if (FLAGS & 0x20) {
            left = PORTRAIT_LEFT;
            right = PORTRAIT_RIGHT;
            coordinate = PORTRAIT_RIGHT;
            sourceX = PORTRAIT_LOCAL(coordinate);
            func_0004c848(left, 5, right, 52, sourceX, 0, 0, 47, 0);
        } else
            func_0004cb6c(PORTRAIT_LEFT, 5, PORTRAIT_RIGHT, 52, 0, 0, 0);
        if ((u32)portraitAlpha < 255) {
            EMIT(0xE7000000, 0);
            EMIT(0xFCFFFFFF, 0xFFFCF279);
        }
    }
    if (!(D_800E7A30 & 0x1000) && DIALOGUE->textBuffer[0] != 0) {
        s32 x, y;
        const u8 *text;
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
            if (!(CONTROL & 4))
                y += DIALOGUE->style < 2 ? 5 : 7;
        } else {
            text = DIALOGUE->textBuffer + DIALOGUE->lineOffsets[DIALOGUE->firstLine];
            x = D_800E7A36.left;
            x = TEXT_X(x);
            y = D_800E7A36.top;
            if (!(CONTROL & 4))
                y += DIALOGUE->style < 2 ? 5 : 7;
        }
        func_0004ef34(text, x, y);
    }
    if (DIALOGUE->field42 != 0 && DIALOGUE->field4F == 0) {
        s32 left, top, right, bottom, coordinate;
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
            s32 inputX, screenX, inputY, screenY;
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
        s32 left, top, right, bottom;
        func_000e495c(8);
        func_000E4BE0(0x02093576, 0, 5);
        /* Nine arguments: both source coordinates are on the caller's stack. */
        left = TEXT_X(0);
        top = MARKER_Y(DIALOGUE->field44 + DIALOGUE->field46);
        right = TEXT_X(15);
        bottom = MARKER_BOTTOM(DIALOGUE->field44 + DIALOGUE->field46);
        func_0004c848(left, top, right, bottom, MARKER_SOURCE_X, 0, 0,
                      MARKER_SOURCE_Y(DIALOGUE->field44 + DIALOGUE->field46), 0);
    }
    if (DIALOGUE->field4F != 0) {
        for (line = DIALOGUE->firstLine; line < DIALOGUE->firstLine + DIALOGUE->rows; line++) {
            if (DIALOGUE->lineFlags[line] != 0) {
                s32 left, top, right, bottom;
                func_000e495c(8);
                func_000E4BE0(0x02093576, 0, 5);
                left = TEXT_X(0);
                top = MARKER_Y(line - DIALOGUE->firstLine);
                right = TEXT_X(15);
                bottom = MARKER_BOTTOM(line - DIALOGUE->firstLine);
                func_0004c848(left, top, right, bottom, MARKER_SOURCE_X, 0, 0,
                              MARKER_SOURCE_Y(line - DIALOGUE->firstLine), 0);
            }
        }
    }
    if (DIALOGUE->style == 0 && !(CONTROL & 4)) {
        s32 x, y;
        func_00008A74(0, 0, 320, 240);
        if (DIALOGUE->alpha == 255)
            func_000e495c(8);
        else {
            func_000e495c(2);
            EMIT(0xFA000000, DIALOGUE->alpha | 0xFFFFFF00);
            EMIT(0xFCFF97FF, 0xFF2CFE7F);
        }
        func_000E4DC8(0x02093576, 0, 3);
        x = DIALOGUE->pointerX;
        y = DIALOGUE->pointerY;
        func_0004c848(x, y, x + 7, y + 12, DIALOGUE->pointerSide * 8, DIALOGUE->pointerEdge * 19,
                      DIALOGUE->pointerSide * 8 + 7, DIALOGUE->pointerEdge * 19 + 12, 0);
        func_00008A74(D_800E7A36.left, D_800E7A36.top, D_800E7A36.right, D_800E7A36.bottom);
    }
    func_000e4930();
    return 0;
}
