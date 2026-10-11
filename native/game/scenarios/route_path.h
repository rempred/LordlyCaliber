#ifndef LORDLY_ROUTE_PATH_H
#define LORDLY_ROUTE_PATH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Fixed native 64x64 routing grid. Returns 1/reached, 0/no route, -1/bad input.
 * path includes both endpoints; count and cost are cleared on failure. */
int lordly_route_path(const uint8_t terrain[4096], const uint16_t costs[32],
                      const uint8_t transitions[1024], const uint8_t *blocked,
                      int start, int goal, uint16_t path[4096], int *count,
                      uint32_t *cost);
#ifdef __cplusplus
}
#endif
#endif
