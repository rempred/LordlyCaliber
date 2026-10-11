#ifndef LORDLY_DIRECTOR_CONTROL_FLOW_H
#define LORDLY_DIRECTOR_CONTROL_FLOW_H
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned int LordlyDirectorWord;
typedef int LordlyDirectorSigned;
typedef char LordlyDirectorWordIs32[sizeof(LordlyDirectorWord) == 4 ? 1 : -1];
typedef char LordlyDirectorSignedIs32[sizeof(LordlyDirectorSigned) == 4 ? 1 : -1];
/* Returns 0 on a native result, 1 when the bounded stream is exhausted,
 * 2 for invalid arguments. Native result -1 remains distinct from exhaustion. */
int lordly_director_compare(const LordlyDirectorWord *stream, int count, int cursor,
                           int value, int skip, int scan_mode, int *nesting, int *result);
int lordly_director_find_label(const LordlyDirectorWord *stream, int count,
                              int value, int *result);
#ifdef __cplusplus
}
#endif
#endif
