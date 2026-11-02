// robot_controller.ino - the real sketch for the Final project.
// Receives one-character commands from the laptop and drives the motors.
// Works over USB cable NOW, over HC-05 Bluetooth module later,
// with no change, it listens to both.
//
// Commands:
//   F forward   B backward   L turn left   R turn right   S stop
//   0..9 set speed (0 = slowest usable, 9 = full)
//   P ping (does nothing except keep the watchdog happy)
//
// SAFETY WATCHDOG: if no command arrives for 500 ms, the motors stop.
// So if the laptop crashes, Python is closed, or Bluetooth drops -> robot stops.
//
// Wiring (L298N): ENA D5, IN1 D7, IN2 D8, IN3 D9, IN4 D10, ENB D6
// Wiring (HC-05, later):
//   HC-05 VCC -> 5V, GND -> GND
//   HC-05 TXD -> D2
//   HC-05 RXD -> D3 through a divider: D3 --1k-- RXD --2k-- GND
// Do NOT use D0/D1 for the HC-05 (breaks uploading).

#include <SoftwareSerial.h>

const int ENA = 5, IN1 = 7, IN2 = 8;    // LEFT side
const int IN3 = 10, IN4 = 9, ENB = 6;   // RIGHT side
SoftwareSerial bt(2, 3);                // RX = D2 (from HC-05 TXD), TX = D3 (to HC-05 RXD)

const unsigned long WATCHDOG_MS = 500;
const int MIN_PWM = 110;   // below this most cheap motors just hum; raise it if speed 0 doesn't move
const int MAX_PWM = 255;
const float TURN_FACTOR = 0.85;   // turns are a bit slower than straight driving

int speedPwm = 180;
char current = 'S';
unsigned long lastCmd = 0;

void side(int en, int a, int b, int dir, int pwm) {
  digitalWrite(a, dir == 1 ? HIGH : LOW);
  digitalWrite(b, dir == -1 ? HIGH : LOW);
  analogWrite(en, dir == 0 ? 0 : pwm);
}

void apply(char c) {
  int turn = (int)(speedPwm * TURN_FACTOR);
  switch (c) {
    case 'F': side(ENA, IN1, IN2, -1, speedPwm);  side(ENB, IN3, IN4,-1, speedPwm);  break;
    case 'B': side(ENA, IN1, IN2, 1, speedPwm); side(ENB, IN3, IN4, 1, speedPwm); break;
    case 'L': side(ENA, IN1, IN2, -1, turn);     side(ENB, IN3, IN4, 1, turn);      break;
    case 'R': side(ENA, IN1, IN2, 1, turn);      side(ENB, IN3, IN4, -1, turn);     break;
    default:  side(ENA, IN1, IN2, 0, 0);         side(ENB, IN3, IN4, 0, 0);         c = 'S';
  }
  current = c;
}

void handle(char c, Stream &port) {
  if (c >= 'a' && c <= 'z') c -= 32;   // accept lowercase too
  if (c >= '0' && c <= '9') {
    speedPwm = map(c - '0', 0, 9, MIN_PWM, MAX_PWM);
    lastCmd = millis();
    if (current != 'S') apply(current);   // update speed of current motion
    port.print("speed "); port.println(speedPwm);
    return;
  }
  if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S' || c == 'P') {
    lastCmd = millis();
    if (c != 'P' && c != current) {
      apply(c);
      port.println(c);   // echo, so the laptop can confirm
    }
  }
  // anything else (newlines, noise) is ignored
}

void setup() {
  int pins[] = {ENA, IN1, IN2, IN3, IN4, ENB};
  for (int p : pins) pinMode(p, OUTPUT);
  apply('S');
  Serial.begin(9600);   // USB
  bt.begin(9600);       // HC-05 default baud
  Serial.println("READY");
  bt.println("READY");
}

void loop() {
  while (Serial.available()) handle(Serial.read(), Serial);
  while (bt.available())     handle(bt.read(), bt);

  if (current != 'S' && millis() - lastCmd > WATCHDOG_MS) {
    apply('S');
    Serial.println("WATCHDOG STOP");
    bt.println("WATCHDOG STOP");
  }
}
