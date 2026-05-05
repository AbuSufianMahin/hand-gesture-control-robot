# 🤖 Arduino Robot Car

A 4-wheel robot car controlled via Arduino, using an L298N motor driver, PWM signal, ultrasonic sensor (servo-mounted), and remote hand gesture recognition via MediaPipe.

---

## 🧰 Hardware Components

- Arduino (Uno or compatible)
- L298N Motor Driver Module
- 4x DC Motors (wheels)
- Servo Motor (for ultrasonic sensor rotation)
- Ultrasonic Sensor (HC-SR04)
- External battery pack (for motors)
- USB cable (for Arduino programming)
---

## ⚡ Wiring / Connection Description

### L298N → Arduino

| L298N Pin | Arduino Pin | Controls              |
|-----------|-------------|----------------------|
| IN1       | 11          | Left Front wheel     |
| IN2       | 9           | Left Back wheel      |
| IN3       | 10          | Right Front wheel    |
| IN4       | 8           | Right Back wheel     |
| ENA       | 7           | Left side enable     |
| ENB       | 13          | Right side enable    |

### L298N → Motors

| L298N Output  | Connected To                        |
|---------------|-------------------------------------|
| OUT1, OUT2    | Motor Driver — Right Front & Back   |
| OUT3, OUT4    | Motor Driver — Left Front & Back    |

### Power

| Connection              | Description                              |
|-------------------------|------------------------------------------|
| Battery (+) → L298N VCC | Powers the motors                        |
| Battery (−) → L298N GND | Common ground                            |
| L298N GND → Arduino GND | **Required** — shared common ground      |
| USB → Arduino            | Programming & Serial Monitor only        |

> ⚠️ Always connect Battery GND and Arduino GND together. Without a common ground, the motor driver won't receive correct signals from the Arduino.

---

## 💻 Software Setup

### Prerequisites

- Python 3.x
- Arduino IDE
- PowerShell (Run as Administrator for execution policy)
### 1. Create a Virtual Environment

```bash
py -m venv a_venv
.\a_venv\Scripts\activate
```

### 2. Set PowerShell Execution Policy (Run as Administrator)

```powershell
Set-ExecutionPolicy -ExecutionPolicy Unrestricted
```

### 3. Install Python Dependencies

```bash
pip install opencv-python mediapipe requests
```

---

### Stopping arduino code
```
void setup() {}
void loop() {}
```
