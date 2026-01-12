#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "network";
const char* password = "password";

// Optional: super simple “auth” token so random devices on your LAN can’t drive it.
// Set to "" to disable.
const char* API_TOKEN = "letmein";

WebServer server(80);

int motor1pin1 = 26;
int motor1pin2 = 27;

int motor2pin1 = 12;
int motor2pin2 = 13;

// ----- YOUR motor functions (replace with yours) -----
void stop(){
  digitalWrite(motor1pin1, LOW);
  digitalWrite(motor1pin2, LOW);
  digitalWrite(motor2pin1, LOW);
  digitalWrite(motor2pin2, LOW);
}

void backward(){
  digitalWrite(motor1pin1, HIGH);
  digitalWrite(motor1pin2, LOW);
  digitalWrite(motor2pin1, HIGH);
  digitalWrite(motor2pin2, LOW);
}

void forward(){
  digitalWrite(motor1pin1, LOW);
  digitalWrite(motor1pin2, HIGH);
  digitalWrite(motor2pin1, LOW);
  digitalWrite(motor2pin2, HIGH);
}

void right(){
  digitalWrite(motor1pin1, LOW);
  digitalWrite(motor1pin2, HIGH);
  digitalWrite(motor2pin1, HIGH);
  digitalWrite(motor2pin2, LOW);
}

void left(){
  digitalWrite(motor1pin1, HIGH);
  digitalWrite(motor1pin2, LOW);
  digitalWrite(motor2pin1, LOW);
  digitalWrite(motor2pin2, HIGH);
}

// ----------------------------------------------------

bool checkToken() {
  if (String(API_TOKEN).length() == 0) return true;
  if (!server.hasArg("t")) return false;
  return server.arg("t") == API_TOKEN;
}

void handleRoot() {
  // Simple controller page (served by ESP32)
  // Uses press-and-hold: mousedown/touchstart to move, mouseup/touchend to stop
  String html = R"HTML(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 Motor Control</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 20px; }
    .grid { display: grid; grid-template-columns: 100px 100px 100px; gap: 12px; width: max-content; }
    button { font-size: 18px; padding: 18px; border-radius: 12px; border: 1px solid #ccc; }
    .wide { grid-column: span 3; }
    .row2 { grid-row: span 1; }
    .hint { margin-top: 14px; color: #555; }
    input { padding: 8px; font-size: 16px; width: 140px; }
  </style>
</head>
<body>
  <h2>ESP32 Motor Control</h2>

  <div>
    <label>Token (optional): </label>
    <input id="token" placeholder="token" />
  </div>

  <p class="hint">Press & hold buttons (or use WASD keys). Release to stop.</p>

  <div class="grid">
    <div></div>
    <button id="btnF">^</button>
    <div></div>

    <button id="btnL"><</button>
    <button id="btnS">STOP</button>
    <button id="btnR">></button>

    <div></div>
    <button id="btnB">v</button>
    <div></div>

    <button class="wide" id="btnPing">Ping</button>
  </div>

  <pre id="log"></pre>

<script>
  const log = (msg) => document.getElementById('log').textContent = msg;

  function tokenQS() {
    const t = document.getElementById('token').value.trim();
    return t ? `&t=${encodeURIComponent(t)}` : '';
  }

  async function sendCmd(m) {
    try {
      const r = await fetch(`/cmd?m=${encodeURIComponent(m)}${tokenQS()}`);
      const txt = await r.text();
      log(txt);
    } catch (e) {
      log('Error: ' + e);
    }
  }

  function bindHold(btnId, moveCmd) {
    const btn = document.getElementById(btnId);

    const start = (e) => { e.preventDefault(); sendCmd(moveCmd); };
    const end   = (e) => { e.preventDefault(); sendCmd('stop'); };

    btn.addEventListener('mousedown', start);
    btn.addEventListener('touchstart', start, {passive:false});

    btn.addEventListener('mouseup', end);
    btn.addEventListener('mouseleave', end);
    btn.addEventListener('touchend', end, {passive:false});
    btn.addEventListener('touchcancel', end, {passive:false});
  }

  bindHold('btnF', 'fwd');
  bindHold('btnB', 'back');
  bindHold('btnL', 'left');
  bindHold('btnR', 'right');

  document.getElementById('btnS').onclick = () => sendCmd('stop');
  document.getElementById('btnPing').onclick = () => sendCmd('ping');

  // Keyboard controls: WASD, space to stop
  document.addEventListener('keydown', (e) => {
    if (e.repeat) return;
    if (e.key === 'w') sendCmd('fwd');
    if (e.key === 's') sendCmd('back');
    if (e.key === 'a') sendCmd('left');
    if (e.key === 'd') sendCmd('right');
    if (e.key === ' ') sendCmd('stop');
  });

  document.addEventListener('keyup', (e) => {
    if (['w','a','s','d'].includes(e.key)) sendCmd('stop');
  });

  log('Ready.');
</script>
</body>
</html>
)HTML";

  server.send(200, "text/html", html);
}

void handleCmd() {
  if (!checkToken()) {
    server.send(403, "text/plain", "Forbidden: missing/invalid token");
    return;
  }

  String m = server.hasArg("m") ? server.arg("m") : "";
  m.toLowerCase();

  if (m == "fwd") {
    forward();
    server.send(200, "text/plain", "OK: forward");
  } else if (m == "back") {
    backward();
    server.send(200, "text/plain", "OK: backward");
  } else if (m == "left") {
    left();
    server.send(200, "text/plain", "OK: left");
  } else if (m == "right") {
    right();
    server.send(200, "text/plain", "OK: right");
  } else if (m == "stop") {
    stop();
    server.send(200, "text/plain", "OK: stop");
  } else if (m == "ping") {
    server.send(200, "text/plain", "pong");
  } else {
    server.send(400, "text/plain", "Bad request. Use /cmd?m=fwd|back|left|right|stop");
  }
}

void setup() {
  Serial.begin(115200);

  // Your motor pin setup goes here...
  // initMotors();
  pinMode(motor1pin1, OUTPUT);
  pinMode(motor1pin2, OUTPUT);
  pinMode(motor2pin1,   OUTPUT);
  pinMode(motor2pin2, OUTPUT);


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("Open: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.begin();
}

void loop() {
  server.handleClient();
}
