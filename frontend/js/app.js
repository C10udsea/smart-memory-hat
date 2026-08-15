// 前端交互逻辑：对接智能记忆帽后端接口
// 后端地址默认指向本机，部署时按需修改
const API_BASE = 'http://127.0.0.1:8000';

document.addEventListener('DOMContentLoaded', () => {
  // 初始化时间范围，默认最近 24 小时
  const now = new Date();
  const start = new Date(now.getTime() - 24 * 3600 * 1000);
  setDatetime('startTime', start);
  setDatetime('endTime', now);

  // 标签页切换
  document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
      tab.classList.add('active');
      document.getElementById('panel-' + tab.dataset.tab).classList.add('active');
    });
  });

  // 选择“自定义”时显示起止时间输入
  document.getElementById('range').addEventListener('change', (e) => {
    document.getElementById('customRange').classList.toggle('hidden', e.target.value !== '0');
  });

  // 绑定各功能按钮
  document.getElementById('queryBtn').addEventListener('click', queryMemory);
  document.getElementById('reminderBtn').addEventListener('click', addReminder);
  document.getElementById('uploadBtn').addEventListener('click', uploadFrame);
  document.getElementById('frameFile').addEventListener('change', previewFrame);
});

// 将 Date 格式化为 datetime-local 输入框需要的字符串
function setDatetime(id, date) {
  const pad = n => String(n).padStart(2, '0');
  const val = date.getFullYear() + '-' + pad(date.getMonth() + 1) + '-' + pad(date.getDate()) +
    'T' + pad(date.getHours()) + ':' + pad(date.getMinutes());
  document.getElementById(id).value = val;
}

// 根据快捷范围或自定义时间，返回 Unix 秒
function getTimeRange() {
  const range = document.getElementById('range').value;
  const end = Date.now() / 1000;
  if (range !== '0') {
    return { start: end - parseInt(range, 10) * 3600, end: end };
  }
  const s = new Date(document.getElementById('startTime').value).getTime() / 1000;
  const e = new Date(document.getElementById('endTime').value).getTime() / 1000;
  return { start: s, end: e };
}

// 视觉记忆检索：调用 POST /query_memory
async function queryMemory() {
  const prompt = document.getElementById('userPrompt').value.trim();
  if (!prompt) { alert('请输入问题'); return; }
  const range = getTimeRange();
  const btn = document.getElementById('queryBtn');
  btn.disabled = true; btn.textContent = '查询中…';
  setStatus('queryAnswer', '正在查询…', '');
  try {
    const res = await fetch(API_BASE + '/query_memory', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ user_prompt: prompt, start_time: range.start, end_time: range.end })
    });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();
    setAnswer(data.answer || '(空回答)');
    // 展示后端返回的图片证据（如有）
    const ev = document.getElementById('queryEvidence');
    ev.innerHTML = '';
    if (data.evidence_image) {
      const img = document.createElement('img');
      img.src = data.evidence_image.startsWith('data:') ? data.evidence_image : API_BASE + data.evidence_image;
      ev.appendChild(img);
    }
  } catch (err) {
    setStatus('queryAnswer', '查询失败：' + err.message + '（请确认后端已启动并开启 CORS）', 'err');
  } finally {
    btn.disabled = false; btn.textContent = '查询记忆';
  }
}

function setAnswer(text) {
  const el = document.getElementById('queryAnswer');
  el.textContent = text;
  el.className = 'answer status';
}

function setStatus(id, text, cls) {
  const el = document.getElementById(id);
  el.textContent = text;
  el.className = 'status ' + (cls || '');
}

// 主动记忆提醒：调用 POST /add_reminder（后端接口待实现）
async function addReminder() {
  const target = document.getElementById('targetName').value.trim();
  const text = document.getElementById('reminderText').value.trim();
  if (!target || !text) { alert('请填写目标物品和提醒内容'); return; }
  const btn = document.getElementById('reminderBtn');
  btn.disabled = true; btn.textContent = '提交中…';
  setStatus('reminderStatus', '正在录入…', '');
  try {
    const res = await fetch(API_BASE + '/add_reminder', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target_name: target, reminder_text: text })
    });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();
    setStatus('reminderStatus', '已录入：' + (data.message || 'OK'), 'ok');
  } catch (err) {
    setStatus('reminderStatus', '录入失败：' + err.message + '（后端 /add_reminder 接口尚未实现）', 'err');
  } finally {
    btn.disabled = false; btn.textContent = '录入提醒';
  }
}

// 本地预览选择的图片
function previewFrame() {
  const file = document.getElementById('frameFile').files[0];
  const img = document.getElementById('framePreview');
  if (!file) { img.classList.add('hidden'); return; }
  const reader = new FileReader();
  reader.onload = e => { img.src = e.target.result; img.classList.remove('hidden'); };
  reader.readAsDataURL(file);
}

// 画面帧上传：调用 POST /upload_frame
async function uploadFrame() {
  const input = document.getElementById('frameFile');
  const file = input.files[0];
  if (!file) { alert('请选择图片'); return; }
  const btn = document.getElementById('uploadBtn');
  btn.disabled = true; btn.textContent = '上传中…';
  setStatus('uploadStatus', '正在上传…', '');
  try {
    const fd = new FormData();
    fd.append('file', file);
    const res = await fetch(API_BASE + '/upload_frame', { method: 'POST', body: fd });
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();
    setStatus('uploadStatus', (data.message || '上传成功') + (data.id ? '（' + data.id + '）' : ''), 'ok');
  } catch (err) {
    setStatus('uploadStatus', '上传失败：' + err.message + '（请确认后端已启动并开启 CORS）', 'err');
  } finally {
    btn.disabled = false; btn.textContent = '上传画面帧';
  }
}