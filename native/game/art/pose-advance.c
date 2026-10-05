/* Complete Actor pose interpreter, z64 29E218..29E70C, live 8022E9E8.
 * Legacy effects, signed movement and both sound paths retain original order.
 * Registered private states bypass only the fixed-body clamp. Opcode22 extends
 * state transfers to the same signed-halfword domain as the Actor state fields.
 */
typedef unsigned char u8;
typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
#define B(p, n) (*(u8 *)((u8 *)(p) + (n)))
#define H(p, n) (*(s16 *)((u8 *)(p) + (n)))
#define W(p, n) (*(u32 *)((u8 *)(p) + (n)))
#define S(p, n) (*(s32 *)((u8 *)(p) + (n)))
#define F(p, n) (*(float *)((u8 *)(p) + (n)))
extern int lc_registered_pose(s32, s32, s32, u16);
extern int lc_plain_record(s32, s16, s32, u8, u8 *, u8 *, u8 *);
extern int lc_body_record(s32, s32, u8, s16, s16, s32, u8 *, u8 *, u8 *);
extern void lc_fill(void *, int, int);
extern u16 lc_sounds[], lc_sound_pairs[];
extern u8 *lc_roster;
extern float lc_sound_origin[];
extern void lc_project(float *, u32 *, void *);
extern void lc_sound(void *, u16);
extern void lc_linked_sound(void *, s32, s32, u8, u8, u8);
int lc_advance_pose(u8 *actor) {
    u8 op = 1, a, b, c;
    int i, value, key, x;
    u32 projected[2];
    u8 *row, *object, *dst;
    void *sound_context;
    float point[3];
    while (S(actor, 0xf4) <= 0) {
        W(actor, 0xf0)++;
        if (!B(actor, 0x13d))
            op = lc_plain_record(S(actor, 0xe8), H(actor, 0x134), S(actor, 0xf0), B(actor, 0x146),
                                 &a, &b, &c);
        else {
            s16 source = H(actor, 0xea);
            if (((u16)(source - 135) < 2 || source == 161 || source == -1) &&
                (!LC_PRIVATE_POSE_COUNT ||
                 !lc_registered_pose(S(actor, 0xe8), S(actor, 0xec),
                                     B(actor, 0x146) * 2 + H(actor, 0x13a), H(actor, 0x134))))
                H(actor, 0x134) = H(actor, 0x134) < 50 ? 0 : 50;
            op = lc_body_record(S(actor, 0xe8), S(actor, 0xec), B(actor, 0x146), H(actor, 0x13a),
                                H(actor, 0x134), S(actor, 0xf0), &a, &b, &c);
        }
        if (op == 0)
            W(actor, 0xf4) = 2;
        else if (op == 1) {
            W(actor, 0xf8) = a;
            W(actor, 0xf4) = b;
        } else if (op == 21) {
            W(actor, 0xf8) = a + b * 256;
            W(actor, 0xf4) = c;
        } else if (op == 2) {
            F(actor, 0x11c) -= (s8)a;
            F(actor, 0x124) -= (s8)b;
        } else if (op == 3)
            W(actor, 0xf4) = a;
        else if (op == 4)
            W(actor, 0xf0) = a - 1;
        else if (op == 5 || (LC_WIDE_STATE && op == 22)) {
            H(actor, 0x136) = H(actor, 0x134);
            H(actor, 0x134) = a + (op == 22 ? b * 256 : 0);
            W(actor, 0xf0) = -1;
        } else if (op == 12) {
            F(actor, 0x11c) += (s8)a;
            F(actor, 0x120) += (s8)b;
            F(actor, 0x124) += (s8)c;
        } else if (op == 13 || op == 16) {
            dst = actor + (op == 16 ? 16 : 0);
            if (a == 255)
                lc_fill(dst, b, 16);
            else
                B(dst, a) = b;
        } else if ((u32)(op - 17) < 4) {
            sound_context = (void *)(op < 19 ? 0x800eb240 : 0x800eb290);
            key = a * 256 + b;
            if (op & 1)
                value = lc_sounds[key];
            else if (!B(actor, 0x13d)) {
                point[0] = lc_sound_origin[0];
                point[1] = lc_sound_origin[1];
                point[2] = lc_sound_origin[2];
                lc_project(point, projected, actor + 0xa0);
                x = (int)point[0];
                if ((u32)(key - 590) < 171)
                    value = lc_sounds[key + (x < 106 ? 2 : x < 213 ? 1 : 0)];
                else
                    value = lc_sound_pairs[key * 2 + (x < 160 ? 0 : 1)];
            } else {
                row = lc_roster + 0x1c4 + B(actor, 0x147) * 248;
                object = (u8 *)W(row, B(actor, 0x149) * 4);
                if (object)
                    lc_linked_sound(object + 0x44, S(row, 0x48), S(row, 0x4c), a, b, op);
                continue;
            }
            lc_sound(sound_context, value);
        }
    }

    for (i = 0; i < 16; i++) {
        value = B(actor, i) + (s8)B(actor, i + 16);
        B(actor, i) = value < 0 ? 0 : value > 255 ? 255 : value;
    }
    W(actor, 0xf4) -= 2;
    return op;
}
