#ifndef COMBAT_DRAW_H
#define COMBAT_DRAW_H

/* Sixteen-byte records populated by func_002103EC. */
typedef struct CombatDrawVertex {
    short x;
    short y;
    short field_04;
    unsigned short field_06;
    short u;
    short v;
    unsigned char field_0C[4];
} CombatDrawVertex;

extern void func_002103EC(CombatDrawVertex *, int, int, int, int);

#endif
