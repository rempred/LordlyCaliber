#include "route_path.h"
#include <string.h>

/* Portable adaptation of accepted func_00107E38 and func_00107D60 in
 * accepted/library/src/lib/movement_107d60_107e38.c (CURRENT 1E95BD03).
 * Preserve neighbor order, destination exception, directed terrain transitions,
 * diagonal guard, unsigned 10/14 costs and strict improvement comparisons.
 * The original queue helpers (ROM107850..107D60) order by (cost, cell index).
 * Host-owned heap replaces that tree, allocator, 32-bit pointers and frame yield;
 * the synchronous search has at most 4096 settled cells. No game code is patched.
 */
typedef struct RouteSearch {
    uint32_t cost[4096];
    int parent[4096], heap[4096], position[4096], size;
    uint8_t visited[4096];
} RouteSearch;
static int less(const RouteSearch *s, int a, int b) {
    return s->cost[a] < s->cost[b] || (s->cost[a] == s->cost[b] && a < b);
}
static void exchange(RouteSearch *s, int a, int b) {
    int cell = s->heap[a];
    s->heap[a] = s->heap[b]; s->heap[b] = cell;
    s->position[s->heap[a]] = a; s->position[cell] = b;
}
static void enqueue(RouteSearch *s, int cell) {
    int at = s->position[cell];
    if (at < 0) { at = s->size++; s->heap[at] = cell; s->position[cell] = at; }
    while (at && less(s, s->heap[at], s->heap[(at - 1) / 2])) {
        int up = (at - 1) / 2; exchange(s, at, up); at = up;
    }
}
static int dequeue(RouteSearch *s) {
    int cell, at = 0;
    if (!s->size) return -1;
    cell = s->heap[0]; s->position[cell] = -1;
    if (--s->size) {
        s->heap[0] = s->heap[s->size]; s->position[s->heap[0]] = 0;
        for (;;) {
            int child = at * 2 + 1;
            if (child >= s->size) break;
            if (child + 1 < s->size && less(s, s->heap[child + 1], s->heap[child])) ++child;
            if (!less(s, s->heap[child], s->heap[at])) break;
            exchange(s, at, child); at = child;
        }
    }
    return cell;
}
int lordly_route_path(const uint8_t terrain[4096], const uint16_t costs[32],
                      const uint8_t transitions[1024], const uint8_t *blocked,
                      int start, int goal, uint16_t path[4096], int *count,
                      uint32_t *cost) {
    static const int dx[8] = {-1,0,1,-1,1,-1,0,1};
    static const int dy[8] = {-1,-1,-1,0,0,1,1,1};
    RouteSearch s;
    int i, current = start;
    if (!count || !cost) return -1;
    *count = 0; *cost = 0;
    if (!terrain || !costs || !transitions || !path || start < 0 || start >= 4096 || goal < 0 || goal >= 4096) return -1;
    for (i = 0; i < 4096; ++i) if (terrain[i] >= 32) return -1;
    memset(&s, 0, sizeof(s));
    for (i = 0; i < 4096; ++i) { s.cost[i] = UINT32_MAX; s.parent[i] = -1; s.position[i] = -1; }
    s.cost[start] = 0;
    for (;;) {
        const int x = current % 64, y = current / 64;
        const int currentBlocked = costs[terrain[current]] == UINT16_MAX;
        s.visited[current] = 1;
        for (i = 0; i < 8; ++i) {
            int nx = x + dx[i], ny = y + dy[i], next;
            uint32_t step, sum;
            if (nx < 0 || nx >= 64 || ny < 0 || ny >= 64) continue;
            next = ny * 64 + nx;
            if (dx[i] && dy[i] && currentBlocked &&
                costs[terrain[current + dx[i]]] == UINT16_MAX &&
                costs[terrain[current + dy[i] * 64]] == UINT16_MAX) continue;
            if (s.visited[next] || (blocked && blocked[next])) continue;
            if (next == goal) step = 1;
            else {
                step = costs[terrain[next]];
                if (step == UINT16_MAX || transitions[terrain[current] * 32 + terrain[next]]) continue;
            }
            step *= dx[i] && dy[i] ? 14U : 10U;
            sum = s.cost[current] + step;
            if (sum < s.cost[next]) {
                s.cost[next] = sum; s.parent[next] = current; enqueue(&s, next);
            }
        }
        if (current == goal) break;
        current = dequeue(&s);
        if (current == -1) return 0;
    }
    i = 0; current = goal;
    for (;;) {
        if (i == 4096 || current < 0) return -1;
        path[i++] = (uint16_t)current;
        if (current == start) break;
        current = s.parent[current];
    }
    *count = i; *cost = s.cost[goal];
    for (i = 0; i < *count / 2; ++i) {
        uint16_t swap = path[i]; path[i] = path[*count - i - 1]; path[*count - i - 1] = swap;
    }
    return 1;
}
