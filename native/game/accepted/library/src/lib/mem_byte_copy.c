/* ROM 1FCE0 copies forward; negative lengths perform no stores. */
void mem_byte_copy(unsigned char *source, unsigned char *destination, int length)
{
    int i;
    for (i = 0; i < length; i++)
        *destination++ = *source++;
}
