#ifndef LORDLY_ACTOR_BODY_H
#define LORDLY_ACTOR_BODY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Caller rows remain opaque big-endian 248-byte native records. */
int lordly_body_classification(const uint8_t *row, uint8_t secondary);
/* Accepted class appearance getters use the low byte of each class ID. */
int lordly_body_appearance(const uint8_t *classes, int count, int source, int owner,
                           uint16_t appearance[4]);
/* First carried item whose type is 1..13 or 24; zero if none. */
int lordly_body_equipment(const uint8_t *items, int count, const uint16_t appearance[4],
                          uint16_t *equipment);
int lordly_body_equipment_variant(int source, int equipment);
#ifdef __cplusplus
}
#endif
#endif
