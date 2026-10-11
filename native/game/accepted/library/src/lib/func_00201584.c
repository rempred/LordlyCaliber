unsigned char func_00201584(int sourceArt, int ownerContext)
{
    unsigned char *entry;

    if (*(unsigned char *)(sourceArt * 0x48 + 0x80187C59) != ownerContext) {
        entry = *(unsigned char **)0x801D0684 + ownerContext;
    } else {
        entry = *(unsigned char **)0x801D0684 + sourceArt;
    }
    return *entry;
}
