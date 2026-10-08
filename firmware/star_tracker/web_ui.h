// Single-page web UI, served from flash. Red-on-black to protect night vision.
// It talks to two endpoints: GET /api/status (JSON) and GET /api/cmd?c=<command>.
#pragma once

static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Star Tracker</title>
<style>
:root{--bg:#090000;--fg:#ff5040;--dim:#9a2a20;--line:#3a0c08;--btn:#170303}
*{box-sizing:border-box}
body{background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,sans-serif;
margin:0 auto;padding:12px 16px 40px;max-width:560px}
h1{font-size:18px;margin:6px 0 2px;display:flex;justify-content:space-between;align-items:baseline}
#mode{font-size:22px;text-transform:uppercase;letter-spacing:.06em}
section{border:1px solid var(--line);border-radius:8px;padding:10px 12px;margin:10px 0}
h2{font-size:12px;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);margin:0 0 8px}
button{background:var(--btn);color:var(--fg);border:1px solid var(--dim);border-radius:6px;
padding:10px 12px;font:inherit;min-width:62px;touch-action:manipulation}
button:active{background:var(--line)}
input{background:#110202;color:var(--fg);border:1px solid var(--line);border-radius:4px;
padding:6px;width:5.2em;font:inherit}
label{display:inline-flex;gap:6px;align-items:center;color:var(--dim)}
.row{display:flex;flex-wrap:wrap;align-items:center;gap:6px;margin:4px 0}
.kv{display:grid;grid-template-columns:auto 1fr;gap:2px 14px}
.kv span:nth-child(odd){color:var(--dim)}
#msg{color:var(--dim);font-size:13px;min-height:1.4em}
svg circle,svg line{stroke:var(--dim);fill:none}
svg text{fill:var(--dim);font-size:11px;text-anchor:middle;dominant-baseline:middle}
#star{fill:var(--fg);stroke:none}
</style></head><body>
<h1>Star Tracker <span id="mode">-</span></h1>
<div id="msg">connecting...</div>

<section><h2>Status</h2><div class="kv">
<span>Board angle</span><span id="angle">-</span>
<span>Rate</span><span id="rate">-</span>
<span>Tracking left</span><span id="left">-</span>
<span>Homed</span><span id="homed">-</span>
<span>Fault</span><span id="fault">-</span>
<span>Battery</span><span id="batt">-</span>
</div></section>

<section><h2>Tracking</h2><div class="row">
<button onclick="c('track sidereal')">Sidereal</button>
<button onclick="c('track lunar')">Lunar</button>
<button onclick="c('track solar')">Solar</button>
<button onclick="c('stop')">Stop</button>
</div></section>

<section><h2>Position</h2><div class="row">
<button onclick="c('home')">Home</button>
<button onclick="c('rewind')">Rewind</button>
<button onclick="c('jog -1')">&minus;1 mm</button>
<button onclick="c('jog -0.1')">&minus;0.1</button>
<button onclick="c('jog 0.1')">+0.1</button>
<button onclick="c('jog 1')">+1 mm</button>
</div></section>

<section><h2>Camera (BULB)</h2><div class="row">
<label>Frames <input id="n" type="number" value="40" min="1" max="9999"></label>
<label>Exp s <input id="e" type="number" value="30" min="0.1" step="any"></label>
<label>Gap s <input id="g" type="number" value="3" min="0" step="any"></label>
<label>Delay s <input id="d" type="number" value="5" min="0" step="any"></label>
</div><div class="row">
<button onclick="shoot()">Start</button>
<button onclick="c('shoot stop')">Abort</button>
<button onclick="c('test 5')">Test 5 s</button>
<span id="cam"></span>
</div></section>

<section><h2>Polar alignment</h2><div class="row">
<svg width="150" height="150" viewBox="-75 -75 150 150" aria-label="Polaris clock">
<circle r="58"/><line x1="-6" x2="6"/><line y1="-6" y2="6"/>
<text y="-66">12</text><text x="67">3</text><text y="67">6</text><text x="-67">9</text>
<circle id="star" r="6" cx="0" cy="-58"/></svg>
<div id="pol" style="max-width:300px">Waiting for the phone's clock...</div>
</div></section>

