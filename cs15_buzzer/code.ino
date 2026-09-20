#include <Arduino.h>
#include <WebServer.h>
#include <ESP32Base.h>

// =========================
// PINS
// =========================
#define BUZZER_PIN 4
#define RED_PIN 16
#define GREEN_PIN 17

// =========================
// WEB SERVER
// =========================
WebServer server(80);

// =========================
// BOMB SETTINGS
// =========================
const unsigned long BOMB_TIME = 9000; // 9 seconds

bool bombRunning = false;
bool buzzerState = false;

unsigned long bombStart = 0;
unsigned long lastToggle = 0;

// Starting beep delay controlled by slider.
unsigned long startDelay = 900;


// =========================
// LIGHT CONTROL
// =========================
void setLights(bool red, bool green)
{
  digitalWrite(RED_PIN, red ? HIGH : LOW);
  digitalWrite(GREEN_PIN, green ? HIGH : LOW);
}


void updateLights(unsigned long elapsed)
{
  // Explosion: solid red.
  if (elapsed >= BOMB_TIME)
  {
    setLights(true, false);
    return;
  }

  // 9, 8, 7 seconds:
  // steady green.
  if (elapsed < 3000)
  {
    setLights(false, true);
    return;
  }

  // 6, 5, 4 seconds:
  // alternating red and green.
  if (elapsed < 6000)
  {
    bool phase = ((elapsed / 400) % 2) == 0;
    setLights(phase, !phase);
    return;
  }

  // 3, 2, 1 seconds:
  // faster alternating red and green.
  bool phase = ((elapsed / 150) % 2) == 0;
  setLights(phase, !phase);
}


// =========================
// BEEP SPEED
// =========================
unsigned long currentDelay(unsigned long elapsed)
{
  if (elapsed >= BOMB_TIME)
    return 0;

  float progress =
    (float)elapsed / (float)BOMB_TIME;

  float interval =
    (float)startDelay * (1.0f - progress * 0.92f);

  if (interval < 60.0f)
    interval = 60.0f;

  return (unsigned long)interval;
}


// =========================
// WEB PAGE
// =========================
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport"
      content="width=device-width,initial-scale=1">

<title>CS Bomb Pattern 1</title>

<style>

body{
  margin:0;
  background:#111;
  color:#fff;
  font-family:Arial,sans-serif;
  text-align:center;
}

.card{
  max-width:430px;
  margin:30px auto;
  padding:25px 22px;
  background:#222;
  border-radius:24px;
}

h1{
  font-size:29px;
  margin:5px 0 8px;
}

.pattern{
  color:#aaa;
  font-size:17px;
  margin-bottom:20px;
}

.count{
  font-size:72px;
  font-weight:bold;
  margin:10px;
}

.value{
  font-size:22px;
  margin:15px;
}

input[type=range]{
  width:100%;
  height:12px;
  accent-color:#e53935;
}

button{
  width:100%;
  padding:17px;
  margin-top:28px;
  border:0;
  border-radius:14px;
  font-size:20px;
  font-weight:bold;
  color:white;
  background:#e53935;
}

button.on{
  background:#20c76a;
}

.state{
  margin-top:18px;
  font-size:18px;
}

.small{
  color:#888;
  font-size:13px;
  margin-top:20px;
}

</style>
</head>

<body>

<div class="card">

<h1>💣 BOMB PATTERN 1</h1>

<div class="pattern">
9 second countdown
</div>

<div class="count" id="count">
9
</div>

<div class="value">
Starting delay:
<span id="value">900</span> ms
</div>

<input
 id="slider"
 type="range"
 min="200"
 max="1500"
 step="10"
 value="900"
 oninput="changeDelay(this.value)">

<button
 id="button"
 onclick="toggleBomb()">
START
</button>

<div class="state" id="state">
Ready
</div>

<div class="small">
At 0: 💥 solid red + continuous buzzer until STOP
</div>

</div>


<script>

let running = false;


function changeDelay(v)
{
  document.getElementById("value").textContent = v;

  fetch("/delay?v=" + v);
}


function toggleBomb()
{
  running = !running;

  fetch(running ? "/start" : "/stop")
    .then(r => r.text())
    .then(updateButton);
}


