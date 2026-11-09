// ESP32 DevKit V1 — BLE HID Keyboard + Mouse (Simone raw build, v3)
// NO NimBLEHIDDevice — manual GATT service creation to bypass the bug.
// Control: WiFi HTTP + SERIAL. Absolute pointer emulation.

#include <WiFi.h>
#include <WebServer.h>
#include <NimBLEDevice.h>

// ===== CONFIG =====
const char* WIFI_SSID = "HONOR X7C";
const char* WIFI_PASS = "1234567890";

// ===== HID report IDs =====
#define RID_KEYBOARD 1
#define RID_MOUSE    2

// Standard BLE UUIDs (16-bit)
#define UUID_HID_SERVICE          0x1812
#define UUID_HID_INFORMATION      0x2A4C
#define UUID_REPORT_MAP           0x2A4B
#define UUID_REPORT               0x2A4D
#define UUID_PROTOCOL_MODE        0x2A4E
#define UUID_HID_CONTROL_POINT    0x2A4C  // same UUID as HID Info — distinguished by handle
#define UUID_BATTERY_SERVICE      0x180F
#define UUID_BATTERY_LEVEL        0x2A19
#define UUID_CCCD                 0x2902
#define UUID_REPORT_REFERENCE     0x2908

// HID report map: keyboard (id1) + mouse (id2)
static const uint8_t kReportMap[] = {
  0x05, 0x01,        // Generic Desktop
  0x09, 0x06,        // Keyboard
  0xA1, 0x01,        // Collection: Application
  0x85, RID_KEYBOARD,//   Report ID (1)
  0x05, 0x07,        //   Keyboard/Keypad
  0x19, 0xE0, 0x29, 0xE7, //  modifiers 0xE0-0xE7
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08, 0x81, 0x02, //  8x1 modifier bits
  0x75, 0x08, 0x95, 0x01, 0x81, 0x01, //  1 byte padding
  0x75, 0x08, 0x95, 0x06,             //  6 key bytes
  0x15, 0x00, 0x25, 0xFF, 0x05, 0x07, 0x19, 0x00, 0x29, 0xFF,
  0x81, 0x00,
  0xC0,              // End Collection
  0x05, 0x01,        // Generic Desktop
  0x09, 0x02,        // Mouse
  0xA1, 0x01,        // Collection: Application
  0x85, RID_MOUSE,   //   Report ID (2)
  0x09, 0x01, 0xA1, 0x00, //  Pointer
  0x05, 0x09,        //   Button
  0x19, 0x01, 0x29, 0x03, //  3 buttons
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x03, 0x81, 0x02,
  0x75, 0x05, 0x95, 0x01, 0x81, 0x01, //  padding
  0x05, 0x01,        //   Generic Desktop
  0x09, 0x30, 0x09, 0x31, 0x09, 0x38, //  X, Y, Wheel
  0x15, 0x81, 0x25, 0x7F,
  0x75, 0x08, 0x95, 0x03, 0x81, 0x06, //  signed 8-bit x3
  0xC0, 0xC0,        // End Pointer / End Collection
};

// Report packets (must start with report ID byte)
struct KbPkt { uint8_t id, mod, pad, keys[6]; } kb = {RID_KEYBOARD};
struct MsPkt { uint8_t id, btn; int8_t dx, dy, wheel; } ms = {RID_MOUSE};

// BLE handles
NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* chKeyboard = nullptr;
NimBLECharacteristic* chMouse = nullptr;
bool bleReady = false;
bool hostConnected = false;

// pointer state
int screenW = 1080, screenH = 2400;
int curX = 0, curY = 0;
bool homed = false;

void sendKeyboard() {
  if (hostConnected && chKeyboard) { chKeyboard->setValue((uint8_t*)&kb, sizeof(kb)); chKeyboard->notify(); }
}
void sendMouse() {
  if (hostConnected && chMouse) { chMouse->setValue((uint8_t*)&ms, sizeof(ms)); chMouse->notify(); }
}

// ===== Server callbacks =====
class HidServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    hostConnected = true;
    Serial.println("BLE: host connected");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    hostConnected = false;
    homed = false;
    Serial.printf("BLE: disconnected reason=%d, re-adv\n", reason);
    NimBLEDevice::startAdvertising();
  }
};

