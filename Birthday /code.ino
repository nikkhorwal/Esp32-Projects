// Heer's Birthday - ESP32-S3, 0.96" OLED, buzzer, lyric-coloured RGB + candle LEDs, phone web control
// Pins: OLED SDA 13 SCL 14 | buzzer 7 | RGB 4,5,6 | candles 15,16,17,18,10
// (ASCII only on purpose; free ArduinoDroid cuts files at 20000 characters.)
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>
#include <ESP32Base.h>

#define OLED_SDA 13
#define OLED_SCL 14
#define BUZZER_PIN 7
#define BUZZER_BITS 10
#define OCTAVE_SHIFT 0        // 1 = one octave higher, if the buzzer sounds weak
#define BALANCE_REF_HZ 262.0f

// ---- melody (from mavanhung78/Happy_birth_day_oled0.96_V1) ----
#define TEMPO_BPM 140
#define WHOLE_MS ((60000UL * 4) / TEMPO_BPM)
#define SOUND_PCT 70          // note sounds 70%, then 30% silence
const uint16_t melody[] = {
  262, 262, 294, 262, 349, 330,        // C4 C4 D4 C4 F4 E4
  262, 262, 294, 262, 392, 349,        // C4 C4 D4 C4 G4 F4
  262, 262, 523, 440, 349, 330, 294,   // C4 C4 C5 A4 F4 E4 D4
  466, 466, 440, 349, 392, 349         // Bb4 Bb4 A4 F4 G4 F4
};
const uint8_t noteDiv[] = {            // 8 = eighth, 4 = quarter, 2 = half
  8, 8, 4, 4, 4, 2,
  8, 8, 4, 4, 4, 2,
  8, 8, 4, 4, 4, 4, 2,
  8, 8, 4, 4, 4, 2
};
const uint8_t NOTE_COUNT = sizeof(melody) / sizeof(melody[0]);
unsigned long noteMs(uint8_t n) { return WHOLE_MS / noteDiv[n]; }

// OLED lyric highlight per note: line * 2 + word
const uint8_t lyricPos[] = {
  0, 0, 2, 2, 4, 5,
  0, 0, 2, 2, 4, 5,
  0, 0, 2, 2, 4, 5, 5,
  0, 0, 2, 2, 4, 5
};
const char* const lyrics[2][3] = {
  { "HAPPY", "BIRTHDAY", "TO YOU" },
  { "HAPPY", "BIRTHDAY", "DEAR HEER" }
};

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer server(80);
bool oledOk = false;
uint8_t volumePercent = 100;   // slider
uint8_t balancePercent = 75;   // evens out low/high note loudness
uint8_t lightsPercent = 60;    // LED brightness

bool playing = false, soundOn = false;
uint8_t cur = 0;
unsigned long noteStart = 0, doneAt = 0, lastFrame = 0;
enum Screen { S_READY, S_PLAYING, S_DONE };
Screen screenMode = S_READY;
uint8_t frameCount = 0;

// ---------------- buzzer ----------------
void buzzerOff() { ledcWrite(BUZZER_PIN, 0); }

// Passive buzzers are louder on high notes: lower them to match the low ones.
uint32_t dutyForNote(int f) {
  float g = powf(BALANCE_REF_HZ / f, balancePercent / 50.0f);
  if (g > 1) g = 1;
  float level = volumePercent / 100.0f * g;
  if (level <= 0) return 0;
  if (level > 1) level = 1;
  uint32_t d = asinf(level) / 3.14159265f * 1023.0f + 0.5f;
  return d ? d : 1;
}

void buzzerOn(int f) {
  if (volumePercent == 0) { buzzerOff(); return; }
  ledcChangeFrequency(BUZZER_PIN, (uint32_t)f << OCTAVE_SHIFT, BUZZER_BITS);
  ledcWrite(BUZZER_PIN, dutyForNote(f));
}

// ---------------- OLED ----------------
void centerTextAt(const char* t, int cx, int y, uint8_t size) {
  int w = strlen(t) * 6 * size - size;
  display.setTextSize(size);
  display.setCursor(cx - w / 2, y);
  display.print(t);
}

