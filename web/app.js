// 简单前端逻辑：向后端 REST 接口请求并渲染
async function getJSON(path) {
  const r = await fetch(path);
  return await r.json();
}

function renderTrains(arr) {
  const div = document.getElementById('trains');
  div.innerHTML = '';
  arr.forEach(t => {
    const el = document.createElement('div');
    el.className = 'train';
    el.innerHTML = `<strong>${t.train_id}</strong> ${t.from} -> ${t.to} 发车:${t.depart_time} 基价:${t.base_price} 座位:${t.seat_count.join('/')}`;
    div.appendChild(el);
  });
}

function renderPassengers(arr) {
  const div = document.getElementById('passengers');
  div.innerHTML = '';
  arr.forEach(p => {
    const el = document.createElement('div');
    el.className = 'passenger';
    el.textContent = `${p.name} (${p.id_num}) 手机:${p.phone}`;
    div.appendChild(el);
  });
}

function renderBookings(arr) {
  const div = document.getElementById('bookings');
  div.innerHTML = '';
  arr.forEach(b => {
    const el = document.createElement('div');
    el.className = 'booking';
    el.textContent = `${b.order_id} ${b.passenger_name} ${b.train_id} ${b.date} ${b.from}->${b.to} ${b.seat_no} ${b.canceled ? '[已退票]' : ''}`;
    div.appendChild(el);
  });
}

async function reloadAll() {
  try {
    const [trains, passengers, bookings] = await Promise.all([
      getJSON('/api/trains'),
      getJSON('/api/passengers'),
      getJSON('/api/bookings')
    ]);
    renderTrains(trains);
    renderPassengers(passengers);
    renderBookings(bookings);
  } catch (e) {
    alert('刷新失败: ' + e);
  }
}

document.getElementById('reloadAll').addEventListener('click', reloadAll);
document.getElementById('saveAll').addEventListener('click', async () => {
  const r = await fetch('/api/save', { method: 'POST' });
  const j = await r.json();
  alert('保存: ' + (j.success ? '成功' : '失败'));
});
document.getElementById('loadAll').addEventListener('click', async () => {
  const r = await fetch('/api/load', { method: 'POST' });
  const j = await r.json();
  alert('加载: ' + (j.success ? '成功' : '失败'));
  await reloadAll();
});

/* 添加乘客表单 */
document.getElementById('addPassengerForm').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  const fd = new FormData(ev.target);
  const obj = Object.fromEntries(fd.entries());
  const r = await fetch('/api/passengers', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(obj)
  });
  const j = await r.json();
  alert(j.success ? '添加成功' : '添加失败');
  await reloadAll();
  ev.target.reset();
});

/* 订票表单 */
document.getElementById('bookForm').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  const fd = new FormData(ev.target);
  const obj = Object.fromEntries(fd.entries());
  const r = await fetch('/api/bookings', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(obj)
  });
  const j = await r.json();
  if (j.success) alert('订票成功，订单号:' + j.order_id);
  else alert('订票失败: ' + (j.error || '未知'));
  await reloadAll();
  ev.target.reset();
});

/* 退票表单 */
document.getElementById('cancelForm').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  const fd = new FormData(ev.target);
  const obj = Object.fromEntries(fd.entries());
  const r = await fetch('/api/bookings/cancel', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(obj)
  });
  const j = await r.json();
  if (j.success) alert('退票成功');
  else alert('退票失败: ' + (j.error || '未知'));
  await reloadAll();
  ev.target.reset();
});

/* 初始加载 */
reloadAll();