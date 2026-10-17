typedef struct {
    union { long long s; unsigned long long u; double d; unsigned short h[4]; } value;
    signed char *buffer;
    int n0, nz0, n1, nz1, n2, nz2;
    int precision, width;
    unsigned int count;
    int flags;
    signed char qualifier;
} FormatRecord;
typedef struct { long quot, rem; } DivResult;
extern DivResult func_0002c790(long, long);
extern void *memcpy_00023884(void *, const void *, unsigned int);
extern const double D_800AEC00[], D_800AEC50, D_800AEC58;
extern const signed char D_800AEC48[], D_800AEC4C[], D_800AEC60[];
short func_0002c10c(short *, double *);
void func_0002c1a4(FormatRecord *, signed char, signed char *, short, short);

void func_0002bcc0(FormatRecord *r, signed char code)
{
    signed char buffer[32];
    short exponent;
    signed char *write = buffer;
    double value = r->value.d;
    short kind, keep;
    int digits;
    if (r->precision < 0)
        r->precision = 6;
    else if ((r->precision == 0) && ((code == 'g') | (code == 'G')))
        r->precision = 1;
    kind = func_0002c10c(&exponent, &r->value.d);
    if (kind > 0) {
        typedef struct { signed char bytes[3]; } ThreeBytes;
        const signed char *word = D_800AEC4C;
        signed char *out = r->buffer;
        if (kind == 2) word = D_800AEC48;
        r->n1 = 3;
        *(ThreeBytes *)out = *(const ThreeBytes *)word;
        return;
    }
    keep = 0;
    if (kind == 0)
        exponent = 0;
    else {
        if (value < 0.0) value = -value;
        exponent = exponent * 30103 / 100000 - 4;
        if (exponent < 0) {
            int n = (-exponent + 3) & ~3;
            const double *power;
            exponent = -n;
            if (n > 0) for (power = D_800AEC00; n > 0; n >>= 1, ++power)
                if (n & 1) value *= *power;
        } else if (exponent > 0) {
            double factor = D_800AEC50;
            int n;
            const double *power;
            n = exponent &= ~3;
            if (n > 0) for (power = D_800AEC00; n > 0; n >>= 1, ++power)
                if (n & 1) factor *= *power;
            value /= factor;
        }
        digits = r->precision + 6;
        if (code == 'f') { int fixed_precision = r->precision + 10;
            digits = fixed_precision + exponent; }
        if (digits >= 20) digits = 19;
        *write++ = '0';
        for (; (0 < digits) & (0.0 < value); write += 8) {
            DivResult division;
            long chunk = (long)value;
            int i;
            digits -= 8;
            write += 8;
            if (digits > 0) value = (value - chunk) * D_800AEC58;
            for (i = 8; chunk > 0 && --i >= 0; ) {
                division = func_0002c790(chunk, 10);
                *--write = '0' + division.rem;
                chunk = division.quot;
            }
            while (--i >= 0) *--write = '0';
        }
        { signed char *last = write - 1; digits = last - buffer; }
        exponent += 7;
        write = buffer; ++write;
        while (*write == '0') { ++write; --exponent; --digits; }
        { int precision = r->precision;
        if (code == 'f') { int fixed_precision = precision + 1;
            int fixed_exponent = exponent;
            keep = fixed_precision + fixed_exponent; }
        else keep = precision + ((code == 'e') | (code == 'E')); }
        if (keep > digits) keep = digits;
        if (keep > 0) {
            signed char sentinel = '0';
            int i;
            if (keep < digits) sentinel = write[keep] < '5' ? '0' : '9';
            for (i = keep - 1; write[i] == (signed char)sentinel; --i) --keep;
            if (sentinel == '9') ++write[i];
            if (i < 0) { --write; ++keep; ++exponent; }
        }
    }
    func_0002c1a4(r, code, write, keep, exponent);
}

