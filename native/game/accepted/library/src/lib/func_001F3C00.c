#include "game/combat_draw.h"
#include "game/combat_draw_commands.h"
#include "game/combat_pose_record.h"

typedef unsigned char u8;
typedef struct DrawImage {
    u8 field00, field01, field02, format;
    u16 width, height;
} DrawImage;
typedef union DrawMatrix {
    u32 words[16];
    double alignment;
} DrawMatrix;
/* The decoder bounds the indexed trailing bytes of this interior view. */
typedef struct DrawPose {
    u8 unknown00[0x12];
    u8 items[1];
} DrawPose;

typedef struct DrawObject {
    u8 unknown00[0x18];
    int field18;
    short field1C, field1E, field20, field22, field24, field26;
    short field28, field2A, field2C, field2E, field30, field32, field34;
    u8 field36, field37, field38, field39, field3A, field3B, field3C, field3D;
    u8 unknown3E[2];
    void *actor;
    u8 unknown44[0xA];
    short field4E, field50, field52, field54;
} DrawObject;
extern void func_00022e90(float [4][4], float, float, float);
extern u8 func_00043d1c(int, int);
extern void func_0025FD90(short, short *, short *);
extern int func_00205230(int, int);
extern u8 *D_801D06F4;
extern int D_801CEAB0;
extern u32 func_00201E38(int, u32);
extern u32 func_00201E9C(int, u32);
extern int func_00205484(int, int, int, int, int, u32, CombatPoseIndexedRecord *);
extern void func_00205760(int);
extern void func_00205778(int);
extern void *func_00206BE0(int, int, CombatPoseIndexedRecord *, int, int, int, u8 *);
extern int func_0020BFF8(void *);
extern int func_0020C034(void *);
extern u16 func_0020C448(void *);
extern void guMtxF2L(float [4][4], DrawMatrix *);
extern void guScale(DrawMatrix *, float, float, float);
extern DrawMatrix *D_801969B0;
extern CombatDrawVertex *D_80197178;
extern CombatDrawCommand D_801CE930[];

/* The first load word is staged before the preceding sync zero store. */
static __inline__ u32 merge_tile_line(u32 line, u32 tile) { line |= tile; return line; }

static __inline__ u32 tile7_end(u32 end) { end |= 0x07000000u; return end; }
static __inline__ u32 load_width_field(u32 width, int scale) { width--; width *= scale; width &= 0xFFF; width <<= 12; return width; }

static __inline__ u32 line_field(u32 width) { width += 7; width >>= 3; width &= 0x1FF; width <<= 9; return width; }

#define HALF(p,o) (*(short *)((u8 *)(p) + (o)))
#define WORD(p,o) (*(int *)((u8 *)(p) + (o)))
#define BYTE(p,o) (*((u8 *)(p) + (o)))
#define MATRIX_CURSOR D_801969B0
#define VERTEX_CURSOR D_80197178
#define PAIR COMBAT_DRAW_PAIR
#define ZERO_FORMAT_TILE_PAIR(a, b) { \
    CombatDrawCommand *offsetCommand = D_800E9BA0++; \
    offsetCommand->first = (a); \
    offsetCommand->second = (b); \
}
#define FIELD COMBAT_DRAW_FIELD

extern u8 func_00205794(void);
extern int func_00201D8C(u32);
extern u32 func_00201E08(int, u32, u32);
extern void *func_00206888(int, CombatPoseIndexedRecord *, u32, u8 *);
extern void *func_00206340(int, int, CombatPoseIndexedRecord *, int, int, int, u8 *);
extern void *D_801CE8BC;
extern CombatDrawCommand D_801CEA38[], D_801CEA48[], D_801CEA58[], D_801CEA68[];
extern CombatDrawCommand D_801CEA78[], D_801CEA88[], D_801CEA98[];
#define WIDTH(p) func_00201E9C((p)->format, (p)->width)
/* KMC 2.7.2 counts these once-only scopes when weighting register references.
 * The selected five existing WIDTH producers retain the companion address in
 * the retail register without changing the calls or their values. Placement is
 * significant: scoping the zero-format tile-width producer changes scheduling. */
