#include "save_compression.h"
typedef unsigned char PackByte;
typedef unsigned short PackHalf;
typedef unsigned int PackWord;

static PackWord pack_get(const PackByte *p)
{
    return ((PackWord)p[0] << 24) | ((PackWord)p[1] << 16) | ((PackWord)p[2] << 8) | p[3];
}
static void pack_put(PackByte *p, PackWord value)
{
    p[0] = (PackByte)(value >> 24); p[1] = (PackByte)(value >> 16);
    p[2] = (PackByte)(value >> 8); p[3] = (PackByte)value;
}
PackWord lc_save_data_crc(const PackByte *p, PackWord size)
{
    PackWord crc = ~0U, bit;
    while (size--) {
        crc ^= *p++;
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
static PackWord pack_min(PackWord a, PackWord b) { return a < b ? a : b; }

PackWord lc_save_lzss_encode(const PackByte *source, PackWord size,
                            PackByte *output, PackWord capacity, PackHalf *work)
{
    PackHalf *previous;
    PackWord i, hash, pos = 0, written = 0, literal = 0;
    PackWord candidate, distance, length, maximum, count, n, kind;
    PackWord bestLength, bestDistance, bestBytes, bestKind, bytes, depth;
    if (!source || !output || !work || !size || size > LC_SAVE_PACK_MAX_BYTES) return 0;
    previous = work + LC_SAVE_PACK_HEADS;
    for (i = 0; i < LC_SAVE_PACK_HEADS; ++i) work[i] = 0;
    for (i = 0; i < size; ++i) {
        previous[i] = 0;
        if (i + 2 < size) {
            hash = (((PackWord)source[i] << 8) ^ ((PackWord)source[i + 1] << 4) ^ source[i + 2]) & 4095U;
            previous[i] = work[hash]; work[hash] = (PackHalf)(i + 1);
        }
    }
    while (pos < size) {
        bestLength = 1; bestBytes = 1; bestKind = 0; bestDistance = 0;
        maximum = pack_min(size - pos, 260U);
        if (source[pos] == 0 || source[pos] == 255) {
            count = 1;
            while (count < pack_min(maximum, 258U) && source[pos + count] == source[pos]) ++count;
            if (source[pos] == 0 && count >= 2) {
                bestLength = pack_min(count, 33U); bestKind = 1;
            }
            if (count >= 3 && count * bestBytes > bestLength * 2) {
                bestLength = count; bestBytes = 2; bestKind = source[pos] == 0 ? 2 : 3;
            }
        }
        candidate = previous[pos]; depth = 0;
        while (candidate && depth++ < 64) {
            --candidate; distance = pos - candidate;
            length = 0;
            while (length < maximum && source[pos + length] == source[pos + length - distance]) ++length;
            for (kind = 4; kind <= 6; ++kind) {
                bytes = kind - 2;
                n = pack_min(length, kind == 4 ? 18U : kind == 5 ? 67U : 260U);
                if (distance <= (kind == 4 ? 2048U : kind == 5 ? 16384U : 65536U) &&
                    n >= kind - 1 && n * bestBytes > bestLength * bytes) {
                    bestLength = n; bestDistance = distance;
                    bestBytes = bytes; bestKind = kind;
                }
            }
            if (length == maximum) break;
            candidate = previous[candidate];
        }
        if (!bestKind) { ++pos; }
        if (bestKind || pos == size || pos - literal == 64) {
            count = pos - literal;
            if (count) {
                if (written >= capacity || count > capacity - written - 1) return 0;
                output[written++] = (PackByte)(0x40U + count - 1);
                for (i = 0; i < count; ++i) output[written++] = source[literal + i];
            }
            literal = pos;
        }
        if (bestKind) {
            if (bestBytes > capacity - written) return 0;
            distance = bestDistance - 1;
            if (bestKind == 1) output[written++] = (PackByte)(0x20U + bestLength - 2);
            else if (bestKind <= 3) {
                output[written++] = bestKind == 2 ? 2 : 1;
                output[written++] = (PackByte)(bestLength - 3);
            } else if (bestKind == 4) {
                output[written++] = (PackByte)(0x80U | ((bestLength - 3) << 3) | (distance >> 8));
                output[written++] = (PackByte)distance;
            } else if (bestKind == 5) {
                output[written++] = (PackByte)(0x10U | ((bestLength - 4) & 15U));
                output[written++] = (PackByte)(((bestLength - 4) & 48U) << 2 | (distance >> 8));
                output[written++] = (PackByte)distance;
            } else {
                output[written++] = 0; output[written++] = (PackByte)(bestLength - 5);
                output[written++] = (PackByte)(distance >> 8); output[written++] = (PackByte)distance;
            }
            pos += bestLength; literal = pos;
        }
    }
    return written;
}

int lc_save_lzss_decode(PackByte *output, PackWord size, const PackByte *source, PackWord length)
{
    PackWord pos = 0, read = 0, code, count, distance, value, i;
    if (!output || !source || !size || size > LC_SAVE_PACK_MAX_BYTES) return 0;
    while (pos < size) {
        if (read >= length) return 0;
        code = source[read++]; distance = 0; value = 0;
        if (code >= 0x80) {
            if (read >= length) return 0;
            count = ((code >> 3) & 15U) + 3;
            distance = ((code & 7U) << 8) + source[read++] + 1;
        } else if (code >= 0x40) {
            count = (code & 63U) + 1;
            if (count > size - pos || count > length - read) return 0;
            for (i = 0; i < count; ++i) output[pos++] = source[read++];
            continue;
        } else if (code >= 0x20) count = (code & 31U) + 2;
        else if (code >= 0x10) {
            if (length - read < 2) return 0;
            value = source[read++]; count = (code & 15U) + ((value & 192U) >> 2) + 4;
            distance = ((value & 63U) << 8) + source[read++] + 1;
        } else if (code == 0) {
            if (length - read < 3) return 0;
            count = source[read++] + 5; distance = (PackWord)source[read++] << 8;
            distance += source[read++] + 1;
        } else if (code <= 2) {
            if (read >= length) return 0;
            count = source[read++] + 3; value = code == 1 ? 255 : 0;
        } else return 0;
        if (count > size - pos || distance > pos) return 0;
        for (i = 0; i < count; ++i) {
            output[pos] = distance ? output[pos - distance] : (PackByte)value; ++pos;
        }
    }
    return read == length;
}

PackWord lc_save_frame_encode(const PackByte *source, PackWord size,
                             PackByte *output, PackWord capacity, PackHalf *work)
{
    PackWord stored, codec = 1, i;
    if (!source || !output || !work || !size || size > LC_SAVE_PACK_MAX_BYTES || capacity < 32) return 0;
    stored = lc_save_lzss_encode(source, size, output + 32, capacity - 32, work);
    if ((!stored || stored >= size) && size <= capacity - 32) {
        codec = 0; stored = size;
        for (i = 0; i < size; ++i) output[32 + i] = source[i];
    }
    if (!stored) return 0;
    pack_put(output, 0x4C433036U); pack_put(output + 4, 0x5A535631U); /* LC06ZSV1 */
    pack_put(output + 8, size); pack_put(output + 12, stored);
    pack_put(output + 16, lc_save_data_crc(source, size)); pack_put(output + 20, lc_save_data_crc(output + 32, stored));
    pack_put(output + 24, codec); pack_put(output + 28, lc_save_data_crc(output, 28));
    return stored + 32;
}
PackWord lc_save_frame_size(const PackByte *source, PackWord capacity)
{
    PackWord size, stored, codec;
    if (!source || capacity < 32 || pack_get(source) != 0x4C433036U ||
        pack_get(source + 4) != 0x5A535631U || pack_get(source + 28) != lc_save_data_crc(source, 28)) return 0;
    size = pack_get(source + 8); stored = pack_get(source + 12); codec = pack_get(source + 24);
    if (!size || size > LC_SAVE_PACK_MAX_BYTES || !stored || stored > capacity - 32 ||
        codec > 1 || (!codec && stored != size) || pack_get(source + 20) != lc_save_data_crc(source + 32, stored)) return 0;
    return size;
}
PackWord lc_save_frame_decode(PackByte *output, PackWord capacity, const PackByte *source, PackWord length)
{
    PackWord size = lc_save_frame_size(source, length), i;
    if (!output || !size || size > capacity) return 0;
    if (pack_get(source + 24)) {
        if (!lc_save_lzss_decode(output, size, source + 32, pack_get(source + 12))) return 0;
    } else for (i = 0; i < size; ++i) output[i] = source[32 + i];
    return pack_get(source + 16) == lc_save_data_crc(output, size) ? size : 0;
}
