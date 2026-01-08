#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "booking.h"
#include "hash.h"

#define INITIAL_CAPACITY 8
#define HASH_BUCKETS 1031

static HashTable *booking_ht = NULL;

static void *xmalloc(size_t n)
{
	if (!n) {
		return NULL;
	}

	void *p = malloc(n);
	if (!p) {
		perror("malloc");
		exit(1);
	}

	return p;
}

void bookinglist_init(BookingList *L)
{
	L->data = xmalloc(sizeof(Booking) * INITIAL_CAPACITY);
	L->size = 0;
	L->capacity = INITIAL_CAPACITY;

	if (!booking_ht)
		booking_ht = ht_create(HASH_BUCKETS);
}

void bookinglist_free(BookingList *L)
{
	if (!L)
		return;

	free(L->data);
	L->data = NULL;
	L->size = L->capacity = 0;

	if (booking_ht) {
		ht_free(booking_ht);
		booking_ht = NULL;
	}
}

static void bookinglist_expand(BookingList *L)
{
	L->capacity *= 2;
	L->data = realloc(L->data, sizeof(Booking) * L->capacity);

	if (!L->data) {
		perror("realloc");
		exit(1);
	}
}

static void rebuild(BookingList *L)
{
	if (!booking_ht)
		booking_ht = ht_create(HASH_BUCKETS);
	else
		ht_clear(booking_ht);

	for (int i = 0; i < L->size; ++i)
		ht_insert(booking_ht, L->data[i].order_id, i);
}

int booking_find_index(BookingList *BL, const char *order_id)
{
	if (booking_ht) {
		int idx = ht_find(booking_ht, order_id);
		if (idx != -1)
			return idx;
	}

	for (int i = 0; i < BL->size; ++i)
		if (strcmp(BL->data[i].order_id, order_id) == 0)
			return i;

	return -1;
}

static void generate_order_id(char *out, size_t outlen, BookingList *BL,
			      const char *date, const char *train_id)
{
	int serial = 0;

	for (int i = 0; i < BL->size; ++i) {
		Booking *b = &BL->data[i];
		if (strcmp(b->date, date) == 0 && strcmp(b->train_id, train_id) == 0)
			serial++;
	}

	snprintf(out, outlen, "%s-%s-%04d", date, train_id, serial + 1);
}

int booking_create(BookingList *BL, TrainList *TL, PassengerList *PL,
		   const char *date, const char *train_id,
		   const char *from, const char *to,
		   const char *passenger_id, int seat_class, char *out_order_id, size_t order_len)
{
	int pidx = passenger_find_index(PL, passenger_id);
	if (pidx == -1)
		return -1;

	int seat_index, from_idx, to_idx;
	int res = train_allocate_seat(TL, train_id, date, from, to, seat_class,
				      &seat_index, &from_idx, &to_idx);
	if (res != 0)
		return -2;

	if (BL->size >= BL->capacity)
		bookinglist_expand(BL);

	Booking b;

	generate_order_id(b.order_id, sizeof(b.order_id), BL, date, train_id);

	strncpy(b.passenger_id, PL->data[pidx].id_num, ID_LEN - 1);
	b.passenger_id[ID_LEN - 1] = '\0';

	strncpy(b.passenger_name, PL->data[pidx].name, NAME_LEN - 1);
	b.passenger_name[NAME_LEN - 1] = '\0';

	strncpy(b.date, date, DATE_LEN - 1);
	b.date[DATE_LEN - 1] = '\0';

	strncpy(b.train_id, train_id, ID_LEN - 1);
	b.train_id[ID_LEN - 1] = '\0';

	strncpy(b.from, from, STATION_LEN - 1);
	b.from[STATION_LEN - 1] = '\0';

	strncpy(b.to, to, STATION_LEN - 1);
	b.to[STATION_LEN - 1] = '\0';

	int tidx = train_find_index(TL, train_id);
	if (tidx != -1) {
		Train *t = train_get(TL, tidx);
		strncpy(b.depart_time, t->depart_time, TIME_LEN - 1);
		b.depart_time[TIME_LEN - 1] = '\0';
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
		strncpy(out_order_id, b.order_id, order_len - 1);
		out_order_id[order_len - 1] = '\0';
	}

	return 0;
}

