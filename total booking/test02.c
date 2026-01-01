/*
  高铁订票系统（C 语言，单文件）
  增强版：支持区间占座（按站段的座位占用矩阵）与索引（哈希表）加速查找
  - 不做持久化（内存运行），基于之前的内存版 main_no_persist.c 改造
  - 为每个车次维护每个日期的座位占用（按站段），实现精确余票查询与座位分配/释放
  - 为乘客（证件号）、车次（车次号）、订单（订单号）建立哈希索引（字符串->列表索引），通过重建索引保持一致性（添加/删除后重建）
  编译:
    gcc -std=c99 -O2 -o hsr_interval main_interval_index.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NAME_LEN 64
#define ID_LEN 40
#define TIME_LEN 8
#define DATE_LEN 12
#define STATION_LEN 64
#define ORDER_ID_LEN 64

/* -------------------- 数据结构 -------------------- */

typedef struct {
    char name[STATION_LEN];   // 站名
    char arrive[TIME_LEN];
    char depart[TIME_LEN];
    int distance;
} Stop;

/* Forward declare */
struct Train;

/* 每个车次针对某一天的座位占用表（按站段） */
typedef struct {
    char date[DATE_LEN]; // YYYY-MM-DD
    int segment_count;   // stop_count - 1
    // class_seat_seg[c] 指向一个字节数组，大小 = seat_count[c] * segment_count
    // 访问某个座位 s 在段 seg 上的占用： class_seat_seg[c][ s * segment_count + seg ]
    unsigned char **class_seat_seg; // length 4 (for 4 classes)
} TrainDateSeatMap;

typedef struct {
    char train_id[ID_LEN];
    char from[STATION_LEN];
    char to[STATION_LEN];
    char depart_time[TIME_LEN];
    double base_price;
    int running;
    Stop* stops;
    int stop_count;
    int duration_minutes;
    int seat_count[4];
    double seat_price_coef[4];

    /* 新增：每个车次维护一组按日期的座位占用表（动态数组） */
    TrainDateSeatMap *seatmaps;
    int seatmap_count;
    int seatmap_capacity;
} Train;

typedef struct {
    char id_type[32];
    char id_num[ID_LEN];
    char name[NAME_LEN];
    char phone[32];
    char emergency_contact[NAME_LEN];
    char emergency_phone[32];
} Passenger;

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
    char seat_no[16];    // 例如 "2-5"：等级-座号（座号从1开始）
    int seat_class;      // 0..3
    int seat_index;      // 座位在该等级内的索引（从0开始），用于退票时释放占用
    int from_stop_idx;   // 起站索引（用于退票释放）
    int to_stop_idx;     // 终站索引（用于退票释放）
    int canceled;
} Booking;

/* 线性表容器 */
typedef struct {
    Train* data;
    int size;
    int capacity;
} TrainList;

typedef struct {
    Passenger* data;
    int size;
    int capacity;
} PassengerList;

typedef struct {
    Booking* data;
    int size;
    int capacity;
} BookingList;

/* -------------------- 简单哈希表（字符串 -> 索引） 用于建立索引加速查找 -------------------- */
/* 使用 separate chaining（拉链）结构，桶数固定（可扩容若需） */

typedef struct HashNode {
    char *key;           // 复制的字符串key
    int idx;             // 对应在 array list 中的索引
    struct HashNode *next;
} HashNode;

typedef struct {
    HashNode **buckets;
    int bucket_count;
} HashTable;

/* djb2 哈希 */
static unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

HashTable *ht_create(int buckets) {
    HashTable *ht = malloc(sizeof(HashTable));
    if (!ht) { perror("malloc"); exit(1); }
    ht->bucket_count = buckets;
    ht->buckets = calloc(buckets, sizeof(HashNode*));
    if (!ht->buckets) { perror("calloc"); exit(1); }
    return ht;
}

void ht_free(HashTable *ht) {
    if (!ht) return;
    for (int i = 0; i < ht->bucket_count; ++i) {
        HashNode *n = ht->buckets[i];
        while (n) {
            HashNode *t = n->next;
            free(n->key);
            free(n);
            n = t;
        }
    }
    free(ht->buckets);
    free(ht);
}

void ht_clear(HashTable *ht) {
    for (int i = 0; i < ht->bucket_count; ++i) {
        HashNode *n = ht->buckets[i];
        while (n) { HashNode *t = n->next; free(n->key); free(n); n = t; }
        ht->buckets[i] = NULL;
    }
}

void ht_insert(HashTable *ht, const char *key, int idx) {
    unsigned long h = hash_str(key) % ht->bucket_count;
    HashNode *n = malloc(sizeof(HashNode));
    n->key = strdup(key);
    n->idx = idx;
    n->next = ht->buckets[h];
    ht->buckets[h] = n;
}

int ht_find(HashTable *ht, const char *key) {
    unsigned long h = hash_str(key) % ht->bucket_count;
    HashNode *n = ht->buckets[h];
    while (n) {
        if (strcmp(n->key, key) == 0) return n->idx;
        n = n->next;
    }
    return -1;
}

