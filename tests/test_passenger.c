#include <stdio.h>
#include <string.h>
#include "passenger.h"

#define ASSERT(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
    else { printf("OK: %s\n", msg); } \
} while(0)

int main(void) {
    PassengerList PL;
    passengerlist_init(&PL);

    Passenger p;
    memset(&p, 0, sizeof(p));
    strncpy(p.id_type, "身份证", sizeof(p.id_type)-1);
    strncpy(p.id_num, "P123", sizeof(p.id_num)-1);
    strncpy(p.name, "Alice", sizeof(p.name)-1);
    strncpy(p.phone, "1380000", sizeof(p.phone)-1);

    ASSERT(passenger_add(&PL, &p) == 0, "passenger_add returns 0");

    int idx = passenger_find_index(&PL, "P123");
    ASSERT(idx >= 0, "passenger_find_index finds P123");

    Passenger p2 = PL.data[idx];
    ASSERT(strcmp(p2.name, "Alice") == 0, "passenger name matches");

    /* update phone */
    strncpy(p2.phone, "1391111", sizeof(p2.phone)-1);
    ASSERT(passenger_update(&PL, "P123", &p2) == 0, "passenger_update returns 0");
    int idx2 = passenger_find_index(&PL, "P123");
    ASSERT(strcmp(PL.data[idx2].phone, "1391111") == 0, "phone updated");

    ASSERT(passenger_delete(&PL, "P123") == 0, "passenger_delete returns 0");
    ASSERT(passenger_find_index(&PL, "P123") == -1, "passenger not found after delete");

    passengerlist_free(&PL);
    printf("ALL passenger tests passed\n");
    return 0;
}