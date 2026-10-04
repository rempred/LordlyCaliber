int func_002015C8(int sourceArt, int ownerContext, int flagA, int flagB)
{
    int classBlock = ownerContext * 4;
    int flagOffset;

    if (*(unsigned char *)(sourceArt * 0x48 + 0x80187C59) != ownerContext) {
        flagOffset = flagA * 2;
    } else {
        flagOffset = flagA * 2;
        classBlock = sourceArt * 4;
    }
    return *(unsigned short *)((flagOffset + classBlock + flagB) * 2 + *(unsigned int *)0x801D0690) & 0xFFF;
}
