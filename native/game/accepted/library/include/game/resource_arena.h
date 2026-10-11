#ifndef OB64_GAME_RESOURCE_ARENA_H
#define OB64_GAME_RESOURCE_ARENA_H

#include "common/types.h"

/* Early-boot arena headers are 0x20 bytes. These names describe observed
 * list/tree links and extent arithmetic; offset 0x1C remains unidentified. */
typedef struct ArenaNode {
    struct ArenaNode *prev;           /* 0x00 */
    struct ArenaNode *next;           /* 0x04 */
    struct ArenaNode **tree_slot;     /* 0x08 */
    struct ArenaNode *left;           /* 0x0C */
    struct ArenaNode *right;          /* 0x10 */
    u32 used;                        /* 0x14 */
    u32 available;                   /* 0x18 */
    u32 field1C;                     /* 0x1C */
} ArenaNode;

typedef struct ArenaRecord {
    ArenaNode *base;
    ArenaNode *end;
    ArenaNode *root;
} ArenaRecord;

extern ArenaRecord D_800AEDB0[];
extern u16 D_800AEDE0;
extern u16 D_800AEDE2;
extern u32 D_800C4818;

/* Existing assembly helpers, named by their verified ROM entries. */
extern void func_00001D50(ArenaNode *node);
extern void func_00001DE8(ArenaNode **root, ArenaNode *node);
extern ArenaNode *func_00001E3C(ArenaNode *root, u32 bytes);
extern void func_00001E74(void);
extern s32 func_00001F9C(void *pointer);

#endif
