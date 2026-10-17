/* ROM-derived controller address and 32-byte payload CRCs. */
unsigned char func_0002acd0(unsigned short address)
{
    unsigned int crc = 0;
    unsigned int mask;

    for (mask = 0x400; mask != 0; mask >>= 1) {
        crc <<= 1;
        if (address & mask) {
            if (crc & 0x20)
                crc ^= 0x14;
            else
                crc++;
        } else if (crc & 0x20) {
            crc ^= 0x15;
        }
    }
    for (mask = 5; mask != 0; mask--) {
        crc <<= 1;
        if (crc & 0x20)
            crc ^= 0x15;
    }
    return crc & 0x1F;
}

unsigned char func_0002ad3c(unsigned char *data)
{
    unsigned int crc = 0;
    unsigned int count;
    unsigned int mask;

    for (count = 32; count != 0; count--, data++) {
        for (mask = 0x80; mask != 0; mask >>= 1) {
            crc <<= 1;
            if (*data & mask) {
                if (crc & 0x100)
                    crc ^= 0x84;
                else
                    crc++;
            } else if (crc & 0x100) {
                crc ^= 0x85;
            }
        }
    }
    for (; count < 8; count++) {
        crc <<= 1;
        if (crc & 0x100)
            crc ^= 0x85;
    }
    return crc;
}
