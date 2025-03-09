// DB-25 connector wiring to keyset
#define K5 0 // black, p14
#define K2 1 // gray, p10
#define K1 2 // purple, p11
#define K3 3 // blue, p12
#define K4 4 // green, p13
// ground = white, p18

#define LED 13

// USB keyboard emulation: https://www.pjrc.com/teensy/td_keyboard.html 

const char *keys0 = " abcdefghijklmnopqrstuvwxyz,.;? ";
const char *keys1 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ<>:\\\t";
const char *keys2 = " !\"#$%&'()@+-*/^0123456789=[]\x1b\r";

void setup() {
  pinMode(K1, INPUT_PULLUP);
  pinMode(K2, INPUT_PULLUP);
  pinMode(K3, INPUT_PULLUP);
  pinMode(K4, INPUT_PULLUP);
  pinMode(K5, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 0);
}

int prev = 0;
unsigned long prevMillis = 0;
bool registered = false;

void loop() {
  int pressed = (digitalRead(K1) * 16 | digitalRead(K2) * 8 | digitalRead(K3) * 4 | digitalRead(K4) * 2 | digitalRead(K5)) ^ 0x1f;
  if (pressed != prev) {
    prev = pressed;
    prevMillis = millis();
    registered = false;
    digitalWrite(LED, 0);
  } else if (millis() - prevMillis > 100 && !registered) {
    if (prev == 0) return; // Key up
    // Detected a key pattern for more than 100 ms
    registered = true;
    digitalWrite(LED, 1);
    Serial.print(digitalRead(K1), DEC);
    Serial.print(digitalRead(K2), DEC);
    Serial.print(digitalRead(K3), DEC);
    Serial.print(digitalRead(K4), DEC);
    Serial.println(digitalRead(K5), DEC);

    Serial.print(pressed, HEX);
    Serial.println(keys0[pressed]);
    Keyboard.print(keys0[pressed]);
  }


}
