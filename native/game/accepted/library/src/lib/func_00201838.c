unsigned char func_00201838(int sourceArt, int ownerContext)
{
    unsigned char *table;
    unsigned char entry;

    if (*(unsigned char *)(sourceArt * 0x48 + 0x80187C59) == ownerContext) {
        table = *(unsigned char **)0x801D068C;
        entry = table[sourceArt * 2];
        if (entry == 0xFF && table[ownerContext * 2] == entry) {
            return 0;
        }
    }
    return (*(unsigned char **)0x801D068C)[ownerContext * 2];
}