void drawCake(int ox, int oy, uint8_t fr) {
  for (int i = 0; i < 3; i++) {
    int cx = ox + 10 + i * 12, fy = oy + 3;
    display.fillRect(cx, oy + 9, 3, 9, SSD1306_WHITE);
    if (((fr + i) & 1) == 0) {
      display.fillCircle(cx + 1, fy + 2, 2, SSD1306_WHITE);
      display.drawPixel(cx + 1, fy - 1, SSD1306_WHITE);
    } else {
      display.fillCircle(cx + 1, fy + 2, 1, SSD1306_WHITE);
      display.drawFastVLine(cx + 1, fy - 2, 3, SSD1306_WHITE);
    }
  }
  display.drawRoundRect(ox + 5, oy + 18, 34, 11, 3, SSD1306_WHITE);
  display.drawRoundRect(ox, oy + 29, 44, 15, 3, SSD1306_WHITE);
  for (int x = ox + 6; x <= ox + 38; x += 8) display.fillCircle(x, oy + 36, 1, SSD1306_WHITE);
  display.drawFastHLine(ox - 2, oy + 46, 48, SSD1306_WHITE);
}

// size-2 line; the highlighted word is drawn inverted
void drawLyricLine(const char* line, int y, int hiWord) {
  int len = strlen(line), x0 = (128 - len * 12) / 2, wordNo = 0, i = 0;
  display.setTextSize(2);
  while (i < len) {
    int j = i;
    while (j < len && line[j] != ' ') j++;
    int wx = x0 + i * 12;
    bool hi = (wordNo == hiWord);
    if (hi) display.fillRect(wx - 3, y - 2, (j - i) * 12 + 5, 19, SSD1306_WHITE);
    display.setTextColor(hi ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(wx, y);
    for (int k = i; k < j; k++) display.write(line[k]);
    wordNo++;
    i = j + 1;
  }
  display.setTextColor(SSD1306_WHITE);
}

void showSplash() {
  if (!oledOk) return;
  display.clearDisplay();
  drawCake(3, 8, 0);
  centerTextAt("STARTING", 89, 20, 1);
  centerTextAt("WiFi...", 89, 32, 1);
  display.display();
}

void showReady() {
  screenMode = S_READY;
  if (!oledOk) return;
  display.clearDisplay();
  drawCake(3, 8, frameCount);
  centerTextAt("HAPPY", 89, 9, 1);
  centerTextAt("BIRTHDAY", 89, 19, 1);
  centerTextAt("HEER", 89, 31, 3);
  display.display();
}

void showPlaying(uint8_t n) {
  screenMode = S_PLAYING;
  if (!oledOk) return;
  display.clearDisplay();
  display.drawRect(0, 0, 128, 4, SSD1306_WHITE);
  display.fillRect(2, 1, 124 * (n + 1) / NOTE_COUNT, 2, SSD1306_WHITE);
  const char* const* ln = lyrics[(n >= 12 && n < 19) ? 1 : 0];
  for (int l = 0; l < 3; l++)
    drawLyricLine(ln[l], 9 + 17 * l, (lyricPos[n] / 2 == l) ? lyricPos[n] % 2 : -1);
  display.display();
}

void showFinished() {
  screenMode = S_DONE;
  doneAt = millis();
  if (!oledOk) return;
  display.clearDisplay();
  centerTextAt("HAPPY", 64, 2, 2);
  centerTextAt("BIRTHDAY", 64, 22, 2);
  centerTextAt("\x03 HEER \x03", 64, 42, 2);
  display.display();
}

// ---------------- song ----------------
void beginNote(uint8_t n) { buzzerOn(melody[n]); soundOn = true; showPlaying(n); }

void startSong() { playing = true; cur = 0; noteStart = millis(); beginNote(0); }

void stopSong() { playing = false; soundOn = false; buzzerOff(); showReady(); }

void updateSong() {
  if (!playing) return;
  unsigned long now = millis(), total = noteMs(cur);
  if (soundOn && now - noteStart >= total * SOUND_PCT / 100) { buzzerOff(); soundOn = false; }
  if (now - noteStart >= total) {
    noteStart += total;
    if (++cur >= NOTE_COUNT) { playing = false; soundOn = false; buzzerOff(); showFinished(); return; }
    beginNote(cur);
  }
}

void updateIdleScreen() {
  if (playing) return;
  unsigned long now = millis();
  if (screenMode == S_DONE && now - doneAt > 6000) { showReady(); return; }
  if (oledOk && screenMode == S_READY && now - lastFrame > 300) {
    lastFrame = now;
    frameCount++;
    showReady();
  }
}

// ---------------- lights ----------------
// RGB 1 on GPIO 4,5,6. RGB 2 (GPIO 1,2,42) is optional. 5 candle LEDs.
// Every LED needs its own 220-330 ohm resistor.
#define RGB_COMMON_ANODE 1    // 1 = long leg to 3V3 (your LED), 0 = long leg to GND
#define USE_RGB2 0            // 1 once the second RGB LED is wired
#define RGB_COUNT (USE_RGB2 ? 2 : 1)
#define CANDLES 5
const uint8_t RGB_PINS[2][3] = { { 4, 5, 6 }, { 1, 2, 42 } };
const uint8_t CANDLE_PINS[CANDLES] = { 15, 16, 17, 18, 10 };

void hsvToRgb(uint16_t h, uint8_t sat, uint8_t val, uint8_t &r, uint8_t &g, uint8_t &b) {
  h %= 360;
  uint16_t rem = (h % 60) * 255 / 60;
  uint8_t p = val * (255 - sat) / 255;
  uint8_t q = val * (255 - sat * rem / 255) / 255;
  uint8_t t = val * (255 - sat * (255 - rem) / 255) / 255;
  switch (h / 60) {
    case 0: r = val; g = t; b = p; break;
    case 1: r = q; g = val; b = p; break;
    case 2: r = p; g = val; b = t; break;
    case 3: r = p; g = q; b = val; break;
    case 4: r = t; g = p; b = val; break;
    default: r = val; g = p; b = q;
  }
}

void setRgb(uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
  if (led >= RGB_COUNT) return;
  uint8_t v[3] = { r, g, b };
  for (uint8_t c = 0; c < 3; c++) {
    uint16_t x = v[c] * v[c] / 255 * lightsPercent / 100;
    ledcWrite(RGB_PINS[led][c], RGB_COMMON_ANODE ? 255 - x : x);
  }
}

void setHue(uint8_t led, uint16_t hue, uint8_t val) {
  uint8_t r, g, b;
  hsvToRgb(hue, 255, val, r, g, b);
  setRgb(led, r, g, b);
}

void setCandles(uint8_t mask) {
  for (uint8_t i = 0; i < CANDLES; i++)
    digitalWrite(CANDLE_PINS[i], ((mask >> i) & 1) && lightsPercent ? HIGH : LOW);
}

void initLights() {
  // LEDC channel 0 = buzzer, 1 stays unused (shares its timer), RGB uses 2-7
  for (uint8_t led = 0; led < RGB_COUNT; led++)
    for (uint8_t c = 0; c < 3; c++)
      ledcAttachChannel(RGB_PINS[led][c], 5000, 8, 2 + led * 3 + c);
  for (uint8_t i = 0; i < CANDLES; i++) pinMode(CANDLE_PINS[i], OUTPUT);
  setCandles(0);
  setRgb(0, 0, 0, 0);
  setRgb(1, 0, 0, 0);
}

// RGB colour follows the lyric word shown on the OLED
void lyricColor(uint8_t n, uint8_t &r, uint8_t &g, uint8_t &b) {
  static const uint8_t LC[6][3] = {
    { 255, 35, 150 },   // HAPPY    pink
    { 45, 90, 255 },    // BIRTHDAY blue
    { 0, 220, 255 },    // TO       cyan
    { 45, 255, 80 },    // YOU      green
    { 255, 35, 35 },    // DEAR     red
    { 255, 40, 180 }    // HEER     magenta
  };
  bool dear = n >= 12 && n < 19;
  uint8_t p = lyricPos[n];
  uint8_t i = p == 0 ? 0 : p == 2 ? 1 : p == 4 ? (dear ? 4 : 2) : (dear ? 5 : 3);
  r = LC[i][0]; g = LC[i][1]; b = LC[i][2];
}

void updateLights() {
  static unsigned long lastUpdate = 0, lastParty = 0;
  static uint8_t partyMask = 0;
  static uint16_t partyHue = 0;
  unsigned long now = millis();
  if (now - lastUpdate < 20) return;
  lastUpdate = now;

  if (screenMode == S_PLAYING && playing) {
    uint8_t r, g, b;
    lyricColor(cur, r, g, b);
    // higher notes are brighter, and every note pulses gently
    unsigned long soundMs = noteMs(cur) * SOUND_PCT / 100, half = soundMs / 2;
    unsigned long el = min(now - noteStart, soundMs);
    long boost = min((melody[cur] - 262) * 55L / (523 - 262), 55L);
    unsigned long pulse = soundOn ? 120 + (el < half ? el * 100 / half : (soundMs - el) * 100 / half) : 80;
    uint16_t v = min(pulse + boost, 255UL);
    setRgb(0, r * v / 255, g * v / 255, b * v / 255);
    setRgb(1, r * v / 255, g * v / 255, b * v / 255);
    setCandles((1 << (cur * CANDLES / NOTE_COUNT + 1)) - 1);   // fills up with the song
  } else if (screenMode == S_DONE) {
    if (now - lastParty >= 150) { lastParty = now; partyMask = random(1, 32); partyHue = random(360); }
    setHue(0, partyHue, 255);
    setHue(1, partyHue + 180, 255);
    setCandles(partyMask);
  } else {
    uint16_t hue = (now / 15) % 360;     // slow rainbow while waiting
    setHue(0, hue, 120);
    setHue(1, hue + 180, 120);
    setCandles(0);
  }
}

// ---------------- web ----------------
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Happy Birthday, Heer!</title>
<style>
body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:radial-gradient(circle at 50% 0,#2a1630,#140f18 60%);color:#fff;font-family:system-ui,Roboto,Arial,sans-serif}
.c{width:min(92vw,420px);padding:30px 22px;border-radius:28px;text-align:center;background:#211a28;box-shadow:0 15px 50px #0009}
#e{font-size:56px;display:inline-block}
#e.on{animation:b 1s ease-in-out infinite}
@keyframes b{50%{transform:translateY(-8px)}}
h1{margin:12px 0 6px;font-size:28px}
p{color:#b9a9c4;margin:0 0 24px}
button{width:100%;border:0;border-radius:18px;padding:18px;margin:7px 0;font-size:18px;font-weight:700;color:#fff}
.p{background:linear-gradient(135deg,#ff7eb3,#ff5e8e)}
.s{background:#3a2f42}
.v{margin-top:20px;text-align:left;color:#d8cce0}
.v input{width:100%;margin-top:10px;accent-color:#ff5e8e}
#st{margin-top:20px;color:#b9a9c4;font-size:14px}
</style></head><body><div class="c">
<div id="e">&#127874;</div>
<h1>Happy Birthday, Heer!</h1>
<p>Tap play to hear your birthday song</p>
<button class="p" onclick="c('/play')">&#9654; PLAY</button>
<button class="s" onclick="c('/stop')">&#9632; STOP</button>
<div class="v">&#128266; Volume: <b id="volumeV">100</b>%<input id="volume" type="range" value="100" oninput="sl(this)"></div>
<div class="v">&#127926; Note balance: <b id="balanceV">75</b>%<input id="balance" type="range" value="75" oninput="sl(this)"></div>
<div class="v">&#128161; Lights: <b id="lightsV">60</b>%<input id="lights" type="range" value="60" oninput="sl(this)"></div>
<div id="st">Ready</div></div>
<script>
var T={};
function $(i){return document.getElementById(i)}
function c(p){return fetch(p,{cache:'no-store'}).then(function(){u()}).catch(function(){})}
function sl(e){var k=e.id;$(k+'V').textContent=e.value;if(T[k])return;T[k]=setTimeout(function(){T[k]=0;c('/'+k+'?v='+e.value)},120)}
function u(f){fetch('/status',{cache:'no-store'}).then(function(r){return r.json()}).then(function(s){
$('e').className=s.playing?'on':'';
$('st').innerHTML=s.playing?'&#127925; Playing Happy Birthday for Heer&hellip;':'Ready &#127881;';
if(f)['volume','balance','lights'].forEach(function(k){$(k).value=s[k];$(k+'V').textContent=s[k]})
}).catch(function(){})}
u(1);setInterval(function(){u()},1000);
</script></body></html>
)rawliteral";

void setLevel(uint8_t &v) {
  if (server.hasArg("v")) v = constrain(server.arg("v").toInt(), 0, 100);
  if (playing && soundOn) buzzerOn(melody[cur]);   // apply to the sounding note
  server.send(200, "text/plain", "ok");
}

void handleStatus() {
  char b[96];
  snprintf(b, sizeof b, "{\"playing\":%d,\"volume\":%u,\"balance\":%u,\"lights\":%u}",
           playing, volumePercent, balancePercent, lightsPercent);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", b);
}

// ---------------- setup / loop ----------------
void setup() {
  Serial0.begin(115200);
  ledcAttachChannel(BUZZER_PIN, 2000, BUZZER_BITS, 0);
  buzzerOff();
  initLights();

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (oledOk) {
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    showSplash();
  } else {
    Serial0.println("OLED not found - continuing without display");
  }

  ESP32Base.begin();

  server.on("/", []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html; charset=utf-8", PAGE);
  });
  server.on("/play", []() { startSong(); server.send(200, "text/plain", "ok"); });
  server.on("/stop", []() { stopSong(); server.send(200, "text/plain", "ok"); });
  server.on("/volume", []() { setLevel(volumePercent); });
  server.on("/balance", []() { setLevel(balancePercent); });
  server.on("/lights", []() { setLevel(lightsPercent); });
  server.on("/status", handleStatus);
  server.begin();

  showReady();
}

void loop() {
  ESP32Base.loop();
  server.handleClient();
  updateSong();
  updateIdleScreen();
  updateLights();
  delay(1);
}
