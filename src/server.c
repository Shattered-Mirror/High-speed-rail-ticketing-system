/*
  简易 HTTP 服务器（Windows winsock），为前端提供 REST 接口并静态服务 web/*
  编译（Windows + MinGW）:
    gcc -Iinclude src\hash.c src\train.c src\passenger.c src\booking.c src\server.c -o hsr_server.exe -std=c99 -O2 -lws2_32
  运行:
    .\hsr_server.exe
  然后在浏览器打开: http://localhost:8080
*/

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET socket_t;
#else
// 本实现主要面向 Windows。POSIX 版本未包含。
#error "此 server.c 仅为 Windows (Winsock) 实现"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "train.h"
#include "passenger.h"
#include "booking.h"

#define PORT "8080"
#define BUFSIZE 8192

/* 全局数据容器 */
static TrainList g_trains;
static PassengerList g_passengers;
static BookingList g_bookings;

/* HTTP 辅助 */
static void send_response(socket_t client, const char *status, const char *content_type, const char *body) {
    char header[1024];
    int body_len = (int)strlen(body);
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %d\r\n"
                        "Connection: close\r\n"
                        "\r\n", status, content_type, body_len);
    send(client, header, hlen, 0);
    send(client, body, body_len, 0);
}

/* 静态文件读取与返回 */
static int serve_file(socket_t client, const char *path) {
    char fpath[512];
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        snprintf(fpath, sizeof(fpath), "web/index.html");
    } else {
        /* 去掉开头的 / */
        if (path[0] == '/') snprintf(fpath, sizeof(fpath), "web/%s", path+1);
        else snprintf(fpath, sizeof(fpath), "web/%s", path);
    }
    FILE *f = fopen(fpath, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);

    const char *ctype = "text/html; charset=utf-8";
    /* 简单判断类型 */
    const char *ext = strrchr(fpath, '.');
    if (ext) {
        if (strcmp(ext, ".js") == 0) ctype = "application/javascript; charset=utf-8";
        else if (strcmp(ext, ".css") == 0) ctype = "text/css; charset=utf-8";
        else if (strcmp(ext, ".json") == 0) ctype = "application/json; charset=utf-8";
        else if (strcmp(ext, ".html") == 0) ctype = "text/html; charset=utf-8";
    }
    send_response(client, "200 OK", ctype, buf);
    free(buf);
    return 1;
}

/* JSON 值快速解析（非常宽松，查找 "key"\s*:\s*"value" 或 数字 ）*/
static int get_json_string(const char *body, const char *key, char *out, size_t outlen) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    char *p = strstr(body, pat);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    /* 跳过 ':' */
    p++;
    /* 跳过空白 */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') {
        p++;
        char *q = strchr(p, '"');
        if (!q) return 0;
        size_t len = (size_t)(q - p);
        if (len >= outlen) len = outlen - 1;
        strncpy(out, p, len);
        out[len] = 0;
        return 1;
    } else {
        /* 数字或未加引号的词 */
        char tmp[256]; int i = 0;
        while (*p && *p != ',' && *p != '}' && *p != '\n' && i < 250) { tmp[i++] = *p++; }
        tmp[i] = 0;
        /* trim spaces */
        char *s = tmp; while (*s == ' ' || *s == '\t') s++;
        char *e = s + strlen(s) - 1; while (e > s && (*e == ' ' || *e == '\t')) *e-- = 0;
        strncpy(out, s, outlen-1); out[outlen-1] = 0;
        return 1;
    }
}

