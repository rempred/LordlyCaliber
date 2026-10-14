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
extern CombatDrawCommand D_801CE980[];

#define HALF(p,o) (*(short *)((u8 *)(p) + (o)))
#define WORD(p,o) (*(int *)((u8 *)(p) + (o)))
#define BYTE(p,o) (*((u8 *)(p) + (o)))
#define MATRIX_CURSOR D_801969B0
#define VERTEX_CURSOR D_80197178
#define PAIR COMBAT_DRAW_PAIR
#define FIELD COMBAT_DRAW_FIELD

/* The range mask implements the same low-byte field for all inputs. */
#define PACK_RGB_BYTE(input, shift) ({ \
    u32 value = (input); \
    u32 mask = 255u; \
    int within = value <= mask; \
    mask |= -(u32)within; \
    (value << (shift)) & (mask << (shift)); \
})

/* Each destination already truncates the coordinate to its field width. */
#define RANGE_COORDINATE(input, bits) ({ \
    u32 coordinate = (input); \
    u32 fieldMask = (1u << (bits)) - 1u; \
    int within = (coordinate >> (bits)) == 0u; \
    fieldMask |= -(u32)within; \
    coordinate & fieldMask; \
})

/* The u16 half-width already fits this mask. Retaining the equivalent range
 * guard keeps x0 live until KMC combine removes the mask; without it the old
 * allocator reverses x0 and remaining-height registers. */
#define RANGE_COORDINATE_SHIFTED_SUM(input, offset, shift) ({ \
    u32 coordinate = (input); \
    u32 coordinateOffset = (offset); \
    u32 fieldMask = 65535u; \
    int within = (coordinate >> 16) == 0u; \
    fieldMask |= -(u32)within; \
    ((u32)(input) + (coordinateOffset & fieldMask)) << (shift); \
})

