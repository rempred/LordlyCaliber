#include "game/resource_arena.h"

extern s32 resource_find_arena_index(void *pointer);
extern void func_00023940(const char *message, ...);
extern const char D_800ADD98[];
extern const char D_800ADDCC[];
extern const char D_800ADE1C[];

/* Complete reconstructed source for the reviewed three-body owner.
 * Neutral static helper names identify the admitted body addresses; their
 * reconstructed LOCAL binding does not establish original reachability. */
s32 resource_ptr_validate(void *pointer)
{
    s32 arena_index = resource_find_arena_index(pointer);
    ArenaNode *node;
    ArenaNode *previous;
    ArenaNode *next;
    ArenaNode *base;
    ArenaNode *end;
    const char *message;

    if ((u32)pointer & 0xF) {
        /* The common call receives the original pointer on this failure;
         * every later failure receives its adjusted header instead. */
        node = (ArenaNode *)pointer;
        message = D_800ADD98;
        goto failure;
    }

    node = (ArenaNode *)((u32)pointer - 0x20U);
    base = D_800AEDB0[arena_index].base;
    if ((u32)node < (u32)base)
        goto invalid_header;

    /* Retail reads the upper bound after the lower-header guard. */
    end = D_800AEDB0[arena_index].end;
    if ((u32)end < (u32)node)
        goto invalid_header;

    previous = node->prev;
    if ((u32)previous < (u32)base)
        goto invalid_header;
    if ((u32)end < (u32)previous)
        goto invalid_header;
    if ((u32)previous & 0xF)
        goto invalid_header;

    next = node->next;
    if ((u32)next < (u32)base)
        goto invalid_header;
    if ((u32)end < (u32)next)
        goto invalid_header;
    if ((u32)next & 0xF)
        goto invalid_header;

    if ((u32)base < (u32)previous && ((u32)previous->next & 0xF))
        goto invalid_header;
    if ((u32)next < (u32)end && ((u32)next->prev & 0xF))
        goto invalid_header;
    goto check_links;

/* Keeping this real failure block before the link checks preserves KMC's
 * branch layout and the later independent arena-end load. */
invalid_header:
    message = D_800ADDCC;
    goto failure;
check_links:

    /* Preserve the retail repeated link checks and the fresh end expression.
     * The intervening header-failure block lets this KMC source retain all
     * three rereads without qualifiers. Moving that block to the common tail
     * allowed earlier experiments to reuse the initial reads instead. */
    if (previous->next != node ||
        previous != node->prev || next != node->next)
        goto invalid_links;
    if ((u32)next < (u32)D_800AEDB0[arena_index].end &&
        next->prev != node)
        goto invalid_links;
    return 1;

invalid_links:
    message = D_800ADE1C;
failure:
    /* The existing variadic declaration preserves the observed A1 value.
     * The retail callee only homes incoming registers and returns; this does
     * not establish formatting behavior or a new fixed-arity interface. */
    func_00023940(message, node);
    return 0;
}

/* Original ROM1A34..1A3C: return the observed payload-minus12 word. */
static u32 func_00001A34(void *pointer)
{
    ArenaNode *node = (ArenaNode *)((u32)pointer - 0x20U);
    return node->used;
}

/* Original ROM1A3C..1A44: return zero; no input register is read. */
static u32 func_00001A3C(void)
{
    return 0;
}
