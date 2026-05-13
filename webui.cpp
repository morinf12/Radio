#include "webui.h"
#include "config.h"
#include "stations.h"
#include "audio_player.h"
#include "display.h"
#include <WiFi.h>
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
<header>Radio <a href="/wifi">Wi-Fi</a> <a href="/update">OTA</a></header>
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
    <button onclick="save()">Enregistrer et connecter</button>
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
      li.innerHTML = `<span><b>${n.ssid||'(hidden)'}</b> <small>ch ${n.ch}</small></span><small>${n.rssi} dBm</small>`;
      li.onclick = () => { document.getElementById('ssid').value = n.ssid; document.getElementById('pass').focus(); };
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
  document.getElementById('curVer').innerHTML =
    '<b>Build:</b> '+d.version+'<br><b>Release:</b> '+d.release;
});
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
  display_setStatus("Connexion...");
  display_setStreamTitle("");
  audio_play(st.url);
  display_setStatus(audio_isPlaying() ? "En cours" : "Erreur de connexion");
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
    j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",";
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
  s_prefs.putString("ssid", s_server.arg("ssid"));
  s_prefs.putString("pass", s_server.hasArg("pass") ? s_server.arg("pass") : String());
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

static bool tryStation(const String& ssid, const String& pass) {
  Serial.printf("[WiFi] STA -> %s\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.length() ? pass.c_str() : (const char*)nullptr);
  uint32_t t0 = millis();
  while (millis() - t0 < 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("[WiFi] STA IP: ")); Serial.println(WiFi.localIP());
      return true;
    }
    delay(250);
  }
  Serial.println(F("[WiFi] STA connect failed"));
  WiFi.disconnect(true, true);
  return false;
}

static void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHAN);
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
  bool sta = false;
  if (ssid.length()) sta = tryStation(ssid, pass);
  if (!sta) startAccessPoint(); else s_apMode = false;

  MDNS.begin(hostname.c_str());

  // Routes
  s_server.on("/",        hRoot);
  s_server.on("/wifi",    hWifiPage);
  s_server.on("/update",  HTTP_GET, hOtaPage);
  s_server.on("/update",  HTTP_POST, hOtaResult, hOtaUpload);

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
