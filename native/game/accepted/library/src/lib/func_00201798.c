unsigned char func_00201798(int sourceArt, int ownerContext, int actionId, int rawMode)
{
    unsigned char *table = *(unsigned char **)0x801D0694;
    unsigned char *entry;
    unsigned short offset;
    unsigned char key;

    if (*(unsigned char *)(sourceArt * 0x48 + 0x80187C59) == ownerContext) {
        offset = ((unsigned short *)table)[sourceArt];
        entry = table + offset;
        if (offset == 0xFFFF) {
            entry = table + ((unsigned short *)table)[ownerContext];
        }
    } else {
        entry = table + ((unsigned short *)table)[ownerContext];
    }
    while ((key = *entry) != 0xFF) {
        if (key == actionId) {
            return entry[rawMode + 1];
        }
        entry += 4;
    }
    return 0x28;
}
