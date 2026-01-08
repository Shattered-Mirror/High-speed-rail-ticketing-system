#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "train.h"
#include "hash.h"

typedef struct {
	char date[DATE_LEN];
	int segment_count;
	unsigned char **class_seat_seg;
} TrainDateSeatMap;

#define INITIAL_CAPACITY 8
#define HASH_BUCKETS 1031

static HashTable *train_ht = NULL;
static void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { perror("malloc"); exit(1);} return p; }

void trainlist_init(TrainList *L)
{
	L->data = xmalloc(sizeof(Train) * INITIAL_CAPACITY);
	L->size = 0;
	L->capacity = INITIAL_CAPACITY;

	if (!train_ht)
		train_ht = ht_create(HASH_BUCKETS);
}

void trainlist_free(TrainList *L)
{
	if (!L)
		return;

	for (int i = 0; i < L->size; ++i) {
		free(L->data[i].stops);
		TrainDateSeatMap *sm = (TrainDateSeatMap*)L->data[i].seatmaps;
		if (sm) {
			for (int j = 0; j < L->data[i].seatmap_count; ++j) {
				for (int c = 0; c < 4; ++c)
					if (sm[j].class_seat_seg[c])
						free(sm[j].class_seat_seg[c]);
				free(sm[j].class_seat_seg);
			}
			free(sm);
		}
	}

	free(L->data);
	L->data = NULL;
	L->size = L->capacity = 0;

	if (train_ht) {
		ht_free(train_ht);
		train_ht = NULL;
	}
}

static void trainlist_expand(TrainList *L)
{
	L->capacity *= 2;
	L->data = realloc(L->data, sizeof(Train) * L->capacity);
	if (!L->data) { perror("realloc"); exit(1); }
}

static void rebuild_index(TrainList *L)
{
	if (!train_ht)
		train_ht = ht_create(HASH_BUCKETS);
	else
		ht_clear(train_ht);

	for (int i = 0; i < L->size; ++i)
		ht_insert(train_ht, L->data[i].train_id, i);
}

static int train_find_seatmap_idx_internal(Train *t, const char *date)
{
	TrainDateSeatMap *sm = (TrainDateSeatMap*)t->seatmaps;
	for (int i = 0; i < t->seatmap_count; ++i)
		if (strcmp(sm[i].date, date) == 0)
			return i;
	return -1;
}

static int train_create_seatmap_if_missing_internal(Train *t, const char *date)
{
	int idx = train_find_seatmap_idx_internal(t, date);
	if (idx != -1)
		return idx;

	if (t->seatmap_capacity == 0) {
		t->seatmap_capacity = 4;
		t->seatmaps = xmalloc(sizeof(TrainDateSeatMap) * t->seatmap_capacity);
	} else if (t->seatmap_count >= t->seatmap_capacity) {
		t->seatmap_capacity *= 2;
		t->seatmaps = realloc(t->seatmaps, sizeof(TrainDateSeatMap) * t->seatmap_capacity);
		if (!t->seatmaps) { perror("realloc"); exit(1); }
	}

	TrainDateSeatMap *sm = (TrainDateSeatMap*)t->seatmaps + t->seatmap_count;
	strncpy(sm->date, date, DATE_LEN-1);
	sm->date[DATE_LEN-1] = 0;
	sm->segment_count = (t->stop_count >= 1) ? (t->stop_count - 1) : 0;
	sm->class_seat_seg = xmalloc(sizeof(unsigned char*) * 4);
	for (int c = 0; c < 4; ++c) {
		int sc = t->seat_count[c];
		if (sc > 0 && sm->segment_count > 0) {
			sm->class_seat_seg[c] = calloc(sc * sm->segment_count, 1);
			if (!sm->class_seat_seg[c]) { perror("calloc"); exit(1); }
		} else
			sm->class_seat_seg[c] = NULL;
	}
	t->seatmap_count++;
	return t->seatmap_count - 1;
}

static int seatmap_allocate_internal(TrainDateSeatMap *sm, Train *t, int seat_class, int from_idx, int to_idx)
{
	if (!sm)
		return -1;

	int segs = sm->segment_count;
	if (from_idx < 0 || to_idx <= from_idx || to_idx > segs)
		return -1;

	int sc = t->seat_count[seat_class];
	unsigned char *arr = sm->class_seat_seg[seat_class];
	if (!arr)
		return -1;

	for (int s = 0; s < sc; ++s) {
		int base = s * segs;
		int ok = 1;
		for (int seg = from_idx; seg < to_idx; ++seg)
			if (arr[base + seg]) { ok = 0; break; }
		if (ok) {
			for (int seg = from_idx; seg < to_idx; ++seg)
				arr[base + seg] = 1;
			return s;
		}
	}

	return -1;
}

static void seatmap_release_internal(TrainDateSeatMap *sm, Train *t, int seat_class, int seat_index, int from_idx, int to_idx)
{
	if (!sm)
		return;

	int segs = sm->segment_count;
	unsigned char *arr = sm->class_seat_seg[seat_class];
	if (!arr)
		return;

	int base = seat_index * segs;
	for (int seg = from_idx; seg < to_idx; ++seg)
		arr[base + seg] = 0;
}