/* -------------------- 工具与常量 -------------------- */

#define INITIAL_CAPACITY 8
#define HASH_BUCKETS 1031 /* 素数，作为哈希桶数 */

/* 简单内存包装 */
void *xmalloc(size_t n) { void *p = malloc(n); if (!p) { fprintf(stderr,"malloc failed\n"); exit(1);} return p; }
char *strdup_s(const char *s) { if (!s) return NULL; char *p = malloc(strlen(s)+1); strcpy(p,s); return p; }

/* -------------------- 线性表初始化/扩容 -------------------- */

void trainlist_init(TrainList *L) {
    L->data = xmalloc(sizeof(Train) * INITIAL_CAPACITY);
    L->size = 0; L->capacity = INITIAL_CAPACITY;
}

void passengerlist_init(PassengerList *L) {
    L->data = xmalloc(sizeof(Passenger) * INITIAL_CAPACITY);
    L->size = 0; L->capacity = INITIAL_CAPACITY;
}

void bookinglist_init(BookingList *L) {
    L->data = xmalloc(sizeof(Booking) * INITIAL_CAPACITY);
    L->size = 0; L->capacity = INITIAL_CAPACITY;
}

void trainlist_expand(TrainList *L) {
    L->capacity *= 2;
    L->data = realloc(L->data, sizeof(Train) * L->capacity);
    if (!L->data) { fprintf(stderr, "realloc fail\n"); exit(1); }
}
void passengerlist_expand(PassengerList *L) {
    L->capacity *= 2;
    L->data = realloc(L->data, sizeof(Passenger) * L->capacity);
    if (!L->data) { fprintf(stderr, "realloc fail\n"); exit(1); }
}
void bookinglist_expand(BookingList *L) {
    L->capacity *= 2;
    L->data = realloc(L->data, sizeof(Booking) * L->capacity);
    if (!L->data) { fprintf(stderr, "realloc fail\n"); exit(1); }
}

/* -------------------- 索引（哈希表）相关：重建函数 -------------------- */
/* 我们在添加/删除/修改列表后调用对应的 rebuild 函数，保证索引与数组一致 */

/* 全局索引实例（在 main 中初始化） */
HashTable *ht_trains = NULL;
HashTable *ht_passengers = NULL;
HashTable *ht_bookings = NULL;

void rebuild_train_index(TrainList *TL) {
    if (!ht_trains) ht_trains = ht_create(HASH_BUCKETS);
    else ht_clear(ht_trains);
    for (int i = 0; i < TL->size; ++i) ht_insert(ht_trains, TL->data[i].train_id, i);
}

void rebuild_passenger_index(PassengerList *PL) {
    if (!ht_passengers) ht_passengers = ht_create(HASH_BUCKETS);
    else ht_clear(ht_passengers);
    for (int i = 0; i < PL->size; ++i) ht_insert(ht_passengers, PL->data[i].id_num, i);
}

void rebuild_booking_index(BookingList *BL) {
    if (!ht_bookings) ht_bookings = ht_create(HASH_BUCKETS);
    else ht_clear(ht_bookings);
    for (int i = 0; i < BL->size; ++i) ht_insert(ht_bookings, BL->data[i].order_id, i);
}

/* -------------------- 车次：SeatMap（按日期）管理 -------------------- */

/* 查找车次的停靠站索引；返回 -1 表示未找到 */
int train_find_stop_idx(const Train *t, const char *station) {
    for (int i = 0; i < t->stop_count; ++i) if (strcmp(t->stops[i].name, station) == 0) return i;
    return -1;
}

/* 在 train 的 seatmaps 中查找指定日期的映射，返回索引或 -1 */
int train_find_seatmap_idx(Train *t, const char *date) {
    for (int i = 0; i < t->seatmap_count; ++i) {
        if (strcmp(t->seatmaps[i].date, date) == 0) return i;
    }
    return -1;
}

/* 创建并初始化指定日期的 seatmap（如果已存在则返回其索引） */
int train_create_seatmap_if_missing(Train *t, const char *date) {
    int idx = train_find_seatmap_idx(t, date);
    if (idx != -1) return idx;
    /* 需要创建新的 seatmap */
    if (t->seatmap_capacity == 0) {
        t->seatmap_capacity = 4;
        t->seatmaps = xmalloc(sizeof(TrainDateSeatMap) * t->seatmap_capacity);
    } else if (t->seatmap_count >= t->seatmap_capacity) {
        t->seatmap_capacity *= 2;
        t->seatmaps = realloc(t->seatmaps, sizeof(TrainDateSeatMap) * t->seatmap_capacity);
        if (!t->seatmaps) { fprintf(stderr, "realloc fail\n"); exit(1); }
    }
    TrainDateSeatMap *sm = &t->seatmaps[t->seatmap_count];
    strncpy(sm->date, date, DATE_LEN-1); sm->date[DATE_LEN-1] = 0;
    sm->segment_count = (t->stop_count >= 1) ? (t->stop_count - 1) : 0;
    sm->class_seat_seg = xmalloc(sizeof(unsigned char*) * 4);
    for (int c = 0; c < 4; ++c) {
        int sc = t->seat_count[c];
        if (sc > 0 && sm->segment_count > 0) {
            /* allocate sc * segment_count bytes, init 0 */
            sm->class_seat_seg[c] = calloc(sc * sm->segment_count, 1);
            if (!sm->class_seat_seg[c]) { perror("calloc"); exit(1); }
        } else {
            sm->class_seat_seg[c] = NULL;
        }
    }
    t->seatmap_count++;
    return t->seatmap_count - 1;
}