void bleInit() {
  NimBLEDevice::init("ESP32-KBM");
  delay(500);  // Let serial settle
  Serial.println("[1] NimBLE init done");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  
  // Full bonding configuration for persistent HID connection
  NimBLEDevice::setSecurityAuth(true, true, true);  // Bonding enabled: MITM, SC, Bonding
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);  // Keyboard only IO
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);  // Keys we provide
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);  // Keys we accept
  
  Serial.println("[2] Security set");
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new HidServerCallbacks());
  
  // Set preferred connection parameters for stable HID
  bleServer->setConnParams(6, 12, 0, 300);  // min=6*1.25=7.5ms, max=15ms, latency=0, timeout=3s
  Serial.println("[3] Server created");

  // === Build HID Service (0x1812) manually ===
  NimBLEService* hidSvc = bleServer->createService((uint16_t)UUID_HID_SERVICE);
  Serial.printf("[4] HID service created, uuid=%s\n", hidSvc->getUUID().toString().c_str());

  // HID Information characteristic (0x2A4C) — read-only, 2 bytes
  NimBLECharacteristic* hidInfo = hidSvc->createCharacteristic(
      (uint16_t)UUID_HID_INFORMATION, NIMBLE_PROPERTY::READ);
  uint8_t hidInfoVal[] = {0x11, 0x01};  // HID 1.1, country 0
  hidInfo->setValue(hidInfoVal, 2);

  // Report Map characteristic (0x2A4B) — read-only
  NimBLECharacteristic* reportMap = hidSvc->createCharacteristic(
      (uint16_t)UUID_REPORT_MAP, NIMBLE_PROPERTY::READ);
  reportMap->setValue((uint8_t*)kReportMap, sizeof(kReportMap));

  // Protocol Mode characteristic (0x2A4E) — read/write
  NimBLECharacteristic* protoMode = hidSvc->createCharacteristic(
      (uint16_t)UUID_PROTOCOL_MODE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);
  uint8_t protoVal[] = {0x01};  // Report protocol mode
  protoMode->setValue(protoVal, 1);

  // Input Report — Keyboard (0x2A4D) — read/notify + Report Reference
  chKeyboard = hidSvc->createCharacteristic(
      (uint16_t)UUID_REPORT, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  chKeyboard->setValue((uint8_t*)&kb, sizeof(kb));
  NimBLEDescriptor* kbRef = chKeyboard->createDescriptor(
      (uint16_t)UUID_REPORT_REFERENCE, NIMBLE_PROPERTY::READ);
  uint8_t kbRefVal[] = {RID_KEYBOARD, 0x01};  // report ID 1, type Input
  kbRef->setValue(kbRefVal, 2);

  // Input Report — Mouse (0x2A4D) — read/notify + Report Reference
  chMouse = hidSvc->createCharacteristic(
      (uint16_t)UUID_REPORT, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  chMouse->setValue((uint8_t*)&ms, sizeof(ms));
  NimBLEDescriptor* msRef = chMouse->createDescriptor(
      (uint16_t)UUID_REPORT_REFERENCE, NIMBLE_PROPERTY::READ);
  uint8_t msRefVal[] = {RID_MOUSE, 0x01};  // report ID 2, type Input
  msRef->setValue(msRefVal, 2);

  // === Battery Service (0x180F) ===
  NimBLEService* batSvc = bleServer->createService((uint16_t)UUID_BATTERY_SERVICE);
  NimBLECharacteristic* batLevel = batSvc->createCharacteristic(
      (uint16_t)UUID_BATTERY_LEVEL, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  uint8_t batVal[] = {100};
  batLevel->setValue(batVal, 1);

  // === Start everything ===
  Serial.println("[10] About to start server...");
  bleServer->start();
  Serial.println("[11] server->start() = OK");


  // Check if NimBLEServer object also knows about it
  NimBLEService* checkSvc = bleServer->getServiceByUUID(NimBLEUUID((uint16_t)UUID_HID_SERVICE));
  Serial.printf("[11c] getServiceByUUID(0x1812) = %s\n", checkSvc ? "FOUND" : "NULL");

  NimBLEAdvertising* adv = bleServer->getAdvertising();
  adv->setAppearance(0x03C1);  // HID Keyboard
  adv->addServiceUUID((uint16_t)UUID_HID_SERVICE);
  adv->start();
  bleReady = true;
  Serial.println("[12] BLE HID v3 up — raw GATT, no NimBLEHIDDevice");
}

// ===== ABSOLUTE POINTER =====
void homePointer() {
  for (int i = 0; i < 10; i++) {
    ms.dx = -127; ms.dy = -127; sendMouse(); delay(6);
    ms.dx = 0;    ms.dy = 0;    sendMouse(); delay(4);
  }
  ms.btn = 0;
  curX = 0; curY = 0; homed = true;
}

void moveDelta(int dx, int dy) {
  while (dx != 0 || dy != 0) {
    int8_t sx = dx > 127 ? 127 : (dx < -127 ? -127 : dx);
    int8_t sy = dy > 127 ? 127 : (dy < -127 ? -127 : dy);
    ms.dx = sx; ms.dy = sy; sendMouse(); delay(8);
    ms.dx = 0;  ms.dy = 0;  sendMouse(); delay(8);
    dx -= sx; dy -= sy;
  }
}

void moveTo(int ax, int ay) {
  if (!homed) homePointer();
  if (ax < 0) ax = 0; if (ax >= screenW) ax = screenW - 1;
  if (ay < 0) ay = 0; if (ay >= screenH) ay = screenH - 1;
  moveDelta(ax - curX, ay - curY);
  curX = ax; curY = ay;
  delay(10);
}

void mouseBtn(uint8_t b, bool down) {
  if (down) ms.btn |= b; else ms.btn &= ~b;
  sendMouse();
}
void mouseClick(uint8_t b) { mouseBtn(b, true); delay(40); mouseBtn(b, false); delay(20); }

// ===== KEYBOARD =====
static const uint8_t asciimap[128] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2A,0x2B,0x28,0x00,0x00,0x2A,0x2A,0x2A,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x29,0x2A,0x2B,0x2C,
  0x9E,0x1E,0x34,0x20,0x21,0x22,0x23,0x30,0x35,0x36,0x37,0x38,0x39,0x2A,0x2C,0x2D,
  0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x2E,0x2F,0x30,0x31,0x32,0x33,
  0x34,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,
  0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x2F,0x31,0x30,0x20,0x23,
  0x2E,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,
  0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x30,0x37,0x2E,0x1E,0x00
};

void kbClear() { memset(kb.keys, 0, 6); kb.mod = 0; sendKeyboard(); }
void kbHold(uint8_t code, bool shift) {
  kbClear();
  if (shift) kb.mod = 0x02;
  kb.keys[0] = code;
  sendKeyboard(); delay(25);
}
void kbTap(uint8_t code, bool shift) { kbHold(code, shift); delay(30); kbClear(); delay(15); }

void typeChar(char c) {
  bool shift = isupper(c) || strchr("!@#$%^&*()_+{}:\"<>?|~", c);
  uint8_t code = asciimap[(uint8_t)c & 0x7F];
  if (c == ' ') code = 0x2C;
  if (code) { kbTap(code, shift); }
}
void typeStr(const String& s) {
  for (unsigned i = 0; i < s.length(); i++) typeChar(s[i]);
}
void tapKey(const String& k) {
  if (k == "enter") kbTap(0x28, false);
  else if (k == "tab") kbTap(0x2B, false);
  else if (k == "backspace") kbTap(0x2A, false);
  else if (k == "escape") kbTap(0x29, false);
  else if (k == "delete") kbTap(0x4C, false);
  else if (k == "up") kbTap(0x52, false);
  else if (k == "down") kbTap(0x51, false);
  else if (k == "left") kbTap(0x50, false);
  else if (k == "right") kbTap(0x4F, false);
  else if (k == "space") kbTap(0x2C, false);
  else if (k == "home") kbTap(0x4A, false);
  else if (k == "end") kbTap(0x4D, false);
  else if (k == "pageup") kbTap(0x4B, false);
  else if (k == "pagedown") kbTap(0x4E, false);
  else if (k.length() == 1) typeChar(k[0]);
}

// ===== HTTP =====
WebServer httpServer(80);
void okJson(const String& b) { httpServer.send(200, "application/json", b); }

void hTap() {
  moveTo(httpServer.arg("x").toInt(), httpServer.arg("y").toInt());
  mouseClick(1);
  okJson("{\"ok\":true,\"action\":\"tap\"}");
}
void hDoubleTap() {
  moveTo(httpServer.arg("x").toInt(), httpServer.arg("y").toInt());
  mouseClick(1); delay(80); mouseClick(1);
  okJson("{\"ok\":true,\"action\":\"double_tap\"}");
}
void hLongPress() {
  moveTo(httpServer.arg("x").toInt(), httpServer.arg("y").toInt());
  int d = httpServer.arg("duration").toInt(); if (d <= 0) d = 1000;
  mouseBtn(1, true); delay(d); mouseBtn(1, false);
  okJson("{\"ok\":true,\"action\":\"long_press\"}");
}
void doSwipe(int x1, int y1, int x2, int y2, int steps, int del) {
  if (steps <= 0) steps = 20; if (del <= 0) del = 10;
  moveTo(x1, y1);
  mouseBtn(1, true);
  for (int i = 1; i <= steps; i++) {
    int cx = x1 + (x2 - x1) * i / steps, cy = y1 + (y2 - y1) * i / steps;
    moveDelta(cx - curX, cy - curY); curX = cx; curY = cy;
    delay(del);
  }
  mouseBtn(1, false); delay(40);
}
void hSwipe() {
  doSwipe(httpServer.arg("x1").toInt(), httpServer.arg("y1").toInt(),
          httpServer.arg("x2").toInt(), httpServer.arg("y2").toInt(),
          httpServer.arg("steps").toInt(), httpServer.arg("delay").toInt());
  okJson("{\"ok\":true,\"action\":\"swipe\"}");
}
void hType() { typeStr(httpServer.arg("text")); okJson("{\"ok\":true,\"action\":\"type\"}"); }
void hKey()  { tapKey(httpServer.arg("key")); okJson("{\"ok\":true,\"action\":\"key\"}"); }
void hMove() {
  int dx = httpServer.arg("dx").toInt(), dy = httpServer.arg("dy").toInt();
  moveDelta(dx, dy); curX += dx; curY += dy;
  okJson("{\"ok\":true,\"action\":\"move\"}");
}
void hScroll() {
  int amt = httpServer.arg("amount").toInt();
  while (amt != 0) {
    int8_t s = amt > 127 ? 127 : (amt < -127 ? -127 : amt);
    ms.wheel = s; sendMouse(); delay(8); ms.wheel = 0; sendMouse(); delay(8);
    amt -= s;
  }
  okJson("{\"ok\":true,\"action\":\"scroll\"}");
}
void hScreen() {
  if (httpServer.hasArg("width")) screenW = httpServer.arg("width").toInt();
  if (httpServer.hasArg("height")) screenH = httpServer.arg("height").toInt();
  homed = false;
  okJson("{\"ok\":true,\"screen\":\"" + String(screenW) + "x" + String(screenH) + "\"}");
}
void hStatus() {
  okJson(String("{\"ok\":true,\"ip\":\"") + WiFi.localIP().toString() +
         "\",\"ble\":" + (hostConnected ? "true" : "false") +
         ",\"ptr\":[" + curX + "," + curY + "],\"uptime\":" + (millis() / 1000) + "}");
}
void hBatch() {
  String commands = httpServer.arg("cmd");
  int start = 0;
  while ((unsigned)start < commands.length()) {
    int sep = commands.indexOf(';', start);
    if (sep == -1) sep = commands.length();
    String cmd = commands.substring(start, sep);
    int colon = cmd.indexOf(':');
    String action = (colon != -1) ? cmd.substring(0, colon) : cmd;
    String params = (colon != -1) ? cmd.substring(colon + 1) : "";
    if (action == "tap") {
      int c = params.indexOf(',');
      moveTo(params.substring(0, c).toInt(), params.substring(c + 1).toInt());
      mouseClick(1); delay(60);
    } else if (action == "wait") delay(params.toInt());
    else if (action == "type") typeStr(params);
    else if (action == "key") tapKey(params);
    else if (action == "swipe") {
      int c1 = params.indexOf(',');
      int c2 = params.indexOf(',', c1 + 1);
      int c3 = params.indexOf(',', c2 + 1);
      doSwipe(params.substring(0, c1).toInt(), params.substring(c1 + 1, c2).toInt(),
              params.substring(c2 + 1, c3).toInt(), params.substring(c3 + 1).toInt(), 20, 12);
    }
    start = sep + 1; delay(10);
  }
  okJson("{\"ok\":true,\"action\":\"batch\"}");
}

void httpInit() {
  httpServer.on("/tap", hTap);
  httpServer.on("/double_tap", hDoubleTap);
  httpServer.on("/long_press", hLongPress);
  httpServer.on("/swipe", hSwipe);
  httpServer.on("/type", hType);
  httpServer.on("/key", hKey);
  httpServer.on("/move", hMove);
  httpServer.on("/scroll", hScroll);
  httpServer.on("/screen", hScreen);
  httpServer.on("/status", hStatus);
  httpServer.on("/batch", hBatch);
  httpServer.begin();
  Serial.println("HTTP :80 up");
}

// ===== SERIAL =====
void handleLine(String s);
String line = "";
void serialPoll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (line.length()) { handleLine(line); line = ""; }
    } else if (line.length() < 300) line += c;
  }
}
void handleLine(String s) {
  s.trim();
  Serial.print("CMD> "); Serial.println(s);
  int sp = s.indexOf(' ');
  String cmd = (sp == -1) ? s : s.substring(0, sp);
  String arg = (sp == -1) ? "" : s.substring(sp + 1);
  if (cmd == "type") typeStr(arg);
  else if (cmd == "key") tapKey(arg);
  else if (cmd == "tap") {
    int c = arg.indexOf(',');
    moveTo(arg.substring(0, c).toInt(), arg.substring(c + 1).toInt());
    mouseClick(1);
  }
  else if (cmd == "move") {
    int c = arg.indexOf(',');
    int dx = arg.substring(0, c).toInt(), dy = arg.substring(c + 1).toInt();
    moveDelta(dx, dy); curX += dx; curY += dy;
  }
  else if (cmd == "swipe") {
    int c1 = arg.indexOf(',');
    int c2 = arg.indexOf(',', c1 + 1);
    int c3 = arg.indexOf(',', c2 + 1);
    doSwipe(arg.substring(0, c1).toInt(), arg.substring(c1 + 1, c2).toInt(),
            arg.substring(c2 + 1, c3).toInt(), arg.substring(c3 + 1).toInt(), 20, 12);
  }
  else if (cmd == "home") { homed = false; homePointer(); }
  else if (cmd == "status") Serial.printf("ble=%d ptr=%d,%d uptime=%lus\n", hostConnected, curX, curY, millis() / 1000);
  else if (cmd == "help") Serial.println("type|key|tap x,y|move dx,dy|swipe x1,y1,x2,y2|home|status|diag");
  else if (cmd == "diag") {
    Serial.printf("bleReady=%d hostConn=%d\n", bleReady, hostConnected);
    NimBLEService* s = bleServer->getServiceByUUID(NimBLEUUID((uint16_t)UUID_HID_SERVICE));
    Serial.printf("HID svc: %s\n", s?"EXISTS":"NULL");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32 BLE HID v3 (raw GATT) ===");
  bleInit();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 12000) { delay(500); Serial.print("."); }
  if (WiFi.status() == WL_CONNECTED) Serial.println("\nWiFi IP: " + WiFi.localIP().toString());
  else Serial.println("\nWiFi FAILED — serial commands still work: type 'help'");
  httpInit();
  Serial.println("Serial: 'type HELLO' works once paired. Waiting for host...");
}

uint32_t lastWifi = 0;
void loop() {
  httpServer.handleClient();
  serialPoll();
  if (WiFi.status() != WL_CONNECTED && millis() / 1000 - lastWifi > 20) {
    lastWifi = millis() / 1000;
    WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  delay(2);
}