/* 构造 JSON 响应: trains */
static char *api_get_trains_json(void) {
    /* 预估大小并用字符串增长（简易版） */
    size_t cap = 8192;
    char *out = malloc(cap);
    out[0] = 0;
    strcat(out, "[");
    for (int i = 0; i < g_trains.size; ++i) {
        Train *t = &g_trains.data[i];
        char item[2048];
        /* stops array: 仅输出站名数组 */
        char stopsbuf[1024]; stopsbuf[0] = 0;
        strcat(stopsbuf, "[");
        for (int j = 0; j < t->stop_count; ++j) {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "\"%s\"%s", t->stops[j].name, (j+1==t->stop_count)?"":",");
            strcat(stopsbuf, tmp);
        }
        strcat(stopsbuf, "]");
        snprintf(item, sizeof(item),
                 "{\"train_id\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"depart_time\":\"%s\",\"base_price\":%.2f,\"running\":%d,\"duration_minutes\":%d,"
                 "\"seat_count\":[%d,%d,%d,%d],\"seat_coef\":[%.3f,%.3f,%.3f,%.3f],\"stops\":%s}%s",
                 t->train_id, t->from, t->to, t->depart_time, t->base_price, t->running, t->duration_minutes,
                 t->seat_count[0], t->seat_count[1], t->seat_count[2], t->seat_count[3],
                 t->seat_price_coef[0], t->seat_price_coef[1], t->seat_price_coef[2], t->seat_price_coef[3],
                 stopsbuf, (i+1==g_trains.size)?"":",");
        if (strlen(out) + strlen(item) + 10 > cap) { cap *= 2; out = realloc(out, cap); }
        strcat(out, item);
    }
    strcat(out, "]");
    return out;
}

static char *api_get_passengers_json(void) {
    size_t cap = 4096;
    char *out = malloc(cap); out[0]=0; strcat(out,"[");
    for (int i = 0; i < g_passengers.size; ++i) {
        Passenger *p = &g_passengers.data[i];
        char item[512];
        snprintf(item, sizeof(item), "{\"id_type\":\"%s\",\"id_num\":\"%s\",\"name\":\"%s\",\"phone\":\"%s\"}%s",
                 p->id_type, p->id_num, p->name, p->phone, (i+1==g_passengers.size)?"":",");
        if (strlen(out) + strlen(item) + 10 > cap) { cap *= 2; out = realloc(out, cap); }
        strcat(out, item);
    }
    strcat(out, "]");
    return out;
}

static char *api_get_bookings_json(void) {
    size_t cap = 8192;
    char *out = malloc(cap); out[0]=0; strcat(out,"[");
    for (int i = 0; i < g_bookings.size; ++i) {
        Booking *b = &g_bookings.data[i];
        char item[1024];
        snprintf(item, sizeof(item),
                 "{\"order_id\":\"%s\",\"passenger_id\":\"%s\",\"passenger_name\":\"%s\",\"date\":\"%s\",\"train_id\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"depart_time\":\"%s\",\"price\":%.2f,\"seat_no\":\"%s\",\"seat_class\":%d,\"seat_index\":%d,\"from_idx\":%d,\"to_idx\":%d,\"canceled\":%d}%s",
                 b->order_id, b->passenger_id, b->passenger_name, b->date, b->train_id, b->from, b->to, b->depart_time, b->price, b->seat_no, b->seat_class, b->seat_index, b->from_stop_idx, b->to_stop_idx, b->canceled, (i+1==g_bookings.size)?"":",");
        if (strlen(out) + strlen(item) + 10 > cap) { cap *= 2; out = realloc(out, cap); }
        strcat(out, item);
    }
    strcat(out, "]");
    return out;
}

/* POST /api/passengers  添加乘客，body JSON 包含 id_type,id_num,name,phone,emergency_contact,emergency_phone */
static void handle_post_passenger(socket_t client, const char *body) {
    Passenger p; memset(&p,0,sizeof(p));
    get_json_string(body, "id_type", p.id_type, sizeof(p.id_type));
    get_json_string(body, "id_num", p.id_num, sizeof(p.id_num));
    get_json_string(body, "name", p.name, sizeof(p.name));
    get_json_string(body, "phone", p.phone, sizeof(p.phone));
    get_json_string(body, "emergency_contact", p.emergency_contact, sizeof(p.emergency_contact));
    get_json_string(body, "emergency_phone", p.emergency_phone, sizeof(p.emergency_phone));
    int res = passenger_add(&g_passengers, &p);
    if (res == 0) send_response(client, "200 OK", "application/json; charset=utf-8", "{\"success\":true}");
    else send_response(client, "500 Internal", "application/json; charset=utf-8", "{\"success\":false}");
}