/* 释放某个 seatmap 的内存（用于删除车次时） */
void train_free_seatmap(TrainDateSeatMap *sm, const Train *t) {
    if (!sm) return;
    for (int c = 0; c < 4; ++c) {
        if (sm->class_seat_seg[c]) free(sm->class_seat_seg[c]);
    }
    free(sm->class_seat_seg);
}

/* 在给定 seatmap 中，尝试为指定等级分配一个在区间 [from_idx, to_idx) 上空闲的座位。
   返回 seat_index（0-based）并同时把对应段标记为占用；若无空闲返回 -1 */
int seatmap_allocate_seat(TrainDateSeatMap *sm, Train *t, int seat_class, int from_idx, int to_idx) {
    if (!sm) return -1;
    int segs = sm->segment_count;
    if (from_idx < 0 || to_idx <= from_idx || to_idx > sm->segment_count + 0) return -1;
    int sc = t->seat_count[seat_class];
    unsigned char *arr = sm->class_seat_seg[seat_class];
    if (!arr) return -1;
    for (int s = 0; s < sc; ++s) {
        int base = s * segs;
        int free_flag = 1;
        for (int seg = from_idx; seg < to_idx; ++seg) {
            if (arr[base + seg]) { free_flag = 0; break; }
        }
        if (free_flag) {
            /* allocate: mark segments */
            for (int seg = from_idx; seg < to_idx; ++seg) arr[base + seg] = 1;
            return s;
        }
    }
    return -1;
}

/* 退票时释放座位区间（将对应段置为0） */
void seatmap_release_seat(TrainDateSeatMap *sm, Train *t, int seat_class, int seat_index, int from_idx, int to_idx) {
    if (!sm) return;
    int segs = sm->segment_count;
    unsigned char *arr = sm->class_seat_seg[seat_class];
    if (!arr) return;
    int base = seat_index * segs;
    for (int seg = from_idx; seg < to_idx; ++seg) arr[base + seg] = 0;
}

/* -------------------- 辅助查找函数（使用索引） -------------------- */

/* 根据车次号查找索引，使用 ht_trains（更快） */
int trainlist_find(TrainList *L, const char *train_id) {
    if (ht_trains) {
        int idx = ht_find(ht_trains, train_id);
        if (idx != -1) return idx;
    }
    /* fallback */
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].train_id, train_id) == 0) return i;
    return -1;
}

/* 根据证件号查找乘客索引 */
int passengerlist_find(PassengerList *L, const char *id_num) {
    if (ht_passengers) {
        int idx = ht_find(ht_passengers, id_num);
        if (idx != -1) return idx;
    }
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].id_num, id_num) == 0) return i;
    return -1;
}

/* 根据订单号查找订单索引 */
int bookinglist_find(BookingList *L, const char *order_id) {
    if (ht_bookings) {
        int idx = ht_find(ht_bookings, order_id);
        if (idx != -1) return idx;
    }
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].order_id, order_id) == 0) return i;
    return -1;
}

/* 计算指定车次、日期、等级已被占用的座位数（基于精确区间占用：若某座位有任何段被占则计为已占）
   为了支持余票，统计当前 seatmap 中该等级被占用的座位数量 */
int count_booked_for_train_date_class_precise(Train *t, const char *date, int seat_class) {
    int sm_idx = train_find_seatmap_idx(t, date);
    if (sm_idx == -1) return 0;
    TrainDateSeatMap *sm = &t->seatmaps[sm_idx];
    int segs = sm->segment_count;
    int sc = t->seat_count[seat_class];
    unsigned char *arr = sm->class_seat_seg[seat_class];
    if (!arr) return 0;
    int cnt = 0;
    for (int s = 0; s < sc; ++s) {
        int base = s * segs;
        int occupied = 0;
        for (int seg = 0; seg < segs; ++seg) if (arr[base + seg]) { occupied = 1; break; }
        if (occupied) cnt++;
    }
    return cnt;
}

/* -------------------- 生成订单号（不变） -------------------- */
void generate_order_id(char *out, size_t outlen, BookingList *BL, const char *date, const char *train_id) {
    int serial = 0;
    for (int i = 0; i < BL->size; ++i) {
        Booking *b = &BL->data[i];
        if (strcmp(b->date, date) == 0 && strcmp(b->train_id, train_id) == 0) serial++;
    }
    snprintf(out, outlen, "%s-%s-%04d", date, train_id, serial + 1);
}

/* -------------------- 增删改查：Train（增加 seatmap 管理） -------------------- */

