typedef unsigned char u8;
typedef signed int s32;

/* The row's owner-context word is copied from the persistent character. */
typedef struct ActorContextView {
    u8 pad_00[0x4C];
    s32 ownerContext;
} ActorContextView;

s32 func_0020C120(ActorContextView *row)
{
    s32 ownerContext;
    s32 paired;

    if (row == 0) {
        return 0;
    }
    ownerContext = row->ownerContext;
    /* Keep the null return and the ordinary-context default separate for KMC. */
    paired = 0;
    /* Retail evaluates the first two equality tests before branching. */
    if (((ownerContext == 10) | (ownerContext == 25)) || ownerContext == 123) {
        paired = 1;
    }
    return paired;
}
