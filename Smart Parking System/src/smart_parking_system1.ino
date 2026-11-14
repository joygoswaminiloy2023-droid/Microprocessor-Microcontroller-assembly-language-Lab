#include <Adafruit_LiquidCrystal.h>
#include <Servo.h>

Adafruit_LiquidCrystal lcd(0);     // I2C LCD
Servo gate;

const int slotPin[4] = {2, 3, 4, 5};      // slide switch COMMON pins
const int redLed[4]  = {10, 12, A0, A2};
const int grnLed[4]  = {11, 13, A1, A3};
const int TRIG = 6, ECHO = 7, BUZ = 8, SERVO_PIN = 9;
const int CAR_CM = 15;                    // closer than this = car at gate

enum State { IDLE, OPENED, FULL };
State state = IDLE;

int lastFree = -1, lastMsg = -1;
unsigned long lastCheck = 0, lastBeep = 0, clearSince = 0;

bool carAtGate() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long t = pulseIn(ECHO, HIGH, 4000);     // short timeout = fast
  if (t == 0) return false;               // nothing in range = no car
  return (t * 0.01723) < CAR_CM;
}

int updateSlots() {
  int freeSlots = 0;
  for (int i = 0; i < 4; i++) {
    bool occ = (digitalRead(slotPin[i]) == LOW);
    digitalWrite(redLed[i], occ);
    digitalWrite(grnLed[i], !occ);
    if (!occ) freeSlots++;
  }
  return freeSlots;
}

void showScreen(int freeSlots, int msg) {
  if (freeSlots == lastFree && msg == lastMsg) return;
  lastFree = freeSlots;
  lastMsg = msg;
  lcd.setCursor(0, 0);
  lcd.print("Slots free: ");
  lcd.print(freeSlots);
  lcd.print("   ");
  lcd.setCursor(0, 1);
  if (msg == 0) lcd.print("Waiting...      ");
  if (msg == 1) lcd.print("Gate OPENING    ");
  if (msg == 2) lcd.print("PARKING FULL    ");
}

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(slotPin[i], INPUT_PULLUP);
    pinMode(redLed[i], OUTPUT);
    pinMode(grnLed[i], OUTPUT);
  }
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZ, OUTPUT);
  gate.attach(SERVO_PIN);
  gate.write(0);
  lcd.begin(16, 2);
}

void loop() {
  int freeSlots = updateSlots();
  unsigned long now = millis();

  if (now - lastCheck >= 100) {           // check gate sensor 10 times/second
    lastCheck = now;
    bool car = carAtGate();

    if (state == IDLE) {
      if (car) {
        if (freeSlots > 0) { gate.write(90); state = OPENED; clearSince = 0; }
        else               { state = FULL; lastBeep = 0; }
      }
    } else if (state == OPENED) {
      if (car) clearSince = 0;
      else if (clearSince == 0) clearSince = now;
      else if (now - clearSince >= 1500) { gate.write(0); state = IDLE; }
    } else if (state == FULL) {
      if (!car || freeSlots > 0) {        // car left, or a slot became free
        state = IDLE;
        noTone(BUZ);                      // stop buzzer immediately
        digitalWrite(BUZ, LOW);
      }
    }
  }

  if (state == FULL) {
    if (now - lastBeep >= 500) {          // beep while the car waits
      tone(BUZ, 1000, 300);
      lastBeep = now;
    }
  } else {
    noTone(BUZ);                          // buzzer always off outside FULL
  }

  showScreen(freeSlots, state == IDLE ? 0 : (state == OPENED ? 1 : 2));
}