/* POST /api/bookings  订票，body JSON: date,train_id,from,to,passenger_id,seat_class */
static void handle_post_booking(socket_t client, const char *body) {
    char date[64], train_id[64], from[128], to[128], pid[64], clsbuf[16];
    date[0]=train_id[0]=from[0]=to[0]=pid[0]=0; clsbuf[0]=0;
    get_json_string(body, "date", date, sizeof(date));
    get_json_string(body, "train_id", train_id, sizeof(train_id));
    get_json_string(body, "from", from, sizeof(from));
    get_json_string(body, "to", to, sizeof(to));
    get_json_string(body, "passenger_id", pid, sizeof(pid));
    get_json_string(body, "seat_class", clsbuf, sizeof(clsbuf));
    int cls = atoi(clsbuf);
    char orderid[ORDER_ID_LEN];
    int rc = booking_create(&g_bookings, &g_trains, &g_passengers, date, train_id, from, to, pid, cls, orderid, sizeof(orderid));
    if (rc == 0) {
        char resp[256]; snprintf(resp, sizeof(resp), "{\"success\":true,\"order_id\":\"%s\"}", orderid);
        send_response(client, "200 OK", "application/json; charset=utf-8", resp);
    } else if (rc == -1) {
        send_response(client, "400 Bad Request", "application/json; charset=utf-8", "{\"success\":false,\"error\":\"passenger_not_found\"}");
    } else if (rc == -2) {
        send_response(client, "200 OK", "application/json; charset=utf-8", "{\"success\":false,\"error\":\"no_seat\"}");
    } else {
        send_response(client, "500 Internal", "application/json; charset=utf-8", "{\"success\":false}");
    }
}

/* POST /api/bookings/cancel body JSON: order_id */
static void handle_post_cancel(socket_t client, const char *body) {
    char oid[ORDER_ID_LEN]; oid[0]=0;
    get_json_string(body, "order_id", oid, sizeof(oid));
    int rc = booking_cancel(&g_bookings, oid, &g_trains);
    if (rc == 0) send_response(client, "200 OK", "application/json; charset=utf-8", "{\"success\":true}");
    else if (rc == -1) send_response(client, "404 Not Found", "application/json; charset=utf-8", "{\"success\":false,\"error\":\"not_found\"}");
    else if (rc == -2) send_response(client, "200 OK", "application/json; charset=utf-8", "{\"success\":false,\"error\":\"already_canceled\"}");
    else send_response(client, "500 Internal", "application/json; charset=utf-8", "{\"success\":false}");
}

/* POST /api/save  /api/load */
static void handle_post_save(socket_t client) {
    int a = save_trains("trains.txt", &g_trains);
    int b = save_passengers("passengers.txt", &g_passengers);
    int c = save_bookings("bookings.txt", &g_bookings);
    if (a && b && c) send_response(client, "200 OK", "application/json; charset=utf-8", "{\"success\":true}");
    else send_response(client, "500 Internal", "application/json; charset=utf-8", "{\"success\":false}");
}
static void handle_post_load(socket_t client) {
    /* 清空当前内存并加载 */
    trainlist_free(&g_trains); passengerlist_free(&g_passengers); bookinglist_free(&g_bookings);
    trainlist_init(&g_trains); passengerlist_init(&g_passengers); bookinglist_init(&g_bookings);
    int a = load_trains("trains.txt", &g_trains);
    int b = load_passengers("passengers.txt", &g_passengers);
    int c = load_bookings("bookings.txt", &g_bookings, &g_trains);
    if (a && b && c) send_response(client, "200 OK", "application/json; charset=utf-8", "{\"success\":true}");
    else send_response(client, "500 Internal", "application/json; charset=utf-8", "{\"success\":false}");
}

