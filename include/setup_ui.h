#pragma once
#include <Arduino.h>

// Served at "/" when the device is in AP (setup hotspot) mode. Lets the user
// pick a network and enter the WiFi password; posts to /api/wifi then reboots.
static const char SETUP_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sensor · WiFi Setup</title>
<style>
  body { margin:0; font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
         background:#0f172a; color:#e2e8f0; display:flex; min-height:100vh; align-items:center; justify-content:center; padding:16px; }
  .card { background:#1e293b; border-radius:14px; padding:22px; width:100%; max-width:380px; box-shadow:0 4px 20px rgba(0,0,0,.3); }
  h1 { font-size:1.2rem; margin:0 0 4px; }
  p { color:#94a3b8; font-size:.85rem; margin:0 0 16px; }
  label { font-size:.8rem; color:#94a3b8; display:block; margin:12px 0 4px; }
  input, select { width:100%; padding:11px; border-radius:8px; border:1px solid #475569; background:#0f172a; color:#e2e8f0; font-size:1rem; }
  button { width:100%; margin-top:18px; padding:13px; border:none; border-radius:10px; background:#3b82f6; color:#fff; font-size:1rem; font-weight:600; cursor:pointer; }
  button.scan { background:#334155; margin-top:8px; padding:9px; font-size:.85rem; }
  .msg { margin-top:14px; font-size:.85rem; min-height:1.2em; }
  .ok { color:#22c55e; } .err { color:#ef4444; }
</style>
</head>
<body>
  <div class="card">
    <h1>⚡ Sensor WiFi Setup</h1>
    <p>Connect this device to your network.</p>
    <label>Network (SSID)</label>
    <input list="nets" id="ssid" placeholder="Your WiFi name" autocomplete="off">
    <datalist id="nets"></datalist>
    <button class="scan" onclick="scan()">🔄 Scan networks</button>
    <label>Password</label>
    <input type="password" id="pass" placeholder="WiFi password">
    <button onclick="save()">Save &amp; Connect</button>
    <div class="msg" id="msg"></div>
  </div>
<script>
async function scan() {
  const m = document.getElementById('msg'); m.className='msg'; m.textContent='Scanning…';
  try {
    const nets = await (await fetch('/api/wifi/scan')).json();
    const dl = document.getElementById('nets'); dl.innerHTML='';
    nets.forEach(n => { const o=document.createElement('option'); o.value=n.ssid; dl.appendChild(o); });
    m.textContent = nets.length ? `Found ${nets.length} networks` : 'No networks found';
  } catch(e) { m.className='msg err'; m.textContent='Scan failed'; }
}
async function save() {
  const ssid = document.getElementById('ssid').value.trim();
  const pass = document.getElementById('pass').value;
  const m = document.getElementById('msg');
  if (!ssid) { m.className='msg err'; m.textContent='Enter a network name'; return; }
  m.className='msg'; m.textContent='Saving… device will reboot and try to connect.';
  const body = new URLSearchParams({ ssid, pass });
  try {
    const r = await fetch('/api/wifi', { method:'POST', body });
    const j = await r.json();
    if (j.ok) { m.className='msg ok'; m.textContent='Saved! Rebooting — reconnect to your normal WiFi, then open http://sensor.local/'; }
    else { m.className='msg err'; m.textContent = j.error || 'Save failed'; }
  } catch(e) { m.className='msg ok'; m.textContent='Saved — device is rebooting.'; }
}
scan();
</script>
</body>
</html>
)HTML";

// Served at "/update": browser-based firmware upload with a live progress bar.
static const char OTA_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sensor · Firmware Update</title>
<style>
  body { margin:0; font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
         background:#0f172a; color:#e2e8f0; display:flex; min-height:100vh; align-items:center; justify-content:center; padding:16px; }
  .card { background:#1e293b; border-radius:14px; padding:22px; width:100%; max-width:420px; box-shadow:0 4px 20px rgba(0,0,0,.3); }
  h1 { font-size:1.2rem; margin:0 0 4px; }
  p { color:#94a3b8; font-size:.85rem; margin:0 0 16px; }
  input[type=file] { width:100%; padding:10px; border-radius:8px; border:1px dashed #475569; background:#0f172a; color:#e2e8f0; }
  button { width:100%; margin-top:16px; padding:13px; border:none; border-radius:10px; background:#3b82f6; color:#fff; font-size:1rem; font-weight:600; cursor:pointer; }
  button:disabled { opacity:.5; cursor:default; }
  .bar { margin-top:16px; height:10px; border-radius:999px; background:#334155; overflow:hidden; display:none; }
  .bar > i { display:block; height:100%; width:0; background:#22c55e; transition:width .2s; }
  .msg { margin-top:12px; font-size:.85rem; min-height:1.2em; }
  .ok { color:#22c55e; } .err { color:#ef4444; }
  a { color:#3b82f6; font-size:.8rem; }
</style>
</head>
<body>
  <div class="card">
    <h1>⬆️ Firmware Update</h1>
    <p>Upload a compiled <code>firmware.bin</code> (from <code>.pio/build/esp32dev/</code>). The device reboots automatically when done.</p>
    <input type="file" id="file" accept=".bin">
    <button id="go" onclick="upload()">Flash firmware</button>
    <div class="bar" id="bar"><i id="fill"></i></div>
    <div class="msg" id="msg"></div>
    <p style="margin-top:16px"><a href="/">← Back to controls</a></p>
  </div>
<script>
function upload() {
  const f = document.getElementById('file').files[0];
  const msg = document.getElementById('msg');
  if (!f) { msg.className='msg err'; msg.textContent='Choose a .bin file first'; return; }
  const fd = new FormData(); fd.append('firmware', f, f.name);
  const xhr = new XMLHttpRequest();
  document.getElementById('go').disabled = true;
  document.getElementById('bar').style.display = 'block';
  xhr.upload.onprogress = e => {
    if (e.lengthComputable) {
      const pct = Math.round(e.loaded / e.total * 100);
      document.getElementById('fill').style.width = pct + '%';
      msg.className='msg'; msg.textContent = `Uploading… ${pct}%`;
    }
  };
  xhr.onload = () => {
    if (xhr.status === 200) { msg.className='msg ok'; msg.textContent='✅ Update complete — device is rebooting.'; }
    else { msg.className='msg err'; msg.textContent='❌ ' + (xhr.responseText || 'Update failed'); document.getElementById('go').disabled=false; }
  };
  xhr.onerror = () => { msg.className='msg ok'; msg.textContent='Upload finished — device rebooting.'; };
  xhr.open('POST', '/update');
  xhr.send(fd);
}
</script>
</body>
</html>
)HTML";
