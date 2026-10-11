typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;

extern char *D_801CE8BC;  /* deployed-unit service table; units at +0x1C4, stride 0xF8 */
extern u8 *D_8022A974;    /* Director scene block; actor pointers at +0x18 */
extern s8 D_8022AC80;     /* Director scene mode */
extern s16 D_8022A994;    /* current scenario id */

extern s32 func_0020BFF8(char *unit);
extern s32 func_0020C0E8(char *unit);
extern s32 func_0020C32C(char *unit);
extern u8 func_002AD574(char *unit);
extern u8 func_00045e5c(u8 wanted, u8 classId);
extern void func_0002CD70(volatile void *pointer, s32 value, s32 size);
extern void func_002AB810(volatile s32 *slots, char *unit, s32 index, s32 flip, s32 classId, s32 variant, s32 seenLeader);
extern void func_002A9364(s32 slot, s32 startX, s32 startY, s32 targetX, s32 targetY, s32 duration, s32 arg6, s32 arg7);
extern void func_0029E70C(u8 *actor);
extern void func_002AB574(void);

/*
 * Director roster placement. Walks the 20 deployed units, decides per unit
 * whether it takes part (leader/member class filters -1 = any, -2 = any with
 * the extra check, 0 = none, otherwise a class match that is consumed),
 * asks func_002AB810 for the slot list, and places each slot. Names for the
 * unit fields and callee semantics are structural observations.
 */
s32 func_002ABB3C(s32 leaderClass, s32 memberClass, s32 flagC)
{
    /* Retail reads each slot again when passing it to the placement call. */
    volatile s32 slots[4];
    s32 want[2];
    char *unit;
    u8 *actor;
    s32 i;
    s32 j;
    s32 classId;
    s32 skip;
    s32 flip;
    s32 a;
    s32 b;
    u8 classByte;
    s32 mode;
    s32 isNine;
    s32 seenLeader;
    s32 isSix;
    s32 slotIndex;

    want[1] = memberClass;
    want[0] = leaderClass;
    mode = D_8022AC80;
    isNine = mode == -9;
    seenLeader = 0;
    for (i = 0; i < 20; i++) {
        unit = D_801CE8BC + 0x1C4 + i * 0xF8;
        if (*(s32 *)(unit + 0x48) == 0) {
            continue;
        }
        a = want[0];
        b = want[1];
        classId = *(s32 *)(unit + 0x48);
        if (a == -1 && func_0020BFF8(unit)) {
            skip = 0;
        } else if (b == -1 && !func_0020BFF8(unit)) {
            skip = 0;
        } else if (a == -2 && func_0020BFF8(unit) && func_0020C0E8(unit)) {
            skip = 0;
        } else if (b == -2 && !func_0020BFF8(unit) && func_0020C0E8(unit)) {
            skip = 0;
        } else if (a == 0 && func_0020BFF8(unit)) {
            skip = 1;
        } else if (b == 0 && !func_0020BFF8(unit)) {
            skip = 1;
        } else {
            classByte = classId;
            if (func_00045e5c(a, classByte)) {
                want[0] = 0;
                skip = 0;
            } else if (func_00045e5c(b, classByte)) {
                want[1] = 0;
                skip = 0;
            } else {
                skip = 1;
            }
        }
        if (skip == 1) {
            continue;
        }
        if ((D_8022AC80 == -3 || D_8022AC80 == -10) && func_002AD574(unit) == 1) {
            continue;
        }
        /* Retail retains this 28-step loop. The dead assignment prevents KMC
         * from reversing it to a count-down; its original purpose is unknown. */
        for (j = 0; j < 28; j++) {
            flip = j;
        }
        flip = *(s32 *)(unit + 0x58) < 4;
        func_0002CD70(slots, 0xFF, 0x10);
        func_002AB810(slots, unit, i, flip, *(s32 *)(unit + 0x48), *(s32 *)(unit + 0x4C), seenLeader);
        if (*(s32 *)(unit + 0x48) == 0x87) {
            seenLeader = 1;
        }
        /* Separate arms preserve the constant's KMC allocation priority.
         * Combining all six cases into one test swaps saved registers. */
        if (D_8022A994 == 0x3D9 || D_8022A994 == 0x21F || D_8022A994 == 0xC6 || D_8022A994 == 0xF5) {
            flip = 1 - flip;
        } else if (D_8022A994 == 0x1E4 || D_8022A994 == 0x1E5) {
            flip = 1 - flip;
        }
        if (slots[0] == -1) continue;
        /* Compute this in the outer body so KMC keeps the saved mode live. */
        isSix = mode == -6;
        slotIndex = 0;
        do {
            /* A separate initial value keeps KMC from hoisting the flag
             * comparison. Assigning in the condition retains the direct
             * failure edge to default placement. */
            s32 allow = 0;
            if (func_0020C32C(unit) && (!func_0020BFF8(unit) || !func_0020C0E8(unit))) {
                actor = *(u8 **)(D_8022A974 + slots[slotIndex] * 4 + 0x18);
                func_002A9364(slots[slotIndex], -1, -1, *(s16 *)(actor + 0x138) + 0x1C, -1, -1, 0, 1);
                func_0029E70C(actor);
            } else if ((isNine || isSix) && (allow = ((flip == 1) & (flagC == 0))) != 0) {
                actor = *(u8 **)(D_8022A974 + slots[slotIndex] * 4 + 0x18);
                func_002A9364(slots[slotIndex], -1, -1, *(s16 *)(actor + 0x138) + 0x1A, -1, -1, 0, 1);
                func_0029E70C(actor);
            } else {
                func_002A9364(slots[slotIndex], -1, -1, -1, -1, -1, 0, 1);
            }
            slotIndex++;
        } while (slots[slotIndex] != -1);
    }
    func_002AB574();
    return 0;
}
