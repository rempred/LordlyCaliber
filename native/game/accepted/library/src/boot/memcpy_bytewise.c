void memcpy_bytewise(unsigned char *destination, unsigned char *source, unsigned int length)
{
    unsigned char *end;

    end = source + length;
    while (source != end) {
        *destination++ = *source++;
    }
}