short func_0002c10c(short *exponent, double *value)
{
    unsigned short *half = (unsigned short *)value;
    unsigned short first = half[0];
    short e = (first & 0x7ff0) >> 4;
    if (e == 0x7ff) {
        *exponent = 0;
        if ((half[0] & 15) || half[1] || half[2] || half[3]) return 2;
        return 1;
    } else if (e > 0) {
        half[0] = (first & 0x800f) | 0x3ff0;
        *exponent = e - 0x3fe;
        return -1;
    } else if (e < 0) return 2;
    *exponent = 0;
    return 0;
}

void func_0002c1a4(FormatRecord *r, signed char code, signed char *digits, short count, short exponent)
{
    if (count <= 0) { count = 1; digits = (signed char *)D_800AEC60; }
    if (code == 'f' || (((code == 'g') | (code == 'G')) && exponent >= -4 && exponent < r->precision)) {
        ++exponent;
        if (code != 'f') {
            if (!(r->flags & 8) && count < r->precision) r->precision = count;
            r->precision -= exponent;
            if (r->precision < 0) r->precision = 0;
        }
        if (exponent <= 0) {
            r->buffer[r->n1++] = '0';
            if (r->precision > 0 || (r->flags & 8)) r->buffer[r->n1++] = '.';
            /* The one-pass clamp block preserves the legacy compiler's saved-register
             * allocation for count and exponent. A plain if swaps those registers;
             * the block retains the same C clamp behavior. */
            do {
                if (r->precision < -exponent) exponent = -r->precision;
            } while (0);
            r->nz1 = -exponent;
            r->precision += exponent;
            if (r->precision < count) count = r->precision;
            memcpy_00023884(r->buffer + r->n1, digits, r->n2 = count);
            r->nz2 = r->precision - count;
        } else if (count < exponent) {
            memcpy_00023884(r->buffer + r->n1, digits, count);
            r->n1 += count;
            r->nz1 = exponent - count;
            if (r->precision > 0 || (r->flags & 8)) {
                r->buffer[r->n1] = '.';
                ++r->n2;
            }
            r->nz2 = r->precision;
        } else {
            memcpy_00023884(r->buffer + r->n1, digits, exponent);
            r->n1 += exponent;
            count -= exponent;
            if (r->precision > 0 || (r->flags & 8)) r->buffer[r->n1++] = '.';
            if (r->precision < count) count = r->precision;
            memcpy_00023884(r->buffer + r->n1, digits + exponent, count);
            r->n1 += count;
            r->nz1 = r->precision - count;
        }
    } else {
        if ((code == 'g') | (code == 'G')) {
            if (count < r->precision) r->precision = count;
            if (--r->precision < 0) r->precision = 0;
            code = code == 'g' ? 'e' : 'E';
        }
        r->buffer[r->n1++] = *digits++; 
        if (r->precision > 0 || (r->flags & 8)) r->buffer[r->n1++] = '.';
        if (r->precision > 0) {
            --count;
            if (r->precision < count) count = r->precision;
            memcpy_00023884(r->buffer + r->n1, digits, count);
            r->n1 += count;
            r->nz1 = r->precision - count;
        }
        digits = r->buffer + r->n1;
        *digits++ = code;
        if (exponent >= 0) *digits++ = '+';
        else { *digits++ = '-'; exponent = -exponent; }
        if (exponent >= 100) {
            if (exponent >= 1000) { *digits++ = '0' + exponent / 1000; exponent %= 1000; }
            *digits++ = '0' + exponent / 100; exponent %= 100;
        }
        *digits++ = '0' + exponent / 10;
        *digits++ = '0' + exponent % 10;
        r->n2 = digits - (r->buffer + r->n1);
    }
    if ((r->flags & 0x14) == 0x10) {
        int total = r->n0 + r->n1 + r->nz1 + r->n2 + r->nz2;
        if (total < r->width) r->nz0 = r->width - total;
    }
}
