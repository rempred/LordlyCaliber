typedef signed int s32;
typedef unsigned int u32;
extern char D_801EE0A0[];
extern void func_00023940(const char *format, ...);
extern void *resource_alloc(u32 bytes);

s32 *func_00107D60(s32 first, s32 last, s32 *predecessors)
{
    s32 count;
    s32 current;
    s32 *list;

    if (first == last) {
        count = 1;
    } else {
        current = last;
        count = 2;
        for (;;) {
            current = predecessors[current];
            if (current >= 0x1000) {
                func_00023940(D_801EE0A0, current);
                /* The original diagnostic return enters a permanent self-loop. */
                for (;;) {}
            }
            if (current == first)
                break;
            ++count;
        }
    }

    list = resource_alloc((count + 1) * 4);
    list[count] = -1;
    --count;
    current = last;
    while (count >= 0) {
        list[count] = current;
        current = predecessors[current];
        --count;
    }
    return list;
}

typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef struct MovementAux MovementAux;
typedef struct MovementMetadata MovementMetadata;

#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))

/* Only the observed twelve-byte queue layout is described here. The fixed
 * initialization loop does not establish every allocator's capacity. */
typedef struct MovementQueueEntry {
    s16 field00;
    s16 field02;
    s16 field04;
    u16 field06;
    u32 field08;
} MovementQueueEntry;

extern u8 D_801EB520[];
extern s32 D_801E8670;
extern s32 func_001079EC(MovementQueueEntry *queue, s32 *field34,
                        s32 index, u32 oldCost);
extern s32 func_00107850(MovementQueueEntry *queue, s32 *field34,
                        s32 *field38, s32 index, u32 cost);
extern s32 func_00107C58(MovementQueueEntry *queue, s32 *field34);

/* The accepted physical owner also contains eight zero tail bytes. This C
 * describes its complete executable body; it adds no synthetic padding.
 * The original separate table_00142810 remains its own assembly owner. */
