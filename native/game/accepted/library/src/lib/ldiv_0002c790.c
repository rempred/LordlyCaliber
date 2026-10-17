typedef struct { long quot; long rem; } DivResult32;
typedef struct { long long quot; long long rem; } DivResult64;

DivResult32 func_0002c790(long numerator, long denominator)
{
    DivResult32 result;
    result.quot = numerator / denominator;
    result.rem = numerator - denominator * result.quot;
    if (result.quot < 0 && result.rem > 0) {
        ++result.quot;
        result.rem -= denominator;
    }
    return result;
}

DivResult64 func_0002c814(long long numerator, long long denominator)
{
    DivResult64 result;
    result.quot = numerator / denominator;
    result.rem = numerator - denominator * result.quot;
    if (result.quot < 0 && result.rem > 0) {
        ++result.quot;
        result.rem -= denominator;
    }
    return result;
}