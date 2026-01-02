#ifndef TRAIN_H
#define TRAIN_H

#include "hash.h"

#define STATION_LEN 64
#define TIME_LEN 8
#define DATE_LEN 12
#define ID_LEN 40

typedef struct {
    char name[STATION_LEN];
    char arrive[TIME_LEN];
    char depart[TIME_LEN];
    int distance;
} Stop;


typedef struct {
    char train_id[ID_LEN];
    char from[STATION_LEN];
    char to[STATION_LEN];
    char depart_time[TIME_LEN];
    double base_price;
    int running;
    Stop *stops;
    int stop_count;
    int duration_minutes;
    int seat_count[4];
    double seat_price_coef[4];

    void *seatmaps; 
    int seatmap_count;
    int seatmap_capacity;
} Train;


typedef struct {
    Train *data;
    int size;
    int capacity;
} TrainList;


void trainlist_init(TrainList *L);
void trainlist_free(TrainList *L);

int train_add(TrainList *L, Train *t);
int train_delete(TrainList *L, const char *train_id);
int train_update(TrainList *L, const char *train_id, Train *newt);

int train_find_index(TrainList *L, const char *train_id);
Train *train_get(TrainList *L, int idx);

void train_list_all(TrainList *L);


int train_allocate_seat(TrainList *TL, const char *train_id, const char *date,
                        const char *from, const char *to, int seat_class,
                        int *out_seat_index, int *out_from_idx, int *out_to_idx);

int train_release_seat(TrainList *TL, const char *train_id, const char *date,
                       int seat_class, int seat_index, int from_idx, int to_idx);

int train_find_stop_idx(Train *t, const char *station);

#endif 