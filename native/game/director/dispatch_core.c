/* PC control-flow adaptation of func_00284288.c, retained alongside this file.
 * Host pointers/array bounds replace target pointer casts. Game-domain services
 * remain explicit callbacks. Cursor commits, local handoff flags, query scanning,
 * labels and registered-counter arithmetic preserve the recovered operations. */
#include "dispatch_core.h"

int lordly_director_dispatch(LordlyDirectorDispatch *s, const LordlyDirectorWord *words,
                            int count, int budget, LordlyDirectorLookup lookup,
                            LordlyDirectorService service, void *context)
{
    int dispatched;
    if (!s || !words || !lookup || count < 1 || budget < 1 ||
        (s->mode != 0 && s->mode != 1 && s->mode != 2)) return LORDLY_DIRECTOR_INVALID;
    if (!s->active) {
        s->cursor = s->resume_cursor;
        s->nesting = 0;
        s->state_scan_mode = 0;
        s->handoff = 0;
        s->stream_selector = 255;
        s->active = 1;
    }
    for (dispatched = 0; dispatched < budget; ++dispatched) {
        LordlyDirectorDefinition def;
        LordlyDirectorWord command;
        int value = -100, next, depth, status;
        if (s->cursor < 0 || s->cursor >= count) return LORDLY_DIRECTOR_INVALID;
        command = words[s->cursor];
        if (!lookup(context, command, &def) || def.words < 1 || def.words > 32 ||
            (def.query && def.query != 3 && def.query != 4)) return LORDLY_DIRECTOR_INVALID;
        /* An inactive mode-dependent query breaks without skipping its operands
         * in the recovered dispatcher. Those words remain executable. */
        if (def.query && def.mode != -1 && def.mode != s->mode) {
            ++s->cursor;
            continue;
        }
        /* The terminal trailer is resource metadata, never read by this case. */
        if (command == 0x80000001U) {
            s->active = 0;
            return LORDLY_DIRECTOR_END;
        }
        if (def.words > count - s->cursor) return LORDLY_DIRECTOR_INVALID;
        switch (command) {
        case 0: s->handoff = 1; break;
        case 1: s->counter = 1; break;
        case 2: s->counter = 0; break;
        case 0xb5:
            if (s->controller & 0x8000U) s->counter = words[s->cursor + 2] + 1U;
            /* Fall through to the registered counter query. */
        case 0xe:
            value = (LordlyDirectorSigned)(s->counter - 1U);
            break;
        case 0x56: s->state_scan_mode = 1; break;
        case 0x59:
            status = lordly_director_find_label(words, count,
                     (LordlyDirectorSigned)words[s->cursor + 1], &next);
            if (status) return LORDLY_DIRECTOR_INVALID;
            s->resume_cursor = next;
            s->active = 0;
            return LORDLY_DIRECTOR_YIELD;
        case 0x99: s->stream_selector = words[s->cursor + 1] & 255U; break;
        case 0x9a: s->stream_selector = 254; break;
        case 0x80000000U:
            if (s->stream_selector != 255) return LORDLY_DIRECTOR_TRANSFER;
            if (s->handoff) s->resume_cursor = s->cursor + 1;
            if (s->barrier_state == 2) {
                status = lordly_director_find_label(words, count, 12345, &next);
                if (status) return LORDLY_DIRECTOR_INVALID;
                s->barrier_state = 0;
                s->resume_cursor = next;
            }
            s->active = 0;
            return LORDLY_DIRECTOR_YIELD;
        case 0x80000005U:
            if (words[s->cursor + 1] == 0x70 &&
                (!service || !service(context, command, words + s->cursor, def.words, &value)))
                return LORDLY_DIRECTOR_SERVICE;
            break;
        case 0x80000003U:
        case 0xbb:
            return LORDLY_DIRECTOR_TRANSFER;
        /* These recovered cases only consume words, without a service call. */
        case 0x12: case 0x20: case 0x21: case 0x5c: case 0x8e:
        case 0x80000002U: case 0x80000004U: case 0x80000007U:
            break;
        default:
            if (!service || !service(context, command, words + s->cursor, def.words, &value))
                return LORDLY_DIRECTOR_SERVICE;
            break;
        }
        if (value != -100) {
            if (!def.query) return LORDLY_DIRECTOR_INVALID;
            depth = s->nesting;
            status = lordly_director_compare(words, count, s->cursor, value,
                                             def.query - 3, s->state_scan_mode, &depth, &next);
            if (status) return LORDLY_DIRECTOR_INVALID;
            s->nesting = depth;
            s->cursor = next + 1;
        } else if (def.query) {
            /* -100 is the dispatcher's no-query sentinel, even from a producer. */
            ++s->cursor;
        } else s->cursor += def.words;
    }
    return LORDLY_DIRECTOR_BUDGET;
}