int func_001F6098(DrawObject *object, DrawImage *secondary, int mode, int amount)
{
    CombatPoseIndexedRecord decoded;
    float matrix[4][4];
    short interpolated[4];
    short interpolation[9];
    struct { float red, green, blue; u8 amount; } blendState;
    int index;
    void *actor;
    DrawPose *pose;
    int poseId, firstId, secondId, variant;
    int red, green, blue;
    int factor, secondaryOffset;
    int redOffset, greenOffset, zX;
    float z, inverseZ;
    float initialRed, finalBlue;
    float currentRed, currentGreen, currentBlue;
    u8 *colors;
    CombatDrawCommand *modeCommand;
    int commandFirst, commandSecond;
    int top, right, flags, accumulated;
    u8 *image;
    DrawImage *header;
    int flip;
    int left, leftU, rightVertex, rightU;
    int bit10, bit8, capacity, strip, nextRow;
    int remaining, nextRemaining, nextAccumulated, nextNormalRow;
    int row, u0, u1;
    int x0, alpha;
    int halfWidth, renderHalfWidth;

    actor = object->actor;
    poseId = object->field4E;
    firstId = WORD(actor, 0x48);
    secondId = WORD(actor, 0x4C);
    pose = (DrawPose *)((u8 *)object + 0x44);
    variant = func_0020C448(actor);
    factor = BYTE(actor, 0xAB) * object->field34 / 255;
    if (factor == 0) {
        return 0;
    }
    PAIR(0xE7000000, 0);
    PAIR(0xE7000000, 0);
    PAIR(0xE3000A01, 0x00100000);
    PAIR(0xE7000000, 0);
    func_00022e90(matrix, *(float *)0x801CE8EC,
                  *(float *)0x801CE8F0, *(float *)0x801CE8F4);
    matrix[3][0] = object->field1C + object->field22 + object->field50;
    matrix[3][1] = object->field1E + object->field24 + object->field52;
    matrix[3][2] = object->field20 + object->field26 + object->field54;
    guMtxF2L(matrix, MATRIX_CURSOR);
    PAIR(0xDA380000, (u32)MATRIX_CURSOR++);
    PAIR(0xDE000000, (u32)D_801CE980);
    if (mode == 0) {
        PAIR(0xFC629403, 0x1F0CFFFD);
    } else {
        PAIR(0xFC62E203, 0x1F0CAFFD);
    }
    if (object->field37) {
        interpolation[0] = object->field37;
        interpolation[1] = object->field38;
        interpolation[2] = object->field39;
        interpolation[3] = object->field3A;
        interpolation[5] = object->field3B;
        interpolation[6] = object->field3C;
        interpolation[7] = object->field3D;
        /* Group-2 placement interface; the two unused input slots stay unwritten. */
        func_0025FD90(object->field36, interpolation, interpolated);
        red = interpolated[0];
        green = interpolated[1];
        blue = interpolated[2];
    } else {
        red = *(short *)0x80220E70;
        green = *(short *)0x80220E72;
        blue = *(short *)0x80220E74;
    }
    secondaryOffset = (u8)amount;
    secondaryOffset = func_00205230(object->field18, poseId) * secondaryOffset / 256;
    red += object->field2E;
    green += object->field30;
    blendState.red = blendState.green = blendState.blue = 0.0f;
    blue += object->field32;
    zX = object->field1C;
    colors = D_801D06F4 + D_801CEAB0 * 6;
    z = object->field20 + zX + 190;
    if (z < 0.0f) z = 0.0f;
    else if (z > 380.0f) z = 380.0f;
    inverseZ = 380.0f - z;
    blendState.red = inverseZ * ((float)colors[3] - (float)colors[0]) / 380.0f + (float)colors[0];
    blendState.green = inverseZ * ((float)colors[4] - (float)colors[1]) / 380.0f + (float)colors[1];
    finalBlue = inverseZ * ((float)colors[5] - (float)colors[2]) / 380.0f + (float)colors[2];
    {
        float first = blendState.red;
        PAIR(0xFB000000, (blendState.blue = finalBlue, ((u32)(u8)(u32)first << 24) | ((u32)(u8)(u32)blendState.green << 16) |
             ((u32)(u8)(u32)blendState.blue << 8) | 1));
    }
    index = 0;
    while (1) {
        bit10 = func_0020C034(actor);
        if (!func_00205484(firstId, secondId, bit10, func_0020BFF8(actor),
                           poseId, index, &decoded)) {
            break;
        }
        if (pose->items[index] != 0) {
            func_00205778(2);
            bit8 = func_0020BFF8(actor);
            image = (u8 *)func_00206BE0(firstId, secondId, &decoded, bit8,
                                  func_0020C034(actor), variant, &blendState.amount);
            func_00205760(2);
            alpha = factor * pose->items[index] / 255;
            currentRed = blendState.red;
            currentGreen = blendState.green;
            currentBlue = blendState.blue;
            if (blendState.amount == 0) blendState.amount = 1;
            {
                int nextRed = (float)(red * blendState.amount) / (currentRed + (float)blendState.amount);
                int nextGreen = (float)(green * blendState.amount) / (currentGreen + (float)blendState.amount);
                int nextBlue = (float)(blue * blendState.amount) / (currentBlue + (float)blendState.amount);
                /* In-range results retain the preceding component. */
                if (nextRed < 0) red = 0;
                else if (nextRed >= 256) red = 255;
                if (nextGreen < 0) green = 0;
                else if (nextGreen >= 256) green = 255;
                if (nextBlue < 0) blue = 0;
                else if (nextBlue >= 256) blue = 255;
            }
            PAIR(0xFA000000, ((u32)red << 24) | (((u32)green & 255) << 16) |
                 (((u32)blue & 255) << 8) | ((u32)alpha & 255));
            header = (DrawImage *)image;
            capacity = 2048 / (int)func_00201E38(header->format, header->width);
            if (header->format != 3) {
                while (1) {}
            }
            func_00201E38(secondary->format, secondary->width);
            remaining = header->height;
            x0 = decoded.record.field_04;
            top = -decoded.record.field_08;
            right = decoded.record.field_0C + x0;
            flags = decoded.record.field_14;
            if (flags & 1) {
                u0 = decoded.record.field_0C - 1;
                u1 = 0;
            } else {
                u0 = 0;
                u1 = decoded.record.field_0C - 1;
            }
            if (func_00043d1c(BYTE(actor, 0x4B), BYTE(actor, 0x4F)) == 2) {
                decoded.record.field_18 *= *(double *)0x801CFE10;
                decoded.record.field_1C *= *(double *)0x801CFE10;
            }
            accumulated = 0;
            guScale(MATRIX_CURSOR, decoded.record.field_18, decoded.record.field_1C, 1.0f);
            PAIR(0xDA380000, (u32)MATRIX_CURSOR++);
            strip = capacity;
            if (remaining < strip) {
                strip = remaining;
            }
            if (!(flags & 2)) {
                row = 0;
            } else {
                row = remaining - strip;
            }
            left = (short)RANGE_COORDINATE(x0, 16);
            {
                int sourceY;
                leftU = (short)u0;
                rightVertex = (short)(right - 1);
                rightU = (short)u1;
                sourceY = -top;
                /* Keep the full row coordinate separate from its narrowed vertex
                   value, with one definition in each branch. The compiler's local
                   allocation and shared corner-call tail depend on these lifetimes. */
                do {
                    if (!(flags & 2)) {
                        int y = top - accumulated;
                        int bottomY, nextY;
                        func_002103EC(VERTEX_CURSOR, left, (short)y, leftU, (short)row);
                        func_002103EC(VERTEX_CURSOR + 1, rightVertex, (short)y, rightU, (short)row);
                        nextY = y + 1;
                        bottomY = (short)(nextY - strip);

                        func_002103EC(VERTEX_CURSOR + 2, left, bottomY, leftU, (short)(row + strip - 1));
                        func_002103EC(VERTEX_CURSOR + 3, rightVertex, bottomY, rightU, (short)(row + strip - 1));
                    } else {
                        int y = top - accumulated;
                        int bottomY, nextY;

                        func_002103EC(VERTEX_CURSOR, left, (short)y, leftU, (short)(row + strip - 1));
                        func_002103EC(VERTEX_CURSOR + 1, rightVertex, (short)y, rightU, (short)(row + strip - 1));
                        nextY = y + 1;
                        bottomY = (short)(nextY - strip);

                        func_002103EC(VERTEX_CURSOR + 2, left, bottomY, leftU, (short)row);
                        func_002103EC(VERTEX_CURSOR + 3, rightVertex, bottomY, rightU, (short)row);
                    }

                    PAIR(0x01004008, (u32)VERTEX_CURSOR);
                    VERTEX_CURSOR += 4;
                    PAIR(0xFD180000 | FIELD(func_00201E9C(header->format, header->width) - 1, 0, 12), (u32)(image + 8));
                    PAIR(0xF5180000 | FIELD((header->width * 2 + 7) >> 3, 9, 9), 0x07000000);
                    PAIR(0xE6000000, 0);
                    PAIR(0xF4000000 | FIELD((u32)row << 2, 0, 12),
                         0x07000000 | FIELD((u32)(header->width - 1) << 2, 12, 12) | FIELD((u32)(row + strip - 1) << 2, 0, 12));
                    PAIR(0xE7000000, 0);
                    PAIR(0xF5180000 | FIELD((header->width * 2 + 7) >> 3, 9, 9), 0);
                    PAIR(0xF2000000 | FIELD((u32)row << 2, 0, 12),
                         FIELD((u32)(header->width - 1) << 2, 12, 12) | FIELD((u32)(row + strip - 1) << 2, 0, 12));
                    PAIR(0xFD880000 | FIELD((func_00201E9C(secondary->format, secondary->width) >> 1) - 1, 0, 12), (u32)((u8 *)secondary + 8));
                    PAIR(0xF5880180 | FIELD((halfWidth = header->width, ((halfWidth >> 1) + 7) >> 3), 9, 9), 0x07000000);
                    PAIR(0xE6000000, 0);
                    PAIR(0xF4000000 | FIELD(RANGE_COORDINATE_SHIFTED_SUM(x0, secondary->width >> 1, 1), 12, 12) | FIELD((u32)(sourceY + (secondary->height >> 1) + secondaryOffset + row) << 2, 0, 12),
                         0x07000000 | FIELD((u32)((x0 + (secondary->width >> 1)) + header->width - 1) << 1, 12, 12) | FIELD((u32)((sourceY + (secondary->height >> 1) + secondaryOffset + row) + strip - 1) << 2, 0, 12));
                    PAIR(0xE7000000, 0);
                    PAIR(0xF5800180 | FIELD((renderHalfWidth = header->width, ((renderHalfWidth >> 1) + 7) >> 3), 9, 9), 0x01000000);
                    PAIR(0xF2000000 | FIELD((u32)(x0 + (secondary->width >> 1)) << 2, 12, 12) | FIELD((u32)(sourceY + (secondary->height >> 1) + secondaryOffset + row) << 2, 0, 12),
                         0x01000000 | FIELD((u32)((x0 + (secondary->width >> 1)) + header->width - 1) << 2, 12, 12) | FIELD((u32)((sourceY + (secondary->height >> 1) + secondaryOffset + row) + strip - 1) << 2, 0, 12));
                    PAIR(0xF2000000 | FIELD((u32)row << 2, 0, 12),
                         0x01000000 | FIELD((u32)(header->width - 1) << 2, 12, 12) | FIELD((u32)(row + strip - 1) << 2, 0, 12));
                    PAIR(0x06000602, 0x00000406);
                    /* Named successors preserve the separate overlap updates;
                       folding strip - 1 across them changes current-toolchain output. */
                    nextAccumulated = accumulated - 1;
                    accumulated = nextAccumulated + strip;
                    if (!(flags & 2)) {
                        nextNormalRow = row - 1;
                        row = nextNormalRow + strip;
                    } else {
                        nextRow = row + 1;
                        nextRow -= strip;
                        row = nextRow < 0 ? 0 : nextRow;
                    }
                    nextRemaining = remaining + 1;
                    remaining = nextRemaining - strip;
                    if (remaining < 2) {
                        break;
                    }
                    if (strip >= remaining) {
                        strip = remaining;
                    }
                } while (1);
            }
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