function updateButton()
{
  let b =
    document.getElementById("button");

  let s =
    document.getElementById("state");

  if(running)
  {
    b.textContent = "STOP";
    b.classList.add("on");

    s.textContent =
      "Bomb active...";
  }
  else
  {
    b.textContent = "START";
    b.classList.remove("on");

    s.textContent =
      "Ready";

    document.getElementById("count")
      .textContent = "9";
  }
}


// Update countdown display.
setInterval(() =>
{
  fetch("/status")
    .then(r => r.json())
    .then(data =>
    {
      running = data.running;

      let b =
        document.getElementById("button");

      let s =
        document.getElementById("state");

      if(data.running)
      {
        b.textContent = "STOP";
        b.classList.add("on");

        if(data.count == 0)
        {
          document.getElementById("count")
            .textContent = "💥";

          s.textContent =
            "EXPLODED — continuous beep";
        }
        else
        {
          document.getElementById("count")
            .textContent = data.count;

          s.textContent =
            "Bomb active...";
        }
      }
      else
      {
        b.textContent = "START";
        b.classList.remove("on");

        document.getElementById("count")
          .textContent = "9";

        s.textContent =
          "Ready";
      }
    });

}, 100);

</script>

</body>
</html>
)rawliteral";


// =========================
// WEB HANDLERS
// =========================
void handleRoot()
{
  server.send(
    200,
    "text/html",
    PAGE
  );
}


void handleDelay()
{
  if (server.hasArg("v"))
  {
    startDelay =
      server.arg("v").toInt();

    if (startDelay < 200)
      startDelay = 200;

    if (startDelay > 1500)
      startDelay = 1500;
  }

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


void handleStart()
{
  bombRunning = true;
  buzzerState = false;

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  bombStart = millis();
  lastToggle = millis();

  setLights(false, true);

  server.send(
    200,
    "text/plain",
    "STARTED"
  );
}


void handleStop()
{
  bombRunning = false;
  buzzerState = false;

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  setLights(false, false);

  server.send(
    200,
    "text/plain",
    "STOPPED"
  );
}


void handleStatus()
{
  unsigned long elapsed = 0;

  if (bombRunning)
    elapsed = millis() - bombStart;

  unsigned long remaining =
    (elapsed >= BOMB_TIME)
      ? 0
      : (BOMB_TIME - elapsed);

  unsigned long count =
    (remaining + 999) / 1000;

  String json = "{";

  json += "\"running\":";
  json += bombRunning
    ? "true"
    : "false";

  json += ",\"count\":";
  json += String(count);

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}


// =========================
// SETUP
// =========================
void setup()
{
  Serial0.begin(115200);

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  pinMode(
    RED_PIN,
    OUTPUT
  );

  pinMode(
    GREEN_PIN,
    OUTPUT
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  setLights(false, false);


  // ESP32Base handles:
  // Wi-Fi + OTA.
  ESP32Base.begin();


  // Our web server.
  server.on(
    "/",
    handleRoot
  );

  server.on(
    "/delay",
    handleDelay
  );

  server.on(
    "/start",
    handleStart
  );

  server.on(
    "/stop",
    handleStop
  );

  server.on(
    "/status",
    handleStatus
  );

  server.begin();

  Serial0.println(
    "CS Bomb Pattern 1 web server started"
  );
}


// =========================
// LOOP
// =========================
void loop()
{
  // Keep ESP32Base Wi-Fi + OTA alive.
  ESP32Base.loop();

  // Handle web requests.
  server.handleClient();


  if (!bombRunning)
    return;


  unsigned long now =
    millis();

  unsigned long elapsed =
    now - bombStart;


  // Update red/green light effect.
  updateLights(elapsed);


  // =========================
  // EXPLOSION
  // =========================
  // At 0:
  // - solid red
  // - buzzer continuously ON
  // - stays this way until STOP
  if (elapsed >= BOMB_TIME)
  {
    buzzerState = true;

    digitalWrite(
      BUZZER_PIN,
      HIGH
    );

    return;
  }


  // =========================
  // COUNTDOWN BEEP
  // =========================
  unsigned long interval =
    currentDelay(elapsed);

  if (now - lastToggle >= interval)
  {
    lastToggle = now;

    buzzerState =
      !buzzerState;

    digitalWrite(
      BUZZER_PIN,
      buzzerState
        ? HIGH
        : LOW
    );
  }
}
