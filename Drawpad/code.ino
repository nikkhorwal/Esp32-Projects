/*
  DRAW-PAD  -  live notepad on an SSD1306 128x64 OLED
  ESP32-S3 + ESP32Base library

  How it works
  - ESP32Base joins one of your saved Wi-Fi networks (also gives you OTA
    uploads, the esp32-s3-board.local name and the TCP log console on port 23).
  - Open the page on your phone (same Wi-Fi):
        http://esp32-s3-board.local      or the IP shown on the OLED
  - Whatever you draw on the phone canvas is sent over a WebSocket and
    drawn on the OLED instantly.
  - If no saved network is found, the board falls back to its own hotspot
    "DrawPad" and the page is at http://192.168.4.1

  Libraries:
    - ESP32Base            (your zip: Sketch > Include Library > Add .ZIP)
    - Adafruit SSD1306
    - Adafruit GFX Library
    - WebSockets           (by Markus Sattler)

  Wiring (I2C OLED) on the ESP32-S3:
    OLED VCC -> 3V3
    OLED GND -> GND
    OLED SDA -> GPIO8
    OLED SCL -> GPIO9
*/

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebSocketsServer.h>
#include <ESP32Base.h>          // keep last: it maps Serial0 -> ESP32BaseSerial

// ---------- settings ----------
#define SDA_PIN 8
#define SCL_PIN 9
#define OLED_ADDR 0x3C                 // try 0x3D if the screen stays blank
#define SCREEN_W 128
#define SCREEN_H 64

const char* AP_SSID = "DrawPad";       // fallback hotspot only
const char* AP_PASS = "12345678";
// ------------------------------

WebServer server(80);

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
WebSocketsServer ws(81);

bool dirty = false;
unsigned long lastFlush = 0;