void train_add(TrainList *L, Train *t) {
    if (L->size >= L->capacity) trainlist_expand(L);
    /* initialize seatmap fields */
    t->seatmap_count = 0;
    t->seatmap_capacity = 0;
    t->seatmaps = NULL;
    L->data[L->size++] = *t;
    /* rebuild index */
    rebuild_train_index(L);
}

int train_delete(TrainList *L, const char *train_id) {
    int idx = trainlist_find(L, train_id);
    if (idx == -1) return 0;
    /* 释放 stops 内存 */
    free(L->data[idx].stops);
    /* 释放所有 seatmaps */
    for (int i = 0; i < L->data[idx].seatmap_count; ++i) {
        train_free_seatmap(&L->data[idx].seatmaps[i], &L->data[idx]);
    }
    free(L->data[idx].seatmaps);
    /* 移动覆盖 */
    for (int i = idx; i < L->size - 1; ++i) L->data[i] = L->data[i+1];
    L->size--;
    rebuild_train_index(L);
    return 1;
}

int train_update(TrainList *L, const char *train_id, Train *newt) {
    int idx = trainlist_find(L, train_id);
    if (idx == -1) return 0;
    /* 释放旧 stops & seatmaps */
    free(L->data[idx].stops);
    for (int i = 0; i < L->data[idx].seatmap_count; ++i) train_free_seatmap(&L->data[idx].seatmaps[i], &L->data[idx]);
    free(L->data[idx].seatmaps);
    /* copy new */
    newt->seatmaps = NULL;
    newt->seatmap_count = 0;
    newt->seatmap_capacity = 0;
    L->data[idx] = *newt;
    rebuild_train_index(L);
    return 1;
}

void train_print(const Train *t) {
    printf("车次: %s  %s -> %s   发车: %s   票价基准: %.2f   状态: %s\n",
           t->train_id, t->from, t->to, t->depart_time, t->base_price, t->running ? "运行" : "停运");
    printf("  总时长: %d 分钟  各座席数量(Biz/1/2/Stand): %d/%d/%d/%d\n",
           t->duration_minutes,
           t->seat_count[0], t->seat_count[1], t->seat_count[2], t->seat_count[3]);
    printf("  停靠站(%d):\n", t->stop_count);
    for (int i = 0; i < t->stop_count; ++i) {
        Stop *s = &t->stops[i];
        printf("    %d. %s 进:%s 出:%s 里程:%dkm\n", i+1, s->name, s->arrive, s->depart, s->distance);
    }
    printf("  已注册日期座位映射数: %d\n", t->seatmap_count);
}

void train_list_all(TrainList *L) {
    if (L->size == 0) { printf("无车次信息\n"); return; }
    for (int i = 0; i < L->size; ++i) {
        printf("---- [%d] ----\n", i+1);
        train_print(&L->data[i]);
    }
}

/* -------------------- Passenger 操作（添加索引重建调用） -------------------- */

void passenger_add(PassengerList *L, Passenger *p) {
    if (L->size >= L->capacity) passengerlist_expand(L);
    L->data[L->size++] = *p;
    rebuild_passenger_index(L);
}

int passenger_delete(PassengerList *L, const char *id_num) {
    int idx = passengerlist_find(L, id_num);
    if (idx == -1) return 0;
    for (int i = idx; i < L->size - 1; ++i) L->data[i] = L->data[i+1];
    L->size--;
    rebuild_passenger_index(L);
    return 1;
}

int passenger_update(PassengerList *L, const char *id_num, Passenger *pnew) {
    int idx = passengerlist_find(L, id_num);
    if (idx == -1) return 0;
    L->data[idx] = *pnew;
    rebuild_passenger_index(L);
    return 1;
}

void passenger_print(const Passenger *p) {
    printf("姓名:%s  证件:%s(%s)  手机:%s  紧急联系人:%s(%s)\n",
           p->name, p->id_num, p->id_type, p->phone, p->emergency_contact, p->emergency_phone);
}

void passenger_list_all(PassengerList *L) {
    if (L->size == 0) { printf("无乘客信息\n"); return; }
    for (int i = 0; i < L->size; ++i) {
        printf("---- [%d] ----\n", i+1);
        passenger_print(&L->data[i]);
    }
}

/* -------------------- Booking 操作（使用精确区间占座） -------------------- */

void booking_add(BookingList *L, Booking *b) {
    if (L->size >= L->capacity) bookinglist_expand(L);
    L->data[L->size++] = *b;
    rebuild_booking_index(L);
}

int booking_cancel(BookingList *L, const char *order_id, TrainList *TL) {
    int idx = bookinglist_find(L, order_id);
    if (idx == -1) return 0;
    Booking *bk = &L->data[idx];
    if (bk->canceled) return 0;
    /* 释放 seatmap 中对应占用 */
    int tidx = trainlist_find(TL, bk->train_id);
    if (tidx != -1) {
        Train *t = &TL->data[tidx];
        int sm_idx = train_find_seatmap_idx(t, bk->date);
        if (sm_idx != -1) {
            TrainDateSeatMap *sm = &t->seatmaps[sm_idx];
            seatmap_release_seat(sm, t, bk->seat_class, bk->seat_index, bk->from_stop_idx, bk->to_stop_idx);
        }
    }
    bk->canceled = 1;
    /* booking index key still exists (order_id), but status changed; index remains valid */
    return 1;
}

