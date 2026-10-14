typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

extern void *D_801CE8BC;
extern u8 D_801976E8;
extern u8 D_801971F0[];
extern u8 D_80193BC0[];
extern u8 D_80190F80[];
extern u8 D_80195560[];
extern u8 D_801953F0[];
extern void func_0020D9C4(void);
extern void func_00023780(void *, u32);
extern void func_001F0F6C(int);
extern void func_001F102C(int);
extern void func_001F114C(void *);
extern u8 func_00043dc4(int, int);
extern int func_00044238(int, int, int);
extern int func_00045934(int, int, int, int);
extern int func_002015C8(int, int, int, int);
extern void func_00207658(int *, int *, int *, int *, int *, u32);
extern void func_002073CC(int, int);
extern void func_0020C908(void *, u8);
extern void func_0020CBDC(void *, int);
extern u8 func_0020D434(int);
extern int func_0020C014(void *);
extern void func_0020FC7C(void *);
extern int func_0021062C(int, int, int);
extern void func_0021088C(void);
extern void func_00210930(void);

#define BYTE(p,o) (*(u8 *)((u8 *)(p) + (o)))
#define HALF(p,o) (*(u16 *)((u8 *)(p) + (o)))
#define WORD(p,o) (*(int *)((u8 *)(p) + (o)))
#define FLAGS(p) (*(u32 *)((u8 *)(p) + 0x40))
#define FLAG(p,b) ((p) ? ((FLAGS(p) >> (b)) & 1) : 0)

/* These source rows and scene actors have distinct, neutral byte layouts. */
#define COPY_ROW(actor, source, sourceId) { \
    u8 *copyRow = (source); u8 copied1B; int copiedClass, classIsOne; \
    func_00023780((actor), 0xF8); \
    WORD(actor,0x48) = BYTE(copyRow,0x11); \
    copiedClass = BYTE(copyRow,0x12); \
    BYTE(actor,0xF6) = (sourceId); \
    WORD(actor,0x4C) = copiedClass; \
    BYTE(actor,0x31) = BYTE(copyRow,0x13); \
    BYTE(actor,0x33) = BYTE(copyRow,0x1A); \
    HALF(actor,0x22) = HALF(copyRow,0x16); \
    HALF(actor,0x20) = HALF(copyRow,0x18); \
    HALF(actor,0x24) = HALF(copyRow,0x1C); \
    HALF(actor,0x26) = HALF(copyRow,0x1E); \
    HALF(actor,0x28) = HALF(copyRow,0x20); \
    HALF(actor,0x2A) = HALF(copyRow,0x22); \
    HALF(actor,0x2C) = HALF(copyRow,0x24); \
    HALF(actor,0x2E) = HALF(copyRow,0x26); \
    BYTE(actor,0x30) = BYTE(copyRow,0x28); \
    copied1B = BYTE(copyRow,0x1B); BYTE(actor,0x3F) = copied1B; \
    BYTE(actor,0x34) = copied1B; \
    HALF(actor,0x36) = HALF(copyRow,0x2A); \
    HALF(actor,0x38) = HALF(copyRow,0x2C); \
    HALF(actor,0x3A) = HALF(copyRow,0x2E); \
    HALF(actor,0x3C) = HALF(copyRow,0x30); \
    BYTE(actor,0x3E) = BYTE(copyRow,0x32); \
    if (!(actor)) { classIsOne = 0; } else { classIsOne = WORD(actor,0x4C) == 1; } \
    if (classIsOne) BYTE(actor,0x3E) = 0; \
    if (BYTE(copyRow,0x33) & 2) FLAGS(actor) |= 0x200; \
    if (BYTE(copyRow,0x33) & 4) FLAGS(actor) |= 2; \
    BYTE(actor,0x32) = BYTE(copyRow,0x35); \
}

/* Each search observes presence, side, and enable in their original order. */
static __inline__ void *find_peer(int inverseSide)
{
    u8 *base = D_801CE8BC;
    u8 *scan = base;
    u32 search = 0;
    int searchOffset = 0;
    do {
        void *peer = 0;
        int side, enabled;
        if (search < 20) { int present = WORD(scan,0x20C); if (present) { int actorOffset = searchOffset + 0x1C4; peer = base + actorOffset; } }
        if (!peer) side = 0;
        else side = ((FLAGS(peer) >> 8) ^ inverseSide) & 1;
        if (side) {
            if (!peer) enabled = 0;
            else enabled = (FLAGS(peer) >> 9) & 1;
            if (enabled) return peer;
        }
        scan += 0xF8;
        search++;
        searchOffset += 0xF8;
    } while ((int)search < 20);
    return 0;
}

