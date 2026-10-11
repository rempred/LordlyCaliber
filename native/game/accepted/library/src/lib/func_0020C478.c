extern unsigned char *D_801CE8BC;

void *func_0020C478(unsigned int index)
{
    unsigned int offset;
    unsigned char *base;

    if (index >= 20) {
        goto fail;
    }

    base = D_801CE8BC;
    offset = index * 0xF8;
    if (*(void **)(base + offset + 0x20C) == 0) {
        goto fail;
    }

    return &base[offset + 0x1C4];

fail:
    return 0;
}
