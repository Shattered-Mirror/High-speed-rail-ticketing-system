#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "passenger.h"
#include "hash.h"

#define INITIAL_CAPACITY 8
#define HASH_BUCKETS 1031

static HashTable *passenger_ht = NULL;
static void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { perror("malloc"); exit(1);} return p; }

void passengerlist_init(PassengerList *L) {
    L->data = xmalloc(sizeof(Passenger) * INITIAL_CAPACITY);
    L->size = 0; L->capacity = INITIAL_CAPACITY;
    if (!passenger_ht) passenger_ht = ht_create(HASH_BUCKETS);
}

void passengerlist_free(PassengerList *L) {
    if (!L) return;
    free(L->data);
    L->data = NULL;
    L->size = L->capacity = 0;
    if (passenger_ht) { ht_free(passenger_ht); passenger_ht = NULL; }
}

static void passengerlist_expand(PassengerList *L) {
    L->capacity *= 2;
    L->data = realloc(L->data, sizeof(Passenger) * L->capacity);
    if (!L->data) { perror("realloc"); exit(1); }
}

static void rebuild(PassengerList *L) {
    if (!passenger_ht) passenger_ht = ht_create(HASH_BUCKETS);
    else ht_clear(passenger_ht);
    for (int i = 0; i < L->size; ++i) ht_insert(passenger_ht, L->data[i].id_num, i);
}

int passenger_add(PassengerList *L, Passenger *p) {
    if (L->size >= L->capacity) passengerlist_expand(L);
    L->data[L->size++] = *p;
    rebuild(L);
    return 0;
}

int passenger_delete(PassengerList *L, const char *id_num) {
    int idx = passenger_find_index(L, id_num);
    if (idx == -1) return -1;
    for (int i = idx; i < L->size - 1; ++i) L->data[i] = L->data[i+1];
    L->size--;
    rebuild(L);
    return 0;
}

int passenger_update(PassengerList *L, const char *id_num, Passenger *pnew) {
    int idx = passenger_find_index(L, id_num);
    if (idx == -1) return -1;
    L->data[idx] = *pnew;
    rebuild(L);
    return 0;
}

int passenger_find_index(PassengerList *L, const char *id_num) {
    if (passenger_ht) {
        int idx = ht_find(passenger_ht, id_num);
        if (idx != -1) return idx;
    }
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].id_num, id_num) == 0) return i;
    return -1;
}

void passenger_list_all(PassengerList *L) {
    if (!L || L->size == 0) { printf("无乘客信息\n"); return; }
    for (int i = 0; i < L->size; ++i) {
        Passenger *p = &L->data[i];
        printf("---- [%d] ----\n", i+1);
        printf("姓名:%s  证件:%s(%s)  手机:%s  紧急联系人:%s(%s)\n",
               p->name, p->id_num, p->id_type, p->phone, p->emergency_contact, p->emergency_phone);
    }
}