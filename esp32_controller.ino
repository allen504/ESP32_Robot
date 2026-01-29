#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* WIFI_SSID = "network";
const char* WIFI_PASS = "password";

const char* ROBOT_IP  = "192.168.10.3";
const uint16_t ROBOT_HTTP_PORT = 80;

const char* API_TOKEN = "letmein"; // must match robot's API_TOKEN

static const int ENC_A = 22; // CLK
static const int ENC_B = 23; // DT

static const unsigned long STEER_HOLD_MS = 150;
static const unsigned long SEND_MIN_GAP_MS = 80;

volatile int lastA = HIGH;
volatile long tickCount = 0;

enum Cmd { NONE, LEFT, RIGHT, STOP };
Cmd lastSent = NONE;
unsigned long lastTickMs = 0;
unsigned long lastSendMs = 0;

void IRAM_ATTR isrEncA() {
  int a = digitalRead(ENC_A);
  int b = digitalRead(ENC_B);
  if (a != lastA) {
    lastA = a;
    if (a == LOW) {
      if (b == HIGH) tickCount++;
      else tickCount--;
    }
  }
}

String makeCmdUrl(const char* m) {
  String url = "http://";
  url += ROBOT_IP;
  if (ROBOT_HTTP_PORT != 80) {
    url += ":";
    url += ROBOT_HTTP_PORT;
  }
  url += "/cmd?m=";
  url += m;

  // token param name is "t" in your robot code
  if (API_TOKEN && strlen(API_TOKEN) > 0) {
    url += "&t=";
    url += API_TOKEN;
  }
  return url;
}

void sendCmd(Cmd cmd) {
  unsigned long now = millis();
  if (cmd == lastSent) return;
  if (now - lastSendMs < SEND_MIN_GAP_MS) return;

  const char* m = nullptr;
  if (cmd == LEFT)  m = "left";
  if (cmd == RIGHT) m = "right";
  if (cmd == STOP)  m = "stop";
  if (!m) return;

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = makeCmdUrl(m);
  http.begin(url);
  int code = http.GET();
  String body = http.getString();
  http.end();

  Serial.print("GET ");
  Serial.print(url);
  Serial.print(" -> ");
  Serial.print(code);
  Serial.print(" | ");
  Serial.println(body);

  lastSent = cmd;
  lastSendMs = now;
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
  sendCmd(STOP);
}

void loop() {
  static long lastTicks = 0;

  long ticksSnapshot;
  noInterrupts();
  ticksSnapshot = tickCount;
  interrupts();

  if (ticksSnapshot != lastTicks) {
    long delta = ticksSnapshot - lastTicks;
    lastTicks = ticksSnapshot;

   if (delta > 0) sendCmd(LEFT);
else sendCmd(RIGHT);


    lastTickMs = millis();
  }

  if (lastTickMs != 0 && (millis() - lastTickMs) > STEER_HOLD_MS) {
    sendCmd(STOP);
    lastTickMs = 0;
  }
}
