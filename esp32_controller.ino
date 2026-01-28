#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

// ----------------------- USER SETTINGS -----------------------
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Put the robot ESP32's IP address here (check Serial output from the robot at boot)
const char* ROBOT_IP  = "192.168.1.123";

// Most ESP32 web-drive sketches use WebSocket port 81 and path "/"
const uint16_t ROBOT_WS_PORT = 81;
const char* ROBOT_WS_PATH = "/";

// Command mapping (common in ESP32 web-drive examples)
const int CMD_STOP  = 0;
const int CMD_LEFT  = 4;
const int CMD_RIGHT = 8;
// ------------------------------------------------------------

// Rotary encoder pins (change if you want)
static const int ENC_A = 22; // CLK
static const int ENC_B = 23; // DT

// Behavior tuning
static const unsigned long STEER_HOLD_MS = 150;  // after last encoder tick, send STOP
static const unsigned long SEND_MIN_GAP_MS = 25; // don't spam the socket

WebSocketsClient webSocket;

// Encoder state
volatile int lastA = HIGH;
volatile long tickCount = 0;

// Command state
int lastSentCmd = -1;
unsigned long lastTickMs = 0;
unsigned long lastSendMs = 0;

void IRAM_ATTR isrEncA() {
  int a = digitalRead(ENC_A);
  int b = digitalRead(ENC_B);

  // Trigger on changes; determine direction from B at the moment A changes
  if (a != lastA) {
    lastA = a;
    if (a == LOW) { // count on one edge only to reduce bounce sensitivity
      if (b == HIGH) tickCount++;   // one direction
      else tickCount--;             // other direction
    }
  }
}

void sendCmd(int cmd) {
  unsigned long now = millis();
  if (cmd == lastSentCmd) return;
  if (now - lastSendMs < SEND_MIN_GAP_MS) return;

  char msg[8];
  snprintf(msg, sizeof(msg), "%d", cmd);
  webSocket.sendTXT(msg);

  lastSentCmd = cmd;
  lastSendMs = now;

  Serial.print("Sent cmd: ");
  Serial.println(cmd);
}


void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] Connected to: %s\n", payload);
      // Immediately stop on connect (safe default)
      sendCmd(CMD_STOP);
      break;
    case WStype_TEXT:
      // Optional: robot may send text back
      Serial.printf("[WS] RX: %.*s\n", (int)length, (const char*)payload);
      break;
    default:
      break;
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Controller IP: ");
  Serial.println(WiFi.localIP());
}

void setupWebSocket() {
  // server address, port, URL
  webSocket.begin(ROBOT_IP, ROBOT_WS_PORT, ROBOT_WS_PATH); // pattern from library examples :contentReference[oaicite:4]{index=4}
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(2000);
}

void setupEncoder() {
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  lastA = digitalRead(ENC_A);
  attachInterrupt(digitalPinToInterrupt(ENC_A), isrEncA, CHANGE);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupEncoder();
  connectWiFi();
  setupWebSocket();
}

void loop() {
  webSocket.loop();

  // Read accumulated ticks safely
  static long lastTicks = 0;
  long ticksSnapshot;
  noInterrupts();
  ticksSnapshot = tickCount;
  interrupts();

  if (ticksSnapshot != lastTicks) {
    long delta = ticksSnapshot - lastTicks;
    lastTicks = ticksSnapshot;

    // If delta positive -> RIGHT, negative -> LEFT
    if (delta > 0) sendCmd(CMD_RIGHT);
    else sendCmd(CMD_LEFT);

    lastTickMs = millis();
  }

  // If the knob stopped moving, send STOP after a short hold
  if (lastTickMs != 0 && (millis() - lastTickMs) > STEER_HOLD_MS) {
    sendCmd(CMD_STOP);
    lastTickMs = 0;
  }
}
