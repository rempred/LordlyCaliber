#pragma once
#include "engine/rom.h"
namespace lordly {
inline int gameMonthLength(const QByteArray &months, unsigned year, unsigned month) {
    if (month < 1 || month > 15) throw Error("Calendar month must be 1–15");
    const unsigned shifted = (year + 1748) & 65535;
    const bool leap = shifted % 4 == 0 && (shifted % 100 != 0 || shifted % 400 == 0);
    return u8(months, month - 1) + (month == 3 && leap ? 1 : 0);
}
}
