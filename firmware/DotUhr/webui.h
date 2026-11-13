#pragma once
// Web-Oberfläche der DotUhr (wird im Flash abgelegt)
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DotUhr</title>
<style>
:root{--bg:#16110c;--card:#221a12;--line:#3a2c1d;--amber:#ffb000;--amber2:#ff7a00;--txt:#f3e6cf;--dim:#a8957a;--led:#ff3b1f}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.45 ui-monospace,Menlo,Consolas,monospace}
header{padding:22px 16px 12px;text-align:center;background:linear-gradient(#2a1d10,var(--bg))}
h1{margin:0 0 12px;font-size:26px;letter-spacing:.35em;color:var(--amber);text-shadow:0 0 12px #ff9d0066}
.bezel{display:inline-block;padding:12px;border-radius:14px;background:#0b0807;box-shadow:inset 0 0 0 3px #3b2a18,0 6px 18px #0008;max-width:100%}
canvas{display:block;max-width:100%;height:auto;image-rendering:pixelated}
main{max-width:760px;margin:0 auto;padding:8px 12px 40px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px;margin:14px 0}
.card h2{margin:0 0 10px;font-size:15px;color:var(--amber);letter-spacing:.12em;text-transform:uppercase}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin:8px 0}
.row label{min-width:150px;color:var(--dim)}
input,select,button,textarea{font:inherit;color:var(--txt);background:#130e09;border:1px solid var(--line);border-radius:8px;padding:7px 9px}
input[type=number]{width:90px}input[type=text],input[type=password],textarea,select.wide{flex:1;min-width:180px}
input[type=range]{flex:1;accent-color:var(--amber)}input[type=checkbox]{accent-color:var(--amber);width:18px;height:18px}
button{cursor:pointer;background:linear-gradient(var(--amber),var(--amber2));color:#1b1006;border:0;font-weight:700}
button.sec{background:#2d2217;color:var(--txt);border:1px solid var(--line);font-weight:400}
button.warn{background:#5a1d12;color:#ffd9cf}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
.kv{background:#130e09;border-radius:8px;padding:8px 10px}.kv b{display:block;color:var(--amber);font-size:17px}.kv span{color:var(--dim);font-size:12px}
.list div{padding:6px 8px;border-bottom:1px solid var(--line);cursor:pointer}.list div:hover{background:#2d2217}
.save{position:sticky;bottom:10px;text-align:center}.save button{padding:12px 34px;font-size:16px;box-shadow:0 4px 16px #000a}
#toast{position:fixed;left:50%;bottom:70px;transform:translateX(-50%);background:var(--amber);color:#1b1006;padding:9px 16px;border-radius:8px;display:none;font-weight:700}
small{color:var(--dim)}
nav.tabs{position:sticky;top:0;z-index:5;display:flex;gap:6px;overflow-x:auto;padding:10px 12px;background:var(--bg);border-bottom:1px solid var(--line)}
nav.tabs button:first-child{margin-left:auto}nav.tabs button:last-child{margin-right:auto}
nav.tabs button{background:#2d2217;color:var(--txt);border:1px solid var(--line);font-weight:400;white-space:nowrap;padding:8px 14px}
nav.tabs button.on{background:linear-gradient(var(--amber),var(--amber2));color:#1b1006;border-color:transparent;font-weight:700}
.tab{display:none}.tab.on{display:block}
</style></head><body>
<header><h1>DOT·UHR</h1><div class="bezel"><canvas id="cv" width="320" height="80"></canvas></div></header>
<nav class="tabs"><button data-t="start" onclick="tab('start')">⌂ Start</button><button data-t="anzeige" onclick="tab('anzeige')">▦ Anzeige</button><button data-t="wecker" onclick="tab('wecker')">⏰ Wecker</button><button data-t="wetter" onclick="tab('wetter')">☁ Wetter</button><button data-t="system" onclick="tab('system')">⚙ System</button></nav>
<main>
<section class="tab" id="t-start">
<div class="card"><h2>Status</h2><div class="grid" id="st"></div></div>

<div class="card"><h2>Sofort-Nachricht</h2>
<div class="row"><input type="text" id="msg" placeholder="Text, z.B. Essen ist fertig! ♥" maxlength="400">
<input type="number" id="rep" value="1" min="1" max="20" title="Wiederholungen"><button onclick="sendMsg()">Senden</button></div>
<small>Auch per HTTP: <code>http://dotuhr.local/api/message?text=Hallo&amp;repeat=2</code></small></div>

</section>
<section class="tab" id="t-anzeige">
<div class="card"><h2>Ablauf</h2>
<div class="row"><input type="checkbox" id="showClock"><label>Uhr anzeigen</label>für <input type="number" id="clockSec" min="3" max="3600"> s</div>
<div class="row"><input type="checkbox" id="showDate"><label>Datum anzeigen</label>für <input type="number" id="dateSec" min="1" max="60"> s</div>
<div class="row"><input type="checkbox" id="showWeather"><label>Wetter (Symbol)</label>für <input type="number" id="weatherSec" min="1" max="60"> s</div>
<div class="row"><input type="checkbox" id="weatherScroll"><label>Wetter-Laufschrift</label><small>Details als Laufschrift nach dem Symbol</small></div>
<div class="row"><input type="checkbox" id="showText"><label>Eigene Laufschrift</label></div>
<div class="row"><textarea id="text" rows="2" maxlength="400"></textarea></div>
<div class="row"><label>Scroll-Tempo</label><input type="range" id="speed" min="10" max="120" oninput="sv.textContent=this.value+' ms'"><span id="sv"></span></div>
<small>Kleiner Wert = schneller.</small></div>

<div class="card"><h2>Uhr &amp; Datum</h2>
<div class="row"><label>Uhr-Stil</label><select id="clockStyle" class="wide"><option value="0">HH:MM</option><option value="1">HH:MM + Sekundenbalken</option><option value="2">HH:MM:SS (klein)</option></select></div>
<div class="row"><label>Ziffern-Stil</label><select id="clockFont" class="wide" onchange="save()"><option value="0">Klassik</option><option value="1">Rund (LCD)</option></select></div>
<div class="row"><label>24-Stunden</label><input type="checkbox" id="h24"><label style="min-width:0;margin-left:20px">Doppelpunkt blinkt</label><input type="checkbox" id="blink"></div>
<div class="row"><label>Datumsformat</label><select id="dateStyle" class="wide"><option value="0">12.11.</option><option value="1">Do 12.11.</option><option value="2">Donnerstag, 12. November 2026 (Laufschrift)</option></select></div>
</div>

<div class="card"><h2>Helligkeit</h2>
<div class="row"><label>Tag</label><input type="range" id="bright" min="0" max="15" oninput="bv.textContent=this.value;bri(this.value,0)" onchange="bri(this.value,0,1)"><span id="bv"></span></div>
<div class="row"><input type="checkbox" id="nightEn"><label>Nachtmodus</label>von <input type="number" id="nightFrom" min="0" max="23"> bis <input type="number" id="nightTo" min="0" max="23"> Uhr</div>
<div class="row"><label>Nacht</label><input type="range" id="nightBright" min="0" max="15" oninput="nv.textContent=this.value;bri(this.value,1)" onchange="bri(this.value,1,1)"><span id="nv"></span></div>
<div class="row"><input type="checkbox" id="nightOff"><label>Nachts ganz aus</label></div>
<small>Die Regler wirken sofort und werden automatisch gespeichert.</small></div>

</section>
<section class="tab" id="t-wecker">
<div class="card"><h2>Wecker</h2>
<div id="alarms"></div>
<div class="row"><label>Ton</label><select id="sound" class="wide"><option value="0">Piepen</option><option value="1">Melodie</option><option value="2">Klingel</option></select></div>
<div class="row"><label>Buzzer-Typ</label><select id="buzType" class="wide"><option value="0">Passiv (Piezo / Ton-Buzzer)</option><option value="1">Aktiv (piept selbst bei Spannung)</option></select></div>
<div class="row"><label>Schlummern</label><input type="number" id="snooze" min="1" max="60"> min <label style="min-width:0;margin-left:16px">Klingelt max.</label><input type="number" id="ringMax" min="1" max="60"> min</div>
<div class="row"><button class="sec" onclick="save().then(()=>fetch('/api/alarm?action=test'))">Wecker testen</button><button class="sec" onclick="fetch('/api/alarm?action=stop')">Stopp</button></div>
<small>Ohne Wochentag = einmaliger Wecker. Klingeln: <b>Tippen</b> = Schlummern, <b>Halten</b> = Wecker aus. Ein aktiver Wecker zeigt einen Punkt oben rechts.</small></div>

</section>
<section class="tab" id="t-wetter">
<div class="card"><h2>Wetter</h2>
<div class="row"><input type="text" id="q" placeholder="Ort suchen, z.B. Hamburg"><button class="sec" onclick="geo()">Suchen</button></div>
<div class="list" id="geoList"></div>
<div class="row"><label>Ort (Anzeige)</label><input type="text" id="city"></div>
<div class="row"><label>Breite / Länge</label><input type="number" step="0.0001" id="lat" style="width:120px"><input type="number" step="0.0001" id="lon" style="width:120px">
<button class="sec" onclick="post('/api/weather/refresh').then(()=>toast('Wetter wird aktualisiert'))">Jetzt abrufen</button></div>
<small>Wetterdaten: Open-Meteo.com (kostenlos, ohne Anmeldung), Aktualisierung alle 15 min.</small></div>

</section>
<section class="tab" id="t-system">
<div class="card"><h2>Uhrzeit &amp; Datum stellen</h2>
<div class="row"><label>Aktuell auf der Uhr</label><b id="devTime">–</b><small id="devSrc"></small></div>
<div class="row"><button onclick="syncBrowser()">Zeit von diesem Gerät übernehmen</button></div>
<div class="row"><label>Von Hand</label><input type="date" id="setDate"><input type="time" id="setTime" step="1"><button class="sec" onclick="setManual()">Stellen</button></div>
<small>Ohne Internet übernimmt die Uhr die Zeit automatisch vom Handy/PC, sobald diese Seite geöffnet wird. Ist Internet da, stellt sie sich selbst (NTP) und überschreibt die Handeinstellung. Ohne Uhrenbaustein (RTC) geht die Zeit bei Stromausfall verloren.</small>
<div class="row"><label>Zeitzone</label><select id="tzSel" class="wide" onchange="if(this.value)tz.value=this.value">
<option value="CET-1CEST,M3.5.0,M10.5.0/3">Mitteleuropa (Berlin, Wien, Zürich)</option>
<option value="GMT0BST,M3.5.0/1,M10.5.0">London / Lissabon (WET)</option>
<option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Osteuropa (Helsinki, Athen)</option>
<option value="TRT-3">Türkei</option><option value="MSK-3">Moskau</option>
<option value="EST5EDT,M3.2.0,M11.1.0">New York</option><option value="PST8PDT,M3.2.0,M11.1.0">Los Angeles</option>
<option value="UTC0">UTC</option><option value="">Eigene (POSIX)…</option></select></div>
<div class="row"><label>POSIX-TZ</label><input type="text" id="tz"></div>
</div>

<div class="card"><h2>Touch-Taste (TTP223)</h2>
<div class="row"><input type="checkbox" id="touchEn"><label>Touch aktiv</label><span id="tst"></span></div>
<div class="row"><input type="checkbox" id="touchBeep"><label>Klick-Ton</label></div>
<div class="row"><input type="checkbox" id="touchToggle"><label>Selbsthalte-Modus</label><small>nur wenn das Modul auf „selbsthemmend“ gelötet ist – dann: 2× schnell tippen = halten</small></div>
<div class="row"><input type="checkbox" id="touchInv"><label>Signal invertieren</label><small>nur nötig, wenn der Sensor ohne Berührung „gedrückt“ meldet</small></div>
<small><b>Tippen</b>: nächste Anzeige (nachts: Anzeige 15 s an) · <b>1,5 s halten</b>: Anzeige an/aus</small></div>

<div class="card"><h2>WLAN</h2>
<div class="row"><label>Netzwerk (SSID)</label><input type="text" id="ssid"><button class="sec" onclick="scan()">Suchen</button></div>
<div class="list" id="scanList"></div>
<div class="row"><label>Passwort</label><input type="password" id="pass" placeholder="unverändert lassen"></div>
<div class="row"><label>Gerätename</label><input type="text" id="host"><small>→ http://<i>name</i>.local</small></div>
<div class="row"><input type="checkbox" id="lowTx"><label>Sendeleistung reduzieren</label><small>hilft bei vielen ESP32-C3 Super Mini mit Verbindungsproblemen</small></div></div>

<div class="card"><h2>System</h2>
<div class="row"><label>Matrix-Typ</label><select id="hw" class="wide"><option value="0">FC16 (meist bei 4-in-1-Modulen)</option><option value="1">GENERIC</option><option value="2">PAROLA</option><option value="3">ICSTATION</option></select></div>
<div class="row"><label>Anzahl 8x8-Module</label><input type="number" id="modules" min="1" max="16"><small>Text spiegelverkehrt/verdreht? → anderen Matrix-Typ wählen</small></div>
<div class="row"><label>Firmware-Update</label><input type="file" id="fw" accept=".bin"><button class="sec" onclick="ota()">Hochladen</button><span id="otaP"></span></div>
<div class="row"><button class="sec" onclick="confirm('Neu starten?')&&post('/api/restart')">Neustart</button>
<button class="warn" onclick="confirm('Alle Einstellungen löschen?')&&post('/api/reset')">Werkseinstellungen</button></div>
</div>

</section>
<div class="save" id="saveBar"><button onclick="save()">Speichern</button></div>
</main><div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const KEYS=['showClock','clockSec','showDate','dateSec','showWeather','weatherSec','weatherScroll','showText','text','speed',
'clockStyle','clockFont','h24','blink','dateStyle','city','lat','lon','bright','nightEn','nightFrom','nightTo','nightBright','nightOff',
'ssid','pass','host','lowTx','tz','hw','modules','sound','buzType','snooze','ringMax','touchEn','touchInv','touchBeep','touchToggle'];
const DN=['Mo','Di','Mi','Do','Fr','Sa','So'],DB=[1,2,3,4,5,6,0];
function alarmsUI(al){$('alarms').innerHTML=al.map((a,i)=>`<div class="row"><input type="checkbox" id="a${i}en" ${a.en?'checked':''}><label style="min-width:70px">Wecker ${i+1}</label><input type="time" id="a${i}t" value="${String(a.h).padStart(2,'0')}:${String(a.m).padStart(2,'0')}">`+DN.map((d,k)=>`<label style="min-width:0"><input type="checkbox" id="a${i}d${k}" ${a.days>>DB[k]&1?'checked':''}>${d}</label>`).join('')+'</div>').join('')}
function alarmsGet(){return [0,1,2].map(i=>{const t=($('a'+i+'t').value||'07:00').split(':');let d=0;DB.forEach((b,k)=>{if($('a'+i+'d'+k).checked)d|=1<<b});return{en:$('a'+i+'en').checked,h:+t[0],m:+t[1],days:d}})}
let W=32;
let briT=0;function bri(v,n,save){clearTimeout(briT);const u='/api/brightness?v='+v+'&night='+n+(save?'&save=1':'');
 if(save)fetch(u).then(()=>toast('Helligkeit gespeichert'));else briT=setTimeout(()=>fetch(u),60)}
function toast(t){const e=$('toast');e.textContent=t;e.style.display='block';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2500)}
const post=(u,b)=>fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:b?JSON.stringify(b):''});
async function load(){
 const c=await (await fetch('/api/config')).json();
 KEYS.forEach(k=>{const e=$(k);if(!e||c[k]===undefined)return;if(e.type=='checkbox')e.checked=c[k];else e.value=c[k]});
 $('pass').value='';W=c.modules*8;alarmsUI(c.alarms||[]);
 $('sv').textContent=c.speed+' ms';$('bv').textContent=c.bright;$('nv').textContent=c.nightBright;
 const o=[...$('tzSel').options].find(o=>o.value==c.tz);$('tzSel').value=o?c.tz:'';
}
async function save(){
 const d={};KEYS.forEach(k=>{const e=$(k);if(!e)return;
  d[k]=e.type=='checkbox'?e.checked:(e.type=='number'||e.type=='range'||e.tagName=='SELECT')&&k!='tz'?Number(e.value):e.value});
 d.alarms=alarmsGet();
 const r=await (await post('/api/config',d)).json();
 toast(r.restart?'Gespeichert – Neustart…':'Gespeichert');W=d.modules*8;
 if(r.restart)setTimeout(()=>location.reload(),9000);
}
async function sendMsg(){const t=$('msg').value.trim();if(!t)return;
 await fetch('/api/message?text='+encodeURIComponent(t)+'&repeat='+$('rep').value);toast('Gesendet');}
async function scan(){$('scanList').innerHTML='<div>Suche…</div>';
 const l=await (await fetch('/api/scan')).json();
 $('scanList').innerHTML='';l.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{const d=document.createElement('div');
  d.textContent=n.ssid+'  ('+n.rssi+' dBm'+(n.open?', offen':'')+')';d.onclick=()=>{$('ssid').value=n.ssid;$('scanList').innerHTML='';$('pass').focus()};$('scanList').append(d)});}
async function geo(){const q=$('q').value.trim();if(!q)return;
 try{const r=await (await fetch('https://geocoding-api.open-meteo.com/v1/search?count=6&language=de&name='+encodeURIComponent(q))).json();
  $('geoList').innerHTML='';(r.results||[]).forEach(p=>{const d=document.createElement('div');
   d.textContent=p.name+' – '+[p.admin1,p.country].filter(Boolean).join(', ');
   d.onclick=()=>{$('city').value=p.name;$('lat').value=p.latitude.toFixed(4);$('lon').value=p.longitude.toFixed(4);$('geoList').innerHTML=''};$('geoList').append(d)});
  if(!r.results)$('geoList').innerHTML='<div>Nichts gefunden</div>';
 }catch(e){$('geoList').innerHTML='<div>Ortssuche braucht Internet – Koordinaten bitte manuell eintragen.</div>'}}
function ota(){const f=$('fw').files[0];if(!f)return;const fd=new FormData();fd.append('fw',f);
 const x=new XMLHttpRequest();x.upload.onprogress=e=>$('otaP').textContent=Math.round(e.loaded*100/e.total)+' %';
 x.onload=()=>{$('otaP').textContent=x.responseText;setTimeout(()=>location.reload(),10000)};x.open('POST','/update');x.send(fd);}
const cv=$('cv'),cx=cv.getContext('2d');
async function frame(){
 try{const h=await (await fetch('/api/frame')).text();const n=h.length/2;
  if(cv.width!=n*10){cv.width=n*10;}cx.fillStyle='#0b0807';cx.fillRect(0,0,cv.width,cv.height);
  for(let x=0;x<n;x++){const v=parseInt(h.substr(x*2,2),16);for(let y=0;y<8;y++){const on=v>>y&1;
   cx.beginPath();cx.arc(x*10+5,y*10+5,3.6,0,7);cx.fillStyle=on?'#ff3b1f':'#2a1410';
   if(on){cx.shadowColor='#ff3b1f';cx.shadowBlur=6}else cx.shadowBlur=0;cx.fill()}}
 }catch(e){}setTimeout(frame,250)}
async function status(){
 try{const s=await (await fetch('/api/status')).json();const w=s.weather;
  const kv=(k,v)=>`<div class="kv"><b>${v}</b><span>${k}</span></div>`;
  $('tst').textContent=s.touch?'● berührt':'';
  $('devTime').textContent=s.timeValid?s.time:'nicht gestellt';
  $('devSrc').textContent=['','(Internet)','(von Hand)'][s.timeSource]||'';
  if(!s.timeValid&&!window._ts){window._ts=1;syncBrowser(1)}
  $('st').innerHTML=kv('Zeit',s.time.split(' ')[1]||'--')+kv('Datum',s.time.split(' ')[0])+
   (w?kv(w.text,w.temp.toFixed(1)+' °C'):kv('Wetter','–'))+
   kv(s.ap&&!s.connected?'Access-Point':'WLAN '+(s.ssid||''),s.connected?s.ip:'192.168.4.1')+
   kv('Signal',s.connected?s.rssi+' dBm':'–')+kv('Anzeige',s.off?'aus (Nacht)':s.scene+(s.night?' 🌙':''))+
   (s.ringing?kv('Wecker','KLINGELT'):s.snoozing?kv('Wecker','Schlummern'):'')+kv('Laufzeit',Math.floor(s.uptime/3600)+' h '+Math.floor(s.uptime%3600/60)+' min')+kv('Firmware','v'+s.version);
 }catch(e){}setTimeout(status,2000)}
function tab(k){if(!$('t-'+k))k='start';document.querySelectorAll('.tab').forEach(e=>e.classList.toggle('on',e.id=='t-'+k));
 document.querySelectorAll('nav.tabs button').forEach(b=>b.classList.toggle('on',b.dataset.t==k));
 $('saveBar').style.display=k=='start'?'none':'';history.replaceState(null,'','#'+k)}
async function syncBrowser(silent){await fetch('/api/time?epoch='+Math.floor(Date.now()/1000));if(!silent)toast('Uhrzeit übernommen');else toast('Uhrzeit automatisch vom Gerät übernommen')}
async function setManual(){const d=$('setDate').value,t=$('setTime').value;if(!d||!t){toast('Datum und Uhrzeit eingeben');return}
 const r=await fetch('/api/time?local='+d+'T'+(t.length==5?t+':00':t));toast(r.ok?'Uhr gestellt':'Ungültige Eingabe')}
(()=>{const n=new Date(),z=x=>String(x).padStart(2,'0');$('setDate').value=n.getFullYear()+'-'+z(n.getMonth()+1)+'-'+z(n.getDate());$('setTime').value=z(n.getHours())+':'+z(n.getMinutes())+':00'})();
tab((location.hash||'#start').slice(1));
load();frame();status();
</script></body></html>)rawliteral";
