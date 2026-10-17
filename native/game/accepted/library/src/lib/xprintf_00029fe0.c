typedef unsigned int u32;
typedef struct {
    union { long long s; unsigned long long u; double d; unsigned short h[4]; } value;
    signed char *buffer;
    int n0, nz0, n1, nz1, n2, nz2;
    int precision, width;
    unsigned int count;
    int flags;
    signed char qualifier;
} FormatRecord;
typedef void *(*PrintOutput)(void *, const signed char *, unsigned int);
extern const signed char D_800AEA80[], D_800AEAA0[];
extern const int D_800AEA88[];
extern const signed char D_800ABB20[], D_800ABB44[];
extern signed char *strchr(const signed char *, int);
extern int strlen(const signed char *);
extern void func_0002ba70(FormatRecord *, signed char);
extern void func_0002bcc0(FormatRecord *, signed char);
void func_0002a560(FormatRecord *, signed char **, signed char, signed char *);

#define NEXT_ARG(ap, type) \
    ((ap) = (signed char *)(((u32)(ap) + sizeof(type) - 1) & ~(sizeof(type) - 1)) + sizeof(type), \
     *(type *)((ap) - sizeof(type)))
#define OUTPUT(bytes, amount) { \
    if ((amount) > 0) { \
        destination = output(destination, (bytes), (amount)); \
        if (destination == 0) return record.count; \
        record.count += (amount); \
    } \
}
#define PAD(bytes, amount) { \
    if ((amount) > 0) { \
    for (left = (amount); left > 0; left -= chunk) { \
        chunk = (unsigned int)left <= 32U ? left : 32; \
        OUTPUT((bytes), chunk); \
    } \
    } \
}

int _Printf(PrintOutput output, void *destination, const signed char *format, signed char *arguments)
{
    FormatRecord record;
    signed char buffer[32];
    const signed char *scan;
    const signed char *flag;
    int length, left, chunk;
    unsigned char terminal;
    record.count = 0;
    for (;;) {
        scan = format;
        while ((terminal = *scan, (*scan != 0) & (*scan != '%')))
            ++scan;
        length = scan - format;
        OUTPUT(format, length);
        if (terminal == 0)
            break;
        ++scan;
        record.flags = 0;
        while ((flag = strchr(D_800AEA80, *scan)) != 0) {
            record.flags |= D_800AEA88[flag - D_800AEA80];
            ++scan;
        }
        if (*scan == '*') {
            record.width = NEXT_ARG(arguments, int);
            if (record.width < 0) {
                record.width = -record.width;
                record.flags |= 4;
            }
            ++scan;
        } else {
            for (record.width = 0; (unsigned char)(*scan - '0') < 10; ++scan)
                if (record.width < 999)
                    record.width = record.width * 10 + *scan - '0';
        }
        if (*scan != '.')
            record.precision = -1;
        else {
            ++scan;
            if (*scan == '*') {
                record.precision = NEXT_ARG(arguments, int);
                ++scan;
            } else {
                for (record.precision = 0; (unsigned char)(*scan - '0') < 10; ++scan)
                    if (record.precision < 999)
                        record.precision = record.precision * 10 + *scan - '0';
            }
        }
        record.qualifier = strchr(D_800AEAA0, *scan) ? *scan++ : 0;
        if (record.qualifier == 'l' && *scan == 'l') {
            record.qualifier = 'L';
            ++scan;
        }
        func_0002a560(&record, &arguments, *scan, buffer);
        record.width -= record.n0 + record.nz0 + record.n1 + record.nz1 + record.n2 + record.nz2;
        if (!(record.flags & 4)) {
            PAD(D_800ABB20, record.width);
        }
        OUTPUT(buffer, record.n0);
        PAD(D_800ABB44, record.nz0);
        OUTPUT(record.buffer, record.n1);
        PAD(D_800ABB44, record.nz1);
        OUTPUT(record.buffer + record.n1, record.n2);
        PAD(D_800ABB44, record.nz2);
        if (record.flags & 4) {
            PAD(D_800ABB20, record.width);
        }
        format = scan + 1;
    }
    return record.count;
}

void func_0002a560(FormatRecord *record, signed char **arguments, signed char code, signed char *buffer)
{
    record->n0 = record->nz0 = record->n1 = record->nz1 = record->n2 = record->nz2 = 0;
    switch (code) {
    case 'c':
        buffer[record->n0++] = NEXT_ARG(*arguments, int);
        break;
    case 'd': case 'i':
        if (record->qualifier == 'l')
            record->value.s = NEXT_ARG(*arguments, long);
        else if (record->qualifier == 'L')
            record->value.s = NEXT_ARG(*arguments, long long);
        else
            record->value.s = NEXT_ARG(*arguments, int);
        if (record->qualifier == 'h')
            record->value.s = (short)record->value.s;
        if (record->value.s < 0)
            buffer[record->n0++] = '-';
        else if (record->flags & 2)
            buffer[record->n0++] = '+';
        else if (record->flags & 1)
            buffer[record->n0++] = ' ';
        record->buffer = buffer + record->n0;
        func_0002ba70(record, code);
        break;
    case 'o': case 'u': case 'x': case 'X':
        if (record->qualifier == 'l')
            record->value.s = NEXT_ARG(*arguments, long);
        else if (record->qualifier == 'L')
            record->value.s = NEXT_ARG(*arguments, long long);
        else
            record->value.s = NEXT_ARG(*arguments, int);
        if (record->qualifier == 'h')
            record->value.s = (unsigned short)record->value.s;
        else if (record->qualifier == 0)
            record->value.s = (unsigned int)record->value.s;
        if (record->flags & 8) {
            buffer[record->n0++] = '0';
            if ((code == 'x') | (code == 'X'))
                buffer[record->n0++] = code;
        }
        record->buffer = buffer + record->n0;
        func_0002ba70(record, code);
        break;
    case 'e': case 'E': case 'f': case 'g': case 'G':
        record->value.d = NEXT_ARG(*arguments, double);
        if (record->value.h[0] & 0x8000)
            buffer[record->n0++] = '-';
        else if (record->flags & 2)
            buffer[record->n0++] = '+';
        else if (record->flags & 1)
            buffer[record->n0++] = ' ';
        record->buffer = buffer + record->n0;
        func_0002bcc0(record, code);
        break;
    case 'n':
        if (record->qualifier == 'h')
            *NEXT_ARG(*arguments, short *) = record->count;
        else if (record->qualifier == 'l')
            *NEXT_ARG(*arguments, long *) = record->count;
        else if (record->qualifier == 'L')
            *NEXT_ARG(*arguments, long long *) = record->count;
        else
            *NEXT_ARG(*arguments, int *) = record->count;
        break;
    case 'p':
        record->value.s = (long)NEXT_ARG(*arguments, void *);
        record->buffer = buffer + record->n0;
        func_0002ba70(record, 'x');
        break;
    case 's':
        record->buffer = NEXT_ARG(*arguments, signed char *);
        record->n1 = strlen(record->buffer);
        if (record->precision >= 0 && record->precision < record->n1)
            record->n1 = record->precision;
        break;
    case '%':
        buffer[record->n0++] = '%';
        break;
    default:
        buffer[record->n0++] = code;
        break;
    }
}