void booking_print(const Booking *b) {
    printf("订单号:%s  乘客:%s(%s)  日期:%s  车次:%s  %s->%s  发车:%s  票价:%.2f  座位:%s  等级:%d  状态:%s\n",
           b->order_id, b->passenger_name, b->passenger_id, b->date, b->train_id,
           b->from, b->to, b->depart_time, b->price, b->seat_no, b->seat_class,
           b->canceled ? "已退票" : "已出票");
}

void booking_list_all(BookingList *L) {
    if (L->size == 0) { printf("无订单信息\n"); return; }
    for (int i = 0; i < L->size; ++i) {
        printf("---- [%d] ----\n", i+1);
        booking_print(&L->data[i]);
    }
}

/* -------------------- 输入 / UI 辅助 -------------------- */

void input_line(const char *prompt, char *buf, size_t buflen) {
    printf("%s", prompt);
    if (!fgets(buf, buflen, stdin)) { buf[0]=0; return; }
    size_t ln = strlen(buf); if (ln && buf[ln-1]=='\n') buf[ln-1]=0;
}

int input_int(const char *prompt) {
    char buf[64]; input_line(prompt, buf, sizeof(buf));
    return atoi(buf);
}

double input_double(const char *prompt) {
    char buf[64]; input_line(prompt, buf, sizeof(buf));
    return atof(buf);
}

/* -------------------- UI：Train 模块 -------------------- */

void ui_train_add(TrainList *TL) {
    Train t;
    char tmp[256];
    input_line("输入车次号: ", t.train_id, sizeof(t.train_id));
    input_line("输入始发站: ", t.from, sizeof(t.from));
    input_line("输入终到站: ", t.to, sizeof(t.to));
    input_line("输入始发站发车时间(HH:MM): ", t.depart_time, sizeof(t.depart_time));
    t.base_price = input_double("输入基准票价（浮点）: ");
    t.running = input_int("是否运行（1=运行 0=停运）: ");
    t.duration_minutes = input_int("输入运行总时长（分钟）: ");
    t.stop_count = input_int("输入停靠站数量: ");
    if (t.stop_count < 0) t.stop_count = 0;
    t.stops = xmalloc(sizeof(Stop) * t.stop_count);
    for (int i = 0; i < t.stop_count; ++i) {
        printf("  停靠站 %d:\n", i+1);
        input_line("    站名: ", t.stops[i].name, sizeof(t.stops[i].name));
        input_line("    进站时间(HH:MM)（首站输入 - ）: ", t.stops[i].arrive, sizeof(t.stops[i].arrive));
        input_line("    出站时间(HH:MM)（终点输入 - ）: ", t.stops[i].depart, sizeof(t.stops[i].depart));
        t.stops[i].distance = input_int("    里程(km): ");
    }
    printf("输入四类座位数量（商务/一等/二等/站票）:\n");
    for (int i = 0; i < 4; ++i) t.seat_count[i] = input_int("  数量: ");
    printf("输入四类票价系数（相对于基准票，浮点）:\n");
    for (int i = 0; i < 4; ++i) t.seat_price_coef[i] = input_double("  系数: ");
    /* seatmaps 初始化在 train_add 中 */
    t.seatmaps = NULL; t.seatmap_count = 0; t.seatmap_capacity = 0;
    train_add(TL, &t);
    printf("车次添加成功\n");
}

void ui_train_delete(TrainList *TL) {
    char id[ID_LEN]; input_line("输入要停开的车次号: ", id, sizeof(id));
    if (train_delete(TL, id)) printf("删除成功\n");
    else printf("未找到车次\n");
}