int booking_cancel(BookingList *BL, const char *order_id, TrainList *TL)
{
	int idx = booking_find_index(BL, order_id);
	if (idx == -1)
		return -1;

	Booking *bk = &BL->data[idx];
	if (bk->canceled)
		return -2;

	int res = train_release_seat(TL, bk->train_id, bk->date, bk->seat_class,
				     bk->seat_index, bk->from_stop_idx, bk->to_stop_idx);
	if (res != 0) {
		bk->canceled = 1;
		return -3;
	}

	bk->canceled = 1;
	return 0;
}

void booking_list_all(BookingList *L)
{
	if (!L || L->size == 0) {
		printf("无订单信息\n");
		return;
	}

	for (int i = 0; i < L->size; ++i) {
		Booking *b = &L->data[i];
		printf("---- [%d] ----\n", i + 1);
		printf("订单号:%s  乘客:%s(%s)  日期:%s  车次:%s  %s->%s  发车:%s  票价:%.2f  座位:%s  等级:%d  状态:%s\n",
		       b->order_id, b->passenger_name, b->passenger_id, b->date, b->train_id,
		       b->from, b->to, b->depart_time, b->price, b->seat_no, b->seat_class,
		       b->canceled ? "已退票" : "已出票");
	}
}

int save_bookings(const char *filename, BookingList *L)
{
	FILE *f = fopen(filename, "w");
	if (!f)
		return 0;

	fprintf(f, "%d\n", L->size);

	for (int i = 0; i < L->size; ++i) {
		Booking *b = &L->data[i];
		fprintf(f, "%s|%s|%s|%s|%s|%s|%s|%s|%.2f|%s|%d|%d|%d|%d|%d\n",
			b->order_id, b->passenger_id, b->passenger_name, b->date, b->train_id,
			b->from, b->to, b->depart_time, b->price, b->seat_no, b->seat_class,
			b->seat_index, b->from_stop_idx, b->to_stop_idx, b->canceled);
	}

	fclose(f);
	return 1;
}

int load_bookings(const char *filename, BookingList *L, TrainList *TL)
{
	FILE *f = fopen(filename, "r");
	if (!f)
		return 0;

	int count = 0;
	if (fscanf(f, "%d\n", &count) != 1) {
		fclose(f);
		return 0;
	}

	char line[2048];

	for (int i = 0; i < count; ++i) {
		if (!fgets(line, sizeof(line), f)) {
			fclose(f);
			return 0;
		}

		size_t ln = strlen(line);
		if (ln && line[ln - 1] == '\n')
			line[ln - 1] = '\0';

		char *parts[16];
		int p = 0;
		char *tok = strtok(line, "|");
		while (tok && p < 16) {
			parts[p++] = tok;
			tok = strtok(NULL, "|");
		}

		if (p < 15) {
			fclose(f);
			return 0;
		}

		Booking b;
		memset(&b, 0, sizeof(b));

		strncpy(b.order_id, parts[0], ORDER_ID_LEN - 1);
		b.order_id[ORDER_ID_LEN - 1] = '\0';

		strncpy(b.passenger_id, parts[1], ID_LEN - 1);
		b.passenger_id[ID_LEN - 1] = '\0';

		strncpy(b.passenger_name, parts[2], NAME_LEN - 1);
		b.passenger_name[NAME_LEN - 1] = '\0';

		strncpy(b.date, parts[3], DATE_LEN - 1);
		b.date[DATE_LEN - 1] = '\0';

		strncpy(b.train_id, parts[4], ID_LEN - 1);
		b.train_id[ID_LEN - 1] = '\0';

		strncpy(b.from, parts[5], STATION_LEN - 1);
		b.from[STATION_LEN - 1] = '\0';

		strncpy(b.to, parts[6], STATION_LEN - 1);
		b.to[STATION_LEN - 1] = '\0';

		strncpy(b.depart_time, parts[7], TIME_LEN - 1);
		b.depart_time[TIME_LEN - 1] = '\0';

		b.price = atof(parts[8]);

		strncpy(b.seat_no, parts[9], sizeof(b.seat_no) - 1);
		b.seat_no[sizeof(b.seat_no) - 1] = '\0';

		b.seat_class = atoi(parts[10]);
		b.seat_index = atoi(parts[11]);
		b.from_stop_idx = atoi(parts[12]);
		b.to_stop_idx = atoi(parts[13]);
		b.canceled = atoi(parts[14]);

		if (L->size >= L->capacity)
			bookinglist_expand(L);

		L->data[L->size++] = b;

		if (!b.canceled)
			train_mark_seat(TL, b.train_id, b.date, b.seat_class, b.seat_index, b.from_stop_idx, b.to_stop_idx);
	}

	rebuild(L);
	fclose(f);
	return 1;
}