// ---------- web page ----------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Pixel Studio</title>
<style>
:root{
  --bg:#080a0f;--panel:#11151d;--panel2:#171c26;--line:#29313d;
  --text:#f3f5f7;--muted:#8e99a8;--accent:#7cf0c7;--danger:#ff6b7a;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;min-height:100%;background:radial-gradient(circle at 50% 0,#1a222d 0,#080a0f 48%);
color:var(--text);font-family:Inter,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
body{padding:14px}
.app{width:min(100%,1000px);margin:auto}
.top{display:flex;align-items:center;gap:12px;padding:10px 4px 14px}
.logo{width:42px;height:42px;border-radius:13px;background:var(--accent);color:#07100d;
display:grid;place-items:center;font-weight:900;font-size:20px;box-shadow:0 0 28px rgba(124,240,199,.18)}
h1{font-size:17px;margin:0;font-weight:800;letter-spacing:.3px}
.meta{font-size:11px;color:var(--muted);margin-top:3px}
.status{margin-left:auto;border:1px solid var(--line);background:#0e1218;border-radius:999px;
padding:7px 11px;font-size:11px;color:var(--muted);display:flex;align-items:center;gap:7px}
.status i{width:7px;height:7px;border-radius:50%;background:var(--danger)}
.status.live{color:var(--accent)}.status.live i{background:var(--accent);box-shadow:0 0 10px var(--accent)}

.stage{background:var(--panel);border:1px solid var(--line);border-radius:24px;padding:10px;
box-shadow:0 24px 70px rgba(0,0,0,.35)}
.canvasWrap{position:relative;width:100%;aspect-ratio:2/1;background:#000;border-radius:17px;
overflow:hidden;border:1px solid #242b35;box-shadow:inset 0 0 0 1px #050608}
canvas{display:block;width:100%;height:100%;touch-action:none;image-rendering:pixelated;cursor:crosshair}
.coord{position:absolute;left:12px;bottom:10px;padding:5px 8px;border-radius:7px;
background:rgba(0,0,0,.6);font:10px ui-monospace,monospace;color:#9da7b4;pointer-events:none}

.toolbar{margin-top:10px;background:var(--panel2);border:1px solid var(--line);border-radius:18px;padding:10px}
.toolrow{display:grid;grid-template-columns:repeat(5,1fr);gap:7px}
.tool{min-height:48px;border:1px solid transparent;border-radius:12px;background:#10151d;color:#aab3bf;
font:700 11px system-ui,sans-serif;display:flex;flex-direction:column;align-items:center;justify-content:center;
gap:4px;cursor:pointer}
.tool span{font-size:18px;line-height:18px}.tool.active{background:#1a302b;border-color:#3d8c76;color:var(--accent)}
.tool:active,.action:active{transform:scale(.98)}

.controls{display:flex;align-items:center;gap:12px;margin-top:9px;padding:8px 3px}
.label{font-size:10px;font-weight:800;color:var(--muted);letter-spacing:1px}
input[type=range]{flex:1;accent-color:var(--accent);min-width:80px}
.sizeDot{width:30px;height:30px;border-radius:50%;background:#fff;display:grid;place-items:center}
.sizeDot i{display:block;background:#000;border-radius:50%;width:9px;height:9px}

.actions{display:grid;grid-template-columns:1fr 1fr 1.35fr;gap:8px;margin-top:9px}
.action{height:44px;border:1px solid var(--line);border-radius:12px;background:#10151d;color:#d7dde5;
font-weight:750;cursor:pointer}.action.danger{color:#ff9aa5;border-color:#4a252c}
.action:disabled{opacity:.35}

.info{display:flex;justify-content:space-between;padding:10px 4px 2px;color:#697483;font-size:10px}
@media(max-width:520px){
  body{padding:8px}.top{padding-bottom:10px}.logo{width:38px;height:38px}
  .stage{border-radius:19px;padding:7px}.canvasWrap{border-radius:13px}
  .toolrow{grid-template-columns:repeat(5,1fr)}.tool{min-height:46px}.tool span{font-size:17px}
  .actions{grid-template-columns:1fr 1fr 1.2fr}
}
</style>
</head>
<body>
<div class="app">
  <header class="top">
    <div class="logo">✦</div>
    <div>
      <h1>PIXEL STUDIO</h1>
      <div class="meta">SSD1306 · 128 × 64 · live mirror</div>
    </div>
    <div id="status" class="status"><i></i><b id="statusText">OFFLINE</b></div>
  </header>

  <main class="stage">
    <div class="canvasWrap">
      <canvas id="cv"></canvas>
      <div class="coord" id="coord">0, 0</div>
    </div>

    <section class="toolbar">
      <div class="toolrow">
        <button class="tool active" data-tool="pen"><span>✎</span>Pen</button>
        <button class="tool" data-tool="eraser"><span>⌫</span>Eraser</button>
        <button class="tool" data-tool="line"><span>╱</span>Line</button>
        <button class="tool" data-tool="rect"><span>□</span>Rectangle</button>
        <button class="tool" data-tool="circle"><span>○</span>Circle</button>
      </div>

      <div class="controls">
        <div class="label">SIZE</div>
        <input id="sz" type="range" min="1" max="6" value="2">
        <div class="sizeDot"><i id="dot"></i></div>
        <div id="sizeValue" class="label">2 PX</div>
      </div>

      <div class="actions">
        <button class="action" id="undo">↶ Undo</button>
        <button class="action" id="redo">↷ Redo</button>
        <button class="action danger" id="clear">Clear canvas</button>
      </div>
    </section>
  </main>

  <div class="info">
    <span>DRAW ON PHONE → OLED</span>
    <span id="toolName">PEN</span>
  </div>
</div>

<script>
const W=128,H=64,K=5;
const cv=document.getElementById('cv'),ctx=cv.getContext('2d');
cv.width=W*K;cv.height=H*K;
ctx.lineCap='round';ctx.lineJoin='round';

let sock=null,size=2,tool='pen',drawing=false,last=null,base=null;
let pts=[],pend=false;

function clearCanvas(){ctx.fillStyle='#000';ctx.fillRect(0,0,cv.width,cv.height)}
clearCanvas();

function px(s){return s<2?1:2*(s>>1)+1}
function drawSegment(x0,y0,x1,y1,s,erase=false){
  ctx.strokeStyle=erase?'#000':'#fff';
  ctx.lineWidth=px(s)*K;
  ctx.beginPath();ctx.moveTo((x0+.5)*K,(y0+.5)*K);ctx.lineTo((x1+.5)*K,(y1+.5)*K);ctx.stroke();
}
function drawDot(x,y,s,erase=false){drawSegment(x,y,x,y,s,erase)}

function renderBuffer(b){
  clearCanvas();ctx.fillStyle='#fff';
  for(let x=0;x<W;x++)for(let y=0;y<H;y++)
    if((b[x+(y>>3)*W]>>(y&7))&1)ctx.fillRect(x*K,y*K,K,K);
}

function setLive(on){
  const s=document.getElementById('status');
  s.className='status'+(on?' live':'');
  document.getElementById('statusText').textContent=on?'LIVE':'OFFLINE';
}
function connect(){
  sock=new WebSocket('ws://'+location.hostname+':81/');sock.binaryType='arraybuffer';
  sock.onopen=()=>setLive(true);
  sock.onclose=()=>{setLive(false);setTimeout(connect,1000)};
  sock.onmessage=e=>{
    if(typeof e.data==='string'){
      const p=e.data.split(',');
      if(p[0]==='C')clearCanvas();
      else if(p[0]==='P')applyPacket(p);
    }else renderBuffer(new Uint8Array(e.data));
  };
}
connect();

function applyPacket(p){
  if(p.length<6)return;
  const s=+p[1],erase=(+p[2]===0);
  let prev=null;
  for(let i=3;i+1<p.length;i+=2){
    const x=+p[i],y=+p[i+1];
    if(!prev)drawDot(x,y,s,erase);else drawSegment(prev.x,prev.y,x,y,s,erase);
    prev={x,y};
  }
}
function sendText(v){if(sock&&sock.readyState===1)sock.send(v)}
function beginEdit(){sendText('B')}
function sendPoints(arr,erase){
  if(arr.length<2)return;
  sendText('P,'+size+','+(erase?0:1)+','+arr.join(','));
}
function flush(){
  if(!pend)return;
  sendPoints(pts,tool==='eraser');
  const n=pts.length;
  pts=(drawing&&n>=2)?[pts[n-2],pts[n-1]]:[];
  pend=false;
}
setInterval(flush,15);

function pos(e){
  const r=cv.getBoundingClientRect();
  return {
    x:Math.max(0,Math.min(W-1,Math.floor((e.clientX-r.left)/r.width*W))),
    y:Math.max(0,Math.min(H-1,Math.floor((e.clientY-r.top)/r.height*H)))
  };
}
function previewLine(a,b){drawSegment(a.x,a.y,b.x,b.y,size,false)}
function rectPoints(a,b){
  return [a.x,a.y,b.x,a.y,b.x,b.y,a.x,b.y,a.x,a.y];
}
function circlePoints(a,b){
  const cx=(a.x+b.x)/2,cy=(a.y+b.y)/2,rx=Math.abs(b.x-a.x)/2,ry=Math.abs(b.y-a.y)/2;
  const out=[];const n=32;
  for(let i=0;i<=n;i++){const t=i/n*Math.PI*2;out.push(Math.round(cx+rx*Math.cos(t)),Math.round(cy+ry*Math.sin(t)))}
  return out;
}
function drawPreview(points,erase=false){
  for(let i=0;i+3<points.length;i+=2)drawSegment(points[i],points[i+1],points[i+2],points[i+3],size,erase);
}
function updateCoord(p){document.getElementById('coord').textContent=p.x+', '+p.y}

cv.addEventListener('pointerdown',e=>{
  e.preventDefault();cv.setPointerCapture(e.pointerId);
  drawing=true;last=pos(e);updateCoord(last);beginEdit();
  if(tool==='pen'||tool==='eraser'){
    drawDot(last.x,last.y,size,tool==='eraser');pts=[last.x,last.y];pend=true;
  }else{
    base=ctx.getImageData(0,0,cv.width,cv.height);
  }
});
cv.addEventListener('pointermove',e=>{
  if(!drawing)return;e.preventDefault();
  const p=pos(e);updateCoord(p);
  if(tool==='pen'||tool==='eraser'){
    if(p.x!==last.x||p.y!==last.y){
      drawSegment(last.x,last.y,p.x,p.y,size,tool==='eraser');
      pts.push(p.x,p.y);pend=true;last=p;
      if(pts.length>=40)flush();
    }
  }else{
    ctx.putImageData(base,0,0);
    if(tool==='line')previewLine(last,p);
    else if(tool==='rect')drawPreview(rectPoints(last,p));
    else if(tool==='circle')drawPreview(circlePoints(last,p));
  }
});
function finish(e){
  if(!drawing)return;drawing=false;
  const p=pos(e);updateCoord(p);
  if(tool==='pen'||tool==='eraser'){flush();pts=[];base=null;return}
  ctx.putImageData(base,0,0);
  let packet=[];
  if(tool==='line')packet=[last.x,last.y,p.x,p.y];
  if(tool==='rect')packet=rectPoints(last,p);
  if(tool==='circle')packet=circlePoints(last,p);
  drawPreview(packet,tool==='eraser');sendPoints(packet,false);base=null;
}
['pointerup','pointercancel'].forEach(t=>cv.addEventListener(t,finish));

document.querySelectorAll('.tool').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('.tool').forEach(x=>x.classList.remove('active'));
  b.classList.add('active');tool=b.dataset.tool;
  document.getElementById('toolName').textContent=tool.toUpperCase();
});

const sz=document.getElementById('sz'),dot=document.getElementById('dot');
function updateSize(){
  size=+sz.value;document.getElementById('sizeValue').textContent=size+' PX';
  const d=Math.max(5,size*4);dot.style.width=d+'px';dot.style.height=d+'px';
}
sz.oninput=updateSize;updateSize();

document.getElementById('clear').onclick=()=>{
  beginEdit();clearCanvas();pts=[];pend=false;sendText('C');
};
document.getElementById('undo').onclick=()=>{sendText('U')};
document.getElementById('redo').onclick=()=>{sendText('R')};
</script>
</body>
</html>
)rawliteral";

// ---------- drawing on the OLED ----------
static const int FRAME_BYTES = (SCREEN_W * SCREEN_H) / 8;
static const int HISTORY_MAX = 6;
uint8_t undoStack[HISTORY_MAX][FRAME_BYTES];
uint8_t redoStack[HISTORY_MAX][FRAME_BYTES];
int undoCount = 0;
int redoCount = 0;

void stroke(int x0, int y0, int x1, int y1, int s, bool erase) {
  int r = s / 2;
  uint16_t color = erase ? SSD1306_BLACK : SSD1306_WHITE;

  if (r == 0) {
    display.drawLine(x0, y0, x1, y1, color);
    return;
  }

  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    display.fillCircle(x0, y0, r, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void saveUndoSnapshot() {
  if (undoCount < HISTORY_MAX) {
    memcpy(undoStack[undoCount], display.getBuffer(), FRAME_BYTES);
    undoCount++;
  } else {
    memmove(undoStack[0], undoStack[1], (HISTORY_MAX - 1) * FRAME_BYTES);
    memcpy(undoStack[HISTORY_MAX - 1], display.getBuffer(), FRAME_BYTES);
  }
  redoCount = 0;
}

void broadcastFrame() {
  for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    if (ws.clientIsConnected(i)) {
      ws.sendBIN(i, display.getBuffer(), FRAME_BYTES);
    }
  }
}

void doUndo() {
  if (undoCount <= 0) return;

  if (redoCount < HISTORY_MAX) {
    memcpy(redoStack[redoCount], display.getBuffer(), FRAME_BYTES);
    redoCount++;
  } else {
    memmove(redoStack[0], redoStack[1], (HISTORY_MAX - 1) * FRAME_BYTES);
    memcpy(redoStack[HISTORY_MAX - 1], display.getBuffer(), FRAME_BYTES);
  }

  undoCount--;
  memcpy(display.getBuffer(), undoStack[undoCount], FRAME_BYTES);
  dirty = true;
  broadcastFrame();
}

void doRedo() {
  if (redoCount <= 0) return;

  if (undoCount < HISTORY_MAX) {
    memcpy(undoStack[undoCount], display.getBuffer(), FRAME_BYTES);
    undoCount++;
  } else {
    memmove(undoStack[0], undoStack[1], (HISTORY_MAX - 1) * FRAME_BYTES);
    memcpy(undoStack[HISTORY_MAX - 1], display.getBuffer(), FRAME_BYTES);
  }

  redoCount--;
  memcpy(display.getBuffer(), redoStack[redoCount], FRAME_BYTES);
  dirty = true;
  broadcastFrame();
}

void handleMessage(uint8_t num, uint8_t* payload, size_t length) {
  static char buf[400];
  if (length == 0 || length >= sizeof(buf)) return;

  memcpy(buf, payload, length);
  buf[length] = 0;

  if (buf[0] == 'B') {
    saveUndoSnapshot();
    for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
      if (i != num && ws.clientIsConnected(i)) ws.sendTXT(i, payload, length);
    }
    return;
  }

  if (buf[0] == 'U') {
    doUndo();
    return;
  }

  if (buf[0] == 'R') {
    doRedo();
    return;
  }

  if (buf[0] == 'C') {
    display.clearDisplay();
    dirty = true;
    for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
      if (i != num && ws.clientIsConnected(i)) ws.sendTXT(i, payload, length);
    }
    return;
  }

  if (buf[0] != 'P') return;

  // "P,size,mode,x,y,x,y,..."
  // mode: 1 = draw white, 0 = erase black
  char* tok = strtok(buf, ",");       // P
  tok = strtok(NULL, ",");            // size
  if (!tok) return;
  int s = constrain(atoi(tok), 1, 6);

  tok = strtok(NULL, ",");            // mode
  if (!tok) return;
  bool erase = atoi(tok) == 0;

  int px = -1, py = -1;
  while ((tok = strtok(NULL, ",")) != NULL) {
    int x = constrain(atoi(tok), 0, SCREEN_W - 1);
    tok = strtok(NULL, ",");
    if (!tok) break;
    int y = constrain(atoi(tok), 0, SCREEN_H - 1);

    if (px < 0) stroke(x, y, x, y, s, erase);
    else        stroke(px, py, x, y, s, erase);

    px = x;
    py = y;
  }

  // relay drawing to other connected clients
  for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    if (i != num && ws.clientIsConnected(i)) ws.sendTXT(i, payload, length);
  }

  dirty = true;
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      ws.sendBIN(num, display.getBuffer(), FRAME_BYTES);
      break;

    case WStype_TEXT:
      handleMessage(num, payload, length);
      break;

    default:
      break;
  }
}

void setup() {
  Serial0.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial0.println("SSD1306 not found - check wiring / address");
    for (;;) delay(1000);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);   display.println("PIXEL STUDIO");
  display.setCursor(0, 16);  display.println("Connecting WiFi...");
  display.display();

  String ip;
  bool online = ESP32Base.begin();

  if (online) {
    ip = ESP32Base.ipAddress();
    display.clearDisplay();
    display.setCursor(0, 0);   display.println("PIXEL STUDIO");
    display.setCursor(0, 16);  display.println("Open in browser:");
    display.setCursor(0, 28);  display.println(ip);
    display.setCursor(0, 44);  display.println("esp32-s3-board.local");
    display.display();
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    ip = WiFi.softAPIP().toString();

    display.clearDisplay();
    display.setCursor(0, 0);   display.println("PIXEL STUDIO");
    display.setCursor(0, 16);  display.print("WiFi: "); display.println(AP_SSID);
    display.setCursor(0, 28);  display.print("Pass: "); display.println(AP_PASS);
    display.setCursor(0, 44);  display.print("Open "); display.println(ip);
    display.display();
  }

  WiFi.setSleep(false);
  Serial0.println("Open http://" + ip);

  delay(4000);
  display.clearDisplay();
  display.display();

  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  ws.begin();
  ws.onEvent(onWsEvent);
}

void loop() {
  ESP32Base.loop();
  server.handleClient();
  ws.loop();

  if (dirty && millis() - lastFlush >= 25) {
    display.display();
    dirty = false;
    lastFlush = millis();
  }
}