#define FIND_PEER(actor, inverseSide) { \
    void *peer = find_peer(inverseSide); \
    do { \
        if (!peer) break; \
        if ((actor) == peer) break; \
        if (WORD(peer,0x4C) != 0x21) break; \
        if (FLAGS(peer) & 1) break; \
        if ((FLAGS(peer) >> 1) & 1) break; \
        WORD(actor,0x70) = (u8)func_00044238(BYTE(actor,0x4B), BYTE(actor,0x4F), \
                                          func_0020D434(WORD(actor,0x58)) & 255) + 1; \
    } while (0); \
}

/* Queue submission consumes the three pointer positions, not their objects. */
#define SUBMIT_POINTERS(actor) { \
    index = 0; \
    do { \
        if (WORD(actor,0)) func_001F0F6C(WORD(actor,0)); \
        if (WORD(actor,0xC)) func_001F102C(WORD(actor,0xC)); \
        index++; \
        (actor) = (u8 *)(actor) + 4; \
    } while (index < 3); \
}

static __inline__ void *scene_actor(u32 index, int offset)
{
    u8 *scene;
    if (index >= 20) return 0;
    scene = D_801CE8BC;
    if (!WORD(scene + offset,0x20C)) return 0;
    return scene + (offset + 0x1C4);
}
/* Keep the ordinal conversion: it retains GCC's unused predicate slot while
 * the zero-based caller counter shares its register with the later scans. */
static __inline__ void *scene_actor_ordinal(u32 visit, int offset)
{
    u8 *scene;
    visit--;
    if (visit >= 20) return 0;
    scene = D_801CE8BC;
    if (!WORD(scene + offset,0x20C)) return 0;
    return scene + (offset + 0x1C4);
}
/* Adjacent half-open bands reuse the boundary already loaded on that path. */
static __inline__ u8 remap_token(float position, u8 input, const double *bands)
{
    double boundary = bands[0];
    if (position >= boundary) {
        boundary = bands[1];
        if (position < boundary) return ((u8 *)0x801CFC98)[input];
    } else boundary = bands[2];
    if (position >= boundary) {
        boundary = bands[3];
        if (position < boundary) return ((u8 *)0x801CFCA4)[input];
    } else boundary = bands[4];
    if (position >= boundary && position < bands[5]) return ((u8 *)0x801CFCB0)[input];
    return input;
}

static __inline__ u16 auxiliary_value(u32 id, u8 *table) { u16 *entry = (u16 *)table; entry += id; return entry[-0x62]; }
static __inline__ u8 token_column(u8 token) { return (u32)token % 3; }
static __inline__ u8 token_row(u8 token) { return (u32)token / 3; }

