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
  .wifi { font-size:.8rem; color:var(--muted); text-align:right; line-height:1.5; }
  .wifi .hostlink { color:#e2e8f0; font-weight:600; text-decoration:none; }
  .wifi .hostlink:hover { color:var(--accent); }
  .grid { max-width:760px; margin:0 auto; display:grid; grid-template-columns:repeat(auto-fit,minmax(320px,1fr)); gap:16px; }
  .grid + .grid { margin-top:16px; }
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
  .field { display:flex; gap:10px; align-items:flex-end; }
  .field > div { flex:1; }
  input[type=number], input[type=text], input[type=password] { width:100%; padding:9px; border-radius:8px; border:1px solid #475569;
                       background:#0f172a; color:#e2e8f0; font-size:1rem; }
  .save { width:100%; margin-top:12px; background:var(--accent); }
  .btn.mini { flex:0 0 auto; width:auto; padding:9px 16px; background:var(--accent); margin:0; }
  .meta { font-size:.75rem; color:var(--muted); margin-top:8px; min-height:1em; }
  footer { max-width:760px; margin:18px auto 0; font-size:.75rem; color:var(--muted); text-align:center; }
  .modepill { display:flex; gap:6px; }
  .modepill .btn { padding:8px; font-size:.85rem; }
  .section { max-width:760px; margin:16px auto 0; }
  .badge.motion { background:rgba(245,158,11,.18); color:#f59e0b; animation:pulse 1.2s ease-in-out infinite; }
  .badge.broken { background:rgba(239,68,68,.18); color:#f87171; animation:pulse 1.2s ease-in-out infinite; }
  .badge.clear  { background:rgba(148,163,184,.18); color:var(--muted); }
  @keyframes pulse { 0%,100% { opacity:1; } 50% { opacity:.45; } }
  .mstats { display:flex; gap:18px; flex-wrap:wrap; font-size:.8rem; color:var(--muted); margin:6px 0 4px; }
  .mstats b { color:#e2e8f0; font-weight:600; }
  .toggles { display:grid; grid-template-columns:repeat(auto-fit,minmax(140px,1fr)); gap:10px; margin-top:12px; }
  .toggles .btn { padding:10px; font-size:.9rem; }
  .loghead { display:flex; justify-content:space-between; align-items:center; gap:10px; margin-bottom:4px; }
  .loghead .btn { flex:0 0 auto; padding:7px 12px; font-size:.8rem; background:#334155; }
  .log { background:#0b1220; border:1px solid #334155; border-radius:10px; height:320px;
         overflow-y:auto; padding:8px 10px; font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
         font-size:.78rem; line-height:1.6; }
  .log .line { display:flex; gap:8px; white-space:nowrap; align-items:baseline; }
  .log .lt { color:var(--muted); flex:0 0 auto; }
  .log .src { flex:0 0 auto; font-weight:700; font-size:.66rem; padding:1px 6px; border-radius:6px;
              background:#334155; color:#cbd5e1; letter-spacing:.03em; }
  .log .src.s0 { background:rgba(148,163,184,.18); color:#94a3b8; }
  .log .src.s1, .log .src.s2 { background:rgba(59,130,246,.2); color:#60a5fa; }
  .log .src.s3 { background:rgba(168,85,247,.2); color:#c084fc; }
  .log .src.s4 { background:rgba(245,158,11,.2); color:#fbbf24; }
  .log .src.s5 { background:rgba(239,68,68,.2); color:#f87171; }
  .log .lm { font-weight:500; overflow:hidden; text-overflow:ellipsis; }
  .log .line.alert .lm { color:#f59e0b; font-weight:700; }
  .log .empty { color:var(--muted); font-style:italic; }
</style>
</head>
<body>
  <header>
    <h1>⚡ ESP32 Relay Control</h1>
    <div class="wifi" id="wifi">connecting…</div>
  </header>

  <!-- Unified "General Info" event log — pinned to the top for quick checking -->
  <div class="section" style="margin-top:0">
    <div class="card">
      <div class="loghead">
        <h2 style="margin:0">📋 General Info</h2>
        <button class="btn" onclick="clearLog()">Clear log</button>
      </div>
      <div class="meta" style="margin:0 0 8px">Relay, laser, motion &amp; beam events — newest first.</div>
      <div class="log" id="log"><div class="empty">Waiting for events…</div></div>
    </div>
  </div>

  <!-- Relays (built dynamically from /api/status) -->
  <div class="grid" id="grid"></div>

  <!-- Laser emitter + sensors + notifications -->
  <div class="grid" id="devGrid">
    <div class="card" id="laserCard" style="display:none">
      <h2>🔆 Laser <span class="badge off" id="laserBadge">OFF</span></h2>
      <div class="row">
        <button class="btn on"  onclick="laserControl('on')">Turn ON</button>
        <button class="btn off" onclick="laserControl('off')">Turn OFF</button>
      </div>
      <label>Mode</label>
      <div class="modepill">
        <button class="btn ghost" id="laserAuto"   onclick="laserMode('auto')">Auto (cycle)</button>
        <button class="btn ghost" id="laserManual" onclick="laserMode('manual')">Manual (hold)</button>
      </div>
      <div class="settings">
        <div class="field">
          <div><label>ON duration (s)</label><input type="number" min="1" step="1" id="laserOn"></div>
          <div><label>OFF duration (s)</label><input type="number" min="1" step="1" id="laserOff"></div>
        </div>
        <button class="btn save" onclick="saveLaserSettings()">Save settings</button>
        <div class="meta" id="laserMeta">—</div>
      </div>
    </div>

    <div class="card" id="motionCard" style="display:none">
      <h2>🚶 Motion Sensor <span class="badge clear" id="motionBadge">—</span></h2>
      <div class="mstats"><span>Detections: <b id="motionCount">0</b></span><span>Signal: <b id="motionRaw">—</b></span></div>
      <div class="settings">
        <label>Detection delay (ms) — min gap between detections (0 = most responsive)</label>
        <div class="field">
          <div><input type="number" min="0" step="10" id="motionDelay"></div>
          <button class="btn mini" onclick="saveMotionDelay()">Save</button>
        </div>
      </div>
    </div>

    <div class="card" id="receiverCard" style="display:none">
      <h2>🎯 Laser Beam <span class="badge clear" id="receiverBadge">—</span></h2>
      <div class="mstats"><span>Beam breaks: <b id="receiverCount">0</b></span><span>Signal: <b id="receiverRaw">—</b></span></div>
      <div class="settings">
        <label>Detection delay (ms) — min gap between breaks (0 = most responsive)</label>
        <div class="field">
          <div><input type="number" min="0" step="10" id="receiverDelay"></div>
          <button class="btn mini" onclick="saveReceiverDelay()">Save</button>
        </div>
        <label style="margin-top:12px">Beam-present signal level — point the laser at the receiver; if it still shows BROKEN, flip this</label>
        <button class="btn ghost" id="receiverBeam" data-high="1" onclick="toggleReceiverBeam()">Beam present = HIGH</button>
      </div>
    </div>

  </div>

  <!-- Bark notification toggles (master switch + per-source + server config) -->
  <div class="section">
    <div class="card" id="barkCard" style="display:none">
      <h2>🔔 Notifications <span class="badge off" id="barkMasterBadge">OFF</span></h2>
      <div class="row">
        <button class="btn ghost" id="bk_master" onclick="toggleBarkMaster()">Bark notifications: OFF</button>
      </div>
      <div class="meta" style="margin-top:0">Per-source — tap each to toggle it independently (off by default).</div>
      <div class="toggles">
        <button class="btn ghost" id="bk_relay1" onclick="toggleBark('relay1')">Relay 1</button>
        <button class="btn ghost" id="bk_relay2" onclick="toggleBark('relay2')">Relay 2</button>
        <button class="btn ghost" id="bk_motion" onclick="toggleBark('motion')">Motion</button>
        <button class="btn ghost" id="bk_laser"  onclick="toggleBark('laser')">Laser beam</button>
      </div>
      <div class="settings">
        <label>Bark server push URL</label>
        <input type="text" id="barkUrl" placeholder="https://api.day.app/push" autocomplete="off">
        <label style="margin-top:10px">Device key</label>
        <input type="password" id="barkKey" placeholder="leave blank to keep current" autocomplete="off">
        <button class="btn save" onclick="saveBarkConfig()">Save server settings</button>
        <div class="meta" id="barkMsg"></div>
      </div>
    </div>
  </div>

  <div class="grid" style="margin-top:16px">
    <div class="card">
      <h2>⬆️ Firmware</h2>
      <div class="meta" id="fwInfo">—</div>
      <div class="settings">
        <a class="btn save" href="/update" style="display:block; text-align:center; text-decoration:none; line-height:1.2">Open updater</a>
        <div class="meta">Upload a new <code>firmware.bin</code> over the air.</div>
      </div>
    </div>
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
  </div>

  <footer>Auto-refreshing every 2 s · REST API at <code>/api/status</code> · <span id="ver"></span></footer>

<script>
let editing = null; // pause refresh of inputs while user types

async function api(path) {
  const res = await fetch(path, { method:'POST' });
  return res.json();
}

// ---- relays ----
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
        <div><label>ON duration (s)</label><input type="number" min="1" step="1" id="on-${r.id}" value="${r.onDuration}"></div>
        <div><label>OFF duration (s)</label><input type="number" min="1" step="1" id="off-${r.id}" value="${r.offDuration}"></div>
      </div>
      <button class="btn save" onclick="saveSettings(${r.id})">Save settings</button>
      <div class="meta" data-meta>${r.mode==='auto' ? 'Next switch in '+r.remaining+'s' : 'Holding ' + r.state}</div>
    </div>
  </div>`;

async function control(id, action) { render(await api(`/api/control?relay=${id}&action=${action}`)); }
async function setMode(id, mode)   { render(await api(`/api/settings?relay=${id}&mode=${mode}`)); }
async function saveSettings(id) {
  const on  = document.getElementById(`on-${id}`).value;
  const off = document.getElementById(`off-${id}`).value;
  render(await api(`/api/settings?relay=${id}&onDuration=${on}&offDuration=${off}`));
}

function updateRelays(relays) {
  const grid = document.getElementById('grid');
  if (grid.children.length !== relays.length) grid.innerHTML = relays.map(cardTpl).join('');
  relays.forEach(r => {
    const card = grid.querySelector(`.card[data-id="${r.id}"]`);
    if (!card) return;
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
}

// ---- laser emitter ----
async function laserControl(a) { render(await api(`/api/laser/control?action=${a}`)); }
async function laserMode(m)    { render(await api(`/api/laser/settings?mode=${m}`)); }
async function saveLaserSettings() {
  const on  = document.getElementById('laserOn').value;
  const off = document.getElementById('laserOff').value;
  render(await api(`/api/laser/settings?onDuration=${on}&offDuration=${off}`));
}

function updateLaser(l) {
  const card = document.getElementById('laserCard');
  if (!l || !l.enabled) { if (card) card.style.display = 'none'; return; }
  card.style.display = '';
  const badge = document.getElementById('laserBadge');
  badge.textContent = l.state.toUpperCase();
  badge.className = `badge ${l.state}`;
  document.getElementById('laserAuto').classList.toggle('active', l.mode==='auto');
  document.getElementById('laserManual').classList.toggle('active', l.mode==='manual');
  if (editing !== 'laserOn')  document.getElementById('laserOn').value  = l.onDuration;
  if (editing !== 'laserOff') document.getElementById('laserOff').value = l.offDuration;
  document.getElementById('laserMeta').textContent =
    l.mode==='auto' ? `Next switch in ${l.remaining}s` : `Holding ${l.state}`;
}

// ---- motion / beam sensors (same status shape) ----
async function saveMotionDelay()   { render(await api(`/api/motion/delay?ms=${document.getElementById('motionDelay').value||0}`)); }
async function saveReceiverDelay() { render(await api(`/api/receiver/delay?ms=${document.getElementById('receiverDelay').value||0}`)); }

// Flip the beam-present signal level (fixes an inverted receiver module live).
async function toggleReceiverBeam() {
  const b = document.getElementById('receiverBeam');
  const next = b.dataset.high === '1' ? 0 : 1;
  render(await api(`/api/receiver/config?beamHigh=${next}`));
}

// Reflect the persisted beam polarity on the toggle button.
function updateReceiverBeam(r) {
  const b = document.getElementById('receiverBeam');
  if (!b || !r || typeof r.beamHigh === 'undefined') return;
  b.dataset.high = r.beamHigh ? '1' : '0';
  b.textContent = 'Beam present = ' + (r.beamHigh ? 'HIGH' : 'LOW');
  b.classList.toggle('active', !!r.beamHigh);
}

function updateSensor(prefix, s, onLabel, offLabel, onCls) {
  const card = document.getElementById(prefix + 'Card');
  if (!s || !s.enabled) { if (card) card.style.display = 'none'; return; }
  card.style.display = '';
  const badge = document.getElementById(prefix + 'Badge');
  badge.textContent = s.active ? onLabel : offLabel;
  badge.className = 'badge ' + (s.active ? onCls : 'clear');
  document.getElementById(prefix + 'Count').textContent = s.count;
  const rw = document.getElementById(prefix + 'Raw');
  if (rw && typeof s.raw !== 'undefined')
    rw.textContent = (s.raw ? 'HIGH' : 'LOW') + (typeof s.pin !== 'undefined' ? ` (GPIO ${s.pin})` : '');
  const di = document.getElementById(prefix + 'Delay');
  if (di && editing !== prefix + 'Delay') di.value = s.delay;
}

// ---- bark notifications (master switch + server config + 4 per-source toggles) ----
async function toggleBark(src) {
  const b = document.getElementById('bk_' + src);
  const next = b.classList.contains('active') ? 0 : 1;
  render(await api(`/api/bark?source=${src}&enabled=${next}`));
}

async function toggleBarkMaster() {
  const b = document.getElementById('bk_master');
  const next = b.classList.contains('active') ? 0 : 1;
  render(await api(`/api/bark/config?master=${next}`));
}

async function saveBarkConfig() {
  const url = document.getElementById('barkUrl').value.trim();
  const key = document.getElementById('barkKey').value;
  const msg = document.getElementById('barkMsg');
  if (!url) { msg.textContent = 'Enter a push URL'; return; }
  const qs = new URLSearchParams({ url });
  if (key) qs.set('key', key);
  render(await api(`/api/bark/config?${qs.toString()}`));
  document.getElementById('barkKey').value = '';
  msg.textContent = 'Saved.';
}

function updateBark(b) {
  const card = document.getElementById('barkCard');
  if (!b || !b.available) { if (card) card.style.display = 'none'; return; }
  card.style.display = '';
  // Master kill switch (button label + header badge).
  const master = document.getElementById('bk_master');
  master.classList.toggle('active', !!b.master);
  master.textContent = 'Bark notifications: ' + (b.master ? 'ON' : 'OFF');
  const mb = document.getElementById('barkMasterBadge');
  mb.textContent = b.master ? 'ON' : 'OFF';
  mb.className = 'badge ' + (b.master ? 'on' : 'off');
  // Each per-source toggle reflects its own persisted state.
  [['relay1',b.relay1],['relay2',b.relay2],['motion',b.motion],['laser',b.laser]].forEach(([k,v]) => {
    document.getElementById('bk_' + k).classList.toggle('active', !!v);
  });
  // Server endpoint (prefill URL; key field stays blank — secret is never sent back).
  const u = document.getElementById('barkUrl');
  if (u && editing !== 'barkUrl' && typeof b.url === 'string') u.value = b.url;
  const k = document.getElementById('barkKey');
  if (k && editing !== 'barkKey')
    k.placeholder = b.keySet ? 'leave blank to keep current' : 'no key set — required';
}

async function saveWifi() {
  const ssid = document.getElementById('wifiSsid').value.trim();
  const pass = document.getElementById('wifiPass').value;
  const msg = document.getElementById('wifiMsg');
  if (!ssid) { msg.textContent = 'Enter a network name'; return; }
  msg.textContent = 'Saving… device will reboot and reconnect.';
  try {
    await fetch('/api/wifi', { method:'POST', body:new URLSearchParams({ ssid, pass }) });
    msg.textContent = 'Saved — rebooting. Reopen the device URL shortly.';
  } catch(e) { msg.textContent = 'Saved — device is rebooting.'; }
}

function render(data) {
  const w = data.wifi;
  const line = w.connected ? `${w.ssid} · ${w.ip} · ${w.rssi} dBm` : 'WiFi offline';
  const host = w.hostname
    ? `<a class="hostlink" href="http://${w.hostname}.local/">${w.hostname}.local</a><br>`
    : '';
  document.getElementById('wifi').innerHTML = host + line;
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

  updateRelays(data.relays);
  updateLaser(data.laser);
  updateSensor('motion', data.motion, 'MOTION', 'CLEAR', 'motion');
  updateSensor('receiver', data.receiver, 'BROKEN', 'INTACT', 'broken');
  updateReceiverBeam(data.receiver);
  updateBark(data.bark);
}

document.addEventListener('focusin',  e => { if (e.target.tagName==='INPUT') editing = e.target.id; });
document.addEventListener('focusout', e => { if (e.target.tagName==='INPUT') editing = null; });

async function refresh() {
  try { render(await (await fetch('/api/status')).json()); } catch(e) {}
}
refresh();
setInterval(refresh, 2000);

// ---- unified event log (incremental: only fetch events newer than we hold) ----
let logSeq = 0;        // highest event seq the page has rendered
let logPrimed = false; // false until the first batch arrives (forces scroll)
const SRC_TAG = ['SYS', 'R1', 'R2', 'LAS', 'PIR', 'BEAM'];

function logLine(e) {
  const div = document.createElement('div');
  div.className = 'line' + (e.a ? ' alert' : '');
  const ts = e.ts || `+${Math.floor(e.up/1000)}s`;
  const tt = document.createElement('span'); tt.className = 'lt'; tt.textContent = ts;
  const sp = document.createElement('span'); sp.className = 'src s' + e.src; sp.textContent = SRC_TAG[e.src] || ('#'+e.src);
  const mm = document.createElement('span'); mm.className = 'lm'; mm.textContent = e.msg;
  div.append(tt, sp, mm);
  return div;
}

async function refreshLog() {
  let data;
  try { data = await (await fetch(`/api/log?since=${logSeq}`)).json(); }
  catch(e) { return; }
  // Device rebooted or the log was cleared elsewhere → its latest seq fell behind us.
  if (data.latest < logSeq) {
    logSeq = 0; logPrimed = false;
    document.getElementById('log').innerHTML = '';
    return refreshLog();
  }
  const events = data.events || [];
  if (!events.length) return;
  const box = document.getElementById('log');
  const empty = box.querySelector('.empty');
  if (empty) empty.remove();
  const atTop = box.scrollTop < 40;
  // Events arrive oldest→newest; prepend each so the batch ends up newest-first,
  // then drop the whole batch above the rows already shown.
  const frag = document.createDocumentFragment();
  events.forEach(e => { if (e.seq > logSeq) logSeq = e.seq; frag.prepend(logLine(e)); });
  box.prepend(frag);
  while (box.children.length > 999) box.removeChild(box.lastChild); // hard cap (oldest at bottom)
  if (atTop || !logPrimed) box.scrollTop = 0;
  logPrimed = true;
}

async function clearLog() {
  try { await fetch('/api/log/clear', { method:'POST' }); } catch(e) {}
  logSeq = 0; logPrimed = false;
  document.getElementById('log').innerHTML = '<div class="empty">Log cleared.</div>';
}

refreshLog();
setInterval(refreshLog, 2000);
</script>
</body>
</html>
)HTML";
