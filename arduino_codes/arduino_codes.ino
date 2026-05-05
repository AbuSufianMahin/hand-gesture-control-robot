// L298N Pin Mapping
#define ENA 7
#define ENB 13
#define IN1 11
#define IN2 9
#define IN3 10
#define IN4 8

// ultrasonic sensor variables
#define TRIG 3
#define ECHO 2

#define STOP_DISTANCE 15

// Buzzer
#define BUZZER 4

void setup() {
  // pwm
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // ultrasonic
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  Serial.begin(9600);

  // buzzer
  pinMode(BUZZER, OUTPUT);
}

void spinLeft() {
  // Left wheels backward, Right wheels forward
  digitalWrite(ENA, HIGH); 
  digitalWrite(ENB, HIGH);

  // left wheelss => backward
  digitalWrite(IN1, LOW);  
  digitalWrite(IN2, HIGH);
  
  // right wheels => forward
  digitalWrite(IN3, HIGH); 
  digitalWrite(IN4, LOW);
}

void spinRight() {
  // Left wheels forward, Right wheels backward
  digitalWrite(ENA, HIGH); 
  digitalWrite(ENB, HIGH);
  
  // left wheels => forward
  digitalWrite(IN1, HIGH); 
  digitalWrite(IN2, LOW); 

  // right wheels => backward
  digitalWrite(IN3, LOW);  
  digitalWrite(IN4, HIGH);
}

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

void stopAll() {
  digitalWrite(ENA, LOW);
  digitalWrite(ENB, LOW);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

long getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH);
  return duration * 0.034 / 2;
}

void beepBuzzer(bool obstaclePresent) {
  static bool done = false;

  if (!obstaclePresent) {
    done = false;  // reset when obstacle clears
    digitalWrite(BUZZER, LOW);
    return;
  }

  if (done) return;

  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }

  // Long pause
  delay(300);

  // Second set of 3 beeps
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    delay(100);
  }
  done = true;
}

void loop() {
  long distance = getDistance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance < STOP_DISTANCE) {
    Serial.println("Obstacle detected! Stopping.");
    stopAll();
    beepBuzzer(true);
  } else {
    beepBuzzer(false);  // resets the flag and turns off buzzer
    forward();
  }

  delay(100);
}