void ui_train_modify(TrainList *TL) {
    char id[ID_LEN]; input_line("输入要修改的车次号: ", id, sizeof(id));
    int idx = trainlist_find(TL, id);
    if (idx == -1) { printf("未找到车次\n"); return; }
    Train old = TL->data[idx];
    printf("当前信息：\n"); train_print(&old);
    printf("按提示输入新的信息（回车跳过保留原值）\n");
    char buf[128];
    input_line("车次号（回车保留）: ", buf, sizeof(buf));
    Train t = old; // copy
    if (strlen(buf)) { strncpy(t.train_id, buf, ID_LEN-1); t.train_id[ID_LEN-1]=0; }
    input_line("始发站（回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) strncpy(t.from, buf, STATION_LEN-1);
    input_line("终到站（回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) strncpy(t.to, buf, STATION_LEN-1);
    input_line("发车时间（HH:MM，回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) strncpy(t.depart_time, buf, TIME_LEN-1);
    input_line("基准票价（回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) t.base_price = atof(buf);
    input_line("是否运行（1/0 回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) t.running = atoi(buf);
    input_line("总时长（分钟 回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) t.duration_minutes = atoi(buf);
    input_line("是否重新设置停靠站列表? (y/n): ", buf, sizeof(buf));
    if (buf[0]=='y' || buf[0]=='Y') {
        int sc = input_int("输入停靠站数量: ");
        t.stop_count = sc;
        free(t.stops);
        t.stops = xmalloc(sizeof(Stop) * t.stop_count);
        for (int i = 0; i < t.stop_count; ++i) {
            printf("  停靠站 %d:\n", i+1);
            input_line("    站名: ", t.stops[i].name, sizeof(t.stops[i].name));
            input_line("    进站时间: ", t.stops[i].arrive, sizeof(t.stops[i].arrive));
            input_line("    出站时间: ", t.stops[i].depart, sizeof(t.stops[i].depart));
            t.stops[i].distance = input_int("    里程(km): ");
        }
        /* 注意：修改停靠站会使已有 seatmaps 语义失效 —— 这里我们选择在修改停靠站时清除旧的 seatmaps */
        for (int i = 0; i < TL->data[idx].seatmap_count; ++i) train_free_seatmap(&TL->data[idx].seatmaps[i], &TL->data[idx]);
        free(TL->data[idx].seatmaps);
        t.seatmap_count = 0; t.seatmap_capacity = 0; t.seatmaps = NULL;
    }
    input_line("是否修改座位数量? (y/n): ", buf, sizeof(buf));
    if (buf[0]=='y' || buf[0]=='Y') {
        for (int i = 0; i < 4; ++i) {
            char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "  座位 %d 数量（回车保留）: ", i);
            input_line(pbuf, buf, sizeof(buf));
            if (strlen(buf)) t.seat_count[i] = atoi(buf);
        }
        /* 修改座位数后需清除旧 seatmaps（否则数组大小与新座位数不一致） */
        for (int i = 0; i < TL->data[idx].seatmap_count; ++i) train_free_seatmap(&TL->data[idx].seatmaps[i], &TL->data[idx]);
        free(TL->data[idx].seatmaps);
        t.seatmap_count = 0; t.seatmap_capacity = 0; t.seatmaps = NULL;
    }
    input_line("是否修改票价系数? (y/n): ", buf, sizeof(buf));
    if (buf[0]=='y' || buf[0]=='Y') {
        for (int i = 0; i < 4; ++i) {
            char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "  座位 %d 系数（回车保留）: ", i);
            input_line(pbuf, buf, sizeof(buf));
            if (strlen(buf)) t.seat_price_coef[i] = atof(buf);
        }
    }
    train_update(TL, id, &t);
    printf("修改完成\n");
}

void ui_train_query(TrainList *TL) {
    char id[ID_LEN]; input_line("按车次号查询，输入车次号: ", id, sizeof(id));
    int idx = trainlist_find(TL, id);
    if (idx == -1) { printf("无\n"); return; }
    train_print(&TL->data[idx]);
}

/* -------------------- UI：Passenger -------------------- */

void ui_passenger_add(PassengerList *PL) {
    Passenger p;
    input_line("证件类别: ", p.id_type, sizeof(p.id_type));
    input_line("证件号: ", p.id_num, sizeof(p.id_num));
    input_line("姓名: ", p.name, sizeof(p.name));
    input_line("手机号: ", p.phone, sizeof(p.phone));
    input_line("紧急联系人: ", p.emergency_contact, sizeof(p.emergency_contact));
    input_line("紧急联系人电话: ", p.emergency_phone, sizeof(p.emergency_phone));
    passenger_add(PL, &p);
    printf("添加成功\n");
}

void ui_passenger_delete(PassengerList *PL) {
    char id[ID_LEN]; input_line("输入要删除的证件号: ", id, sizeof(id));
    if (passenger_delete(PL, id)) printf("删除成功\n");
    else printf("未找到乘客\n");
}

void ui_passenger_modify(PassengerList *PL) {
    char id[ID_LEN]; input_line("输入要修改的证件号: ", id, sizeof(id));
    int idx = passengerlist_find(PL, id);
    if (idx == -1) { printf("未找到乘客\n"); return; }
    Passenger p = PL->data[idx];
    char buf[128];
    printf("当前信息：\n"); passenger_print(&p);
    input_line("手机号（回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) strncpy(p.phone, buf, sizeof(p.phone)-1);
    input_line("紧急联系人（回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) strncpy(p.emergency_contact, buf, sizeof(p.emergency_contact)-1);
    input_line("紧急联系人电话（回车保留）: ", buf, sizeof(buf)); if (strlen(buf)) strncpy(p.emergency_phone, buf, sizeof(p.emergency_phone)-1);
    passenger_update(PL, id, &p);
    printf("修改完成\n");
}

void ui_passenger_query(PassengerList *PL) {
    char key[64]; input_line("按证件号查询，输入证件号: ", key, sizeof(key));
    int idx = passengerlist_find(PL, key);
    if (idx == -1) printf("未找到\n");
    else passenger_print(&PL->data[idx]);
}

/* -------------------- UI：Booking（使用区间占座） -------------------- */

/* 订票流程：
   - 找到 train、判断起点/终点在停靠站的索引（from_idx < to_idx）
   - 在 train 的 seatmaps 中查找或创建该日期的 seatmap
   - 在指定等级中查找一张对区间 [from_idx, to_idx) 全空闲的座位，分配并标记占用
   - 记录 booking 的 seat_index、from_stop_idx、to_stop_idx 等信息，便于退票时释放
*/
void ui_booking_book(BookingList *BL, TrainList *TL, PassengerList *PL) {
    char date[DATE_LEN]; input_line("输入乘车日期 (YYYY-MM-DD): ", date, sizeof(date));
    char train_id[ID_LEN]; input_line("输入车次号: ", train_id, sizeof(train_id));
    int tidx = trainlist_find(TL, train_id);
    if (tidx == -1) { printf("未找到车次\n"); return; }
    Train *t = &TL->data[tidx];
    char from[STATION_LEN], to[STATION_LEN];
    input_line("起点站: ", from, sizeof(from));
    input_line("终点站: ", to, sizeof(to));
    int from_idx = train_find_stop_idx(t, from);
    int to_idx = train_find_stop_idx(t, to);
    if (from_idx == -1 || to_idx == -1) { printf("起点或终点不在该车停靠站列表中\n"); return; }
    if (!(from_idx < to_idx)) { printf("起点必须在终点之前\n"); return; }
    char idnum[ID_LEN]; input_line("乘车证件号: ", idnum, sizeof(idnum));
    int pidx = passengerlist_find(PL, idnum);
    if (pidx == -1) { printf("未找到乘客，请先添加乘客信息\n"); return; }
    int cls = input_int("座位等级 (0=商务 1=一等 2=二等 3=站票): ");
    if (cls < 0 || cls > 3) { printf("等级错误\n"); return; }

    /* 获取或创建 seatmap */
    int sm_idx = train_create_seatmap_if_missing(t, date);
    TrainDateSeatMap *sm = &t->seatmaps[sm_idx];

    /* 尝试为该等级分配座位 */
    int seat_idx = seatmap_allocate_seat(sm, t, cls, from_idx, to_idx);
    if (seat_idx == -1) { printf("无余票（本区间/等级）\n"); return; }

    Booking b;
    generate_order_id(b.order_id, sizeof(b.order_id), BL, date, train_id);
    strncpy(b.passenger_id, PL->data[pidx].id_num, ID_LEN-1);
    strncpy(b.passenger_name, PL->data[pidx].name, NAME_LEN-1);
    strncpy(b.date, date, DATE_LEN-1);
    strncpy(b.train_id, train_id, ID_LEN-1);
    strncpy(b.from, from, STATION_LEN-1);
    strncpy(b.to, to, STATION_LEN-1);
    strncpy(b.depart_time, t->depart_time, TIME_LEN-1);
    b.seat_class = cls;
    b.seat_index = seat_idx;
    b.from_stop_idx = from_idx;
    b.to_stop_idx = to_idx;
    b.price = t->base_price * t->seat_price_coef[cls];
    snprintf(b.seat_no, sizeof(b.seat_no), "%d-%d", cls+1, seat_idx+1);
    b.canceled = 0;
    booking_add(BL, &b);
    printf("订票成功！订单号: %s  价格: %.2f 座位:%s\n", b.order_id, b.price, b.seat_no);
}

/* 退票：booking_cancel 已实现释放 seatmap 对应占用 */
void ui_booking_cancel(BookingList *BL, TrainList *TL) {
    char oid[ORDER_ID_LEN]; input_line("输入订单号退票: ", oid, sizeof(oid));
    if (booking_cancel(BL, oid, TL)) printf("退票成功（状态置为已退票）\n");
    else printf("未找到订单或已退票\n");
}

void ui_booking_query(BookingList *BL) {
    char oid[ORDER_ID_LEN]; input_line("按订单号查询，输入订单号: ", oid, sizeof(oid));
    int idx = bookinglist_find(BL, oid);
    if (idx == -1) printf("未找到订单\n");
    else booking_print(&BL->data[idx]);
}

void ui_booking_query_by_name(BookingList *BL) {
    char name[NAME_LEN]; input_line("按姓名查询，输入姓名: ", name, sizeof(name));
    int found = 0;
    for (int i = 0; i < BL->size; ++i) {
        if (strcmp(BL->data[i].passenger_name, name) == 0) {
            booking_print(&BL->data[i]); found++;
        }
    }
    if (!found) printf("未找到\n");
}

void ui_booking_query_by_train_date(BookingList *BL) {
    char train_id[ID_LEN]; input_line("车次号: ", train_id, sizeof(train_id));
    char date[DATE_LEN]; input_line("日期 (YYYY-MM-DD): ", date, sizeof(date));
    int found = 0;
    for (int i = 0; i < BL->size; ++i) {
        if (strcmp(BL->data[i].train_id, train_id) == 0 && strcmp(BL->data[i].date, date) == 0) {
            booking_print(&BL->data[i]); found++;
        }
    }
    if (!found) printf("未找到\n");
}

/* 精确余票查询：按照区间占用判断每个等级多少座位被占用 */
void ui_booking_query_remain(BookingList *BL, TrainList *TL) {
    char train_id[ID_LEN]; input_line("车次号: ", train_id, sizeof(train_id));
    char date[DATE_LEN]; input_line("日期 (YYYY-MM-DD): ", date, sizeof(date));
    int tidx = trainlist_find(TL, train_id);
    if (tidx == -1) { printf("未找到车次\n"); return; }
    Train *t = &TL->data[tidx];
    /* 如果没有 seatmap，则所有座位均为空 */
    int sm_idx = train_find_seatmap_idx(t, date);
    printf("车次 %s 在 %s 的余票情况（基于区间占座，单位：空座/总座）:\n", train_id, date);
    for (int cls = 0; cls < 4; ++cls) {
        int total = t->seat_count[cls];
        int used = 0;
        if (sm_idx != -1) used = count_booked_for_train_date_class_precise(t, date, cls);
        int remain = total - used;
        if (remain < 0) remain = 0;
        printf("  等级 %d: %d / %d\n", cls, remain, total);
    }
}

/* -------------------- 菜单与主循环 -------------------- */

int main_menu() {
    puts("==== 高铁订票系统（区间占座 + 索引加速版，内存运行） ====");
    puts("1. 车次信息管理");
    puts("2. 乘客信息管理");
    puts("3. 订票信息管理");
    puts("0. 退出");
    return input_int("请选择: ");
}

int train_menu() {
    puts("---- 车次管理 ----");
    puts("1. 增加车次");
    puts("2. 停开车次（删除）");
    puts("3. 修改车次信息");
    puts("4. 查询（按车次）");
    puts("5. 输出所有车次");
    puts("0. 返回");
    return input_int("选择: ");
}

int passenger_menu() {
    puts("---- 乘客管理 ----");
    puts("1. 添加乘客");
    puts("2. 删除乘客（按证件号）");
    puts("3. 修改乘客（手机号/联系人）");
    puts("4. 查询（按证件号）");
    puts("5. 输出所有乘客");
    puts("0. 返回");
    return input_int("选择: ");
}

int booking_menu() {
    puts("---- 订票管理 ----");
    puts("1. 订票");
    puts("2. 退票（按订单号）");
    puts("3. 订单查询（按订单号）");
    puts("4. 订单查询（按姓名）");
    puts("5. 订单查询（按车次+日期）");
    puts("6. 余票查询（按车次+日期）");
    puts("7. 输出所有订单");
    puts("0. 返回");
    return input_int("选择: ");
}

/* -------------------- 程序启动与退出资源释放 -------------------- */

int main() {
    TrainList TL; PassengerList PL; BookingList BL;
    trainlist_init(&TL); passengerlist_init(&PL); bookinglist_init(&BL);

    /* 初始化全局哈希表实例 */
    ht_trains = ht_create(HASH_BUCKETS);
    ht_passengers = ht_create(HASH_BUCKETS);
    ht_bookings = ht_create(HASH_BUCKETS);

    for (;;) {
        int choice = main_menu();
        if (choice == 0) {
            printf("退出（内存中数据将丢失）\n");
            break;
        } else if (choice == 1) {
            for (;;) {
                int c = train_menu();
                if (c == 0) break;
                if (c == 1) ui_train_add(&TL);
                else if (c == 2) ui_train_delete(&TL);
                else if (c == 3) ui_train_modify(&TL);
                else if (c == 4) ui_train_query(&TL);
                else if (c == 5) train_list_all(&TL);
                else printf("无效选项\n");
            }
        } else if (choice == 2) {
            for (;;) {
                int c = passenger_menu();
                if (c == 0) break;
                if (c == 1) ui_passenger_add(&PL);
                else if (c == 2) ui_passenger_delete(&PL);
                else if (c == 3) ui_passenger_modify(&PL);
                else if (c == 4) ui_passenger_query(&PL);
                else if (c == 5) {
                    passenger_list_all(&PL);
                }
                else printf("无效选项\n");
            }
        } else if (choice == 3) {
            for (;;) {
                int c = booking_menu();
                if (c == 0) break;
                if (c == 1) ui_booking_book(&BL, &TL, &PL);
                else if (c == 2) ui_booking_cancel(&BL, &TL);
                else if (c == 3) ui_booking_query(&BL);
                else if (c == 4) ui_booking_query_by_name(&BL);
                else if (c == 5) ui_booking_query_by_train_date(&BL);
                else if (c == 6) ui_booking_query_remain(&BL, &TL);
                else if (c == 7) booking_list_all(&BL);
                else printf("无效选项\n");
            }
        } else {
            printf("无效选项\n");
        }
    }

    /* 退出前释放动态内存 */
    for (int i = 0; i < TL.size; ++i) {
        for (int j = 0; j < TL.data[i].seatmap_count; ++j) train_free_seatmap(&TL.data[i].seatmaps[j], &TL.data[i]);
        free(TL.data[i].seatmaps);
        free(TL.data[i].stops);
    }
    free(TL.data); free(PL.data); free(BL.data);

    ht_free(ht_trains); ht_free(ht_passengers); ht_free(ht_bookings);
    return 0;
}