void func_0020DB10(int selector)
{
    int sources[18], contexts[18];
    u32 resources[18];
    int variants[18], flag10s[18], flag8s[18];
    int latch, outputCount, slot, outputOffset, index;
    void *actor;
    u8 *selection, *row, *auxiliary;
    /* Source IDs and the later collection count have disjoint lifetimes.
     * Reuse keeps GCC's saved-register allocation; sourceByte retains the
     * byte-width call argument from the same input load. */
    u32 sourceId;
    u8 token;
    u32 actorIndex;

    WORD(D_801CE8BC,0x5124) = (int)func_0020D9C4;
    WORD(D_801CE8BC,0x5128) = -0x11D0;
    WORD(D_801CE8BC,0x512C) = -0x11D0;
    WORD(D_801CE8BC,0x5130) = -0x11D0;
    func_001F0F6C((int)((u8 *)D_801CE8BC + 0x5124));
    outputCount = 0;
    latch = 0;

    if (*(u8 *)0x801976D9) {
        
        slot = 0;
        while (slot < 5) {
            u8 selected = *(u8 *)0x801976E8; u8 rowId, sourceByte;
            selection = D_801971F0 + selected * 0x19;
            rowId = (slot + selection)[2];
            if (rowId) {
                float position;
                actor = (u8 *)D_801CE8BC + (outputCount * 0xF8 + 0x1C4);
                if (selected < 30) {
                    row = D_80193BC0 + rowId * 0x38;
                    auxiliary = D_80190F80;
                } else {
                    row = D_80195560 + rowId * 0x34;
                    auxiliary = D_801953F0;
                }
                position = *(float *)0x801976EC;
                token = remap_token(position, (slot + selection)[7], (double *)0x801D05E0);
                sourceByte = (slot + selection)[2];
                sourceId = sourceByte;
                if (sourceId < 100) {
                    if ((HALF(row,0x18) != 0) | (selector != 0)) {
                        if (*(u8 *)0x801976E8 < 30) {
                            COPY_ROW(actor, D_80193BC0 + sourceId * 0x38, sourceId);
                        } else {
                            func_0020C908(actor, sourceByte);
                        }
                        if (!BYTE(D_801CE8BC,0x6084) && (FLAGS(actor) & 2)) {
                            func_00023780(actor, 0xF8);
                            goto next_slot_0;
                        }
                        if (WORD(actor,0x48) == 0x87 || WORD(actor,0x48) == 0x88 || WORD(actor,0x48) == 0xA1) {
                            token = (slot + selection)[7];
                        }
                        FLAGS(actor) |= 0x500;
                        WORD(actor,0x54) = token_column(token);
                        WORD(actor,0x58) = token_row(token);
                        func_0020CBDC(actor, 0);
                        FIND_PEER(actor, 0);
                        if (WORD(actor,0x48) == 0x87) {
                            SUBMIT_POINTERS(actor);
                            outputCount++;
                            actor = (u8 *)D_801CE8BC + (outputCount * 0xF8 + 0x1C4);
                            func_0020C908(actor, (slot + selection)[2]);
                            WORD(actor,0x54) = token_column(token);
                            WORD(actor,0x58) = token_row(token);
                            func_0020CBDC(actor, 1);
                            WORD(actor,0x70) = 0;
                            WORD(actor,0x6C) = 0;
                        }
                        { int enabled; if (!actor) enabled = 0; else enabled = (FLAGS(actor) >> 9) & 1; if (enabled && (func_00043dc4(BYTE(actor,0x4B), BYTE(actor,0x4F)) & 255) == 2) latch = 1; }
                    }
                } else {
                    if (auxiliary_value(sourceId, auxiliary)) {
                        if (*(u8 *)0x801976E8 < 30) {
                            COPY_ROW(actor, D_80193BC0, 0);
                        } else {
                            func_0020C908(actor, 0);
                        }
                        if (!BYTE(D_801CE8BC,0x6084) && (FLAGS(actor) & 2)) {
                            func_00023780(actor, 0xF8);
                            goto next_slot_0;
                        }
                        FLAGS(actor) |= 0x500;
                        WORD(actor,0x54) = token_column(token);
                        WORD(actor,0x58) = token_row(token);
                        BYTE(actor,0xF6) = (slot + selection)[2];
                        HALF(actor,0x20) = auxiliary_value(BYTE(actor,0xF6), auxiliary);
                        HALF(actor,0x22) = HALF(auxiliary,2);
                        if (BYTE(auxiliary + BYTE(actor,0xF6),0x90) & 4) FLAGS(actor) |= 2;
                        func_0020CBDC(actor, latch);
                        FIND_PEER(actor, 0);
                    }
                }
                SUBMIT_POINTERS(actor);
                outputCount++;
            }
next_slot_0:
            slot++;
        }
    }

    {
        int outputIndex = outputCount;
        latch = 0;
        slot = 0;
        while (slot < 5) {
            u8 selected = *(u8 *)0x801976DC; u8 rowId, sourceByte;
            selection = D_801971F0 + selected * 0x19;
            rowId = (slot + selection)[2];
            if (rowId) {
                float position;
                if (*(u16 *)0x80197B60 & 0x8000) {
                    if (slot != 0) goto next_slot_1;
                }
                actor = (u8 *)D_801CE8BC + (outputIndex * 0xF8 + 0x1C4);
                if (selected < 30) {
                    row = D_80193BC0 + rowId * 0x38;
                    auxiliary = D_80190F80;
                } else {
                    row = D_80195560 + rowId * 0x34;
                    auxiliary = D_801953F0;
                }
                position = *(float *)0x801976E0;
                token = remap_token(position, (slot + selection)[7], (double *)0x801D0610);
                sourceByte = (slot + selection)[2];
                sourceId = sourceByte;
                if (sourceId < 100) {
                    if ((HALF(row,0x18) != 0) | (selector != 0)) {
                        if (*(u8 *)0x801976DC < 30) {
                            COPY_ROW(actor, D_80193BC0 + sourceId * 0x38, sourceId);
                        } else {
                            func_0020C908(actor, sourceByte);
                        }
                        if (!BYTE(D_801CE8BC,0x6084) && (FLAGS(actor) & 2)) {
                            func_00023780(actor, 0xF8);
                            goto next_slot_1;
                        }
                        
                        FLAGS(actor) &= ~0x500;
                        WORD(actor,0x54) = 2 - token_column(token);
                        WORD(actor,0x58) = 8 - token_row(token);
                        func_0020CBDC(actor, 0);
                        FIND_PEER(actor, 1);
                        
                        { int enabled; if (!actor) enabled = 0; else enabled = (FLAGS(actor) >> 9) & 1; if (enabled && (func_00043dc4(BYTE(actor,0x4B), BYTE(actor,0x4F)) & 255) == 2) latch = 1; }
                    }
                } else {
                    if (auxiliary_value(sourceId, auxiliary)) {
                        if (*(u8 *)0x801976DC < 30) {
                            COPY_ROW(actor, D_80193BC0, 0);
                        } else {
                            func_0020C908(actor, 0);
                        }
                        if (!BYTE(D_801CE8BC,0x6084) && (FLAGS(actor) & 2)) {
                            func_00023780(actor, 0xF8);
                            goto next_slot_1;
                        }
                        FLAGS(actor) &= ~0x500;
                        WORD(actor,0x54) = 2 - token_column(token);
                        WORD(actor,0x58) = 8 - token_row(token);
                        BYTE(actor,0xF6) = (slot + selection)[2];
                        HALF(actor,0x20) = auxiliary_value(BYTE(actor,0xF6), auxiliary);
                        HALF(actor,0x22) = HALF(auxiliary,2);
                        if (BYTE(auxiliary + BYTE(actor,0xF6),0x90) & 4) FLAGS(actor) |= 2;
                        func_0020CBDC(actor, latch);
                        FIND_PEER(actor, 1);
                    }
                }
                SUBMIT_POINTERS(actor);
                outputIndex++;
                
            }
next_slot_1:
            slot++;
        }
    }

    sourceId = 0;
    {
    int sortOffset;
    sortOffset = 0;
    actorIndex = 0;
    while (actorIndex < 20) {
        actor = scene_actor_ordinal(actorIndex + 1u, sortOffset);
        if (actor) {
            u32 key, shift; int context;
            int variant, flag10, flag8;
            variant = func_00045934(HALF(actor,0x36), HALF(actor,0x38), HALF(actor,0x3A), HALF(actor,0x3C)) & 0xFFFF;
            flag8 = (FLAGS(actor) >> 8) & 1;
            flag10 = (FLAGS(actor) >> 10) & 1;
            for (index = 0; index < sourceId; index++) {
                if (WORD(actor,0x48) == sources[index] && variant == variants[index] &&
                    flag10 == flag10s[index] && flag8 == flag8s[index]) break;
            }
            if (index < sourceId) goto next_actor;
            {
                key = func_002015C8(WORD(actor,0x48), WORD(actor,0x4C), flag10, flag8);
                for (index = 0; index < sourceId; index++) {
                    if (key < resources[index]) break;
                }
                for (shift = sourceId; index < shift; shift--) {
                    sources[shift] = sources[shift - 1];
                    contexts[shift] = contexts[shift - 1];
                    resources[shift] = resources[shift - 1];
                    variants[shift] = variants[shift - 1];
                    flag10s[shift] = flag10s[shift - 1];
                    flag8s[shift] = flag8s[shift - 1];
                }
                sources[shift] = WORD(actor,0x48);
                context = WORD(actor,0x4C);
                sourceId++;
                resources[shift] = key;
                variants[shift] = variant;
                flag10s[shift] = flag10;
                flag8s[shift] = flag8;
                contexts[shift] = context;
            }
        }
next_actor:
        actorIndex++;
        sortOffset += 0xF8;
    }
    }
    if (sourceId) {
        actorIndex = 0;
        while (actorIndex < sourceId) {
            u32 run = 1;
            /* Retail compares run with the total count even after actorIndex advances. */
            while (run < sourceId && resources[actorIndex + run] == resources[actorIndex]) run++;
            func_00207658(sources + actorIndex, contexts + actorIndex, flag8s + actorIndex,
                          flag10s + actorIndex, variants + actorIndex, run);
            actorIndex += run;
        }
        func_002073CC(5, 0);
        func_0021088C();
        {
        int policyOffset; void *policyActor;
        sourceId = 0;
        policyOffset = 0;
        do {
            policyActor = scene_actor(sourceId, policyOffset);
            if (policyActor && !((FLAGS(policyActor) >> 1) & 1) &&
                !func_0021062C(WORD(policyActor,0x48), WORD(policyActor,0x4C), 4)) {
                int side = func_0020C014(policyActor);
                u8 *status = &D_801976E8;
                if (side) status -= 0xC;
                if (status[1] & 2) {
                    FLAGS(policyActor) |= 4;
                    func_0020FC7C(policyActor);
                }
            }
            sourceId++;
            policyOffset += 0xF8;
        } while ((int)sourceId < 20);
        }
        func_00210930();
        {
        int updateOffset; void *updateActor;
        actorIndex = 0;
        updateOffset = 0;
        do {
            updateActor = scene_actor(actorIndex, updateOffset);
            if (updateActor) {
                index = 0;
                do {
                    func_001F114C(updateActor);
                    index++;
                } while (index < 10);
            }
            actorIndex++;
            updateOffset += 0xF8;
        } while ((int)actorIndex < 20);
        }
    }
}
