#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "train.h"

/* Simple assertion helper */
#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
    else { printf("OK: %s\n", msg); } \
} while(0)

int main(void) {
    TrainList TL;
    trainlist_init(&TL);

    /* create a train with 4 stops: A B C D and class2 with 2 seats */
    Train t;
    memset(&t, 0, sizeof(t));
    strncpy(t.train_id, "T1", ID_LEN-1);
    strncpy(t.from, "A", STATION_LEN-1);
    strncpy(t.to, "D", STATION_LEN-1);
    strncpy(t.depart_time, "08:00", TIME_LEN-1);
    t.base_price = 100.0;
    t.running = 1;
    t.duration_minutes = 120;
    t.stop_count = 4;
    t.stops = malloc(sizeof(Stop) * t.stop_count);
    strncpy(t.stops[0].name, "A", STATION_LEN-1);
    strncpy(t.stops[1].name, "B", STATION_LEN-1);
    strncpy(t.stops[2].name, "C", STATION_LEN-1);
    strncpy(t.stops[3].name, "D", STATION_LEN-1);
    /* seat_count: only set class 2 to 2 seats */
    t.seat_count[0] = 0;
    t.seat_count[1] = 0;
    t.seat_count[2] = 2;
    t.seat_count[3] = 0;
    for (int i=0;i<4;i++) t.seat_price_coef[i]=1.0;

    ASSERT(train_add(&TL, &t) == 0, "train_add should succeed");

    int idx = train_find_index(&TL, "T1");
    ASSERT(idx >= 0, "train_find_index finds T1");

    Train *tp = train_get(&TL, idx);
    ASSERT(tp != NULL, "train_get returns non-null");

    int sidx = train_find_stop_idx(tp, "B");
    ASSERT(sidx == 1, "train_find_stop_idx B == 1");

    /* allocate two overlapping bookings on [A,C) => segments 0..1 */
    int seat_index, from_idx, to_idx;
    int r = train_allocate_seat(&TL, "T1", "2026-01-10", "A", "C", 2, &seat_index, &from_idx, &to_idx);
    ASSERT(r == 0 && seat_index == 0, "first allocation succeeds seat 0");

    r = train_allocate_seat(&TL, "T1", "2026-01-10", "A", "C", 2, &seat_index, &from_idx, &to_idx);
    ASSERT(r == 0 && seat_index == 1, "second allocation succeeds seat 1");

    r = train_allocate_seat(&TL, "T1", "2026-01-10", "A", "C", 2, &seat_index, &from_idx, &to_idx);
    ASSERT(r != 0, "third allocation fails (no seat)");

    /* release seat 0 */
    r = train_release_seat(&TL, "T1", "2026-01-10", 2, 0, 0, 2);
    ASSERT(r == 0, "release seat 0 succeeds");

    r = train_allocate_seat(&TL, "T1", "2026-01-10", "A", "C", 2, &seat_index, &from_idx, &to_idx);
    ASSERT(r == 0 && seat_index >= 0, "allocation after release succeeds");

    /* cleanup */
    trainlist_free(&TL);
    printf("ALL train tests passed\n");
    return 0;
}