static int seatmap_mark_index_internal(TrainDateSeatMap *sm, Train *t, int seat_class, int seat_index, int from_idx, int to_idx)
{
	if (!sm)
		return -1;

	int segs = sm->segment_count;
	if (!sm->class_seat_seg[seat_class])
		return -1;
	if (seat_index < 0 || seat_index >= t->seat_count[seat_class])
		return -1;
	if (from_idx < 0 || to_idx <= from_idx || to_idx > segs)
		return -1;

	unsigned char *arr = sm->class_seat_seg[seat_class];
	int base = seat_index * segs;
	for (int seg = from_idx; seg < to_idx; ++seg)
		arr[base + seg] = 1;
	return 0;
}

int train_add(TrainList *L, Train *t)
{
	if (L->size >= L->capacity)
		trainlist_expand(L);

	t->seatmaps = NULL;
	t->seatmap_count = 0;
	t->seatmap_capacity = 0;
	L->data[L->size++] = *t;
	rebuild_index(L);
	return 0;
}

int train_delete(TrainList *L, const char *train_id)
{
	int idx = train_find_index(L, train_id);
	if (idx == -1)
		return -1;

	free(L->data[idx].stops);
	TrainDateSeatMap *sm = (TrainDateSeatMap*)L->data[idx].seatmaps;
	if (sm) {
		for (int i = 0; i < L->data[idx].seatmap_count; ++i) {
			for (int c = 0; c < 4; ++c)
				if (sm[i].class_seat_seg[c])
					free(sm[i].class_seat_seg[c]);
			free(sm[i].class_seat_seg);
		}
		free(sm);
	}

	for (int i = idx; i < L->size - 1; ++i)
		L->data[i] = L->data[i+1];

	L->size--;
	rebuild_index(L);
	return 0;
}

int train_update(TrainList *L, const char *train_id, Train *newt)
{
	int idx = train_find_index(L, train_id);
	if (idx == -1)
		return -1;

	free(L->data[idx].stops);
	TrainDateSeatMap *sm = (TrainDateSeatMap*)L->data[idx].seatmaps;
	if (sm) {
		for (int i = 0; i < L->data[idx].seatmap_count; ++i) {
			for (int c = 0; c < 4; ++c)
				if (sm[i].class_seat_seg[c])
					free(sm[i].class_seat_seg[c]);
			free(sm[i].class_seat_seg);
		}
		free(sm);
	}

	newt->seatmaps = NULL;
	newt->seatmap_count = 0;
	newt->seatmap_capacity = 0;
	L->data[idx] = *newt;
	rebuild_index(L);
	return 0;
}

int train_find_index(TrainList *L, const char *train_id)
{
	if (train_ht) {
		int idx = ht_find(train_ht, train_id);
		if (idx != -1)
			return idx;
	}

	for (int i = 0; i < L->size; ++i)
		if (strcmp(L->data[i].train_id, train_id) == 0)
			return i;

	return -1;
}

Train *train_get(TrainList *L, int idx)
{
	if (idx < 0 || idx >= L->size)
		return NULL;
	return &L->data[idx];
}

void train_list_all(TrainList *L)
{
	if (!L || L->size == 0) {
		printf("无车次信息\n");
		return;
	}

	for (int i = 0; i < L->size; ++i) {
		Train *t = &L->data[i];
		printf("---- [%d] ----\n", i+1);
		printf("车次: %s  %s -> %s   发车: %s   基价: %.2f  状态: %s\n",
		       t->train_id, t->from, t->to, t->depart_time, t->base_price, t->running ? "运行":"停运");
		printf(" 停靠站(%d)  座位(B/1/2/S): %d/%d/%d/%d\n", t->stop_count, t->seat_count[0], t->seat_count[1], t->seat_count[2], t->seat_count[3]);
		printf(" 已注册日期 seatmaps: %d\n", t->seatmap_count);
	}
}

int train_find_stop_idx(Train *t, const char *station)
{
	for (int i = 0; i < t->stop_count; ++i)
		if (strcmp(t->stops[i].name, station) == 0)
			return i;
	return -1;
}

int train_allocate_seat(TrainList *TL, const char *train_id, const char *date,
                        const char *from, const char *to, int seat_class,
                        int *out_seat_index, int *out_from_idx, int *out_to_idx)
{
	int tidx = train_find_index(TL, train_id);
	if (tidx == -1)
		return -1;

	Train *t = &TL->data[tidx];
	int fidx = train_find_stop_idx(t, from);
	int tidx_stop = train_find_stop_idx(t, to);
	if (fidx == -1 || tidx_stop == -1)
		return -1;
	if (!(fidx < tidx_stop))
		return -1;

	int sm_idx = train_create_seatmap_if_missing_internal(t, date);
	TrainDateSeatMap *sm = (TrainDateSeatMap*)t->seatmaps + sm_idx;
	int seat_idx = seatmap_allocate_internal(sm, t, seat_class, fidx, tidx_stop);
	if (seat_idx == -1)
		return -1;

	if (out_seat_index)
		*out_seat_index = seat_idx;
	if (out_from_idx)
		*out_from_idx = fidx;
	if (out_to_idx)
		*out_to_idx = tidx_stop;
	return 0;
}

