#pragma once
#include <Arduino.h>

// Single-page WebUI served at "/". Pure HTML/CSS/JS, no external dependencies,
// so it loads even with no internet access. It talks to the JSON API exposed
// by the firmware (see main.cpp).
static const char WEB_UI_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Relay Control</title>
<style>
  :root { --bg:#0f172a; --card:#1e293b; --muted:#94a3b8; --on:#22c55e; --off:#ef4444; --accent:#3b82f6; }
  * { box-sizing:border-box; }
  body { margin:0; font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
         background:var(--bg); color:#e2e8f0; padding:16px; }
  header { max-width:760px; margin:0 auto 16px; display:flex; justify-content:space-between; align-items:center; gap:12px; flex-wrap:wrap; }
  h1 { font-size:1.25rem; margin:0; }
  .wifi { font-size:.8rem; color:var(--muted); text-align:right; }
  .grid { max-width:760px; margin:0 auto; display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:16px; }
  .card { background:var(--card); border-radius:14px; padding:18px; box-shadow:0 4px 20px rgba(0,0,0,.25); }
  .card h2 { margin:0 0 4px; font-size:1.05rem; display:flex; align-items:center; gap:8px; }
  .badge { font-size:.7rem; padding:2px 8px; border-radius:999px; font-weight:700; letter-spacing:.04em; }
  .badge.on { background:rgba(34,197,94,.18); color:var(--on); }
  .badge.off { background:rgba(239,68,68,.18); color:var(--off); }
  .row { display:flex; gap:10px; margin:14px 0; align-items:center; }
  .btn { flex:1; border:none; border-radius:10px; padding:12px; font-size:1rem; font-weight:600;
         cursor:pointer; color:#fff; transition:transform .05s, opacity .15s; }
  .btn:active { transform:scale(.97); }
  .btn.on { background:var(--on); }
  .btn.off { background:var(--off); }
  .btn.ghost { background:#334155; }
  .btn.ghost.active { background:var(--accent); }
  label { font-size:.8rem; color:var(--muted); display:block; margin-bottom:4px; }
  .settings { margin-top:14px; border-top:1px solid #334155; padding-top:14px; }
  .field { display:flex; gap:10px; }
  .field > div { flex:1; }
  input[type=number] { width:100%; padding:9px; border-radius:8px; border:1px solid #475569;
                       background:#0f172a; color:#e2e8f0; font-size:1rem; }
  .save { width:100%; margin-top:12px; background:var(--accent); }
  .meta { font-size:.75rem; color:var(--muted); margin-top:8px; min-height:1em; }
  footer { max-width:760px; margin:18px auto 0; font-size:.75rem; color:var(--muted); text-align:center; }
  .modepill { display:flex; gap:6px; }
  .modepill .btn { padding:8px; font-size:.85rem; }
  .section { max-width:760px; margin:16px auto 0; }
  .badge.motion { background:rgba(245,158,11,.18); color:#f59e0b; animation:pulse 1.2s ease-in-out infinite; }
  .badge.clear  { background:rgba(148,163,184,.18); color:var(--muted); }
  @keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:.45; } }
  .mstats { display:flex; gap:18px; flex-wrap:wrap; font-size:.8rem; color:var(--muted); margin:6px 0 12px; }
  .mstats b { color:#e2e8f0; font-weight:600; }
  .loghead { display:flex; justify-content:space-between; align-items:center; gap:10px; margin-bottom:8px; }
  .loghead .btn { flex:0 0 auto; padding:7px 12px; font-size:.8rem; background:#334155; }
  .log { background:#0b1220; border:1px solid #334155; border-radius:10px; height:300px;
         overflow-y:auto; padding:8px 10px; font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
         font-size:.78rem; line-height:1.55; }
  .log .line { display:flex; gap:10px; white-space:nowrap; }
  .log .lt { color:var(--muted); flex:0 0 auto; }
  .log .lm { font-weight:600; }
  .log .line.on  .lm { color:#f59e0b; }
  .log .line.off .lm { color:var(--muted); font-weight:400; }
  .log .empty { color:var(--muted); font-style:italic; }
</style>
</head>
<body>
  <header>
    <h1>⚡ ESP32 Relay Control</h1>
    <div class="wifi" id="wifi">connecting…</div>
  </header>
  <div class="grid" id="grid"></div>

  <div class="grid" style="margin-top:16px">
    <div class="card">
      <h2>📶 WiFi</h2>
      <div class="meta" id="wifiInfo">—</div>
      <div class="settings">
        <label>Network (SSID)</label>
        <input type="text" id="wifiSsid" placeholder="WiFi name" autocomplete="off">
        <label style="margin-top:10px">Password</label>
        <input type="password" id="wifiPass" placeholder="leave blank to keep current">
        <button class="btn save" onclick="saveWifi()">Save &amp; reconnect</button>
        <div class="meta" id="wifiMsg"></div>
      </div>
    </div>
    <div class="card">
      <h2>⬆️ Firmware</h2>
      <div class="meta" id="fwInfo">—</div>
      <div class="settings">
        <a class="btn save" href="/update" style="display:block; text-align:center; text-decoration:none; line-height:1.2">Open updater</a>
        <div class="meta">Upload a new <code>firmware.bin</code> over the air.</div>
      </div>
    </div>
  </div>

  <div class="section" id="motionSection" style="display:none">
    <div class="card">
      <h2>🚶 Motion Sensor
        <span class="badge clear" id="motionBadge">—</span>
      </h2>
      <div class="mstats">
        <span>Last trigger: <b id="motionLast">—</b></span>
        <span>Detections: <b id="motionCount">0</b></span>
      </div>
      <div class="loghead">
        <label style="margin:0">History log (newest at bottom, up to 999)</label>
        <button class="btn" onclick="clearMotionLog()">Clear log</button>
      </div>
      <div class="log" id="motionLog"><div class="empty">Waiting for sensor events…</div></div>
    </div>
  </div>

  <footer>Auto-refreshing every 2 s · REST API at <code>/api/status</code> · <span id="ver"></span></footer>

<script>
const cardTpl = (r) => `
  <div class="card" data-id="${r.id}">
    <h2>Relay ${r.id}
      <span class="badge ${r.state}" data-badge>${r.state.toUpperCase()}</span>
    </h2>
    <div class="row">
      <button class="btn on"  onclick="control(${r.id},'on')">Turn ON</button>
      <button class="btn off" onclick="control(${r.id},'off')">Turn OFF</button>
    </div>
    <label>Mode</label>
    <div class="modepill">
      <button class="btn ghost ${r.mode==='auto'?'active':''}"   onclick="setMode(${r.id},'auto')">Auto (cycle)</button>
      <button class="btn ghost ${r.mode==='manual'?'active':''}" onclick="setMode(${r.id},'manual')">Manual (hold)</button>
    </div>
    <div class="settings">
      <div class="field">
        <div>
          <label>ON duration (s)</label>
          <input type="number" min="1" step="1" id="on-${r.id}" value="${r.onDuration}">
        </div>
        <div>
          <label>OFF duration (s)</label>
          <input type="number" min="1" step="1" id="off-${r.id}" value="${r.offDuration}">
        </div>
      </div>
      <button class="btn save" onclick="saveSettings(${r.id})">Save settings</button>
      <div class="meta" data-meta>${r.mode==='auto' ? 'Next switch in '+r.remaining+'s' : 'Holding ' + r.state}</div>
    </div>
  </div>`;

let editing = null; // pause refresh of inputs while user types

async function api(path) {
  const res = await fetch(path, { method:'POST' });
  return res.json();
}

async function control(id, action) { render(await api(`/api/control?relay=${id}&action=${action}`)); }
async function setMode(id, mode)   { render(await api(`/api/settings?relay=${id}&mode=${mode}`)); }
async function saveSettings(id) {
  const on  = document.getElementById(`on-${id}`).value;
  const off = document.getElementById(`off-${id}`).value;
  render(await api(`/api/settings?relay=${id}&onDuration=${on}&offDuration=${off}`));
}

async function saveWifi() {
  const ssid = document.getElementById('wifiSsid').value.trim();
  const pass = document.getElementById('wifiPass').value;
  const msg = document.getElementById('wifiMsg');
  if (!ssid) { msg.textContent = 'Enter a network name'; return; }
  msg.textContent = 'Saving… device will reboot and reconnect.';
  try {
    await fetch('/api/wifi', { method:'POST', body:new URLSearchParams({ ssid, pass }) });
    msg.textContent = 'Saved — rebooting. Reopen http://relay.local/ shortly.';
  } catch(e) { msg.textContent = 'Saved — device is rebooting.'; }
}

function render(data) {
  document.getElementById('wifi').textContent =
    data.wifi.connected ? `${data.wifi.ssid} · ${data.wifi.ip} · ${data.wifi.rssi} dBm`
                        : 'WiFi offline';
  const wi = document.getElementById('wifiInfo');
  if (wi) wi.textContent = data.wifi.connected
    ? `Connected to ${data.wifi.ssid} (${data.wifi.ip}, ${data.wifi.rssi} dBm)`
    : 'Not connected';
  const ssidIn = document.getElementById('wifiSsid');
  if (ssidIn && editing !== 'wifiSsid' && !ssidIn.value) ssidIn.value = data.wifi.ssid || '';
  const fw = document.getElementById('fwInfo');
  if (fw && data.device) fw.textContent = `Firmware ${data.device.version} · uptime ${data.device.uptime}s · ${data.device.heap} B free`;
  const ver = document.getElementById('ver');
  if (ver && data.device) ver.textContent = 'v' + data.device.version;
  const grid = document.getElementById('grid');
  if (grid.children.length !== data.relays.length) {
    grid.innerHTML = data.relays.map(cardTpl).join('');
    return;
  }
  data.relays.forEach(r => {
    const card = grid.querySelector(`.card[data-id="${r.id}"]`);
    const badge = card.querySelector('[data-badge]');
    badge.textContent = r.state.toUpperCase();
    badge.className = `badge ${r.state}`;
    card.querySelector('[data-meta]').textContent =
      r.mode==='auto' ? `Next switch in ${r.remaining}s` : `Holding ${r.state}`;
    card.querySelectorAll('.modepill .btn').forEach((b,i) =>
      b.classList.toggle('active', (i===0) === (r.mode==='auto')));
    if (editing !== `on-${r.id}`)  card.querySelector(`#on-${r.id}`).value  = r.onDuration;
    if (editing !== `off-${r.id}`) card.querySelector(`#off-${r.id}`).value = r.offDuration;
  });
  updateMotion(data.motion);
}

function updateMotion(m) {
  const section = document.getElementById('motionSection');
  if (!m || !m.enabled) { if (section) section.style.display = 'none'; return; }
  section.style.display = '';
  const badge = document.getElementById('motionBadge');
  badge.textContent = m.active ? 'MOTION' : 'CLEAR';
  badge.className = 'badge ' + (m.active ? 'motion' : 'clear');
  document.getElementById('motionCount').textContent = m.count;
  document.getElementById('motionLast').textContent =
    m.lastSeq === 0 ? 'none yet'
                    : (m.lastTs || `+${Math.floor(m.lastUp/1000)}s uptime`);
}

document.addEventListener('focusin',  e => { if (e.target.tagName==='INPUT') editing = e.target.id; });
document.addEventListener('focusout', e => { if (e.target.tagName==='INPUT') editing = null; });

async function refresh() {
  try { render(await (await fetch('/api/status')).json()); } catch(e) {}
}
refresh();
setInterval(refresh, 2000);

// ---- Motion history log (incremental: only fetch events newer than we hold) ----
let motionSeq = 0;        // highest event seq the page has rendered
let motionPrimed = false; // false until the first batch arrives (forces scroll)

function logLine(e) {
  const div = document.createElement('div');
  div.className = 'line ' + (e.m ? 'on' : 'off');
  const ts = e.ts || `+${Math.floor(e.up/1000)}s uptime`;
  const tt = document.createElement('span'); tt.className = 'lt'; tt.textContent = ts;
  const mm = document.createElement('span'); mm.className = 'lm';
  mm.textContent = e.m ? 'Motion detected' : 'No motion';
  div.append(tt, mm);
  return div;
}

async function refreshLog() {
  let data;
  try { data = await (await fetch(`/api/motion/log?since=${motionSeq}`)).json(); }
  catch(e) { return; }
  // Device rebooted or log was cleared elsewhere → its latest seq fell behind us.
  if (data.latest < motionSeq) {
    motionSeq = 0; motionPrimed = false;
    document.getElementById('motionLog').innerHTML = '';
    return refreshLog();
  }
  const events = data.events || [];
  if (!events.length) return;
  const box = document.getElementById('motionLog');
  const empty = box.querySelector('.empty');
  if (empty) empty.remove();
  const atBottom = box.scrollHeight - box.scrollTop - box.clientHeight < 40;
  const frag = document.createDocumentFragment();
  events.forEach(e => { if (e.seq > motionSeq) motionSeq = e.seq; frag.append(logLine(e)); });
  box.append(frag);
  while (box.children.length > 999) box.removeChild(box.firstChild); // hard cap
  if (atBottom || !motionPrimed) box.scrollTop = box.scrollHeight;
  motionPrimed = true;
}

async function clearMotionLog() {
  try { await fetch('/api/motion/clear', { method:'POST' }); } catch(e) {}
  motionSeq = 0; motionPrimed = false;
  document.getElementById('motionLog').innerHTML =
    '<div class="empty">Log cleared.</div>';
}

refreshLog();
setInterval(refreshLog, 2000);
</script>
</body>
</html>
)HTML";
