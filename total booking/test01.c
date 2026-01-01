/*
  高铁订票系统（C 语言，单文件）—— 内存版（不做文件持久化）
  编译:
    gcc -std=c99 -O2 -o hsr main_no_persist.c
*/
#include <stdio.h>  // 标准输入输出
#include <stdlib.h>  // 标准库（包含 malloc/free 等）
#include <string.h>  // 字符串操作
#include <time.h>    //时间相关

#define NAME_LEN 64  // 姓名最大长度
#define ID_LEN 40   // 证件号最大长度
#define TIME_LEN 8  // 时间字符串长度（"HH:MM" + 终止符 = 6，取8留余量）
#define DATE_LEN 12  // 日期字符串长度（"YYYY-MM-DD" + 终止符 = 11，取12留余量）
#define STATION_LEN 64  // 站名最大长度
#define ORDER_ID_LEN 64  // 订单号最大长度

#define INITIAL_CAPACITY 8  // 线性表初始容量（动态数组初始分配8个元素的空间）

// 停靠站信息
typedef struct {
    char name[STATION_LEN];   //站名
    char arrive[TIME_LEN];    // 进站时间（格式"HH:MM"）
    char depart[TIME_LEN];    // 出站时间（格式"HH:MM"）
    int distance;         // 里程(km)
} Stop;

// 列车信息
typedef struct {
    char train_id[ID_LEN];   // 车次号
    char from[STATION_LEN];  // 始发站
    char to[STATION_LEN];   // 终到站
    char depart_time[TIME_LEN];    // 发车时间
    double base_price;    // 基准票价
    int running;       // 运行状态（1=运行 0=停运）
    Stop* stops;      // 停靠站列表（动态分配数组）
    int stop_count;    // 停靠站数量
    int duration_minutes;     // 总时长（分钟）
    int seat_count[4];       // 四类座位数量：商务/一等/二等/站票
    double seat_price_coef[4];     // 四类座位票价系数（相对于基准票价）
} Train;

// 乘客信息
typedef struct {
    char id_type[32];   // 证件类型
    char id_num[ID_LEN];   // 证件号码
    char name[NAME_LEN];   // 姓名
    char phone[32];   // 手机号
    char emergency_contact[NAME_LEN];   // 紧急联系人
    char emergency_phone[32];   // 紧急联系人电话
} Passenger;

// 订票信息
typedef struct {
    char order_id[ORDER_ID_LEN];   // 订单号
    char passenger_id[ID_LEN];      // 乘客证件号
    char passenger_name[NAME_LEN];   // 乘客姓名
    char date[DATE_LEN];       // 乘车日期
    char train_id[ID_LEN];   // 车次号
    char from[STATION_LEN];   // 起始站
    char to[STATION_LEN];   // 终到站
    char depart_time[TIME_LEN];   // 发车时间
    double price;      // 票价
    char seat_no[16];    // 座位号 （格式如 "1-23" 表示1等座23号）
    int seat_class;     // 座位等级（0=商务 1=一等 2=二等 3=站票）
    int canceled;   // 退票状态（0=已出票 1=已退票）
} Booking;

// 列车列表
typedef struct {
    Train* data;    // 动态分配数组
    int size;    // 当前元素数量
    int capacity;   // 数组容量 (可容纳的最大元素数)
} TrainList;

// 类似的还有PassengerList和BookingList

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

// 安全的内存分配函数xmalloc，失败则退出程序，避免每次都要检查malloc返回值。
void *xmalloc(size_t n) {   
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "内存分配失败\n"); exit(1); }
    return p;
}
/* void *：void指针（通用指针）
   为什么返回指针而不是值？
   因为malloc分配的是内存块，而不是单个值
   为什么一定要返回？
   return p 的本质是把分配好的内存地址"交给"调用者。如果不返回，分配的内存就"丢失"了

   xmalloc自定义安全内存分配函数（"x"通常表示"extended"或"safe"版本）
   size_t 是C标准库中定义的无符号整数类型，专门用于表示对象的大小、长度或数量  */



