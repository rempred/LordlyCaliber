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
typedef struct DrawScene {
    u8 unknown00[0x6080];
    int field6080;
} DrawScene;
extern DrawScene *D_801CE8BC;
/* The decoder bounds the indexed trailing bytes of this interior view. */
typedef struct DrawPose {
    u8 unknown00[0x12];
    u8 items[1];
} DrawPose;

extern void func_00022e90(float [4][4], float, float, float);
extern u8 func_00043d1c(int, int);
extern void *func_001F0A9C(int);
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

int func_001F5654(void *object)
{
    CombatPoseIndexedRecord decoded;
    float matrix[4][4];
    int index;
    void *actor;
    DrawPose *pose;
    int poseId, firstId, secondId, variant;
    u8 factor;
    int top, accumulated;
    u8 *image;
    DrawImage *header;
    int flip;
    int left, leftU, rightVertex, rightU;
    int bit10, bit8, capacity, strip, nextRow;
    int remaining, nextRemaining, nextAccumulated, nextNormalRow;
    int row, u0, u1, right;
    int x0, flags, alpha;
    u8 *drawObject;

    actor = *(void **)((u8 *)object + 0x40);
    poseId = HALF(object, 0x4E);
    firstId = WORD(actor, 0x48);
    secondId = WORD(actor, 0x4C);
    variant = func_0020C448(actor);
    factor = BYTE(actor, 0xAB) * HALF(object, 0x34) / 255;
    drawObject = (u8 *)object;
    object = (u8 *)object + 0x44;
    pose = (DrawPose *)object;
    if (factor == 0) {
        return 0;
    }
    PAIR(0xE7000000, 0);
    PAIR(0xE7000000, 0);
    PAIR(0xE3000A01, 0x00100000);
    PAIR(0xE7000000, 0);
    func_00022e90(matrix, *(float *)0x801CE8EC,
                  *(float *)0x801CE8F0, *(float *)0x801CE8F4);
    index = 0;
    matrix[3][0] = HALF(drawObject, 0x1C) + HALF(drawObject, 0x22) + HALF(drawObject, 0x50);
    matrix[3][1] = HALF(drawObject, 0x1E) + HALF(drawObject, 0x24) + HALF(drawObject, 0x52);
    matrix[3][2] = HALF(drawObject, 0x20) + HALF(drawObject, 0x26) + HALF(drawObject, 0x54);
    guMtxF2L(matrix, MATRIX_CURSOR);
    PAIR(0xDA380000, (u32)MATRIX_CURSOR++);
    PAIR(0xDE000000, (u32)D_801CE980);
    PAIR(0xFD900000, (u32)func_001F0A9C(0x00322172) + 8);

    PAIR(0xF59001F0, 0x07010040);
    PAIR(0xE6000000, 0);
    PAIR(0xF3000000, 0x0703F800);
    PAIR(0xE7000000, 0);
    PAIR(0xF58003F0, 0x01010040);
    PAIR(0xF2000000, 0x0103C03C);
    PAIR(0xF2000000 | FIELD((0u - (D_801CE8BC->field6080 & 15)) << 2, 0, 12),
         0x01040000 | FIELD((0u - ((D_801CE8BC->field6080 & 15) + 16)) << 2, 0, 12));
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
                                  func_0020C034(actor), variant, 0);
            func_00205760(2);
            header = (DrawImage *)image;
            capacity = 3840 / (int)func_00201E38(header->format, header->width);
            if (header->format != 3) {
                while (1) {}
            }
            alpha = factor * pose->items[index] / 255;
            PAIR(0xFA000000, 0x80FFFF00 | (alpha & 255));
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
                decoded.record.field_18 *= *(double *)0x801CFE08;
                decoded.record.field_1C *= *(double *)0x801CFE08;
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
            flags &= 2;
            flip = flags;
            left = (short)x0;
            leftU = (short)u0;
            rightVertex = (short)(right - 1);
            rightU = (short)u1;
            /* Keep the full row coordinate separate from its narrowed vertex
               value, with one definition in each branch. The compiler's local
               allocation and shared corner-call tail depend on these lifetimes. */
            do {
                if (!flip) {
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
                PAIR(0x06000602, 0x00000406);
                /* Named successors preserve the separate overlap updates;
                   folding strip - 1 across them changes current-toolchain output. */
                nextAccumulated = accumulated - 1;
                accumulated = nextAccumulated + strip;
                if (!flip) {
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

