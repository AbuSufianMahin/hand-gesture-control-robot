//all working but the wheels.


#include <SoftwareSerial.h>

// L298N Pin Mapping
#define ENA 7
#define ENB 13
#define IN1 11
#define IN2 9
#define IN3 10
#define IN4 8

// Ultrasonic sensor
#define TRIG 3
#define ECHO 2
#define STOP_DISTANCE 20

// Buzzer
#define BUZZER 4

// ESP32 communication
SoftwareSerial espSerial(5, 6);

// State
char currentCmd = 'S';
char lastLoggedCmd = ' ';
char lastMoveCmd = 'S';  // last non-stop command
unsigned long lastCmdTime = 0;
#define WIFI_TIMEOUT_MS 500

// Non-blocking buzzer
unsigned long buzzerLastToggle = 0;
bool buzzerState = false;
int buzzerOnTime = 0;
int buzzerOffTime = 0;
bool buzzerActive = false;

void setBuzzerPattern(int onMs, int offMs) {
  buzzerOnTime = onMs;
  buzzerOffTime = offMs;
  buzzerActive = true;
  if (!buzzerState) {
    digitalWrite(BUZZER, HIGH);
    buzzerState = true;
    buzzerLastToggle = millis();
  }
}

void stopBuzzer() {
  buzzerActive = false;
  buzzerState = false;
  digitalWrite(BUZZER, LOW);
}

void updateBuzzer() {
  if (!buzzerActive) return;
  unsigned long now = millis();
  unsigned long elapsed = now - buzzerLastToggle;

  if (buzzerState && elapsed >= (unsigned long)buzzerOnTime) {
    digitalWrite(BUZZER, LOW);
    buzzerState = false;
    buzzerLastToggle = now;
  } else if (!buzzerState && elapsed >= (unsigned long)buzzerOffTime) {
    digitalWrite(BUZZER, HIGH);
    buzzerState = true;
    buzzerLastToggle = now;
  }
}

// ── Motion ────────────────────────────────────────────────────
void forward() {
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void spinLeft() {
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void spinRight() {
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopAll() {
  digitalWrite(ENA, LOW);
  digitalWrite(ENB, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ── Ultrasonic ────────────────────────────────────────────────
long getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 20000);  // 20ms timeout (~3.4m max)
  if (duration == 0) return 999;               // no echo = clear path
  return duration * 0.034 / 2;
}

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);

  Serial.begin(9600);
  espSerial.begin(9600);

  stopAll();
  Serial.println("=== Robot Ready ===");
}

void loop() {
  unsigned long now = millis();

  // ── Update non-blocking buzzer ──
  updateBuzzer();

  // ── Read command from ESP32 ──
  if (espSerial.available() > 0) {
    char cmd = (char)espSerial.read();
    if (cmd == 'F' || cmd == 'B' || cmd == 'L' || cmd == 'R' || cmd == 'S') {
      currentCmd = cmd;
      lastCmdTime = now;
    }
  }

  // ── WiFi timeout ──
  if (lastCmdTime != 0 && (now - lastCmdTime) > WIFI_TIMEOUT_MS) {
    if (currentCmd != 'S') {
      Serial.println("[WIFI] Connection lost! Stopping.");
      currentCmd = 'S';
      setBuzzerPattern(1000, 500);
    }
  }

  // ── Ultrasonic ──
  long distance = getDistance();


  // Block forward if obstacle
  if (currentCmd == 'F' && distance < STOP_DISTANCE) {
    if (lastLoggedCmd != 'X') {
      Serial.print("[OBSTACLE] Obstacle found! At Distance: ");
      Serial.print(distance);
      Serial.println(" cm. Stopping.");
      lastLoggedCmd = 'X';
    }
    stopAll();
    Serial.println("[DEBUG] stopAll() called");
    Serial.print("[DEBUG] ENA pin state: ");
    Serial.println(digitalRead(ENA));
    Serial.print("[DEBUG] ENB pin state: ");
    Serial.println(digitalRead(ENB));
    setBuzzerPattern(150, 100);
    return;
  }

  // ── Execute command ──
  switch (currentCmd) {
    case 'F':
      forward();
      if (currentCmd != lastLoggedCmd) Serial.println("[MOVE] Moving Forward");
      stopBuzzer();
      break;

    case 'B':
      backward();
      if (currentCmd != lastLoggedCmd) Serial.println("[MOVE] Moving Backward");
      setBuzzerPattern(200, 400);  // truck reversing beep
      break;

    case 'L':
      spinLeft();
      if (currentCmd != lastLoggedCmd) Serial.println("[MOVE] Turning Left");
      stopBuzzer();
      break;

    case 'R':
      spinRight();
      if (currentCmd != lastLoggedCmd) Serial.println("[MOVE] Turning Right");
      stopBuzzer();
      break;

    case 'S':
    default:
      stopAll();
      if (currentCmd != lastLoggedCmd) Serial.println("[MOVE] Stopped");
      stopBuzzer();
      break;
  }

  lastLoggedCmd = currentCmd;
}