/* 线性表初始化 */
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

/* 线性表扩容 */
void trainlist_expand(TrainList *L) {
    L->capacity *= 2;
    L->data = realloc(L->data, sizeof(Train) * L->capacity);
    if (!L->data) { fprintf(stderr, "realloc fail\n"); exit(1); }
}
//如果分配失败，返回 NULL，且原来的内存块保持不变

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

/* 查找 */
int trainlist_find(TrainList *L, const char *train_id) {
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].train_id, train_id) == 0) return i;
    return -1;
}
/*如果你要修改或学习这个代码，最好添加命名常量来提高代码质量。这体现了防御性编程和代码可维护性的思想
 #define NOT_FOUND -1
int find_item(...) {
    // ...
    return NOT_FOUND;  // 清晰明了
}  
*/
int passengerlist_find(PassengerList *L, const char *id_num) {
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].id_num, id_num) == 0) return i;
    return -1;
}
int bookinglist_find(BookingList *L, const char *order_id) {
    for (int i = 0; i < L->size; ++i) if (strcmp(L->data[i].order_id, order_id) == 0) return i;
    return -1;
}
int count_booked_for_train_date_class(BookingList *BL, const char *train_id, const char *date, int seat_class) {
    int cnt = 0;
    for (int i = 0; i < BL->size; ++i) {
        Booking *b = &BL->data[i];
        if (b->canceled) continue;
        if (strcmp(b->train_id, train_id) == 0 && strcmp(b->date, date) == 0 && b->seat_class == seat_class) cnt++;
    }
    return cnt;
}
/*：统计 "某个车次在某一天某个座位等级已经卖出去了多少张票"。
如：
列车信息 (Train)：
┌──────────────┬──────────────┐
| 车次: G123   | 一等座总数: 50 |
└──────────────┴──────────────┘

订单信息 (Booking)：
┌─────┬──────┬──────┬──────┬──────────┐
| 车次 | 日期       | 等级 | 状态     |
├─────┼──────┼──────┼──────┼──────────┤
| G123|2024-12-25| 1   | 已出票     | ← 计数
| G123|2024-12-25| 1   | 已出票     | ← 计数  
| G123|2024-12-25| 1   | 已退票     | ← 不计数（canceled=1）
| G123|2024-12-25| 2   | 已出票     | ← 不计数（等级不同）
| G456|2024-12-25| 1   | 已出票     | ← 不计数（车次不同）
| G123|2024-12-26| 1   | 已出票     | ← 不计数（日期不同）
└─────┴──────┴──────┴──────┴──────────┘

统计结果：G123在2024-12-25的一等座已售出2张


BookingList BL 在内存中：
┌─────────────────────────────────────┐
│ data: 0x1000 → 指向堆上的数组       │
│ size: 10                            │
│ capacity: 20                        │
└─────────────────────────────────────┘
    ↓
堆内存（数组）：
地址: 0x1000: ┌─────────────────────┐  ← data[0]
              │ Booking 结构体 #0   │
              │ order_id: "..."     │
              │ passenger_id: "..." │
              │ ...                 │
地址: 0x1100: ├─────────────────────┤  ← data[1]
              │ Booking 结构体 #1   │
              │ ...                 │
地址: 0x1200: ├─────────────────────┤  ← data[2]
              │ Booking 结构体 #2   │
              │ ...                 │
              ├─────────────────────┤
              │ ... 其他元素 ...    │
              └─────────────────────┘
*/


void generate_order_id(char *out, size_t outlen, BookingList *BL, const char *date, const char *train_id) {
    int serial = 0;
    for (int i = 0; i < BL->size; ++i) {
        Booking *b = &BL->data[i];
        if (strcmp(b->date, date) == 0 && strcmp(b->train_id, train_id) == 0) serial++;
    }
    snprintf(out, outlen, "%s-%s-%04d", date, train_id, serial + 1);
}

