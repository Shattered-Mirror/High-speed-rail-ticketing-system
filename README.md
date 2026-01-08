# 高铁订票系统
**前言**

本库有四个分支 main，master ， feature1，feature2

master分支下total booking中的有test1,test2

test1是简单的订票程序，数据结构用到了顺序表和动态数组

test2是加强版的，在test1的基础上加了区间占座与座位分配，索引结构（提高查询效率）

master分支下exercise是对一些基础知识的训练，如文件读取，数据结构，指针等

feature1 是在test2下将代码拆分为 .c/.h 模块并写单元测试

feature2 是在feature1下持久化（文本文件）


版本说明
- 语言：C（兼容 C99）
- 实现特点：
  - 模块化拆分（include/ + src/）
  - 支持区间占座（按站段为每个座位维护占用位图）
  - 使用简单哈希索引加速按关键字段查找（车次号、证件号、订单号）
  - 文本持久化（trains.txt / passengers.txt / bookings.txt）
  - 控制台友好界面（简易“GUI”菜单）

目录结构（project-root）
- include/
  - hash.h
  - train.h
  - passenger.h
  - booking.h
- src/
  - hash.c
  - train.c
  - passenger.c
  - booking.c
- tests/
  - test_train.c
  - test_passenger.c
  - test_booking.c
- main.c
- 示例数据（供测试）：
  - trains.txt
  - passengers.txt
  - bookings.txt

主要功能
- 车次管理（Train）
  - 添加/删除/修改（简化）/查询/列出
  - 车次包含停靠站列表、各级座位数与票价系数
  - 为每个车次按日期维护 seatmap（区间占座）
- 乘客管理（Passenger）
  - 添加/删除/修改/查询/列出
- 订票管理（Booking）
  - 订票（按区间分配具体座位）
  - 退票（释放区间）
  - 按订单号/姓名/车次+日期查询
  - 列出所有订单
- 持久化
  - 将数据保存到文本文件：trains.txt / passengers.txt / bookings.txt
  - 启动时自动尝试载入
  - 加载 bookings.txt 时会恢复区间占座（依据保存的 seat_index/from_idx/to_idx）

主要数据结构（概要）
- Train
  - train_id, from, to, depart_time, base_price, running, stops[], stop_count
  - seat_count[4], seat_price_coef[4]
  - seatmaps（按日期的 TrainDateSeatMap 列表，内部维护每座每段占用位图）
- TrainDateSeatMap（内部）
  - date, segment_count, class_seat_seg[4]（每项为 seat_count[c] * segment_count 的字节数组）
- Passenger
  - id_type, id_num（主键）, name, phone, emergency_contact, emergency_phone
- Booking
  - order_id, passenger_id, passenger_name, date, train_id, from, to, depart_time
  - price, seat_no, seat_class, seat_index, from_stop_idx, to_stop_idx, canceled
- 索引
  - 简单字符串哈希表（djb2 + separate chaining）用于快速查找索引（返回数组下标）

文本持久化格式
- trains.txt
  - 第一行：车次数量（整数）
  - 每趟车：
    train_id|from|to|depart_time|base_price|running|duration_minutes|stop_count|seat0|seat1|seat2|seat3|coef0|coef1|coef2|coef3
    随后 stop_count 行：
    stop_name|arrive|depart|distance
- passengers.txt
  - 第一行：乘客数量
  - 每行：
    id_type|id_num|name|phone|emergency_contact|emergency_phone
- bookings.txt
  - 第一行：订单数量
  - 每行：
    order_id|passenger_id|passenger_name|date|train_id|from|to|depart_time|price|seat_no|seat_class|seat_index|from_idx|to_idx|canceled

示例数据（可直接保存并测试）
- 已在项目 README 附带示例内容，请将 trains.txt / passengers.txt / bookings.txt 放置在可执行文件同一目录。

单元测试
- tests/ 下包含三个测试文件（test_train.c / test_passenger.c / test_booking.c）。
- 编译示例（以 test_train 为例）：
  gcc -Iinclude src\hash.c src\train.c src\passenger.c src\booking.c tests\test_train.c -o test_train.exe -std=c99 -O2
- 运行测试：.\test_train.exe（返回 0 表示全部通过）

注意事项与已知限制
- 订票的座位分配为精确区间占用（按站段），但座位分配策略为“先找到第一个完全空闲的座位并分配”——属于简化策略，不支持优先靠窗/靠走道等偏好。
- 并发/多进程访问未处理
- 输入格式（时间/日期/站名）未做严格校验，请按提示输入正确格式。
