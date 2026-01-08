#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "train.h"
#include "passenger.h"
#include "booking.h"

static void input_line(const char *prompt, char *buf, size_t buflen)
{
	printf("%s", prompt);
	if (!fgets(buf, buflen, stdin)) { buf[0]=0; return; }
	size_t ln = strlen(buf);
	if (ln && buf[ln-1]=='\n')
		buf[ln-1]=0;
}

static int input_int(const char *prompt)
{
	char buf[64];
	input_line(prompt, buf, sizeof(buf));
	return atoi(buf);
}

static double input_double(const char *prompt)
{
	char buf[64];
	input_line(prompt, buf, sizeof(buf));
	return atof(buf);
}

/* menus */
int main_menu()
{
	puts("==== 高铁订票系统（模块化+持久化） ====");
	puts("1. 车次信息管理");
	puts("2. 乘客信息管理");
	puts("3. 订票信息管理");
	puts("4. 保存所有数据");
	puts("5. 载入所有数据");
	puts("0. 退出");
	return input_int("请选择: ");
}

int train_menu()
{
	puts("---- 车次管理 ----");
	puts("1. 增加车次");
	puts("2. 停开车次（删除）");
	puts("3. 修改车次信息");
	puts("4. 查询（按车次）");
	puts("5. 输出所有车次");
	puts("0. 返回");
	return input_int("选择: ");
}

int passenger_menu()
{
	puts("---- 乘客管理 ----");
	puts("1. 添加乘客");
	puts("2. 删除乘客（按证件号）");
	puts("3. 修改乘客（手机号/联系人）");
	puts("4. 查询（按证件号）");
	puts("5. 输出所有乘客");
	puts("0. 返回");
	return input_int("选择: ");
}