#define COMPANION_WIDTH(p) ({ u32 widthValue; do { widthValue = WIDTH(p); } while (0); widthValue; })

int func_001F3C00(void *input, int setup, int mode,
                  DrawImage *optional, int xOffset, int yOffset)
{
    CombatPoseIndexedRecord decoded;
    float matrix[4][4];
    short interpolated[4];
    short interpolation[9];
    struct { float red, green, blue; u8 amount; } blend;
    int index;
    DrawObject *object;
    void *actor;
    DrawPose *pose;
    int poseId, firstId, secondId;
    int red, green, blue, factor;
    int variant;
    CombatDrawCommand *stateA, *stateB, *currentState;
    int remaining, left, top, flags, accumulated;
    u8 *image;
    DrawImage *header, *companion;
    int leftVertex, leftU, rightVertex, rightU, tileBytes;
    int bit10, bit8, alpha, stride, capacity, right;
    u8 saved;
    short u0, u1;
    int strip, row, colorMask;
    int nextAccumulated, nextNormalRow, nextRemaining;

    object = (DrawObject *)input;
    actor = object->actor;
    poseId = object->field4E;
    firstId = WORD(actor, 0x48);
    secondId = WORD(actor, 0x4C);
    pose = (DrawPose *)((u8 *)object + 0x44);
    variant = func_0020C448(actor);
    currentState = 0;
    factor = BYTE(actor, 0xAB) * object->field34 / 255;
    if (factor == 0) return 0;
    PAIR(0xE7000000, 0);
    PAIR(0xE7000000, 0);
    PAIR(0xE3000A01, 0x00100000);
    PAIR(0xE7000000, 0);
    if (setup) {
        func_00022e90(matrix, *(float *)0x801CE8EC, *(float *)0x801CE8F0, *(float *)0x801CE8F4);
    } else {
        func_00022e90(matrix, 0.0f, 0.0f, 0.0f);
    }
    matrix[3][0] = object->field1C + object->field22 + HALF(pose, 0x0C);
    matrix[3][1] = object->field1E + object->field24 + HALF(pose, 0x0E);
    matrix[3][2] = object->field20 + object->field26 + HALF(pose, 0x10);
    guMtxF2L(matrix, MATRIX_CURSOR);
    PAIR(0xDA380000, (u32)MATRIX_CURSOR++);
    PAIR(0xDE000000, (u32)D_801CE930);
    if (object->field37) {
        interpolation[0] = object->field37;
        interpolation[1] = object->field38;
        interpolation[2] = object->field39;
        interpolation[3] = object->field3A;
        interpolation[5] = object->field3B;
        interpolation[6] = object->field3C;
        interpolation[7] = object->field3D;
        /* The group-2 interpolation interface leaves its unused slots unwritten. */
        func_0025FD90(object->field36, interpolation, interpolated);
        red = interpolated[0]; green = interpolated[1]; blue = interpolated[2];
    } else {
        red = *(short *)0x80220E70;
        green = *(short *)0x80220E72;
        blue = *(short *)0x80220E74;
    }
    if (firstId == 0x100) colorMask = *(u8 *)0x801D082C & 0x40;
    else colorMask = *(u8 *)0x801D082C & 0x20;
    if (colorMask) {
        red = red * *(u8 *)0x801D0828 / 255;
        green = green * *(u8 *)0x801D0829 / 255;
        blue = blue * *(u8 *)0x801D082A / 255;
        factor = factor * *(u8 *)0x801D082B / 255;
    }
    blend.red = blend.green = blend.blue = 0.0f;
    PAIR(0xE200001C, 0x0C184240);
    if (optional) {
        int counter = WORD(D_801CE8BC, 0x6080);
        int width = WIDTH(optional);
        int end = WIDTH(optional) - 1;
        int maskWidth = func_00201D8C(optional->width);
        func_001FD56C((u8 *)optional + 8, 0x180, 0, optional->field02,
                       optional->format, width, optional->height, 0, 0, end,
                       optional->height - 1, 0, 0, 0, maskWidth,
                       func_00201D8C(optional->height), 0, 0);
        PAIR(0xF2000000 | FIELD((-counter) % (optional->width * 4), 12, 12) |
             FIELD((-counter) % (optional->height * 4), 0, 12),
             FIELD(optional->width * 4 - counter % (optional->width * 4), 12, 12) |
             FIELD(optional->height * 4 - counter % (optional->height * 4), 0, 12));
    }
    if (WORD(actor, 0xA0)) {
        stateA = D_801CEA38;
        stateB = D_801CEA48;
        PAIR(0xFB000000, ((u32)BYTE(object, 0x2F) << 24) |
             ((u32)BYTE(object, 0x31) << 16) | ((u32)BYTE(object, 0x33) << 8) | BYTE(object, 0x35));
    } else if ((mode == 0) & (optional == 0)) {
        stateA = D_801CEA58;
        stateB = D_801CEA68;
    } else if ((mode == 0) & (optional != 0)) {
        stateA = D_801CEA78;
        stateB = stateA;
    } else {
        float z, inverseZ, finalBlue;
        int zX, redOffset, greenOffset;
        u8 *colors;
        redOffset = red + object->field2E;
        greenOffset = green + object->field30;
        red = redOffset;
        green = greenOffset;
        blue += object->field32;
        zX = object->field1C;
        colors = D_801D06F4 + D_801CEAB0 * 6;
        z = object->field20 + zX + 190;
        if (z < 0.0f) z = 0.0f;
        else if (z > 380.0f) z = 380.0f;
        inverseZ = 380.0f - z;
        blend.red = inverseZ * ((float)colors[3] - (float)colors[0]) / 380.0f + (float)colors[0];
        blend.green = inverseZ * ((float)colors[4] - (float)colors[1]) / 380.0f + (float)colors[1];
        finalBlue = inverseZ * ((float)colors[5] - (float)colors[2]) / 380.0f + (float)colors[2];
        {
            float first = blend.red;
            PAIR(0xFB000000, (blend.blue = finalBlue, ((u32)first << 24) |
                 (((u32)blend.green & 255) << 16) | (((u32)blend.blue & 255) << 8) | 1));
        }
        PAIR(0xE200001C, 0x0C184240);
        stateA = D_801CEA88;
        stateB = D_801CEA98;
    }
    index = 0;
    while (1) {
        companion = 0;
        if (pose->items[index] != 0) {
        if (firstId == 0x100) {
            saved = func_00205794();
            if (!func_00205608(WORD(actor, 0x50), poseId, index, &decoded.record)) break;
            if (optional) {
                func_00205778(2);
                image = func_00206888(WORD(actor, 0x50), &decoded, WORD(actor, 0x4C), &blend.amount);
                func_00205760(saved & 255);
            } else {
                image = func_00206888(WORD(actor, 0x50), &decoded, WORD(actor, 0x4C), &blend.amount);
            }
        } else if ((void *)WORD(actor, 0x18) == object || (void *)WORD(actor, 0x1C) == object) {
            if (!func_00205608(object->field18, poseId, index, &decoded.record)) break;
            image = func_00206888(object->field18, &decoded, 0, &blend.amount);
        } else {
            bit10 = func_0020C034(actor);
            if (!func_00205484(firstId, secondId, bit10, func_0020BFF8(actor), poseId, index, &decoded)) break;
            bit8 = func_0020BFF8(actor);
            image = func_00206340(firstId, secondId, &decoded, bit8, func_0020C034(actor), variant, &blend.amount);
        }
        if (!image) { while (1) {} }
        alpha = factor * pose->items[index] / 255;
        if (!WORD(actor, 0xA0)) {
            int nextRed, nextGreen, nextBlue;
            float currentRed, currentGreen, currentBlue;
            currentRed = blend.red;
            currentGreen = blend.green;
            currentBlue = blend.blue;
            if (!blend.amount) blend.amount = 1;
            nextRed = (float)(red * blend.amount) / (currentRed + (float)blend.amount);
            nextGreen = (float)(green * blend.amount) / (currentGreen + (float)blend.amount);
            nextBlue = (float)(blue * blend.amount) / (currentBlue + (float)blend.amount);
            /* In-range results retain the preceding component. */
            if (nextRed < 0) red = 0; else if (nextRed >= 256) red = 255;
            if (nextGreen < 0) green = 0; else if (nextGreen >= 256) green = 255;
            if (nextBlue < 0) blue = 0; else if (nextBlue >= 256) blue = 255;
        } else if (((BYTE(image, 2) == 4) & (mode == 0)) && (func_00205794() & 1)) {
            red = 255; green = 200; blue = 130;
        }
        PAIR(0xFA000000, ((u32)red << 24) | (((u32)green & 255) << 16) | (((u32)blue & 255) << 8) | (alpha & 255));
        header = (DrawImage *)image;
        stride = func_00201E38(header->format, header->width);
        if (*(u16 *)((u8 *)header + 2) == 2) {
            /* Stage the actual height, byte extent, and resulting image address. */
            {
                int imageHeight = header->height;
                int payloadBytes = stride * imageHeight;
                companion = (DrawImage *)(image + 8 + payloadBytes);
            }
            capacity = 4096 / (int)(stride + func_00201E38(companion->format, companion->width));
            if (currentState != stateB) {
                currentState = stateB;
                PAIR(0xDE000000, (u32)currentState);
            }
        } else if (optional) {
            func_00201E08(optional->format, optional->width, optional->height);
            capacity = 2048 / stride;
            if (currentState != stateB) {
                currentState = stateB;
                PAIR(0xDE000000, (u32)currentState);
            }
        } else {
            capacity = 4096 / stride;
            if (currentState != stateA) {
                currentState = stateA;
                PAIR(0xDE000000, (u32)currentState);
            }
        }
        remaining = header->height;
        left = decoded.record.field_04 + xOffset;
        right = decoded.record.field_0C + decoded.record.field_04 + xOffset;
        top = yOffset - decoded.record.field_08;
        if (BYTE(object, 0x15) & 1) {
            left = -right;
            right = -(decoded.record.field_04 + xOffset);
        }
        flags = BYTE(object, 0x15) ^ (decoded.record.field_14 & 3);
        if (flags & 1) { u0 = decoded.record.field_0C - 1; u1 = 0; }
        else { u0 = 0; u1 = decoded.record.field_0C - 1; }
        if (func_00043d1c(BYTE(actor, 0x4B), BYTE(actor, 0x4F)) == 2) {
            decoded.record.field_18 *= *(double *)0x801CFE00;
            decoded.record.field_1C *= *(double *)0x801CFE00;
        }
        accumulated = 0;
        guScale(MATRIX_CURSOR, decoded.record.field_18, decoded.record.field_1C, 1.0f);
        strip = capacity;
        PAIR(0xDA380000, (u32)MATRIX_CURSOR++);
        if (remaining < strip) strip = remaining;
        row = 0;
        if (flags & 2) row = remaining - strip;
        leftVertex = (short)left;
        leftU = (short)u0;
        rightVertex = (short)(right - 1);
        rightU = (short)u1;
        tileBytes = capacity * stride;
next_row:
        {
            CombatDrawCommand *sizeCommand;
            u32 sizeWidth, sizeEnd;
            if (!(flags & 2)) {
                int y = top - accumulated;
                int nextY, bottomY;
                func_002103EC(VERTEX_CURSOR, leftVertex, (short)y, leftU, (short)row);
                func_002103EC(VERTEX_CURSOR + 1, rightVertex, (short)y, rightU, (short)row);
                nextY = y + 1;
                bottomY = (short)(nextY - strip);
                func_002103EC(VERTEX_CURSOR + 2, leftVertex, bottomY, leftU, (short)(row + strip - 1));
                func_002103EC(VERTEX_CURSOR + 3, rightVertex, bottomY, rightU, (short)(row + strip - 1));
            } else {
                int y = top - accumulated;
                int nextY, bottomY;
                func_002103EC(VERTEX_CURSOR, leftVertex, (short)y, leftU, (short)(row + strip - 1));
                func_002103EC(VERTEX_CURSOR + 1, rightVertex, (short)y, rightU, (short)(row + strip - 1));
                nextY = y + 1;
                bottomY = (short)(nextY - strip);
                func_002103EC(VERTEX_CURSOR + 2, leftVertex, bottomY, leftU, (short)row);
                func_002103EC(VERTEX_CURSOR + 3, rightVertex, bottomY, rightU, (short)row);
            }
            PAIR(0x01004008, (u32)VERTEX_CURSOR);
            VERTEX_CURSOR += 4;
            if ((companion == 0) & (optional == 0)) {
                int width = WIDTH(header);
                func_001FCB28(image + 8, header->field02, header->format, width, strip,
                               0, row, WIDTH(header) - 1, row + strip - 1, 0, 0, 0, 0, 0, 0, 0);
                goto triangles;
            } else if (optional) {
                u16 optionalEndBits;
                PAIR(0xFD180000 | FIELD(WIDTH(header) - 1, 0, 12), (u32)(image + 8));
                PAIR(0xF5180000 | line_field(WIDTH(header) * 2), 0x07000000);
                PAIR(0xE6000000, 0);
                PAIR(0xF4000000 | FIELD(row * 4, 0, 12),
                     0x07000000 | FIELD((WIDTH(header) - 1) * 4, 12, 12) | (optionalEndBits = FIELD((row + strip - 1) * 4, 0, 12)));
                PAIR(0xE7000000, 0);
                PAIR(0xF5180000 | line_field(WIDTH(header) * 2), 0x01000000);
                sizeCommand = D_800E9BA0;
                D_800E9BA0 = sizeCommand + 1;
                sizeCommand->first = 0xF2000000 | FIELD(row * 4, 0, 12);
                sizeWidth = FIELD((WIDTH(header) - 1) * 4, 12, 12);
                sizeEnd = 0x01000000 | optionalEndBits;
            } else {
                int rowField, endField, tileOffset;
                /* This cache holds only the masked 12-bit end coordinate. */
                u16 primaryEnd;
                CombatDrawCommand *companionCommand;
                CombatDrawCommand *companionSizeCommand;
                u32 companionTile, companionSizeWord;
                if (companion->format == 0) {
                    CombatDrawCommand *loadSync;
                    PAIR(0xFD880000 | FIELD((COMPANION_WIDTH(companion) >> 1) - 1, 0, 12), (u32)((u8 *)companion + 8));
                    ZERO_FORMAT_TILE_PAIR(FIELD(((WIDTH(companion) >> 1) + 7) >> 3, 9, 9) | ({
                        u32 tileWord;
                        tileOffset = tileBytes / 8 & 511;
                        tileWord = 0xF5880000u;
                        loadSync = D_800E9BA0;
                        tileWord = tileOffset | tileWord;
                        tileWord;
                    }), 0x07000000);
                    D_800E9BA0 = loadSync + 1;
                    loadSync->first = 0xE6000000;
                    PAIR(({ u32 firstWord = 0xF4000000 | ({ do { rowField = row * 4 & 0xFFF; } while (0); rowField; }); loadSync->second = 0; firstWord; }), load_width_field(COMPANION_WIDTH(companion), 2) | tile7_end((endField = (row + strip - 1) * 4 & 0xFFF)));
                    companionCommand = D_800E9BA0;
                    D_800E9BA0 = companionCommand + 1;
                    companionCommand->first = 0xE7000000;
                    companionCommand->second = 0;
                    ++D_800E9BA0;
                    {
                        u32 renderLineWord, renderTileBase;
                        companionTile = COMPANION_WIDTH(companion);
                        companionTile = ((companionTile >> 1) + 7) >> 3;
                        companionTile &= 511;
                        renderLineWord = companionTile << 9;
                        /* This stage preserves KMC scheduling of the tile-base reload. */
                        renderTileBase = tileBytes / 8 & 511;
                        companionTile = 0xF5800000u;
                        companionSizeCommand = D_800E9BA0;
                        companionTile = renderTileBase | companionTile;
                        companionTile = merge_tile_line(renderLineWord, companionTile);
                    }
                } else {
                    CombatDrawCommand *defaultSync;
                    PAIR(0xFD880000 | FIELD(COMPANION_WIDTH(companion) - 1, 0, 12), (u32)((u8 *)companion + 8));
                    PAIR(FIELD((WIDTH(companion) + 7) >> 3, 9, 9) | (tileOffset = (tileBytes / 8 & 511) | 0xF5880000), 0x07000000);
                    defaultSync = D_800E9BA0++;
                    defaultSync->first = 0xE6000000;
                    PAIR(({ u32 firstWord = 0xF4000000 | ({ do { rowField = row * 4 & 0xFFF; } while (0); rowField; }); defaultSync->second = 0; firstWord; }), load_width_field(COMPANION_WIDTH(companion), 4) | tile7_end((endField = (row + strip - 1) * 4 & 0xFFF)));
                    companionCommand = D_800E9BA0;
                    D_800E9BA0 = companionCommand + 1;
                    companionCommand->first = 0xE7000000;
                    companionCommand->second = 0;
                    ++D_800E9BA0;
                    companionTile = WIDTH(companion);
                    companionTile = ((companionTile + 7) >> 3) & 511;
                    companionTile <<= 9;
                    tileOffset = (tileBytes / 8 & 511) | 0xF5880000;
                    companionSizeCommand = D_800E9BA0;
                    /* This once-only merge scope raises the real tile value
                     * above strip in KMC allocation priority. Together with the
                     * width scopes it preserves the retail strip register and
                     * initial min sequence; no extra operation is emitted. */
                    do { companionTile |= tileOffset; } while (0);
                }
                companionCommand[1].first = companionTile;
                /* Keep the zero store before the next command-cursor update. */
                do { companionCommand[1].second = 0; } while (0);
                D_800E9BA0 = companionSizeCommand + 1;
                /* KMC loop notes retain the retail first-word scheduling. */
                do {
                    companionSizeWord = 0xF2000000u;
                    companionSizeWord |= rowField;
                    companionSizeCommand->first = companionSizeWord;
                } while (0);
                rowField = row * 4;
                companionSizeCommand->second = FIELD((WIDTH(companion) - 1) * 4, 12, 12) | endField;
                PAIR(0xFD100000 | FIELD(WIDTH(header) - 1, 0, 12), (u32)(image + 8));
                rowField &= 0xFFF;
                PAIR(0xF5100000 | line_field(WIDTH(header) * 2), 0x07000000);
                PAIR(0xE6000000, 0);
                PAIR(0xF4000000 | rowField, load_width_field(WIDTH(header), 4) | tile7_end((primaryEnd = (row + strip - 1) * 4 & 0xFFF)));
                PAIR(0xE7000000, 0);
                PAIR(0xF5100000 | line_field(WIDTH(header) * 2), 0x01000000);
                sizeCommand = D_800E9BA0;
                D_800E9BA0 = sizeCommand + 1;
                /* Preserve the same packet boundary for the primary size word. */
                do { sizeCommand->first = (rowField |= 0xF2000000); } while (0);
                sizeWidth = FIELD((WIDTH(header) - 1) * 4, 12, 12);
                sizeEnd = primaryEnd | 0x01000000;
            }
            sizeCommand->second = sizeWidth | sizeEnd;
triangles:
            PAIR(0x06000602, 0x00000406);
            nextAccumulated = accumulated - 1;
            accumulated = nextAccumulated + strip;
            if (!(flags & 2)) {
                nextNormalRow = row - 1;
                row = nextNormalRow + strip;
            }
            else { int next = row + 1; next -= strip; row = next < 0 ? 0 : next; }
            nextRemaining = remaining + 1;
            remaining = nextRemaining - strip;
            if (remaining < 2) goto rows_done;
            if (strip >= remaining) strip = remaining;
            goto next_row;
        }
rows_done:
        PAIR(0xD8380002, 0x40);
        }
        index++;
    }
    PAIR(0xD8380002, 0x40);
    PAIR(0xE7000000, 0);
    PAIR(0xE7000000, 0);
    PAIR(0xE3000A01, 0);
    PAIR(0xE7000000, 0);
    return 0;
}