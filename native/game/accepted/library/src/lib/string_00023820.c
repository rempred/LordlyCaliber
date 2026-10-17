/* Independently reconstructed from the three complete retail string bodies. */
signed char *strchr(const signed char *text, int value)
{
    signed char byte = value;
    while (*text != byte) {
        if (*text++ == 0)
            return 0;
    }
    return (signed char *)text;
}

int strlen(signed char *text)
{
    signed char *end = text;
    if (*text != 0) {
        do {
            end++;
        } while (*end != 0);
    }
    return end - text;
}

void *memcpy_00023884(void *destination, const void *source, unsigned int count)
{
    unsigned char *write = destination;
    const unsigned char *read = source;
    while (count != 0) {
        *write++ = *read++;
        count--;
    }
    return destination;
}