int booking_menu()
{
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

void save_all(TrainList *TL, PassengerList *PL, BookingList *BL)
{
	if (save_trains("trains.txt", TL))
		printf("已保存 trains.txt\n");
	else
		printf("保存 trains.txt 失败\n");

	if (save_passengers("passengers.txt", PL))
		printf("已保存 passengers.txt\n");
	else
		printf("保存 passengers.txt 失败\n");

	if (save_bookings("bookings.txt", BL))
		printf("已保存 bookings.txt\n");
	else
		printf("保存 bookings.txt 失败\n");
}

void load_all(TrainList *TL, PassengerList *PL, BookingList *BL)
{
	if (load_trains("trains.txt", TL))
		printf("已载入 trains.txt\n");
	else
		printf("未找到 trains.txt 或载入失败\n");

	if (load_passengers("passengers.txt", PL))
		printf("已载入 passengers.txt\n");
	else
		printf("未找到 passengers.txt 或载入失败\n");

	if (load_bookings("bookings.txt", BL, TL))
		printf("已载入 bookings.txt 并重建座位占用\n");
	else
		printf("未找到 bookings.txt 或载入失败\n");
}

int main(void)
{
	TrainList TL;
	PassengerList PL;
	BookingList BL;

	trainlist_init(&TL);
	passengerlist_init(&PL);
	bookinglist_init(&BL);

	load_all(&TL, &PL, &BL);

	for (;;) {
		int ch = main_menu();

		if (ch == 0) {
			char buf[8];
			input_line("退出并保存数据? (y/n): ", buf, sizeof(buf));
			if (buf[0]=='y' || buf[0]=='Y')
				save_all(&TL, &PL, &BL);
			printf("退出\n");
			break;
		} else if (ch == 1) {
			for (;;) {
				int c = train_menu();
				if (c == 0)
					break;

				if (c == 1) {
					Train t;
					char tmp[256];
					input_line("输入车次号: ", t.train_id, sizeof(t.train_id));
					input_line("输入始发站: ", t.from, sizeof(t.from));
					input_line("输入终到站: ", t.to, sizeof(t.to));
					input_line("输入发车时间(HH:MM): ", t.depart_time, sizeof(t.depart_time));
					t.base_price = input_double("输入基准票价: ");
					t.running = input_int("是否运行(1/0): ");
					t.duration_minutes = input_int("总时长(分钟): ");
					t.stop_count = input_int("停靠站数量: ");
					if (t.stop_count < 0)
						t.stop_count = 0;
					t.stops = malloc(sizeof(Stop) * t.stop_count);
					for (int i = 0; i < t.stop_count; ++i) {
						printf("停靠站 %d:\n", i+1);
						input_line("  站名: ", t.stops[i].name, sizeof(t.stops[i].name));
						input_line("  进站时间 (HH:MM or -): ", t.stops[i].arrive, sizeof(t.stops[i].arrive));
						input_line("  出站时间 (HH:MM or -): ", t.stops[i].depart, sizeof(t.stops[i].depart));
						t.stops[i].distance = input_int("  里程(km): ");
					}
					printf("输入四类座位数量(商务/一等/二等/站票):\n");
					for (int i = 0; i < 4; ++i)
						t.seat_count[i] = input_int("  数量: ");
					printf("输入四类票价系数:\n");
					for (int i = 0; i < 4; ++i)
						t.seat_price_coef[i] = input_double("  系数: ");
					train_add(&TL, &t);
					puts("添加完成");
				} else if (c == 2) {
					char id[ID_LEN];
					input_line("输入要停开的车次号: ", id, sizeof(id));
					if (train_delete(&TL, id) == 0)
						puts("删除成功");
					else
						puts("删除失败/未找到");
				} else if (c == 3) {
					char id[ID_LEN];
					input_line("输入要修改的车次号: ", id, sizeof(id));
					int idx = train_find_index(&TL, id);
					if (idx == -1) {
						puts("未找到");
						continue;
					}
					Train old = *train_get(&TL, idx);
					Train t = old;
					char buf[128];
					input_line("车次号（回车保留）: ", buf, sizeof(buf));
					if (strlen(buf))
						strncpy(t.train_id, buf, ID_LEN-1);
					input_line("始发站（回车保留）: ", buf, sizeof(buf));
					if (strlen(buf))
						strncpy(t.from, buf, STATION_LEN-1);
					input_line("终到站（回车保留）: ", buf, sizeof(buf));
					if (strlen(buf))
						strncpy(t.to, buf, STATION_LEN-1);
					train_update(&TL, id, &t);
					puts("修改完成");
				} else if (c == 4) {
					char id[ID_LEN];
					input_line("输入车次号: ", id, sizeof(id));
					int idx = train_find_index(&TL, id);
					if (idx == -1)
						puts("未找到");
					else {
						Train *t = train_get(&TL, idx);
						printf("车次: %s  %s->%s  发车:%s  基价:%.2f\n", t->train_id, t->from, t->to, t->depart_time, t->base_price);
					}
				} else if (c == 5) {
					train_list_all(&TL);
				} else {
					puts("无效选项");
				}
			}
		} else if (ch == 2) {
			for (;;) {
				int c = passenger_menu();
				if (c == 0)
					break;

				if (c == 1) {
					Passenger p;
					input_line("证件类别: ", p.id_type, sizeof(p.id_type));
					input_line("证件号: ", p.id_num, sizeof(p.id_num));
					input_line("姓名: ", p.name, sizeof(p.name));
					input_line("手机号: ", p.phone, sizeof(p.phone));
					input_line("紧急联系人: ", p.emergency_contact, sizeof(p.emergency_contact));
					input_line("紧急联系人电话: ", p.emergency_phone, sizeof(p.emergency_phone));
					passenger_add(&PL, &p);
					puts("添加成功");
				} else if (c == 2) {
					char id[ID_LEN];
					input_line("证件号: ", id, sizeof(id));
					if (passenger_delete(&PL, id) == 0)
						puts("删除成功");
					else
						puts("未找到");
				} else if (c == 3) {
					char id[ID_LEN];
					input_line("证件号: ", id, sizeof(id));
					int idx = passenger_find_index(&PL, id);
					if (idx == -1) {
						puts("未找到");
						continue;
					}
					Passenger p = PL.data[idx];
					char buf[128];
					input_line("手机号（回车保留）: ", buf, sizeof(buf));
					if (strlen(buf))
						strncpy(p.phone, buf, sizeof(p.phone)-1);
					input_line("紧急联系人（回车保留）: ", buf, sizeof(buf));
					if (strlen(buf))
						strncpy(p.emergency_contact, buf, sizeof(p.emergency_contact)-1);
					input_line("紧急联系人电话（回车保留）: ", buf, sizeof(buf));
					if (strlen(buf))
						strncpy(p.emergency_phone, buf, sizeof(p.emergency_phone)-1);
					passenger_update(&PL, id, &p);
					puts("修改成功");
				} else if (c == 4) {
					char id[ID_LEN];
					input_line("证件号: ", id, sizeof(id));
					int idx = passenger_find_index(&PL, id);
					if (idx == -1)
						puts("未找到");
					else {
						Passenger *p = &PL.data[idx];
						printf("姓名:%s  证件:%s  手机:%s\n", p->name, p->id_num, p->phone);
					}
				} else if (c == 5) {
					passenger_list_all(&PL);
				} else {
					puts("无效选项");
				}
			}
		} else if (ch == 3) {
			for (;;) {
				int c = booking_menu();
				if (c == 0)
					break;

				if (c == 1) {
					char date[DATE_LEN], train_id[ID_LEN], from[STATION_LEN], to[STATION_LEN], pid[ID_LEN], orderid[ORDER_ID_LEN];
					input_line("日期(YYYY-MM-DD): ", date, sizeof(date));
					input_line("车次号: ", train_id, sizeof(train_id));
					input_line("起点站: ", from, sizeof(from));
					input_line("终点站: ", to, sizeof(to));
					input_line("乘车证件号: ", pid, sizeof(pid));
					int cls = input_int("座位等级(0..3): ");
					int res = booking_create(&BL, &TL, &PL, date, train_id, from, to, pid, cls, orderid, sizeof(orderid));
					if (res == 0)
						printf("订票成功，订单号: %s\n", orderid);
					else if (res == -1)
						puts("乘客不存在");
					else if (res == -2)
						puts("无余票/分配失败");
					else
						puts("订票失败");
				} else if (c == 2) {
					char oid[ORDER_ID_LEN];
					input_line("输入订单号退票: ", oid, sizeof(oid));
					int res = booking_cancel(&BL, oid, &TL);
					if (res == 0)
						puts("退票成功");
					else if (res == -1)
						puts("未找到订单");
					else if (res == -2)
						puts("订单已退票");
					else
						puts("退票异常");
				} else if (c == 3) {
					char oid[ORDER_ID_LEN];
					input_line("订单号: ", oid, sizeof(oid));
					int idx = booking_find_index(&BL, oid);
					if (idx == -1)
						puts("未找到订单");
					else {
						Booking *b = &BL.data[idx];
						printf("订单号:%s  乘客:%s  日期:%s  车次:%s  %s->%s  座位:%s  状态:%s\n",
							   b->order_id, b->passenger_name, b->date, b->train_id, b->from, b->to, b->seat_no, b->canceled ? "已退票":"已出票");
					}
				} else if (c == 4) {
					char name[NAME_LEN];
					input_line("姓名: ", name, sizeof(name));
					int found = 0;
					for (int i = 0; i < BL.size; ++i)
						if (strcmp(BL.data[i].passenger_name, name) == 0) {
							printf("订单:%s  %s->%s  %s\n", BL.data[i].order_id, BL.data[i].from, BL.data[i].to, BL.data[i].seat_no);
							found++;
						}
					if (!found)
						puts("未找到");
				} else if (c == 5) {
					char train_id[ID_LEN], date[DATE_LEN];
					input_line("车次号: ", train_id, sizeof(train_id));
					input_line("日期(YYYY-MM-DD): ", date, sizeof(date));
					int found = 0;
					for (int i = 0; i < BL.size; ++i)
						if (strcmp(BL.data[i].train_id, train_id) == 0 && strcmp(BL.data[i].date, date) == 0) {
							printf("订单:%s  %s->%s  %s\n", BL.data[i].order_id, BL.data[i].from, BL.data[i].to, BL.data[i].seat_no);
							found++;
						}
					if (!found)
						puts("未找到");
				} else if (c == 6) {
					char train_id[ID_LEN], date[DATE_LEN];
					input_line("车次号: ", train_id, sizeof(train_id));
					input_line("日期(YYYY-MM-DD): ", date, sizeof(date));
					int idx = train_find_index(&TL, train_id);
					if (idx == -1) { puts("未找到车次"); continue; }
					Train *t = train_get(&TL, idx);
					printf("车次 %s 在 %s 的余票（按等级）:\n", train_id, date);
					for (int cls = 0; cls < 4; ++cls) {
						/* 计算被占用座位数（通过扫描 booking list，以保持主程序简洁） */
						int used = 0;
						for (int i = 0; i < BL.size; ++i) {
							if (!BL.data[i].canceled && strcmp(BL.data[i].train_id, train_id)==0 && strcmp(BL.data[i].date, date)==0 && BL.data[i].seat_class==cls)
								used++;
						}
						int remain = t->seat_count[cls] - used;
						if (remain < 0)
							remain = 0;
						printf("  等级 %d: %d / %d\n", cls, remain, t->seat_count[cls]);
					}
				} else if (c == 7) {
					booking_list_all(&BL);
				} else {
					puts("无效选项");
				}
			}
		} else if (ch == 4) {
			save_all(&TL, &PL, &BL);
		} else if (ch == 5) {
			load_all(&TL, &PL, &BL);
		} else {
			puts("无效选项");
		}
	}

	trainlist_free(&TL);
	passengerlist_free(&PL);
	bookinglist_free(&BL);
	return 0;
}