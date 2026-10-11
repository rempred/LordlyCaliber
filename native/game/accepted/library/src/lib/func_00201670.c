float func_00201670(int sourceArt, int ownerContext)
{
    float value;
    unsigned char entry;

    if (*(unsigned char *)(sourceArt * 0x48 + 0x80187C59) == ownerContext &&
        (entry = (*(unsigned char **)0x801D0698)[sourceArt * 2]) != 0xFF) {
        value = (float)entry / 16.0f;
    } else {
        value = (float)(*(unsigned char **)0x801D0698)[ownerContext * 2] / 16.0f;
    }
    return value;
}
