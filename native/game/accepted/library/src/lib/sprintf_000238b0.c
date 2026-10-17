extern void *memcpy_00023884(void *, const void *, unsigned int);
extern int _Printf(void *(*)(void *, const char *, unsigned int), void *, const char *, char *);
void *mempcpy(void *, const char *, unsigned int);

int sprintf(char *buffer, const char *format, ...)
{
    char *arguments = (char *)__builtin_next_arg(format);
    int result = _Printf(mempcpy, buffer, format, arguments);
    if (result >= 0)
        buffer[result] = 0;
    return result;
}

void *mempcpy(void *buffer, const char *bytes, unsigned int count)
{
    memcpy_00023884(buffer, bytes, count);
    return (char *)buffer + count;
}