s32 func_00107E38(MovementAux *aux)
{
    MovementMetadata *metadata;
    u32 viewWord;
    u32 blockedCost;
    s32 *head;
    s32 *tail;
    s32 current;
    s32  width;
    s32 height;
    union { s32 initialIndex; s32 next; } indexOrNext;
    u8 * cells;
    u16 * cellCosts;
    u8 * flags;
    u32 * costs;
    s32 * predecessors;
    MovementQueueEntry * queue;

    metadata = FIELD(aux, MovementMetadata *, 0x18);
    width = FIELD(metadata, s32, 0x10);
    height = FIELD(metadata, s32, 0x14);
    cells = FIELD(metadata, u8 *, 0x18);
    cellCosts = FIELD(metadata, u16 *, 0x1C);
    flags = FIELD(aux, u8 *, 0x1C);
    costs = FIELD(aux, u32 *, 0x20);
    predecessors = FIELD(aux, s32 *, 0x24);
    queue = FIELD(aux, MovementQueueEntry *, 0x30);

    if (FIELD(aux, s32, 0x28) == 0) {
        s32 count = height * width;

        FIELD(aux, s32, 0x28) = 1;
        indexOrNext.initialIndex = FIELD(aux, s32, 0x00);
        FIELD(aux, s32, 0x2C) = indexOrNext.initialIndex;
        for (current = 0; current < count; ++current) {
            flags[current] = 0;
            costs[current] = ~0U;
        }
        costs[FIELD(aux, s32, 0x2C)] = 0;
        {
            /* Capture the real queue marker before the counter setup. */
            s32 emptyQueueLink = -1;
            for (current = 0xFFF; current >= 0; --current)
                queue[current].field04 = emptyQueueLink;
        }
        FIELD(aux, s32, 0x34) = 0;
        FIELD(aux, s32, 0x38) = 0;
        queue[FIELD(aux, s32, 0x34)].field00 = -1;
        queue[FIELD(aux, s32, 0x34)].field02 = -1;
        queue[FIELD(aux, s32, 0x34)].field08 = ~0U;
        queue[FIELD(aux, s32, 0x34)].field04 = -1;
    }

    blockedCost = 0xFFFF;
    for (;;) {
        u8 *currentCell;
        u8 *aboveCell;
        u8 *belowCell;
        u32 *currentCost;
        s32 currentBlocked;
        u32 currentCellId;
        s32 x;
        s32 y;
        s32 direction;

        current = FIELD(aux, s32, 0x2C);
        FIELD(aux, s32, 0x2C) = -1;
        viewWord = (u32)cells + (u32)current;
        /* The real byte read precedes the retained address copy in source.
         * This keeps both through KMC combine; scheduling places the copy
         * before the byte load in the generated instructions. */
        currentCellId = *(u8 *)viewWord;
        currentCell = (u8 *)viewWord;
        aboveCell = cells + (current - width);
        belowCell = cells + (current + width);
        currentCost = (u32 *)((u32)current * 4 + (u32)costs);
        currentBlocked = cellCosts[currentCellId] == 0xFFFF;
        y = current / width;
        x = current % width;
        flags[current] = 1;
        for (direction = 1; direction < 9; ++direction) {
            s32 neighbor = -1;
            u8 *mask;
            u32 fiveCost;
            u32 step;
            u32 sum;
            u32 oldCost;
            union { s32 horizontal; u32 currentWord; } work;
            u32 *neighborCost;

            switch (direction) {
            case 1: {
                u32 inverse;
                s32 nonzero;
                s32 diagonalGuard;
                if (FIELD(aux, u32, 0x08) < (u32)x) {
                    if (FIELD(aux, u32, 0x0C) < (u32)y) {
                        work.horizontal = current - 1;
                        neighbor = work.horizontal - width;
                    }
                }
                inverse = ~neighbor;
                nonzero = inverse != 0;
                diagonalGuard = nonzero & currentBlocked;
                if (diagonalGuard &&
                    cellCosts[currentCell[-1]] == blockedCost &&
                    cellCosts[*aboveCell] == blockedCost)
                    neighbor = -1;
                break;
            }
            case 2:
                if (FIELD(aux, u32, 0x0C) < (u32)y)
                    neighbor = current - width;
                break;
            case 3: {
                u32 inverse;
                s32 nonzero;
                s32 diagonalGuard;
                if ((u32)x < FIELD(aux, u32, 0x10)) {
                    if (FIELD(aux, u32, 0x0C) < (u32)y) {
                        work.horizontal = current + 1;
                        neighbor = work.horizontal - width;
                    }
                }
                inverse = ~neighbor;
                nonzero = inverse != 0;
                diagonalGuard = nonzero & currentBlocked;
                if (diagonalGuard &&
                    cellCosts[currentCell[1]] == blockedCost &&
                    cellCosts[*aboveCell] == blockedCost)
                    neighbor = -1;
                break;
            }
            case 4:
                if (FIELD(aux, u32, 0x08) < (u32)x)
                    neighbor = current - 1;
                break;
            case 5:
                if ((u32)x < FIELD(aux, u32, 0x10))
                    neighbor = current + 1;
                break;
            case 6: {
                u32 inverse;
                s32 nonzero;
                s32 diagonalGuard;
                if (FIELD(aux, u32, 0x08) < (u32)x) {
                    if ((u32)y < FIELD(aux, u32, 0x14)) {
                        work.horizontal = current - 1;
                        neighbor = work.horizontal + width;
                    }
                }
                inverse = ~neighbor;
                nonzero = inverse != 0;
                diagonalGuard = nonzero & currentBlocked;
                if (diagonalGuard &&
                    cellCosts[currentCell[-1]] == blockedCost &&
                    cellCosts[*belowCell] == blockedCost)
                    neighbor = -1;
                break;
            }
            case 7:
                if ((u32)y < FIELD(aux, u32, 0x14))
                    neighbor = current + width;
                break;
            case 8: {
                u32 inverse;
                s32 nonzero;
                s32 diagonalGuard;
                if ((u32)x < FIELD(aux, u32, 0x10)) {
                    if ((u32)y < FIELD(aux, u32, 0x14)) {
                        work.horizontal = current + 1;
                        neighbor = work.horizontal + width;
                    }
                }
                inverse = ~neighbor;
                nonzero = inverse != 0;
                diagonalGuard = nonzero & currentBlocked;
                if (diagonalGuard &&
                    cellCosts[currentCell[1]] == blockedCost &&
                    cellCosts[*belowCell] == blockedCost)
                    neighbor = -1;
                break;
            }
            }

            if (neighbor == -1 || flags[neighbor] != 0)
                continue;
            mask = FIELD(aux, u8 *, 0x3C);
            if (mask != 0 && mask[neighbor] != 0)
                continue;
            if (neighbor == FIELD(aux, s32, 0x04)) {
                step = 1;
            } else {
                u8 cell = cells[neighbor];
                u8 *transitionRow;
                step = cellCosts[cell];
                if (step == blockedCost)
                    continue;
                transitionRow = D_801EB520 + ((u32)cells[current] << 5);
                if (transitionRow[cell] != 0)
                    continue;
            }

            /* Preserve the unsigned arithmetic sequence, including the
             * common scaling of the goal's literal base cost. */
            fiveCost = step * 5;
            step = fiveCost * 2;
            if (((direction == 1) | (direction == 3)) ||
                ((direction == 6) | (direction == 8))) {
                u32 numerator = (fiveCost * 16 - step) * 2;
                step = numerator / 10U;
            }
            work.currentWord = *currentCost;
            neighborCost = (u32 *)((u32)neighbor * 4 + (u32)costs);
            oldCost = *neighborCost;
            sum = work.currentWord + step;
            if (sum < oldCost) {
                head = &FIELD(aux, s32, 0x34);
                tail = &FIELD(aux, s32, 0x38);
                if (oldCost != ~0U)
                    func_001079EC(queue, head,
                                  neighbor, oldCost);
                func_00107850(queue, head,
                              tail, neighbor, sum);
                /* Retail reloads the current cost after the queue calls. */
                *neighborCost = *currentCost + step;
                predecessors[neighbor] = current;
            }
        }

        if (current == FIELD(aux, s32, 0x04))
            goto reached_goal;
        indexOrNext.next = func_00107C58(queue, &FIELD(aux, s32, 0x34));
        if (indexOrNext.next == -1)
            goto queue_empty;
        {
            s32 countdown = D_801E8670;
            FIELD(aux, s32, 0x2C) = indexOrNext.next;
            --countdown;
            D_801E8670 = countdown;
            if (countdown == 0) {
                /* This disjoint exit phase keeps the reset local in KMC,
                 * while the shared -1 remains eligible for rematerialization. */
                viewWord = 0x5A;
                D_801E8670 = (s32)viewWord;
                return 0;
            }
        }
        continue;
reached_goal:
        FIELD(aux, s32, 0x28) = 0;
        D_801E8670 = 0x5A;
        return 1;
queue_empty:
        FIELD(aux, s32, 0x28) = 0;
        D_801E8670 = 0x5A;
        return 1;
    }
}
