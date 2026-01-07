class TrainTicketPrintJob:
    """高铁票打印任务类（包含图片中所有高铁票信息）"""
    def __init__(self, ticket_id, passenger_name, submit_time, 
                 start_station, end_station, train_num, car_no, seat_no, 
                 seat_class, travel_date, travel_time, price):
        # 原有基础信息
        self.ticket_id = ticket_id       # 车票编号
        self.passenger_name = passenger_name  # 乘客姓名
        self.submit_time = submit_time  # 提交打印时间（24小时制）
        self.start_time = None          # 开始打印时间
        self.complete_time = None       # 完成打印时间
        self.wait_time = 0              # 等待时间（分钟）
        self.pages = 1  # 高铁票固定1页/张，贴合实际
        
        # 图片中高铁票的核心信息（全部添加）
        self.start_station = start_station  # 出发站（如"广州北站"）
        self.end_station = end_station      # 到达站（如"广州南站"）
        self.train_num = train_num          # 车次（如"G6109"）
        self.car_no = car_no                # 车厢号（如"07车"）
        self.seat_no = seat_no              # 座位号（如"06A号"）
        self.seat_class = seat_class        # 座位等级（如"二等座"）
        self.travel_date = travel_date      # 乘车日期（如"2025年09月16日"）
        self.travel_time = travel_time      # 乘车时间（如"12:16"）
        self.price = price                  # 票价（如"¥22.0元"）
    
    def get_station_en(self, station_cn):
        """根据中文站名获取英文（贴合图片中的英文翻译）"""
        station_map = {
            "广州北站": "Guangzhoubei",
            "广州南站": "Guangzhounan",
            "深圳北站": "Shenzhenbei",
            "长沙南站": "Changshanan",
            "北京西站": "Beijingxi",
            "上海虹桥站": "Shanghaihongqiao",
            "杭州东站": "Hangzhoudong",
            "成都东站": "Chengdudong",
            "武汉站": "Wuhan",
            "郑州东站": "Zhengzhoudong"
        }
        return station_map.get(station_cn, station_cn)  # 没有映射时返回中文

class PrintQueue:
    """高铁票打印队列（无修改，保持原有功能）"""
    def __init__(self):
        self.queue = []
    def enqueue(self, job):
        """入队：添加高铁票打印任务"""
        self.queue.append(job)
    def dequeue(self):
        """出队：取出队首任务"""
        if not self.is_empty():
            return self.queue.pop(0)
        return None
    def is_empty(self):
        """判断队列是否为空"""
        return len(self.queue) == 0

def time_to_minutes(time_str):
    """24小时制时间转分钟数（如"7:30"→450）"""
    hour, minute = map(int, time_str.split(":"))
    return hour * 60 + minute

def minutes_to_time(minutes):
    """分钟数转回24小时制（如450→"7:30"）"""
    hour = minutes // 60
    minute = minutes % 60
    return f"{hour}:{minute:02d}"

def simulate_train_ticket_printing(print_speed, initial_time, ticket_jobs):
    """
    高铁票打印模拟（贴合高铁站实际，新增高铁票信息打印）
    :param print_speed: 打印速度（张/分钟，如5张/分钟）
    :param initial_time: 打印机开机时间（24小时制，如"6:00"）
    :param ticket_jobs: 车票任务列表（包含图片中所有信息的元组）
    元组格式：(车票编号, 乘客姓名, 提交时间, 出发站, 到达站, 车次, 车厢号, 座位号, 座位等级, 乘车日期, 乘车时间, 票价)
    """
    print_queue = PrintQueue()
    current_time = time_to_minutes(initial_time)  # 当前时间（分钟）
    total_wait_time = 0
    completed_tickets = 0
    
    # 按提交时间排序（模拟高铁站客流顺序）
    ticket_jobs.sort(key=lambda x: time_to_minutes(x[2]))
    job_idx = 0
    n_tickets = len(ticket_jobs)
    
    print("===== 高铁站车票打印调度 =====")
    print(f"打印机开机时间：{initial_time} | 打印速度：{print_speed}张/分钟\n")
    
    while job_idx < n_tickets or not print_queue.is_empty():
        # 1. 加入当前时间前提交的车票打印任务
        while job_idx < n_tickets:
            # 从元组中取出所有信息（对应__init__的参数顺序）
            ticket_id, passenger, submit_time, start_station, end_station, train_num, \
            car_no, seat_no, seat_class, travel_date, travel_time, price = ticket_jobs[job_idx]
            
            submit_min = time_to_minutes(submit_time)
            if submit_min <= current_time:
                # 创建任务对象（传入所有高铁票信息）
                job = TrainTicketPrintJob(
                    ticket_id=ticket_id,
                    passenger_name=passenger,
                    submit_time=submit_time,
                    start_station=start_station,
                    end_station=end_station,
                    train_num=train_num,
                    car_no=car_no,
                    seat_no=seat_no,
                    seat_class=seat_class,
                    travel_date=travel_date,
                    travel_time=travel_time,
                    price=price
                )
                # 补充英文站名（调用类内方法）
                job.start_station_en = job.get_station_en(start_station)
                job.end_station_en = job.get_station_en(end_station)
                
                print_queue.enqueue(job)
                job_idx += 1
            else:
                break
        
        # 2. 处理车票打印任务（新增高铁票详情打印）
        if not print_queue.is_empty():
            current_job = print_queue.dequeue()
            
            # 记录开始/完成时间
            current_job.start_time = minutes_to_time(current_time)
            print_duration = current_job.pages / print_speed  # 1页耗时
            complete_min = current_time + int(print_duration)
            current_job.complete_time = minutes_to_time(complete_min)
            
            # 计算等待时间（最小为0）
            submit_min = time_to_minutes(current_job.submit_time)
            current_job.wait_time = max(0, current_time - submit_min)
            total_wait_time += current_job.wait_time
            completed_tickets += 1
            
            # 打印车票详情（完全贴合图片中的高铁票样式）
            print("="*60)
            print(f"【高铁票打印完成】")
            print(f"车票编号：{current_job.ticket_id} | 乘客：{current_job.passenger_name}")
            print(f"出发站：{current_job.start_station}({current_job.start_station_en}) | 到达站：{current_job.end_station}({current_job.end_station_en})")
            print(f"车次：{current_job.train_num} | 车厢/座位：{current_job.car_no}{current_job.seat_no} | 座位等级：{current_job.seat_class}")
            print(f"乘车日期：{current_job.travel_date} | 乘车时间：{current_job.travel_time}")
            print(f"票价：{current_job.price} | 仅供报销使用")
            print(f"报销凭证遗失不补 | 退票改签时须交回车站")
            print(f"\n打印调度信息：")
            print(f"  提交打印：{current_job.submit_time} | 开始打印：{current_job.start_time} | 打印完成：{current_job.complete_time}")
            print(f"  等待打印时长：{current_job.wait_time}分钟")
            print("="*60 + "\n")
            
            # 更新当前时间
            current_time = complete_min
        else:
            # 无任务时，跳转到下一张车票提交时间
            next_submit_min = time_to_minutes(ticket_jobs[job_idx][2])
            current_time = next_submit_min
    
    # 打印统计（高铁站运营视角）
    print("===== 打印任务统计（高铁站） =====")
    print(f"累计打印车票：{completed_tickets}张")
    if completed_tickets > 0:
        print(f"平均等待时长：{total_wait_time / completed_tickets:.1f}分钟")
    print(f"打印机工作时段：{initial_time} ~ {minutes_to_time(current_time)}")

