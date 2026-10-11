#include "site-count.h"

/* Adapted from the US rev0 initializer at z64 ROM 0x127864..0x1278D0,
 * live RAM 0x801D3124..0x801D3190 (static overlay descriptor 7).
 * The count comes from the placement resource, not the SCINCSV header. */
uint32_t lordly_initial_site_count(uint8_t scenario, uint32_t placement_count) {
    switch (scenario) {
    case 39:
        return 3;
    case 47:
    case 48:
    case 60:
    case 61:
        return 6;
    case 51:
        return 4;
    default:
        return placement_count;
    }
}
