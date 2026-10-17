typedef unsigned int u32;
typedef unsigned long long u64;
typedef struct {
    union { long long s; u64 u; double d; } value;
    char *buffer;
    int n0, nz0, n1, nz1, n2, nz2;
    int precision, width, field_2C, flags;
} FormatRecord;
typedef struct { long long quot, rem; } DivResult64;
extern const unsigned char D_800ABC30[], D_800ABC44[];
extern DivResult64 func_0002c814(long long, long long);
extern void *memcpy_00023884(void *, const void *, unsigned int);

void func_0002ba70(FormatRecord *record, signed char code)
{
    char buffer[24];
    DivResult64 division;
    const unsigned char *digits = D_800ABC30;
    int base, index;
    u64 magnitude;
    char *write;
    if (code == 'X')
        digits = D_800ABC44;
    base = code == 'o' ? 8 : (code != 'x') & (code != 'X') ? 10 : 16;
    magnitude = record->value.u;
    index = 24;
    if (((code == 'd') | (code == 'i')) && record->value.s < 0)
        magnitude = -magnitude;
    if (magnitude != 0 || record->precision != 0)
        buffer[--index] = digits[magnitude % (unsigned int)base];
    record->value.u = magnitude / (unsigned int)base;
    /* KMC retains the integer offset as the first address-add operand. */
    write = (char *)(index + (u32)buffer);
    while ((record->value.s > 0) & (index > 0)) {
        division = func_0002c814(record->value.s, base);
        record->value.s = division.quot;
        *--write = digits[division.rem];
        --index;
    }
    memcpy_00023884(record->buffer, &buffer[index], record->n1 = 24 - index);
    if (record->n1 < record->precision)
        record->nz0 = record->precision - record->n1;
    if (record->precision < 0 && (record->flags & 0x14) == 0x10) {
        index = record->width - record->n0 - record->nz0 - record->n1;
        if (index > 0)
            record->nz0 += index;
    }
}