<section><h2>Settings</h2><div class="row">
<label>R mm <input id="R" type="number" step="0.01"></label>
<label>c mm <input id="cc" type="number" step="0.01"></label>
<label>Lead mm <input id="lead" type="number" step="0.001"></label>
<label>Lon &deg;E <input id="lon" type="number" step="0.0001"></label>
<label>Trim ppm <input id="trim" type="number" step="1"></label>
</div><div class="row">
<button onclick="apply()">Apply</button>
<button onclick="c('save')">Save</button>
<button onclick="c('pec on')">PEC on</button>
<button onclick="c('pec off')">PEC off</button>
<button onclick="c('sethome')">Set home here</button>
</div></section>

<script>
const $=id=>document.getElementById(id);
let filled=false;
async function c(cmd){
  try{
    const r=await fetch('/api/cmd?c='+encodeURIComponent(cmd));
    const j=await r.json();
    $('msg').textContent=cmd+': '+(j.ok?'ok':(j.error||'error'));
  }catch(e){$('msg').textContent='no connection';}
  poll();
}
function shoot(){c(`shoot ${$('n').value} ${$('e').value} ${$('g').value} ${$('d').value}`);}
async function apply(){
  for(const [k,id] of [['radius','R'],['offset','cc'],['lead','lead'],['lon','lon']])
    await c(`set ${k} ${$(id).value}`);
  await c(`trim ${$('trim').value}`);
}
function hm(s){s=Math.max(0,Math.round(s));const h=Math.floor(s/3600),m=Math.floor(s%3600/60);
  return h+'h '+String(m).padStart(2,'0')+'m';}
function clockText(h){let H=Math.floor(h),M=Math.round((h-H)*60);if(M==60){H++;M=0;}
  if(H==0)H=12;if(H>12)H-=12;return H+':'+String(M).padStart(2,'0');}
async function poll(){
  try{
    const j=await (await fetch('/api/status')).json();
    $('mode').textContent=j.mode+(j.guiding?' (guide)':'');
    $('angle').textContent=j.angle_deg.toFixed(3)+'°  ('+j.height_mm.toFixed(2)+' mm)';
    $('rate').textContent=j.mode=='tracking'?j.rate+' '+j.rate_arcsec_s.toFixed(3)+'"/s':'-';
    $('left').textContent=j.homed?hm(j.remaining_s):'home first';
    $('homed').textContent=j.homed?'yes':'no';
    $('fault').textContent=j.fault;
    $('batt').textContent=j.battery_v!==undefined?j.battery_v.toFixed(2)+' V':'-';
    const k=j.camera;
    $('cam').textContent=k.state=='idle'?'':`${k.state} ${k.done}/${k.total}, ${hm(k.remaining_s)} left`;
    if(!filled){filled=true;const g=j.geometry;$('R').value=g.radius_mm;$('cc').value=g.offset_mm;
      $('lead').value=g.lead_mm;$('lon').value=j.lon_deg;$('trim').value=j.trim_ppm;}
    if(j.time_valid&&j.polaris){
      const p=j.polaris,a=p.clock*Math.PI/6;
      $('star').setAttribute('cx',(58*Math.sin(a)).toFixed(1));
      $('star').setAttribute('cy',(-58*Math.cos(a)).toFixed(1));
      $('pol').textContent=`Facing north, put Polaris on the reticle ring at ${clockText(p.clock)} `+
        `o'clock (hour angle ${p.hour_angle_h.toFixed(2)} h, ${p.separation_deg.toFixed(2)}° from the pole).`;
    }
  }catch(e){$('msg').textContent='no connection';}
}
c('time '+Date.now());
setInterval(poll,1000);
</script></body></html>
)HTML";