/* 请求处理主逻辑（极简解析） */
static void handle_client(socket_t client) {
    char buf[BUFSIZE];
    int received = recv(client, buf, BUFSIZE-1, 0);
    if (received <= 0) { closesocket(client); return; }
    buf[received] = 0;
    /* 解析请求行 */
    char method[8], path[256];
    if (sscanf(buf, "%7s %255s", method, path) != 2) { closesocket(client); return; }

    /* 查找 body（Content-Length） */
    char *body = strstr(buf, "\r\n\r\n");
    int content_len = 0;
    if (body) {
        body += 4;
        char *cl = strcasestr(buf, "Content-Length:");
        if (cl) {
            cl += strlen("Content-Length:");
            while (*cl == ' ') cl++;
            content_len = atoi(cl);
        }
    } else {
        body = "";
    }

    /* Route handling */
    if (strcmp(method, "GET") == 0) {
        if (strncmp(path, "/api/trains", 11) == 0) {
            char *json = api_get_trains_json();
            send_response(client, "200 OK", "application/json; charset=utf-8", json);
            free(json);
        } else if (strncmp(path, "/api/passengers", 15) == 0) {
            char *json = api_get_passengers_json();
            send_response(client, "200 OK", "application/json; charset=utf-8", json);
            free(json);
        } else if (strncmp(path, "/api/bookings", 13) == 0) {
            char *json = api_get_bookings_json();
            send_response(client, "200 OK", "application/json; charset=utf-8", json);
            free(json);
        } else {
            if (!serve_file(client, path)) {
                send_response(client, "404 Not Found", "text/plain; charset=utf-8", "Not Found");
            }
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/api/passengers") == 0) {
            handle_post_passenger(client, body);
        } else if (strcmp(path, "/api/bookings") == 0) {
            handle_post_booking(client, body);
        } else if (strcmp(path, "/api/bookings/cancel") == 0) {
            handle_post_cancel(client, body);
        } else if (strcmp(path, "/api/save") == 0) {
            handle_post_save(client);
        } else if (strcmp(path, "/api/load") == 0) {
            handle_post_load(client);
        } else {
            send_response(client, "404 Not Found", "application/json; charset=utf-8", "{\"error\":\"not_found\"}");
        }
    } else {
        send_response(client, "405 Method Not Allowed", "text/plain; charset=utf-8", "Method Not Allowed");
    }

    closesocket(client);
}

int main(void) {
    /* 初始化数据结构并尝试加载文件 */
    trainlist_init(&g_trains);
    passengerlist_init(&g_passengers);
    bookinglist_init(&g_bookings);
    load_trains("trains.txt", &g_trains);
    load_passengers("passengers.txt", &g_passengers);
    load_bookings("bookings.txt", &g_bookings, &g_trains);

    /* Winsock 初始化 */
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    struct addrinfo hints, *res = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        fprintf(stderr, "getaddrinfo failed\n");
        WSACleanup();
        return 1;
    }

    socket_t listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket failed\n");
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    if (bind(listen_sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed\n");
        closesocket(listen_sock);
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(res);

    if (listen(listen_sock, 10) == SOCKET_ERROR) {
        fprintf(stderr, "listen failed\n");
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    printf("Server running at http://localhost:%s\n", PORT);
    while (1) {
        struct sockaddr_in client_addr; int addrlen = sizeof(client_addr);
        socket_t client = accept(listen_sock, (struct sockaddr*)&client_addr, &addrlen);
        if (client == INVALID_SOCKET) { continue; }
        handle_client(client);
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}