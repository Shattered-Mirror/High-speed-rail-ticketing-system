#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "booking.h"
#include "hash.h"

#define INITIAL_CAPACITY 8
#define HASH_BUCKETS 1031

static HashTable *booking_ht = NULL;
static void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { perror("malloc"); exit(1);} return p; }

void bookinglist_init(BookingList *L) {
    L->data = xmalloc(sizeof(Booking) * INITIAL_CAPACITY);
    L->size = 0; L->capacity = INITIAL_CAPACITY;
    if (!booking_ht) booking_ht = ht_create(HASH_BUCKETS);
}

void bookinglist_free(BookingList *L) {
    if (!L) return;
    free(L->data);
    L->data = NULL;
    L->size = L->capacity = 0;
    if (booking_ht) { ht_free(booking_ht); booking_ht = NULL; }
}

static void bookinglist_expand(BookingList *L) {
    L->capacity *= 2;
    L->data = realloc(L->data, sizeof(Booking) * L->capacity);
    if (!L->data) { perror("realloc"); exit(1); }
}

static void rebuild(BookingList *L) {
    if (!booking_ht) booking_ht = ht_create(HASH_BUCKETS);
    else ht_clear(booking_ht);
    for (int i = 0; i < L->size; ++i) ht_insert(booking_ht, L->data[i].order_id, i);
}

int booking_find_index(BookingList *BL, const char *order_id) {
    if (booking_ht) {
        int idx = ht_find(booking_ht, order_id);
        if (idx != -1) return idx;
    }
    for (int i = 0; i < BL->size; ++i) if (strcmp(BL->data[i].order_id, order_id) == 0) return i;
    return -1;
}

static void generate_order_id(char *out, size_t outlen, BookingList *BL, const char *date, const char *train_id) {
    int serial = 0;
    for (int i = 0; i < BL->size; ++i) {
        Booking *b = &BL->data[i];
        if (strcmp(b->date, date) == 0 && strcmp(b->train_id, train_id) == 0) serial++;
    }
    snprintf(out, outlen, "%s-%s-%04d", date, train_id, serial + 1);
}

int booking_create(BookingList *BL, TrainList *TL, PassengerList *PL,
                   const char *date, const char *train_id,
                   const char *from, const char *to,
                   const char *passenger_id, int seat_class, char *out_order_id, size_t order_len) {

    int pidx = passenger_find_index(PL, passenger_id);
    if (pidx == -1) return -1;

    int seat_index, from_idx, to_idx;
    int res = train_allocate_seat(TL, train_id, date, from, to, seat_class, &seat_index, &from_idx, &to_idx);
    if (res != 0) return -2; 

 
    if (BL->size >= BL->capacity) bookinglist_expand(BL);
    Booking b;
    generate_order_id(b.order_id, sizeof(b.order_id), BL, date, train_id);
    strncpy(b.passenger_id, PL->data[pidx].id_num, ID_LEN-1);
    strncpy(b.passenger_name, PL->data[pidx].name, NAME_LEN-1);
    strncpy(b.date, date, DATE_LEN-1);
    strncpy(b.train_id, train_id, ID_LEN-1);
    strncpy(b.from, from, STATION_LEN-1);
    strncpy(b.to, to, STATION_LEN-1);

    int tidx = train_find_index(TL, train_id);
    if (tidx != -1) {
        Train *t = train_get(TL, tidx);
        strncpy(b.depart_time, t->depart_time, TIME_LEN-1);
        b.price = t->base_price * t->seat_price_coef[seat_class];
    } else {
        b.depart_time[0] = '\0';
        b.price = 0.0;
    }
    b.seat_class = seat_class;
    b.seat_index = seat_index;
    b.from_stop_idx = from_idx;
    b.to_stop_idx = to_idx;
    b.canceled = 0;
    snprintf(b.seat_no, sizeof(b.seat_no), "%d-%d", seat_class + 1, seat_index + 1);

    BL->data[BL->size++] = b;
    rebuild(BL);
    if (out_order_id) {
        strncpy(out_order_id, b.order_id, order_len-1); out_order_id[order_len-1] = 0;
    }
    return 0;
}

int booking_cancel(BookingList *BL, const char *order_id, TrainList *TL) {
    int idx = booking_find_index(BL, order_id);
    if (idx == -1) return -1;
    Booking *bk = &BL->data[idx];
    if (bk->canceled) return -2;

    int res = train_release_seat(TL, bk->train_id, bk->date, bk->seat_class, bk->seat_index, bk->from_stop_idx, bk->to_stop_idx);
    if (res != 0) {

        bk->canceled = 1;
        return -3;
    }
    bk->canceled = 1;
    return 0;
}

void booking_list_all(BookingList *L) {
    if (!L || L->size == 0) { printf("无订单信息\n"); return; }
    for (int i = 0; i < L->size; ++i) {
        Booking *b = &L->data[i];
        printf("---- [%d] ----\n", i+1);
        printf("订单号:%s  乘客:%s(%s)  日期:%s  车次:%s  %s->%s  发车:%s  票价:%.2f  座位:%s  等级:%d  状态:%s\n",
               b->order_id, b->passenger_name, b->passenger_id, b->date, b->train_id,
               b->from, b->to, b->depart_time, b->price, b->seat_no, b->seat_class,
               b->canceled ? "已退票" : "已出票");
    }
}