/* Train 操作 */
void train_add(TrainList *L, Train *t) {
    if (L->size >= L->capacity) trainlist_expand(L);//检查是否扩容
    L->data[L->size++] = *t;  //将传入的Train结构体复制到数组末尾，并增加size计数
}
int train_delete(TrainList *L, const char *train_id) {
    int idx = trainlist_find(L, train_id);  //查找车次索引
    if (idx == -1) return 0;   // 未找到返回0（失败）
    free(L->data[idx].stops);  //释放停靠站动态数组内存
    for (int i = idx; i < L->size - 1; ++i) L->data[i] = L->data[i+1];  // 向前移动元素覆盖删除位置
    L->size--; // 大小减1，返回1（成功）
    return 1;
}
int train_update(TrainList *L, const char *train_id, Train *newt) {
    int idx = trainlist_find(L, train_id);  //查找车次索引
    if (idx == -1) return 0;   // 未找到返回0（失败）
    free(L->data[idx].stops); //释放旧的停靠站动态数组内存
   L->data[idx] = *newt;   //用新的Train结构体覆盖旧的
    return 1;  //返回1（成功）
}
void train_print(const Train *t) {
    printf("车次: %s  %s -> %s   发车: %s   票价基准: %.2f   状态: %s\n",
           t->train_id, t->from, t->to, t->depart_time, t->base_price, t->running ? "运行" : "停运");
    printf("  总时长: %d 分钟  各座席数量(Biz/1/2/Stand): %d/%d/%d/%d\n",
           t->duration_minutes, t->seat_count[0], t->seat_count[1], t->seat_count[2], t->seat_count[3]);
    printf("  停靠站(%d):\n", t->stop_count);
    for (int i = 0; i < t->stop_count; ++i) {
        Stop *s = &t->stops[i];
        printf("    %d. %s 进:%s 出:%s 里程:%dkm\n", i+1, s->name, s->arrive, s->depart, s->distance);
    }
}
void train_list_all(TrainList *L) {
    if (L->size == 0) { printf("无车次信息\n"); return; }
    for (int i = 0; i < L->size; ++i) {
        printf("---- [%d] ----\n", i+1);  
        train_print(&L->data[i]);
    }
}

