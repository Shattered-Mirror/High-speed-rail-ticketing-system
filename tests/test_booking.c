#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "booking.h"

#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
    else { printf("OK: %s\n", msg); } \
} while(0)

int main(void) {
    TrainList TL; PassengerList PL; BookingList BL;
    trainlist_init(&TL); passengerlist_init(&PL); bookinglist_init(&BL);

    /* create train with single seat in class 2 */
    Train t;
    memset(&t, 0, sizeof(t));
    strncpy(t.train_id, "T100", ID_LEN-1);
    strncpy(t.from, "A", STATION_LEN-1);
    strncpy(t.to, "B", STATION_LEN-1);
    strncpy(t.depart_time, "09:00", TIME_LEN-1);
    t.base_price = 50.0;
    t.running = 1;
    t.duration_minutes = 60;
    t.stop_count = 2;
    t.stops = malloc(sizeof(Stop) * t.stop_count);
    strncpy(t.stops[0].name, "A", STATION_LEN-1);
    strncpy(t.stops[1].name, "B", STATION_LEN-1);
    t.seat_count[0] = 0; t.seat_count[1] = 0; t.seat_count[2] = 1; t.seat_count[3] = 0;
    for (int i=0;i<4;i++) t.seat_price_coef[i]=1.0;
    ASSERT(train_add(&TL, &t) == 0, "train added");

    /* add passenger */
    Passenger p;
    memset(&p,0,sizeof(p));
    strncpy(p.id_type, "ID", sizeof(p.id_type)-1);
    strncpy(p.id_num, "PX", sizeof(p.id_num)-1);
    strncpy(p.name, "Bob", sizeof(p.name)-1);
    ASSERT(passenger_add(&PL, &p) == 0, "passenger added");

    /* first booking should succeed */
    char order_id[ORDER_ID_LEN];
    int res = booking_create(&BL, &TL, &PL, "2026-01-11", "T100", "A", "B", "PX", 2, order_id, sizeof(order_id));
    ASSERT(res == 0, "first booking success");
    ASSERT(strlen(order_id) > 0, "order id produced");

    /* second booking same seat should fail (no seat) */
    char order2[ORDER_ID_LEN];
    res = booking_create(&BL, &TL, &PL, "2026-01-11", "T100", "A", "B", "PX", 2, order2, sizeof(order2));
    ASSERT(res == -2, "second booking fails due to no seat");

    /* cancel first booking */
    res = booking_cancel(&BL, order_id, &TL);
    ASSERT(res == 0, "cancel booking succeeded");

    /* after cancel, booking again should succeed */
    char order3[ORDER_ID_LEN];
    res = booking_create(&BL, &TL, &PL, "2026-01-11", "T100", "A", "B", "PX", 2, order3, sizeof(order3));
    ASSERT(res == 0, "booking after cancel succeeds");

    bookinglist_free(&BL);
    passengerlist_free(&PL);
    trainlist_free(&TL);
    printf("ALL booking tests passed\n");
    return 0;
}