int train_release_seat(TrainList *TL, const char *train_id, const char *date,
                       int seat_class, int seat_index, int from_idx, int to_idx)
{
	int tidx = train_find_index(TL, train_id);
	if (tidx == -1)
		return -1;

	Train *t = &TL->data[tidx];
	int sm_idx = train_find_seatmap_idx_internal(t, date);
	if (sm_idx == -1)
		return -1;

	TrainDateSeatMap *sm = (TrainDateSeatMap*)t->seatmaps + sm_idx;
	seatmap_release_internal(sm, t, seat_class, seat_index, from_idx, to_idx);
	return 0;
}

int train_mark_seat(TrainList *TL, const char *train_id, const char *date,
                    int seat_class, int seat_index, int from_idx, int to_idx)
{
	int tidx = train_find_index(TL, train_id);
	if (tidx == -1)
		return -1;

	Train *t = &TL->data[tidx];
	int sm_idx = train_create_seatmap_if_missing_internal(t, date);
	TrainDateSeatMap *sm = (TrainDateSeatMap*)t->seatmaps + sm_idx;
	return seatmap_mark_index_internal(sm, t, seat_class, seat_index, from_idx, to_idx);
}

int save_trains(const char *filename, TrainList *L)
{
	FILE *f = fopen(filename, "w");
	if (!f)
		return 0;

	fprintf(f, "%d\n", L->size);
	for (int i = 0; i < L->size; ++i) {
		Train *t = &L->data[i];
		fprintf(f, "%s|%s|%s|%s|%.2f|%d|%d|%d|%d|%d|%d|%d|%.3f|%.3f|%.3f|%.3f\n",
		        t->train_id, t->from, t->to, t->depart_time, t->base_price, t->running,
		        t->duration_minutes, t->stop_count, t->seat_count[0], t->seat_count[1],
		        t->seat_count[2], t->seat_count[3],
		        t->seat_price_coef[0], t->seat_price_coef[1], t->seat_price_coef[2], t->seat_price_coef[3]);
		for (int j = 0; j < t->stop_count; ++j) {
			Stop *s = &t->stops[j];
			fprintf(f, "%s|%s|%s|%d\n", s->name, s->arrive, s->depart, s->distance);
		}
	}
	fclose(f);
	return 1;
}

int load_trains(const char *filename, TrainList *L)
{
	FILE *f = fopen(filename, "r");
	if (!f)
		return 0;

	int count = 0;
	if (fscanf(f, "%d\n", &count) != 1) { fclose(f); return 0; }
	char line[1024];

	for (int i = 0; i < count; ++i) {
		if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
		size_t ln = strlen(line);
		if (ln && line[ln-1]=='\n')
			line[ln-1]=0;

		char *parts[20];
		int p = 0;
		char *tok = strtok(line, "|");
		while (tok && p < 20) { parts[p++] = tok; tok = strtok(NULL, "|"); }
		if (p < 16) { fclose(f); return 0; }

		Train t;
		memset(&t, 0, sizeof(t));
		strncpy(t.train_id, parts[0], ID_LEN-1);
		strncpy(t.from, parts[1], STATION_LEN-1);
		strncpy(t.to, parts[2], STATION_LEN-1);
		strncpy(t.depart_time, parts[3], TIME_LEN-1);
		t.base_price = atof(parts[4]);
		t.running = atoi(parts[5]);
		t.duration_minutes = atoi(parts[6]);
		t.stop_count = atoi(parts[7]);
		t.seat_count[0] = atoi(parts[8]); t.seat_count[1] = atoi(parts[9]);
		t.seat_count[2] = atoi(parts[10]); t.seat_count[3] = atoi(parts[11]);
		t.seat_price_coef[0] = atof(parts[12]); t.seat_price_coef[1] = atof(parts[13]);
		t.seat_price_coef[2] = atof(parts[14]); t.seat_price_coef[3] = atof(parts[15]);

		if (t.stop_count > 0) {
			t.stops = xmalloc(sizeof(Stop) * t.stop_count);
			for (int j = 0; j < t.stop_count; ++j) {
				if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
				size_t ln2 = strlen(line);
				if (ln2 && line[ln2-1]=='\n') line[ln2-1]=0;
				char *sp[4];
				int q = 0;
				char *tok2 = strtok(line, "|");
				while (tok2 && q < 4) { sp[q++] = tok2; tok2 = strtok(NULL, "|"); }
				if (q < 4) { fclose(f); return 0; }
				strncpy(t.stops[j].name, sp[0], STATION_LEN-1);
				strncpy(t.stops[j].arrive, sp[1], TIME_LEN-1);
				strncpy(t.stops[j].depart, sp[2], TIME_LEN-1);
				t.stops[j].distance = atoi(sp[3]);
			}
		} else {
			t.stops = NULL;
		}

		t.seatmaps = NULL;
		t.seatmap_capacity = 0;
		t.seatmap_count = 0;
		train_add(L, &t);
	}

	fclose(f);
	return 1;
}