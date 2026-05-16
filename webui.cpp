#include "webui.h"
#include "config.h"
#include "stations.h"
#include "audio_player.h"
#include "display.h"
#include "controls.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <ArduinoJson.h>

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#ifndef FW_RELEASE
#define FW_RELEASE "unknown"
#endif

static WebServer s_server(80);
static DNSServer s_dns;
static Preferences s_prefs;
static bool s_apMode = true;

bool   webui_isApMode()        { return s_apMode; }
String webui_currentSsidOrIp() {
  if (s_apMode) return WiFi.softAPIP().toString();
  return WiFi.localIP().toString();
}

// ============================================================================
//  HTML pages (PROGMEM) - dark theme, same look-and-feel as the horloge UI.
// ============================================================================

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radio</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
         background:#0e1726; color:#e6edf3; }
  header { background:#1f6feb; padding:14px 18px; font-size:20px; font-weight:600; }
  header a { color:white; text-decoration:none; margin-left:14px; font-size:14px; opacity:.85; }
  main { max-width:520px; margin:0 auto; padding:16px; }
  .card { background:#161f33; border:1px solid #243049; border-radius:10px;
          padding:16px; margin-bottom:14px; }
  h2 { margin:0 0 12px 0; font-size:16px; color:#9fb3d1; font-weight:600;
       text-transform:uppercase; letter-spacing:1px; }
  button { padding:10px; border-radius:8px; border:1px solid #2c3a5a;
           background:#0e1726; color:#e6edf3; font-size:15px; cursor:pointer; width:100%; margin:4px 0; }
  button.primary  { background:#1f6feb; border-color:#1f6feb; color:white; font-weight:600; }
  button.danger   { background:#3b0d0d; border-color:#6b1a1a; color:#f08a8a; }
  .row { display:flex; align-items:center; gap:10px; margin:8px 0; }
  .row label { width:120px; color:#9fb3d1; flex-shrink:0; }
  .row input, .row select { flex:1; padding:8px; border-radius:6px;
        border:1px solid #2c3a5a; background:#0e1726; color:#e6edf3; font-size:15px; }
  .now { text-align:center; padding:6px 0; }
  .now .name { font-size:24px; font-weight:700; color:#e6edf3; }
  .now .title { font-size:14px; color:#7ee787; min-height:18px; margin-top:6px;
                white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
  .vol { display:flex; align-items:center; gap:10px; }
  .vol input { flex:1; }
  .stations { list-style:none; padding:0; margin:0; }
  .stations li { display:flex; align-items:center; gap:8px; padding:8px;
        border:1px solid #243049; border-radius:6px; margin:4px 0; cursor:pointer; }
  .stations li.cur { background:#1c2640; border-color:#1f6feb; }
  .stations li input.n { flex:1; background:transparent; color:#e6edf3;
        border:0; font-size:15px; padding:4px; }
  .stations li input.u { flex:2; background:transparent; color:#9fb3d1;
        border:0; font-size:13px; padding:4px; font-family:monospace; }
  .stations li button { width:auto; padding:6px 10px; }
  a { color:#7ee787; }
  footer { font-size:12px; color:#7280a0; text-align:center; padding:12px; }
</style>
</head>
<body>
<header>Radio <a href="/wifi">Wi-Fi</a> <a href="/update">OTA</a> <a href="/debug">Debug</a></header>
<main>

  <section class="card">
    <h2>Lecture en cours</h2>
    <div class="now">
      <div class="name" id="curName">-</div>
      <div class="title" id="curTitle">-</div>
    </div>
    <div class="row" style="margin-top:10px">
      <button class="primary" onclick="ctrl('prev')">&laquo; Pr&eacute;c.</button>
      <button class="primary" onclick="ctrl('toggle')" id="ppBtn">Lecture</button>
      <button class="primary" onclick="ctrl('next')">Suiv. &raquo;</button>
    </div>
  </section>

  <section class="card">
    <h2>Volume</h2>
    <div class="vol">
      <span id="volV">0</span>
      <input id="vol" type="range" min="0" max="21" value="0">
      <button style="width:auto;padding:6px 10px" onclick="ctrl('mute')" id="muteBtn">Muet</button>
    </div>
  </section>

  <section class="card">
    <h2>Stations</h2>
    <ul class="stations" id="stList"></ul>
    <button onclick="addStation()">+ Ajouter une station</button>
    <button class="primary" onclick="saveStations()">Enregistrer la liste</button>
    <button class="danger" onclick="resetStations()">R&eacute;initialiser (Montr&eacute;al par d&eacute;faut)</button>
  </section>

  <section class="card">
    <h2>Affichage</h2>
    <div class="row">
      <label>R&eacute;tro&eacute;clairage</label>
      <input id="bl" type="range" min="0" max="100" value="80"> <span id="blV">80</span>%
    </div>
    <button class="primary" onclick="saveDisplay()">Enregistrer</button>
  </section>

  <footer>
    <div id="fwver">Firmware: ...</div>
    <a href="/wifi">Configuration Wi-Fi</a> &nbsp;|&nbsp;
    <a href="/update">Mise &agrave; jour firmware (OTA)</a>
  </footer>
</main>

<script>
function ctrl(a) {
  fetch('/api/ctrl?a='+a).then(refreshNow);
}

function refreshNow() {
  fetch('/api/now').then(r=>r.json()).then(d=>{
    document.getElementById('curName').textContent  = d.name || '(aucune)';
    document.getElementById('curTitle').textContent = d.title || '\u2014';
    document.getElementById('ppBtn').textContent    = d.playing ? 'Pause' : 'Lecture';
    document.getElementById('vol').value            = d.vol;
    document.getElementById('volV').textContent     = d.vol;
    document.getElementById('muteBtn').textContent  = d.muted ? 'R\u00e9activer' : 'Muet';
    // highlight current station
    document.querySelectorAll('.stations li').forEach((li,i)=>{
      li.classList.toggle('cur', i === d.cur);
    });
  }).catch(()=>{});
}

function loadStations() {
  fetch('/api/stations').then(r=>r.json()).then(arr=>{
    const ul = document.getElementById('stList');
    ul.innerHTML = '';
    arr.forEach((s,i)=> ul.appendChild(stationRow(s.name, s.url)));
    refreshNow();
  });
}

function stationRow(name, url) {
  const li = document.createElement('li');
  const ni = document.createElement('input'); ni.className='n'; ni.value=name||''; ni.placeholder='Nom';
  const ui = document.createElement('input'); ui.className='u'; ui.value=url||'';  ui.placeholder='https://stream...';
  const pl = document.createElement('button'); pl.textContent='\u25B6';
  pl.onclick = ()=>{
    const idx = Array.from(li.parentNode.children).indexOf(li);
    fetch('/api/play?i='+idx).then(refreshNow);
  };
  const rm = document.createElement('button'); rm.textContent='\u00D7'; rm.title='Supprimer';
  rm.onclick = ()=> li.remove();
  li.appendChild(ni); li.appendChild(ui); li.appendChild(pl); li.appendChild(rm);
  return li;
}

function addStation() {
  document.getElementById('stList').appendChild(stationRow('', ''));
}

function saveStations() {
  const arr = [];
  document.querySelectorAll('#stList li').forEach(li=>{
    const n = li.querySelector('.n').value.trim();
    const u = li.querySelector('.u').value.trim();
    if (u.length) arr.push({name:n||'(sans nom)', url:u});
  });
  fetch('/api/stations', { method:'POST', headers:{'Content-Type':'application/json'},
                            body: JSON.stringify(arr) })
    .then(()=>{ alert('Enregistr\u00e9!'); loadStations(); });
}

function resetStations() {
  if (!confirm('R\u00e9initialiser la liste aux stations Montr\u00e9al par d\u00e9faut?')) return;
  fetch('/api/stations/reset', { method:'POST' }).then(loadStations);
}

document.getElementById('vol').addEventListener('change', function(){
  fetch('/api/vol?v='+this.value).then(refreshNow);
});
document.getElementById('vol').addEventListener('input', function(){
  document.getElementById('volV').textContent = this.value;
});

function saveDisplay() {
  const bl = document.getElementById('bl').value;
  fetch('/api/display?bl='+bl).then(()=>alert('Enregistr\u00e9!'));
}
document.getElementById('bl').oninput = function(){
  document.getElementById('blV').textContent = this.value;
};
fetch('/api/display').then(r=>r.json()).then(d=>{
  document.getElementById('bl').value = d.bl;
  document.getElementById('blV').textContent = d.bl;
});

fetch('/api/version').then(r=>r.json()).then(d=>{
  let t = 'Firmware: ' + d.version;
  if (d.release) t += ' (' + d.release + ')';
  document.getElementById('fwver').textContent = t;
});

loadStations();
setInterval(refreshNow, 3000);
</script>
</body>
</html>
)HTML";

// ---------------- Wi-Fi page (same look as horloge) -------------------------
static const char WIFI_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Configuration Wi-Fi</title>
<style>
  :root { color-scheme: dark; }
  body { margin:0; font-family: system-ui, sans-serif; background:#0e1726; color:#e6edf3; }
  header { background:#1f6feb; padding:14px 18px; font-size:20px; font-weight:600; }
  main { max-width: 520px; margin:0 auto; padding:16px; }
  .card { background:#161f33; border:1px solid #243049; border-radius:10px;
          padding:16px; margin-bottom:14px; }
  h2 { margin:0 0 12px 0; font-size:16px; color:#9fb3d1;
       text-transform:uppercase; letter-spacing:1px; }
  input, select, button { width:100%; padding:10px; border-radius:6px;
        border:1px solid #2c3a5a; background:#0e1726; color:#e6edf3;
        font-size:15px; box-sizing:border-box; margin:6px 0; }
  button { background:#1f6feb; border-color:#1f6feb; color:white;
           cursor:pointer; font-weight:600; }
  button.secondary { background:#3d2230; border-color:#6b2230; }
  ul { list-style:none; padding:0; margin:0; }
  li { padding:8px 10px; border-radius:6px; cursor:pointer;
       display:flex; justify-content:space-between; align-items:center;
       border:1px solid #243049; margin:4px 0; }
  li:hover { background:#1c2640; }
  li.lock::after { content:"\01F512"; }
  small { color:#9fb3d1; }
  a { color:#7ee787; }
  .ok  { color:#7ee787; }
  .err { color:#f08a8a; }
</style>
</head>
<body>
<header>Configuration Wi-Fi</header>
<main>

  <section class="card">
    <h2>Actuel</h2>
    <div id="cur"><small>chargement...</small></div>
  </section>

  <section class="card">
    <h2>R&eacute;seaux disponibles</h2>
    <button onclick="scan()">Actualiser</button>
    <ul id="list"><li><small>recherche...</small></li></ul>
  </section>

  <section class="card">
    <h2>Connexion</h2>
    <input id="ssid" type="text" placeholder="SSID" autocomplete="off">
    <input id="pass" type="password" placeholder="Mot de passe (vide si ouvert)">
    <input id="pickedBssid" type="hidden">
    <input id="pickedCh"    type="hidden">
    <button onclick="save()">Enregistrer et connecter</button>
    <div id="pickedLabel"><small></small></div>
    <div id="msg"></div>
  </section>

  <section class="card">
    <h2>Nom d'h&ocirc;te</h2>
    <input id="hname" type="text" placeholder="radio" maxlength="32" autocomplete="off">
    <button onclick="saveHostname()">Enregistrer le nom</button>
    <div id="hmsg"></div>
  </section>

  <section class="card">
    <h2>R&eacute;initialisation</h2>
    <button class="secondary" onclick="reset()">Oublier le Wi-Fi et red&eacute;marrer en AP</button>
  </section>

  <section class="card">
    <h2>Red&eacute;marrage</h2>
    <button class="secondary" onclick="reboot()">Red&eacute;marrer la radio</button>
    <div id="rmsg"></div>
  </section>

  <p><a href="/">&laquo; retour</a></p>
</main>

<script>
async function refresh() {
  try {
    const s = await (await fetch('/api/wifi/status')).json();
    let html = `<div>Mode : <b>${s.mode}</b></div>`;
    if (s.mode === 'STA') {
      html += `<div>SSID : <b>${s.ssid||''}</b></div>`;
      html += `<div>IP : <b>${s.ip||''}</b></div>`;
      html += `<div>RSSI : <b>${s.rssi} dBm</b></div>`;
    } else {
      html += `<div>AP SSID : <b>${s.ap_ssid}</b></div>`;
      html += `<div>AP IP : <b>${s.ap_ip}</b></div>`;
      if (s.saved_ssid) html += `<div>SSID STA enregistr\u00e9 : <b>${s.saved_ssid}</b></div>`;
    }
    document.getElementById('cur').innerHTML = html;
  } catch(e) {}
}
async function scan() {
  document.getElementById('list').innerHTML = '<li><small>recherche...</small></li>';
  try {
    const nets = await (await fetch('/api/wifi/scan')).json();
    const ul = document.getElementById('list');
    ul.innerHTML = '';
    if (!nets.length) { ul.innerHTML = '<li><small>aucun r\u00e9seau trouv\u00e9</small></li>'; return; }
    nets.forEach(n => {
      const li = document.createElement('li');
      if (n.enc) li.classList.add('lock');
      li.innerHTML = `<span><b>${n.ssid||'(hidden)'}</b> <small>ch ${n.ch} - ${n.bssid||''}</small></span><small>${n.rssi} dBm</small>`;
      li.onclick = () => {
        document.getElementById('ssid').value = n.ssid;
        document.getElementById('pickedBssid').value = n.bssid||'';
        document.getElementById('pickedCh').value    = n.ch||'';
        document.getElementById('pickedLabel').textContent =
          (n.bssid && n.ch) ? `\u00e9pingl\u00e9 sur ch ${n.ch} (${n.bssid})` : '';
        document.getElementById('pass').focus();
      };
      ul.appendChild(li);
    });
  } catch(e) { document.getElementById('list').innerHTML = '<li><small class="err">\u00e9chec du scan</small></li>'; }
}
async function save() {
  const ssid = document.getElementById('ssid').value.trim();
  const pass = document.getElementById('pass').value;
  const msg  = document.getElementById('msg');
  if (!ssid) { msg.innerHTML = '<span class="err">SSID requis</span>'; return; }
  msg.innerHTML = '<small>enregistrement et red\u00e9marrage en mode STA...</small>';
  const fd = new URLSearchParams(); fd.append('ssid', ssid); fd.append('pass', pass);
  // If the user clicked a specific row in the scan list we also pin its
  // BSSID/channel so the radio connects to THAT AP, not a co-named neighbour.
  // Only send when the SSID still matches what was clicked, otherwise the
  // user typed a different name manually and we let the firmware re-scan.
  const pickedSsid = document.getElementById('ssid').value.trim();
  const pBssid     = document.getElementById('pickedBssid').value;
  const pCh        = document.getElementById('pickedCh').value;
  if (pBssid && pCh && pickedSsid === ssid) {
    fd.append('bssid', pBssid);
    fd.append('ch', pCh);
  }
  await fetch('/api/wifi/save', { method:'POST', body: fd });
  msg.innerHTML = '<span class="ok">Enregistr\u00e9. Red\u00e9marrage en cours.</span>';
}
async function reset() {
  if (!confirm('Oublier le Wi-Fi enregistr\u00e9 et red\u00e9marrer en mode AP?')) return;
  await fetch('/api/wifi/reset', { method:'POST' });
  document.getElementById('msg').innerHTML = '<span class="ok">R\u00e9initialis\u00e9. Red\u00e9marrage en AP...</span>';
}
async function reboot() {
  if (!confirm('Red\u00e9marrer la radio maintenant?')) return;
  document.getElementById('rmsg').innerHTML = '<span class="ok">Red\u00e9marrage en cours...</span>';
  try { await fetch('/api/reboot', { method:'POST' }); } catch(e){}
}
async function saveHostname() {
  const h = document.getElementById('hname').value.trim();
  if (!h) { document.getElementById('hmsg').innerHTML='<span class="err">Nom requis</span>'; return; }
  const fd = new URLSearchParams(); fd.append('hostname', h);
  const r = await fetch('/api/wifi/hostname', { method:'POST', body: fd });
  if (r.ok) document.getElementById('hmsg').innerHTML='<span class="ok">Enregistr\u00e9 (actif au red\u00e9marrage)</span>';
  else document.getElementById('hmsg').innerHTML='<span class="err">Erreur</span>';
}
refresh(); scan();
fetch('/api/wifi/hostname').then(r=>r.json()).then(d=>{ document.getElementById('hname').value = d.hostname||''; });
</script>
</body>
</html>
)HTML";

// ---------------- Debug page (simulate front buttons) ------------------------
static const char DEBUG_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radio - Debug</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: system-ui, -apple-system, sans-serif;
         background: #0e1726; color: #e6edf3; }
  header { background: #1f6feb; padding: 14px 18px; font-size: 20px; font-weight: 600; }
  header a { color: white; text-decoration: none; margin-left: 16px; font-size: 14px; opacity: 0.85; }
  main { max-width: 520px; margin: 0 auto; padding: 16px; }
  .card { background: #161f33; border: 1px solid #243049; border-radius: 10px;
          padding: 16px; margin-bottom: 14px; }
  h2 { margin: 0 0 12px; font-size: 16px; color: #9fb3d1; text-transform: uppercase; letter-spacing: 1px; }

  .now { text-align:center; padding:6px 0; }
  .now .name  { font-size:22px; font-weight:700; }
  .now .title { font-size:13px; color:#7ee787; min-height:18px; margin-top:6px;
                white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
  .meta { display:flex; justify-content:space-around; margin-top:10px;
          font-size:13px; color:#9fb3d1; }
  .meta b { color:#e6edf3; }

  .pad { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr 1fr 1fr;
         grid-template-rows: auto auto auto; gap: 8px; }
  .btn { padding: 14px 0; border-radius: 10px; border: 2px solid #2c3a5a;
         background: #1a2540; color: #e6edf3; font-size: 16px; font-weight: 700;
         cursor: pointer; text-align: center; user-select: none;
         transition: background 0.1s; }
  .btn:active, .btn.pressed { background: #1f6feb; border-color: #1f6feb; }
  .btn-up    { grid-column: 2; grid-row: 1; }
  .btn-down  { grid-column: 2; grid-row: 3; }
  .btn-left  { grid-column: 1; grid-row: 2; }
  .btn-right { grid-column: 3; grid-row: 2; }
  .btn-a     { grid-column: 5; grid-row: 1; background: #0d3b1e; border-color: #1a6b35; }
  .btn-b     { grid-column: 6; grid-row: 1; background: #3b0d0d; border-color: #6b1a1a; }
  .btn-a:active { background: #1a6b35; }
  .btn-b:active { background: #6b1a1a; }
  .hint { font-size:12px; color:#7280a0; margin-top:10px; line-height:1.6; }
  .hint code { background:#0e1726; padding:1px 6px; border-radius:4px; color:#e6edf3; }
  .status { font-size: 12px; color: #7280a0; text-align: center; margin-top: 8px; }
</style>
</head>
<body>
<header>Radio - Debug <a href="/">Accueil</a> <a href="/wifi">Wi-Fi</a> <a href="/update">OTA</a></header>
<main>

<section class="card">
  <h2>Etat</h2>
  <div class="now">
    <div class="name"  id="curName">-</div>
    <div class="title" id="curTitle">-</div>
  </div>
  <div class="meta">
    <div>Vol: <b id="mVol">-</b></div>
    <div>Mute: <b id="mMute">-</b></div>
    <div>Lecture: <b id="mPlay">-</b></div>
  </div>
</section>

<section class="card">
  <h2>Boutons</h2>
  <div class="pad" id="pad">
    <div class="btn btn-up"    data-btn="up">&#9650; UP</div>
    <div class="btn btn-left"  data-btn="left">&#9664; L</div>
    <div class="btn btn-right" data-btn="right">R &#9654;</div>
    <div class="btn btn-down"  data-btn="down">&#9660; DN</div>
    <div class="btn btn-a"     data-btn="a">A</div>
    <div class="btn btn-b"     data-btn="b">B</div>
  </div>
  <div class="hint">
    UP/DOWN : volume &nbsp; LEFT/RIGHT : station prec./suiv.<br>
    A : lecture / pause &nbsp; B : reserve<br>
    Clavier : <code>&#8593;&#8595;&#8592;&#8594;</code>, <code>A</code>, <code>B</code>
  </div>
  <div class="status" id="status">Pret</div>
</section>

</main>
<script>
function esc(s){ const d=document.createElement('div'); d.textContent=s==null?'':s; return d.innerHTML; }

async function refresh(){
  try{
    const r = await fetch('/api/now');
    const s = await r.json();
    document.getElementById('curName').textContent  = s.name  || '-';
    document.getElementById('curTitle').textContent = s.title || '-';
    document.getElementById('mVol').textContent  = s.vol;
    document.getElementById('mMute').textContent = s.muted ? 'oui' : 'non';
    document.getElementById('mPlay').textContent = s.playing ? 'oui' : 'non';
  }catch(e){}
}

let lastPress = 0;
const COOLDOWN = 120;

async function press(btn){
  const now = Date.now();
  if (now - lastPress < COOLDOWN) return;
  lastPress = now;
  document.getElementById('status').textContent = 'Envoi: ' + btn.toUpperCase();
  // Flash the button visually.
  const el = document.querySelector('[data-btn="'+btn+'"]');
  if (el) { el.classList.add('pressed'); setTimeout(()=>el.classList.remove('pressed'), 120); }
  try{
    const fd = new URLSearchParams();
    fd.append('btn', btn);
    await fetch('/api/debug/press', { method:'POST', body: fd });
    document.getElementById('status').textContent = 'OK: ' + btn.toUpperCase();
    setTimeout(refresh, 80);
  }catch(e){
    document.getElementById('status').textContent = 'Erreur';
  }
}

const pad = document.getElementById('pad');
pad.addEventListener('touchstart', function(e){
  const b = e.target.closest('[data-btn]');
  if (b) { e.preventDefault(); press(b.dataset.btn); }
}, {passive:false});
pad.addEventListener('mousedown', function(e){
  const b = e.target.closest('[data-btn]');
  if (b) press(b.dataset.btn);
});

document.addEventListener('keydown', function(e){
  const map = { ArrowUp:'up', ArrowDown:'down', ArrowLeft:'left', ArrowRight:'right',
                a:'a', A:'a', Enter:'a', b:'b', B:'b', Escape:'b' };
  const btn = map[e.key];
  if (btn) { e.preventDefault(); press(btn); }
});

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)HTML";

// ---------------- OTA page --------------------------------------------------
static const char OTA_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mise &agrave; jour firmware</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
         background:#0e1726; color:#e6edf3; }
  header { background:#1f6feb; padding:14px 18px; font-size:20px; font-weight:600; }
  main { max-width:520px; margin:0 auto; padding:16px; }
  .card { background:#161f33; border:1px solid #243049; border-radius:10px;
          padding:16px; margin-bottom:14px; }
  h2 { margin:0 0 12px 0; font-size:16px; color:#9fb3d1;
       text-transform:uppercase; letter-spacing:1px; }
  input[type=file] { display:none; }
  .filebtn { display:block; width:100%; padding:10px; border-radius:6px;
             border:1px solid #2c3a5a; background:#0e1726; color:#e6edf3;
             font-size:15px; margin:6px 0; cursor:pointer; text-align:center; }
  button { width:100%; padding:10px; border-radius:8px; border:1px solid #2c3a5a;
           background:#0e1726; color:#e6edf3; font-size:15px; cursor:pointer; margin:6px 0; }
  button.primary { background:#1f6feb; border-color:#1f6feb; color:white; font-weight:600; }
  button.upload  { background:#c85000; border-color:#c85000; color:white; font-weight:600; }
  .bar { height:14px; background:#243049; border-radius:7px; overflow:hidden; margin-top:14px; }
  .bar>div { height:100%; background:#7ee787; width:0%; transition:width 0.2s; }
  pre { color:#9fb3d1; white-space:pre-wrap; word-break:break-word; }
  a { color:#7ee787; }
</style>
</head>
<body>
<header>Mise &agrave; jour firmware</header>
<main>
  <section class="card">
    <h2>Version actuelle</h2>
    <div id="curVer">...</div>
  </section>
  <section class="card">
    <h2>V&eacute;rifier les mises &agrave; jour</h2>
    <button class="primary" type="button" onclick="checkUpdate()">V&eacute;rifier sur GitHub</button>
    <div id="updStatus" style="margin-top:10px;color:#9fb3d1;font-size:14px"></div>
  </section>
  <section class="card">
    <h2>T&eacute;l&eacute;verser le firmware</h2>
    <p style="color:#9fb3d1;font-size:13px">S&eacute;lectionnez un fichier <code>.bin</code> compil&eacute; et envoyez-le.</p>
    <form id="f" enctype="multipart/form-data" method="POST" action="/update">
      <input type="file" id="fw" name="firmware" accept=".bin" required>
      <label class="filebtn" for="fw" id="fl">Choisir un fichier</label>
      <button class="upload" type="submit">Envoyer le firmware</button>
    </form>
    <div class="bar"><div id="p"></div></div>
    <pre id="log"></pre>
  </section>
  <p><a href="/">&laquo; retour</a></p>
</main>
<script>
document.getElementById('fw').addEventListener('change',function(){
  document.getElementById('fl').textContent = this.files[0] ? this.files[0].name : 'Choisir un fichier';
});
fetch('/api/version').then(r=>r.json()).then(d=>{
  const el = document.getElementById('curVer');
  el.innerHTML = '<b>Build:</b> '+d.version+'<br><b>Release:</b> '+(d.release||'?');
  el.dataset.release = d.release || '';
  el.dataset.build   = d.version || '';
});
function checkUpdate(){
  const s = document.getElementById('updStatus');
  s.innerHTML = 'V\u00e9rification...';
  s.style.color = '#9fb3d1';
  const el = document.getElementById('curVer');
  const curRelease = el.dataset.release || '';
  const curBuild   = el.dataset.build   || '';
  fetch('https://api.github.com/repos/morinf12/Radio/releases/latest')
  .then(r=>{ if(!r.ok) throw new Error('HTTP '+r.status); return r.json(); })
  .then(d=>{
    const tag = d.tag_name || '?';
    const isUpToDate = (curRelease === tag);
    s.innerHTML = '<b>Install\u00e9:</b> '+curRelease+' ('+curBuild+')<br><b>Disponible:</b> '+tag;
    if (isUpToDate) {
      s.innerHTML += '<br><span style="color:#7ee787;font-size:16px">&#x2714; Votre firmware est \u00e0 jour</span>';
      s.style.color = '#7ee787';
    } else {
      s.innerHTML += '<br><span style="color:#f0883e;font-size:16px;font-weight:bold">&#x26A0; Une nouvelle version est disponible!</span>';
      s.innerHTML += '<br><a href="'+d.html_url+'" target="_blank" style="color:#58a6ff">Notes de version</a>';
      s.style.color = '#d29922';
      const bin = d.assets && d.assets.find(a=>a.name.endsWith('.bin'));
      if (bin) {
        s.innerHTML += '<br><a href="'+bin.browser_download_url+'" style="display:inline-block;margin-top:8px;padding:10px 16px;background:#238636;color:white;border-radius:6px;text-decoration:none;font-weight:bold">&#x2B07; T\u00e9l\u00e9charger '+tag+'</a>';
        s.innerHTML += '<br><small style="color:#9fb3d1">Puis utilisez le formulaire ci-dessous pour l\'envoyer</small>';
      }
    }
  })
  .catch(e=>{ s.textContent = 'Erreur: '+e.name+': '+e.message; s.style.color = '#f85149'; });
}
const f=document.getElementById('f'),p=document.getElementById('p'),l=document.getElementById('log');
f.addEventListener('submit',e=>{
  e.preventDefault();
  const fd=new FormData(f);
  const x=new XMLHttpRequest();
  x.upload.onprogress=ev=>{ if(ev.lengthComputable){
    p.style.width=(ev.loaded/ev.total*100).toFixed(1)+'%'; }};
  x.onload=()=>{ l.textContent=x.status+' '+x.responseText;
    if(x.status===200){ l.textContent+='\nRed\u00e9marrage en cours...'; }};
  x.onerror=()=>{ l.textContent='Erreur d\'envoi'; };
  x.open('POST','/update'); x.send(fd);
});
</script>
</body>
</html>
)HTML";

// ============================================================================
//  Handlers
// ============================================================================

static String jsonEscape(const String& s) {
  String o; o.reserve(s.length() + 4);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if      (c == '"')  o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c < 0x20)  { char b[8]; snprintf(b,sizeof(b),"\\u%04x",c); o += b; }
    else                o += c;
  }
  return o;
}

static void hRoot()    { s_server.send_P(200, "text/html", INDEX_HTML); }
static void hWifiPage(){ s_server.send_P(200, "text/html", WIFI_HTML); }
static void hOtaPage() { s_server.send_P(200, "text/html", OTA_HTML); }
static void hDebugPage(){ s_server.send_P(200, "text/html", DEBUG_HTML); }

static void hDebugPress() {
  if (!s_server.hasArg("btn")) { s_server.send(400, "text/plain", "missing btn"); return; }
  String b = s_server.arg("btn");
  CtrlEvent ev = CTRL_NONE;
  if      (b == "up")    ev = CTRL_ROT_CW;
  else if (b == "down")  ev = CTRL_ROT_CCW;
  else if (b == "left")  ev = CTRL_PREV;
  else if (b == "right") ev = CTRL_NEXT;
  else if (b == "a")     ev = CTRL_ENC_PRESS;
  else if (b == "b")     ev = CTRL_ENC_LONG;
  if (ev != CTRL_NONE) controls_inject(ev);
  s_server.send(200, "application/json", "{\"ok\":true}");
}

static void hVersion() {
  char buf[160];
  snprintf(buf, sizeof(buf),
    "{\"version\":\"%s\",\"release\":\"%s\"}", FW_VERSION, FW_RELEASE);
  s_server.send(200, "application/json", buf);
}

static void hNow() {
  int idx = stations_currentIndex();
  String name = (idx >= 0 && idx < stations_count()) ? stations_get(idx).name : String();
  String j = "{";
  j += "\"name\":\""    + jsonEscape(name) + "\",";
  j += "\"title\":\""   + jsonEscape(audio_streamTitle()) + "\",";
  j += "\"playing\":"   + String(audio_isPlaying() ? "true" : "false") + ",";
  j += "\"vol\":"       + String(audio_getVolume()) + ",";
  j += "\"muted\":"     + String(audio_isMuted() ? "true" : "false") + ",";
  j += "\"cur\":"       + String(idx);
  j += "}";
  s_server.send(200, "application/json", j);
}

static void playIndex(int idx) {
  if (idx < 0) idx = stations_count() - 1;
  if (idx >= stations_count()) idx = 0;
  if (stations_count() == 0) return;
  stations_setCurrentIndex(idx);
  const Station& st = stations_get(idx);
  display_setStation(st.name, idx, stations_count());
  display_setStreamTitle("");
  // connecttohost() is async; status is promoted by radio.cpp loop() once
  // playback actually starts.
  bool ok = audio_play(st.url);
  display_setStatus(ok ? "Connexion..." : "Erreur de connexion");
}

static void hPlay() {
  int i = s_server.hasArg("i") ? s_server.arg("i").toInt() : stations_currentIndex();
  playIndex(i);
  hNow();
}

static void hCtrl() {
  String a = s_server.hasArg("a") ? s_server.arg("a") : "";
  if      (a == "next")   playIndex(stations_currentIndex() + 1);
  else if (a == "prev")   playIndex(stations_currentIndex() - 1);
  else if (a == "toggle") {
    if (audio_isPlaying()) audio_stop();
    else                   playIndex(stations_currentIndex());
  } else if (a == "mute") audio_setMute(!audio_isMuted());
  display_setVolume(audio_getVolume(), MAX_VOLUME, audio_isMuted());
  hNow();
}

static void hVol() {
  if (s_server.hasArg("v")) {
    int v = s_server.arg("v").toInt();
    if (v < 0) v = 0; if (v > MAX_VOLUME) v = MAX_VOLUME;
    audio_setVolume((uint8_t)v);
    display_setVolume(audio_getVolume(), MAX_VOLUME, audio_isMuted());
  }
  hNow();
}

static void hStationsGet() {
  String j = "[";
  for (int i = 0; i < stations_count(); ++i) {
    if (i) j += ",";
    j += "{\"name\":\"" + jsonEscape(stations_get(i).name)
       + "\",\"url\":\""   + jsonEscape(stations_get(i).url) + "\"}";
  }
  j += "]";
  s_server.send(200, "application/json", j);
}

static void hStationsPost() {
  String body = s_server.arg("plain");
  JsonDocument doc;
  if (deserializeJson(doc, body)) { s_server.send(400, "text/plain", "bad json"); return; }
  if (!doc.is<JsonArray>())       { s_server.send(400, "text/plain", "expected array"); return; }
  Station list[STATIONS_MAX]; int n = 0;
  for (JsonObject o : doc.as<JsonArray>()) {
    if (n >= STATIONS_MAX) break;
    list[n].name = String((const char*)(o["name"] | ""));
    list[n].url  = String((const char*)(o["url"]  | ""));
    if (list[n].url.length()) n++;
  }
  stations_setAll(list, n);
  s_server.send(200, "application/json", "{\"ok\":true}");
}

static void hStationsReset() {
  stations_resetDefaults();
  s_server.send(200, "application/json", "{\"ok\":true}");
}

static void hDisplay() {
  if (s_server.hasArg("bl")) {
    int v = s_server.arg("bl").toInt();
    if (v < 0) v = 0; if (v > 100) v = 100;
    display_setBacklight((uint8_t)v);
    Preferences p; if (p.begin("radio", false)) { p.putUChar("bl", v); p.end(); }
  }
  String j = "{\"bl\":" + String(display_getBacklight()) + "}";
  s_server.send(200, "application/json", j);
}

static void hWifiStatus() {
  String j = "{";
  if (s_apMode) {
    j += "\"mode\":\"AP\",";
    j += "\"ap_ssid\":\"" + String(WIFI_AP_SSID) + "\",";
    j += "\"ap_ip\":\""   + WiFi.softAPIP().toString() + "\",";
    j += "\"saved_ssid\":\"" + s_prefs.getString("ssid", "") + "\"";
  } else {
    j += "\"mode\":\"STA\",";
    j += "\"ssid\":\"" + WiFi.SSID() + "\",";
    j += "\"ip\":\""   + WiFi.localIP().toString() + "\",";
    j += "\"rssi\":"   + String(WiFi.RSSI());
  }
  j += "}";
  s_server.send(200, "application/json", j);
}

static void hWifiScan() {
  int n = WiFi.scanNetworks(false, false);
  String j = "[";
  bool first = true;
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i).isEmpty()) continue;
    if (!first) j += ","; first = false;
    uint8_t* b = WiFi.BSSID(i);
    char bs[18];
    snprintf(bs, sizeof(bs), "%02X:%02X:%02X:%02X:%02X:%02X",
             b[0], b[1], b[2], b[3], b[4], b[5]);
    j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",";
    j += "\"bssid\":\""  + String(bs) + "\",";
    j += "\"rssi\":"    + String(WiFi.RSSI(i)) + ",";
    j += "\"ch\":"      + String(WiFi.channel(i)) + ",";
    j += "\"enc\":"     + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true");
    j += "}";
  }
  j += "]";
  WiFi.scanDelete();
  s_server.send(200, "application/json", j);
}

static void hWifiSave() {
  if (!s_server.hasArg("ssid")) { s_server.send(400, "text/plain", "missing ssid"); return; }
  String ssid = s_server.arg("ssid");
  s_prefs.putString("ssid", ssid);
  s_prefs.putString("pass", s_server.hasArg("pass") ? s_server.arg("pass") : String());
  // If the UI sent a specific BSSID+channel (user picked a row in the scan
  // list) - prime the fast-connect cache with it so the next boot connects
  // to THAT AP, not whichever co-named one happens to be loudest.
  bool primed = false;
  if (s_server.hasArg("ch") && s_server.hasArg("bssid")) {
    int    ch  = s_server.arg("ch").toInt();
    String bs  = s_server.arg("bssid");
    unsigned b[6] = {0};
    if (ch > 0 && ch <= 14 &&
        sscanf(bs.c_str(), "%x:%x:%x:%x:%x:%x",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
      uint8_t bssid[6] = { (uint8_t)b[0], (uint8_t)b[1], (uint8_t)b[2],
                           (uint8_t)b[3], (uint8_t)b[4], (uint8_t)b[5] };
      s_prefs.putUChar("ch", (uint8_t)ch);
      s_prefs.putBytes("bssid", bssid, 6);
      s_prefs.putString("ssid_seen", ssid);
      primed = true;
      Serial.printf("[WiFi] user-pinned ch=%d bssid=%s\n", ch, bs.c_str());
    }
  }
  if (!primed) {
    // No specific AP picked - clear cache so boot does a fresh scan.
    s_prefs.remove("ch");
    s_prefs.remove("bssid");
    s_prefs.remove("ssid_seen");
  }
  s_server.send(200, "application/json", "{\"ok\":true}");
  delay(500); ESP.restart();
}

static void hWifiReset() {
  s_prefs.remove("ssid"); s_prefs.remove("pass");
  s_server.send(200, "application/json", "{\"ok\":true}");
  delay(500); ESP.restart();
}

static void hReboot() {
  s_server.send(200, "application/json", "{\"ok\":true}");
  delay(500); ESP.restart();
}

static void hWifiHostname() {
  if (s_server.method() == HTTP_GET) {
    String h = s_prefs.getString("hostname", "radio");
    s_server.send(200, "application/json", "{\"hostname\":\"" + h + "\"}");
  } else {
    if (!s_server.hasArg("hostname")) { s_server.send(400, "text/plain", "missing"); return; }
    String h = s_server.arg("hostname"); h.trim();
    if (h.length() == 0 || h.length() > 32) { s_server.send(400, "text/plain", "invalid"); return; }
    s_prefs.putString("hostname", h);
    WiFi.setHostname(h.c_str());
    s_server.send(200, "application/json", "{\"ok\":true}");
  }
}

static void hCaptive() {
  if (s_apMode) {
    s_server.sendHeader("Location",
      String("http://") + WiFi.softAPIP().toString() + "/wifi", true);
    s_server.send(302, "text/plain", "");
  } else {
    s_server.send(404, "text/plain", "not found");
  }
}

// ----- OTA (Update.h) -------------------------------------------------------
static void hOtaResult() {
  bool ok = !Update.hasError();
  s_server.sendHeader("Connection", "close");
  s_server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.errorString());
  if (ok) { delay(500); ESP.restart(); }
}
static void hOtaUpload() {
  HTTPUpload& up = s_server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] start: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) Serial.printf("[OTA] done: %u bytes\n", up.totalSize);
    else                  Update.printError(Serial);
  }
}

// ============================================================================
//  Setup
// ============================================================================

static const char* authModeName(int a) {
  switch (a) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA-PSK";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2-PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2-PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3-PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3-PSK";
    default:                        return "?";
  }
}

static void scanForSsid(const String& ssid, int& bestRssi, uint8_t bssid[6], int& channel) {
  bestRssi = -1000; channel = 0; memset(bssid, 0, 6);
  Serial.println(F("[WiFi] scanning..."));
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks(false, true);   // sync, show hidden
  bool found = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == ssid) {
      Serial.printf("[WiFi] FOUND '%s' rssi=%d ch=%d auth=%s\n",
                    WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                    authModeName(WiFi.encryptionType(i)));
      found = true;
      if (WiFi.RSSI(i) > bestRssi) {
        bestRssi = WiFi.RSSI(i);
        channel  = WiFi.channel(i);
        memcpy(bssid, WiFi.BSSID(i), 6);
      }
    }
  }
  if (!found) Serial.printf("[WiFi] SSID '%s' NOT seen (%d nets scanned)\n", ssid.c_str(), n);
  WiFi.scanDelete();
}

// Last raw 802.11 disconnect reason reported by the IDF. Useful to diagnose
// auth/4-way-handshake failures (e.g. repeater rejecting us). See
// esp_wifi_types.h wifi_err_reason_t for the code list.
static volatile uint8_t s_lastDisconnectReason = 0;
static const char* wifiReasonStr(uint8_t r) {
  switch (r) {
    case 1:   return "UNSPECIFIED";
    case 2:   return "AUTH_EXPIRE";
    case 3:   return "AUTH_LEAVE";
    case 4:   return "ASSOC_EXPIRE";
    case 5:   return "ASSOC_TOOMANY";
    case 6:   return "NOT_AUTHED";
    case 7:   return "NOT_ASSOCED";
    case 8:   return "ASSOC_LEAVE";
    case 9:   return "ASSOC_NOT_AUTHED";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
    case 16:  return "GROUP_KEY_UPDATE_TIMEOUT";
    case 17:  return "IE_IN_4WAY_DIFFERS";
    case 18:  return "GROUP_CIPHER_INVALID";
    case 19:  return "PAIRWISE_CIPHER_INVALID";
    case 20:  return "AKMP_INVALID";
    case 23:  return "IEEE_802_1X_AUTH_FAILED";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    case 206: return "AP_TSF_RESET";
    case 207: return "ROAMING";
    default:  return "?";
  }
}

static bool tryStation(const String& ssid, const String& pass) {
  Serial.printf("[WiFi] STA -> %s\n", ssid.c_str());
  WiFi.persistent(false);          // do not write creds to flash behind our back
  WiFi.mode(WIFI_OFF);             // clear any prior AP/STA state
  delay(100);
  WiFi.mode(WIFI_STA);

  // Register a one-shot disconnect-reason logger so we can see WHY association
  // or 4-way handshake fails (esp. against repeaters with mixed WPA2/WPA3).
  static bool s_evtRegistered = false;
  if (!s_evtRegistered) {
    s_evtRegistered = true;
    WiFi.onEvent([](WiFiEvent_t e, WiFiEventInfo_t info) {
      uint8_t r = info.wifi_sta_disconnected.reason;
      s_lastDisconnectReason = r;
      Serial.printf("[WiFi] disconnect reason=%u (%s)\n", r, wifiReasonStr(r));
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  }
  s_lastDisconnectReason = 0;
  WiFi.setSleep(false);            // disable PS - common cause of HS timeouts
  // Accept WPA-PSK as the lowest acceptable auth so a WPA2/WPA3-mixed AP can
  // negotiate down to WPA2-PSK (classic ESP32 has no WPA3 supplicant).
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
  WiFi.setAutoReconnect(true);

  // ---- Fast-path: try the cached channel + BSSID from NVS first ----------
  // After a successful connect we persist the AP's channel and BSSID. On the
  // next boot we skip the ~2 s active scan entirely and go straight to that
  // (ssid, channel, bssid) tuple. If the AP has moved, this attempt fails
  // within a couple of seconds and we fall through to the scan path below.
  uint8_t cachedBssid[6] = {0};
  uint8_t cachedCh = (uint8_t)s_prefs.getUChar("ch", 0);
  size_t  bsLen    = s_prefs.getBytes("bssid", cachedBssid, 6);
  bool    haveCache = (cachedCh > 0 && bsLen == 6);
  String  cachedSsid = s_prefs.getString("ssid_seen", "");
  if (haveCache && cachedSsid != ssid) haveCache = false; // cache is for a different SSID

  auto attempt = [&](uint8_t ch, const uint8_t* bssid, uint32_t timeoutMs) -> bool {
    wifi_config_t wcfg = {};
    strncpy((char*)wcfg.sta.ssid,     ssid.c_str(), sizeof(wcfg.sta.ssid) - 1);
    strncpy((char*)wcfg.sta.password, pass.c_str(), sizeof(wcfg.sta.password) - 1);
    // arduino-esp32 2.0.7+ enables pmf_cfg.capable by default, which some
    // routers refuse the 4-way handshake with (reason 204 HANDSHAKE_TIMEOUT).
    wcfg.sta.pmf_cfg.capable  = false;
    wcfg.sta.pmf_cfg.required = false;
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    if (ch > 0) {
      wcfg.sta.channel = ch;
      if (bssid) {
        memcpy(wcfg.sta.bssid, bssid, 6);
        wcfg.sta.bssid_set = 1;
      }
    }
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_start();
    esp_wifi_connect();
    uint32_t t0 = millis();
    wl_status_t last = WL_IDLE_STATUS;
    while (millis() - t0 < timeoutMs) {
      wl_status_t s = WiFi.status();
      if (s != last) { Serial.printf("[WiFi] status=%d\n", (int)s); last = s; }
      if (s == WL_CONNECTED) return true;
      if (s == WL_NO_SSID_AVAIL || s == WL_CONNECT_FAILED) {
        Serial.println(F("[WiFi] giving up early (no SSID / auth fail)"));
        break;
      }
      delay(250);
    }
    return false;
  };

  auto saveCache = [&]() {
    uint8_t* b = WiFi.BSSID();
    if (!b) return;
    s_prefs.putUChar("ch", (uint8_t)WiFi.channel());
    s_prefs.putBytes("bssid", b, 6);
    s_prefs.putString("ssid_seen", ssid);
    Serial.printf("[WiFi] cached ch=%d bssid=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  WiFi.channel(), b[0],b[1],b[2],b[3],b[4],b[5]);
  };

  if (haveCache) {
    Serial.printf("[WiFi] fast-connect: ch=%d bssid=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  cachedCh, cachedBssid[0], cachedBssid[1], cachedBssid[2],
                  cachedBssid[3], cachedBssid[4], cachedBssid[5]);
    if (attempt(cachedCh, cachedBssid, 6000)) {
      Serial.print(F("[WiFi] STA IP: ")); Serial.println(WiFi.localIP());
      Serial.printf("[WiFi] RSSI: %d dBm, channel: %d\n", WiFi.RSSI(), WiFi.channel());
      saveCache();
      return true;
    }
    Serial.println(F("[WiFi] fast-connect failed, falling back to scan"));
    WiFi.disconnect(true, false);
    // Invalidate stale cache so we don't keep retrying it next boot.
    s_prefs.remove("ch");
    s_prefs.remove("bssid");
    s_prefs.remove("ssid_seen");
  }

  // ---- Slow path: scan, pin to strongest BSSID, then connect -------------
  int    bestRssi = -1000, channel = 0;
  uint8_t bssid[6] = {0};
  scanForSsid(ssid, bestRssi, bssid, channel);
  bool havePin = (channel > 0);
  if (havePin) {
    Serial.printf("[WiFi] pinning to BSSID %02X:%02X:%02X:%02X:%02X:%02X ch=%d (rssi=%d)\n",
                  bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], channel, bestRssi);
  }
  if (attempt(havePin ? (uint8_t)channel : 0, havePin ? bssid : nullptr, 15000)) {
    Serial.print(F("[WiFi] STA IP: ")); Serial.println(WiFi.localIP());
    Serial.printf("[WiFi] RSSI: %d dBm, channel: %d\n", WiFi.RSSI(), WiFi.channel());
    saveCache();
    return true;
  }
  Serial.printf("[WiFi] STA connect failed, final status=%d, last reason=%u (%s)\n",
                (int)WiFi.status(), s_lastDisconnectReason,
                wifiReasonStr(s_lastDisconnectReason));
  WiFi.disconnect(true, true);
  return false;
}

static void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  // Force FCC regulatory domain so the radio uses the FCC power table (up to
  // 20 dBm on 2.4 GHz). Some IDF builds default to a more restrictive domain
  // ("01" world-safe) that caps TX at ~15 dBm. Must be called AFTER WiFi mode
  // selection but BEFORE softAP() / connect, otherwise the cap from the old
  // domain stays in effect for the active interface.
  wifi_country_t country = {
    .cc = "US",
    .schan = 1,
    .nchan = 11,
    .max_tx_power = 84,           // 84 * 0.25 dBm = 21 dBm
    .policy = WIFI_COUNTRY_POLICY_MANUAL,
  };
  esp_wifi_set_country(&country);
  // Raise the per-interface TX cap before bringing up the AP. Using the raw
  // IDF call (in 0.25 dBm units) rather than WiFi.setTxPower() lets us ask
  // for 21 dBm; the chip will clamp internally to whatever its phy_init blob
  // / calibration supports (usually 19.5 dBm on ESP32-D0WD).
  esp_wifi_set_max_tx_power(84);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHAN);
  // softAP() may have re-applied a default; re-assert max power.
  esp_wifi_set_max_tx_power(84);
  int8_t txp = 0;
  esp_wifi_get_max_tx_power(&txp);
  Serial.printf("[WiFi] AP up, TX power = %.2f dBm\n", txp * 0.25f);
  Serial.print(F("[WiFi] AP IP: ")); Serial.println(WiFi.softAPIP());
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", WiFi.softAPIP());
  s_apMode = true;
}

void webui_begin() {
  s_prefs.begin("wifi", false);
  String hostname = s_prefs.getString("hostname", "radio");
  WiFi.setHostname(hostname.c_str());

  String ssid = s_prefs.getString("ssid", "");
  String pass = s_prefs.getString("pass", "");
  Serial.printf("[WiFi] credentials from NVS, ssid='%s' (len=%u), pass len=%u\n",
                ssid.c_str(), (unsigned)ssid.length(), (unsigned)pass.length());
  bool sta = false;
  if (ssid.length()) sta = tryStation(ssid, pass);
  if (!sta) startAccessPoint(); else s_apMode = false;

  MDNS.begin(hostname.c_str());

  // Routes
  s_server.on("/",        hRoot);
  s_server.on("/wifi",    hWifiPage);
  s_server.on("/update",  HTTP_GET, hOtaPage);
  s_server.on("/update",  HTTP_POST, hOtaResult, hOtaUpload);
  s_server.on("/debug",   HTTP_GET, hDebugPage);
  s_server.on("/api/debug/press", HTTP_POST, hDebugPress);
  s_server.on("/api/version",         hVersion);
  s_server.on("/api/now",             hNow);
  s_server.on("/api/play",            hPlay);
  s_server.on("/api/ctrl",            hCtrl);
  s_server.on("/api/vol",             hVol);
  s_server.on("/api/stations",        HTTP_GET,  hStationsGet);
  s_server.on("/api/stations",        HTTP_POST, hStationsPost);
  s_server.on("/api/stations/reset",  HTTP_POST, hStationsReset);
  s_server.on("/api/display",         hDisplay);

  s_server.on("/api/wifi/status",     hWifiStatus);
  s_server.on("/api/wifi/scan",       hWifiScan);
  s_server.on("/api/wifi/save",       HTTP_POST, hWifiSave);
  s_server.on("/api/wifi/reset",      HTTP_POST, hWifiReset);
  s_server.on("/api/wifi/hostname",   hWifiHostname);
  s_server.on("/api/reboot",          HTTP_POST, hReboot);

  // Captive-portal catch-alls
  s_server.on("/generate_204",        hCaptive);
  s_server.on("/fwlink",              hCaptive);
  s_server.on("/hotspot-detect.html", hCaptive);
  s_server.onNotFound(                hCaptive);

  s_server.begin();
  Serial.println(F("[Web] HTTP server started"));
}

void webui_loop() {
  if (s_apMode) s_dns.processNextRequest();
  s_server.handleClient();
}