# 高铁票打印专用测试（新增8组不同类型的运行示例）
if __name__ == "__main__":
    # 模拟高铁站打印任务（覆盖不同城市、车次、座位等级、票价）
    train_ticket_jobs = [
        # 示例1：图片原型（广州北→广州南，G6109，二等座）
        ("G6109-001", "张三", "10:00", "广州北站", "广州南站", "G6109", "07车", "06A号", "二等座", "2025年09月16日", "12:16", "¥22.0元"),
        # 示例2：同车次不同座位（广州北→广州南，G6109，二等座）
        ("G6109-002", "李四", "10:01", "广州北站", "广州南站", "G6109", "07车", "06B号", "二等座", "2025年09月16日", "12:16", "¥22.0元"),
        # 示例3：跨城短途（广州南→深圳北，G8888，一等座）
        ("G8888-001", "王五", "10:05", "广州南站", "深圳北站", "G8888", "03车", "12C号", "一等座", "2025年09月16日", "14:45", "¥74.5元"),
        # 示例4：跨城长途（广州南→长沙南，G1108，商务座）
        ("G1108-005", "赵六", "10:10", "广州南站", "长沙南站", "G1108", "01车", "01A号", "商务座", "2025年09月16日", "09:30", "¥890.0元"),
        # 示例5：北方线路（北京西→郑州东，G507，二等座）
        ("G507-012", "孙七", "11:30", "北京西站", "郑州东站", "G507", "12车", "08D号", "二等座", "2025年09月16日", "16:20", "¥309.0元"),
        # 示例6：东部线路（上海虹桥→杭州东，G735，无座）
        ("G735-008", "周八", "11:35", "上海虹桥站", "杭州东站", "G735", "08车", "无座", "无座", "2025年09月16日", "18:15", "¥73.0元"),
        # 示例7：西部线路（成都东→武汉，G3788，一等座）
        ("G3788-015", "吴九", "14:20", "成都东站", "武汉站", "G3788", "05车", "09F号", "一等座", "2025年09月16日", "10:00", "¥580.5元"),
        # 示例8：早高峰车票（广州南→深圳北，G6253，二等座）
        ("G6253-003", "郑十", "07:15", "广州南站", "深圳北站", "G6253", "10车", "15E号", "二等座", "2025年09月16日", "08:40", "¥66.5元"),
        # 示例9：晚高峰车票（长沙南→广州南，G6002，二等座）
        ("G6002-020", "钱十一", "17:40", "长沙南站", "广州南站", "G6002", "09车", "07A号", "二等座", "2025年09月16日", "18:50", "¥314.0元"),
        # 示例10：深夜车票（武汉→北京西，G516，硬卧代座）
        ("G516-018", "马十二", "20:10", "武汉站", "北京西站", "G516", "15车", "21号", "硬卧代座", "2025年09月16日", "21:30", "¥520.0元")
    ]
    
    # 高铁站打印机参数：8张/分钟（高速），开机时间6:00
    simulate_train_ticket_printing(print_speed=8, initial_time="6:00", ticket_jobs=train_ticket_jobs)