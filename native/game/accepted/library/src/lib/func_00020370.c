/* ROM 20370 proves the 76-byte slot stride and nested +0x20 field. */
typedef struct L4EffectSlot {
    unsigned char field00[0x20];
    unsigned char effect[0x2C];
} L4EffectSlot;
typedef struct L4Synth {
    unsigned char field00[0x30];
    void *mainBus;
    L4EffectSlot *slots;
} L4Synth;
extern void func_0001ca94(void *, void *, void *);
extern int func_0001f2a4(int *, int, int);
extern int func_0001ec50(void *, int, void *);

void *func_00020370(L4Synth *synth, short bus, void *config, void *heap)
{
    func_0001ca94(synth->slots[bus].effect, config, heap);
    func_0001f2a4((int *)synth->slots[bus].effect, 1,
                 (int)&synth->slots[bus]);
    func_0001ec50(synth->mainBus, 2, synth->slots[bus].effect);
    return synth->slots[bus].effect;
}
