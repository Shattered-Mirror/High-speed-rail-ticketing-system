#ifndef BOOKING_H
#define BOOKING_H

#include "train.h"
#include "passenger.h"

#define ORDER_ID_LEN 64

typedef struct {
    char order_id[ORDER_ID_LEN];
    char passenger_id[ID_LEN];
    char passenger_name[NAME_LEN];
    char date[DATE_LEN];
    char train_id[ID_LEN];
    char from[STATION_LEN];
    char to[STATION_LEN];
    char depart_time[TIME_LEN];
    double price;
    char seat_no[16];
    int seat_class;
    int seat_index;   
    int from_stop_idx; 
    int to_stop_idx;
    int canceled;
} Booking;

typedef struct {
    Booking *data;
    int size;
    int capacity;
} BookingList;

void bookinglist_init(BookingList *L);
void bookinglist_free(BookingList *L);

int booking_create(BookingList *BL, TrainList *TL, PassengerList *PL,
                   const char *date, const char *train_id,
                   const char *from, const char *to,
                   const char *passenger_id, int seat_class, char *out_order_id, size_t order_len);

int booking_cancel(BookingList *BL, const char *order_id, TrainList *TL);

int booking_find_index(BookingList *BL, const char *order_id);

void booking_list_all(BookingList *L);


int save_bookings(const char *filename, BookingList *L);
int load_bookings(const char *filename, BookingList *L, TrainList *TL);

#endif /* BOOKING_H */