// motor_test.ino - checks wiring before anything else.
// The robot drives forward, backward, left, right (1.5 s each, with pauses), then repeats.
//
// L298N -> Arduino Uno
//   ENA -> D5
//   IN1 -> D7
//   IN2 -> D8
//   IN3 -> D9
//   IN4 -> D10
//   ENB -> D6
//   GND -> Arduino GND (MUST be connected, together with the battery's minus)

const int ENA = 5, IN1 = 7, IN2 = 8;    // channel A = LEFT side motors
const int IN3 = 10, IN4 = 9, ENB = 6;   // channel B = RIGHT side motors
const int SPEED = 180;

void side(int en, int a, int b, int dir) {   // dir: 1 forward, -1 backward, 0 stop
  digitalWrite(a, dir == 1 ? HIGH : LOW);
  digitalWrite(b, dir == -1 ? HIGH : LOW);
  analogWrite(en, dir == 0 ? 0 : SPEED);
}

void drive(int left, int right, const char* name) {
  Serial.println(name);
  side(ENA, IN1, IN2, left);
  side(ENB, IN3, IN4, right);
}

void setup() {
  int pins[] = {ENA, IN1, IN2, IN3, IN4, ENB};
  for (int p : pins) pinMode(p, OUTPUT);
  Serial.begin(9600);
  Serial.println("Motor test starting in 3 s...");
  delay(3000);
}

void loop() {
  drive(-1, -1, "FORWARD");   delay(1500); drive(0, 0, "stop"); delay(1000);
  drive(1, 1, "BACKWARD"); delay(1500); drive(0, 0, "stop"); delay(1000);
  drive(-1, 1, "LEFT (spin)");  delay(1500); drive(0, 0, "stop"); delay(1000);
  drive(1, -1, "RIGHT (spin)"); delay(1500); drive(0, 0, "stop"); delay(3000);
}