/* Passenger 操作 */
void passenger_add(PassengerList *L, Passenger *p) {
    if (L->size >= L->capacity) passengerlist_expand(L);
    L->data[L->size++] = *p;
}
int passenger_delete(PassengerList *L, const char *id_num) {
    int idx = passengerlist_find(L, id_num);
    if (idx == -1) return 0;
    for (int i = idx; i < L->size - 1; ++i) L->data[i] = L->data[i+1];
    L->size--;
    return 1;
}
int passenger_update(PassengerList *L, const char *id_num, Passenger *pnew) {
    int idx = passengerlist_find(L, id_num);
    if (idx == -1) return 0;
    L->data[idx] = *pnew;
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

/* Booking 操作 */
void booking_add(BookingList *L, Booking *b) {
    if (L->size >= L->capacity) bookinglist_expand(L);
    L->data[L->size++] = *b;
}
int booking_cancel(BookingList *L, const char *order_id) {
    int idx = bookinglist_find(L, order_id);
    if (idx == -1) return 0;
    L->data[idx].canceled = 1;
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

/* 交互工具 */
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

/* UI: Train */
void ui_train_add(TrainList *TL) {
    Train t; char tmp[256];
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
    Train t = old;
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
    }
    input_line("是否修改座位数量? (y/n): ", buf, sizeof(buf));
    if (buf[0]=='y' || buf[0]=='Y') {
        for (int i = 0; i < 4; ++i) {
            char pbuf[32];
            snprintf(pbuf, sizeof(pbuf), "  座位 %d 数量（回车保留）: ", i);
            input_line(pbuf, buf, sizeof(buf));
            if (strlen(buf)) t.seat_count[i] = atoi(buf);
        }
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

/* UI: Passenger */
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

/* UI: Booking */
void ui_booking_book(BookingList *BL, TrainList *TL, PassengerList *PL) {
    char date[DATE_LEN]; input_line("输入乘车日期 (YYYY-MM-DD): ", date, sizeof(date));
    char train_id[ID_LEN]; input_line("输入车次号: ", train_id, sizeof(train_id));
    int tidx = trainlist_find(TL, train_id);
    if (tidx == -1) { printf("未找到车次\n"); return; }
    Train *t = &TL->data[tidx];
    char from[STATION_LEN], to[STATION_LEN];
    input_line("起点站: ", from, sizeof(from));
    input_line("终点站: ", to, sizeof(to));
    char idnum[ID_LEN]; input_line("乘车证件号: ", idnum, sizeof(idnum));
    int pidx = passengerlist_find(PL, idnum);
    if (pidx == -1) { printf("未找到乘客，请先添加乘客信息\n"); return; }
    int cls = input_int("座位等级 (0=商务 1=一等 2=二等 3=站票): ");
    if (cls < 0 || cls > 3) { printf("等级错误\n"); return; }
    int booked = count_booked_for_train_date_class(BL, train_id, date, cls);
    if (booked >= t->seat_count[cls]) { printf("无余票\n"); return; }
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
    b.price = t->base_price * t->seat_price_coef[cls];
    snprintf(b.seat_no, sizeof(b.seat_no), "%d-%d", cls+1, booked+1);
    b.canceled = 0;
    booking_add(BL, &b);
    printf("订票成功！订单号: %s  价格: %.2f 座位:%s\n", b.order_id, b.price, b.seat_no);
}
void ui_booking_cancel(BookingList *BL) {
    char oid[ORDER_ID_LEN]; input_line("输入订单号退票: ", oid, sizeof(oid));
    if (booking_cancel(BL, oid)) printf("退票成功（状态置为已退票）\n");
    else printf("未找到订单\n");
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
        if (strcmp(BL->data[i].passenger_name, name) == 0) { booking_print(&BL->data[i]); found++; }
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
void ui_booking_query_remain(BookingList *BL, TrainList *TL) {
    char train_id[ID_LEN]; input_line("车次号: ", train_id, sizeof(train_id));
    char date[DATE_LEN]; input_line("日期 (YYYY-MM-DD): ", date, sizeof(date));
    int tidx = trainlist_find(TL, train_id);
    if (tidx == -1) { printf("未找到车次\n"); return; }
    Train *t = &TL->data[tidx];
    printf("车次 %s 在 %s 的余票情况（简化为整列剩余）：\n", train_id, date);
    for (int cls = 0; cls < 4; ++cls) {
        int booked = count_booked_for_train_date_class(BL, train_id, date, cls);
        int remain = t->seat_count[cls] - booked;
        if (remain < 0) remain = 0;
        printf("  等级 %d: %d / %d\n", cls, remain, t->seat_count[cls]);
    }
}

/* 菜单 */
int main_menu() {
    puts("==== 高铁订票系统（内存运行，无持久化） ====");
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

int main() {
    TrainList TL; PassengerList PL; BookingList BL;
    trainlist_init(&TL); passengerlist_init(&PL); bookinglist_init(&BL);

    /* NOTE: 不做文件加载/保存，所有数据仅保留在内存中 */

    for (;;) {
        int choice = main_menu();
        if (choice == 0) {
            printf("退出（未保存数据，内存中数据将丢失）\n");
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
                else if (c == 5) passenger_list_all(&PL);
                else printf("无效选项\n");
            }
        } else if (choice == 3) {
            for (;;) {
                int c = booking_menu();
                if (c == 0) break;
                if (c == 1) ui_booking_book(&BL, &TL, &PL);
                else if (c == 2) ui_booking_cancel(&BL);
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

    for (int i = 0; i < TL.size; ++i) free(TL.data[i].stops);
    free(TL.data); free(PL.data); free(BL.data);
    return 0;
}