/*
 * ============================================================
 *  Gesture-Controlled Collision-Avoidance Robot — ESP32
 * ============================================================
 *  Control flow:  gesture → Wi-Fi command → obstacle check → move / stop
 *
 *  Hardware
 *  --------
 *  Motor Driver  : L298N
 *    ENA (PWM)   → GPIO 25      ENB (PWM)   → GPIO 26
 *    IN1         → GPIO 27      IN2         → GPIO 14
 *    IN3         → GPIO 12      IN4         → GPIO 13
 *
 *  Ultrasonic    : HC-SR04 on Servo
 *    TRIG        → GPIO 5       ECHO        → GPIO 18
 *
 *  Servo         : GPIO 19  (sweep during scan)
 *
 *  Buzzer        : GPIO 23  (active LOW buzzer; flip BUZZER_ON if active HIGH)
 *  Status LED    : GPIO 2   (built-in LED or external)
 *
 *  Suggestions implemented
 *  -----------------------
 *  ✓ Servo-mounted ultrasonic for 3-point obstacle scan
 *  ✓ PWM speed control — slows before turning / near obstacles
 *  ✓ Gesture accepted only after stable detection (done in Python)
 *  ✓ Wi-Fi watchdog — robot stops if no command received within timeout
 *  ✓ Buzzer + LED for command / obstacle / connection status
 *  ✓ Decoupled control flow: HTTP handler → command buffer → loop
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ── Wi-Fi credentials ─────────────────────────────────────────
const char* SSID     = "YOUR_SSID";
const char* PASSWORD = "YOUR_PASSWORD";

// ── Pin definitions ───────────────────────────────────────────
// Motor A (Left side)
const int ENA  = 25;
const int IN1  = 27;
const int IN2  = 14;

// Motor B (Right side)
const int ENB  = 26;
const int IN3  = 12;
const int IN4  = 13;

// Ultrasonic
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// Servo
const int SERVO_PIN = 19;

// Buzzer & LED
const int BUZZER_PIN = 23;
const int LED_PIN    = 2;

// ── Speed settings (0-255) ────────────────────────────────────
const int SPEED_FULL   = 220;   // straight-line cruise speed
const int SPEED_TURN   = 170;   // speed while turning
const int SPEED_SLOW   = 120;   // caution zone speed
const int SPEED_STOP   = 0;

// ── Obstacle thresholds (cm) ──────────────────────────────────
const int DIST_STOP    = 15;    // hard stop
const int DIST_SLOW    = 35;    // slow down

// ── Wi-Fi watchdog ────────────────────────────────────────────
const unsigned long WATCHDOG_MS = 3000;   // stop if silent for 3 s
unsigned long lastCmdTime       = 0;

// ── State ─────────────────────────────────────────────────────
String pendingCommand = "stop";   // latest command from HTTP handler
String activeMotion   = "stop";

// ── LEDC PWM channels (ESP32 specific) ───────────────────────
const int PWM_FREQ    = 1000;
const int PWM_RES     = 8;          // 8-bit → 0-255
const int CH_ENA      = 0;
const int CH_ENB      = 1;

// ── Servo & sensor ───────────────────────────────────────────
Servo scanServo;
WebServer server(80);

// ─────────────────────────────────────────────────────────────
//  Low-level motor helpers
// ─────────────────────────────────────────────────────────────

void setMotorA(int dir, int spd) {
  // dir: 1=fwd, -1=rev, 0=brake
  ledcWrite(CH_ENA, spd);
  digitalWrite(IN1, dir == 1  ? HIGH : LOW);
  digitalWrite(IN2, dir == -1 ? HIGH : LOW);
}

void setMotorB(int dir, int spd) {
  ledcWrite(CH_ENB, spd);
  digitalWrite(IN3, dir == 1  ? HIGH : LOW);
  digitalWrite(IN4, dir == -1 ? HIGH : LOW);
}

void motorsStop() {
  setMotorA(0, 0);
  setMotorB(0, 0);
}

void driveForward(int spd)  { setMotorA(1,  spd); setMotorB(1,  spd); }
void driveBackward(int spd) { setMotorA(-1, spd); setMotorB(-1, spd); }
void turnLeft(int spd)      { setMotorA(-1, spd); setMotorB(1,  spd); }
void turnRight(int spd)     { setMotorA(1,  spd); setMotorB(-1, spd); }

// ─────────────────────────────────────────────────────────────
//  Ultrasonic measurement
// ─────────────────────────────────────────────────────────────

long measureCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000); // 25 ms timeout ≈ 430 cm
  if (duration == 0) return 400;                  // out of range → safe
  return duration * 0.034 / 2;
}

// 3-point scan: left (60°), center (90°), right (120°)
// Returns minimum distance found
long scanObstacles() {
  int  angles[] = {60, 90, 120};
  long minDist  = 400;
  for (int i = 0; i < 3; i++) {
    scanServo.write(angles[i]);
    delay(150);                  // let servo settle
    long d = measureCm();
    if (d < minDist) minDist = d;
  }
  scanServo.write(90);           // return to center
  return minDist;
}

// ─────────────────────────────────────────────────────────────
//  Buzzer / LED feedback
// ─────────────────────────────────────────────────────────────

// BUZZER_ON / BUZZER_OFF — set to LOW/HIGH if your buzzer is active-LOW
#define BUZZER_ON  HIGH
#define BUZZER_OFF LOW

void beep(int ms) {
  digitalWrite(BUZZER_PIN, BUZZER_ON);
  delay(ms);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
}

// Non-blocking LED blink state
bool     ledState    = false;
unsigned long ledToggleAt = 0;
int      blinkPeriod = 0;  // 0 = solid on, >0 = blink period ms

void updateLED() {
  if (blinkPeriod == 0) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }
  if (millis() > ledToggleAt) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    ledToggleAt = millis() + blinkPeriod;
  }
}

// ─────────────────────────────────────────────────────────────
//  HTTP command handler
// ─────────────────────────────────────────────────────────────

void handleCmd() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    action.toLowerCase();

    // Validate against known commands
    if (action == "forward"  || action == "backward" ||
        action == "left"     || action == "right"    ||
        action == "stop") {
      pendingCommand = action;
      lastCmdTime    = millis();

      // Single beep on every accepted command
      beep(40);

      server.send(200, "text/plain", "OK:" + action);
      Serial.println("[HTTP] CMD: " + action);
    } else {
      server.send(400, "text/plain", "Unknown action: " + action);
    }
  } else {
    server.send(400, "text/plain", "Missing ?action=");
  }
}

void handleStatus() {
  String json = "{";
  json += "\"motion\":\"" + activeMotion + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ─────────────────────────────────────────────────────────────
//  Execute motion with obstacle awareness
// ─────────────────────────────────────────────────────────────

void executeCommand(const String& cmd) {
  // Always scan first for forward motion
  if (cmd == "forward") {
    long dist = scanObstacles();

    if (dist <= DIST_STOP) {
      // Hard stop + warning beeps
      motorsStop();
      activeMotion = "stop(obstacle)";
      beep(80); delay(60); beep(80);
      blinkPeriod = 100;   // fast blink = danger
      Serial.printf("[OBS] Blocked at %ld cm\n", dist);
      return;
    } else if (dist <= DIST_SLOW) {
      // Slow mode + single beep
      driveForward(SPEED_SLOW);
      activeMotion = "forward(slow)";
      beep(40);
      blinkPeriod = 300;   // medium blink = caution
      Serial.printf("[OBS] Slowing at %ld cm\n", dist);
      return;
    } else {
      driveForward(SPEED_FULL);
      activeMotion = "forward";
      blinkPeriod  = 0;    // solid = all-clear
    }

  } else if (cmd == "backward") {
    driveBackward(SPEED_FULL);
    activeMotion = "backward";
    blinkPeriod  = 0;

  } else if (cmd == "left") {
    turnLeft(SPEED_TURN);
    activeMotion = "left";
    blinkPeriod  = 500;

  } else if (cmd == "right") {
    turnRight(SPEED_TURN);
    activeMotion = "right";
    blinkPeriod  = 500;

  } else {
    // "stop" or unknown
    motorsStop();
    activeMotion = "stop";
    blinkPeriod  = 800;    // slow blink = idle
  }

  Serial.println("[MOTION] " + activeMotion);
}

// ─────────────────────────────────────────────────────────────
//  Wi-Fi setup with status feedback
// ─────────────────────────────────────────────────────────────

void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s", SSID);
  WiFi.begin(SSID, PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    // Double-beep every 5 attempts to indicate still trying
    if (attempts % 5 == 0) { beep(30); delay(80); beep(30); }
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    // Three short beeps = connected
    for (int i = 0; i < 3; i++) { beep(60); delay(80); }
    blinkPeriod = 800;    // slow blink = idle but connected
  } else {
    Serial.println("\n[WiFi] FAILED — running offline (no remote control)");
    // Long beep = error
    beep(500);
    blinkPeriod = 200;    // rapid blink = no connection
  }
}

// ─────────────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // PWM channels for enable pins
  ledcSetup(CH_ENA, PWM_FREQ, PWM_RES);
  ledcSetup(CH_ENB, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, CH_ENA);
  ledcAttachPin(ENB, CH_ENB);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Buzzer & LED
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_OFF);
  pinMode(LED_PIN, OUTPUT);

  // Servo — center at startup
  scanServo.attach(SERVO_PIN);
  scanServo.write(90);
  delay(500);

  // Start stopped
  motorsStop();

  // Connect Wi-Fi
  connectWiFi();

  // HTTP routes
  server.on("/cmd",    HTTP_GET, handleCmd);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Server started on port 80");

  lastCmdTime = millis();
}

// ─────────────────────────────────────────────────────────────
//  Main loop
// ─────────────────────────────────────────────────────────────

void loop() {
  server.handleClient();
  updateLED();

  // ── Wi-Fi watchdog ──────────────────────────────────────
  if (millis() - lastCmdTime > WATCHDOG_MS) {
    if (activeMotion != "stop") {
      Serial.println("[WDG] Timeout — stopping robot");
      pendingCommand = "stop";
      motorsStop();
      activeMotion = "stop";
      beep(200);           // long beep = watchdog triggered
      blinkPeriod = 200;   // rapid blink = connection lost
    }
    // Reset timer so we don't spam the serial port
    lastCmdTime = millis();
  }

  // ── Wi-Fi reconnect ─────────────────────────────────────
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Lost — reconnecting…");
      WiFi.reconnect();
    }
  }

  // ── Execute the latest buffered command ─────────────────
  // Only re-run if command changed or we are in forward mode
  // (forward re-scans every loop to catch new obstacles)
  static String lastExecuted = "";
  if (pendingCommand != lastExecuted || pendingCommand == "forward") {
    executeCommand(pendingCommand);
    lastExecuted = pendingCommand;
  }

  delay(50);   // ~20 Hz loop rate — fast enough for smooth response
}
