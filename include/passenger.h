#ifndef PASSENGER_H
#define PASSENGER_H

#define NAME_LEN 64
#define ID_LEN 40

typedef struct {
    char id_type[32];
    char id_num[ID_LEN];
    char name[NAME_LEN];
    char phone[32];
    char emergency_contact[NAME_LEN];
    char emergency_phone[32];
} Passenger;

typedef struct {
    Passenger *data;
    int size;
    int capacity;
} PassengerList;

void passengerlist_init(PassengerList *L);
void passengerlist_free(PassengerList *L);

int passenger_add(PassengerList *L, Passenger *p);
int passenger_delete(PassengerList *L, const char *id_num);
int passenger_update(PassengerList *L, const char *id_num, Passenger *pnew);

int passenger_find_index(PassengerList *L, const char *id_num);

void passenger_list_all(PassengerList *L);

int save_passengers(const char *filename, PassengerList *L);
int load_passengers(const char *filename, PassengerList *L);

#endif 