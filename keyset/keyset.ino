/*
 * Interface between Engelbart keyset and USB
 *
 * Ken Shirriff, righto.com
 *
 * Note: to compile, set Tools->USB Type to Keyboard
 */

 #include "USBHost_t36.h"

// DB-25 connector wiring to keyset
// Colors are arbitrary, corresponding to my cable
#define K5 0 // black, p14
#define K2 1 // gray, p10
#define K1 2 // purple, p11
#define K3 3 // blue, p12
#define K4 4 // green, p13
// ground = white, p18

const char *keys0 = " abcdefghijklmnopqrstuvwxyz,.;? ";
const char *keys1 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ<>:\\\t";
const char *keys2 = " !\"#$%&'()@+-*/^0123456789=[]\x1b\r";

#define LED 13 // Light LED on each character

USBHost myusb;

USBHIDParser hid1(myusb);
MouseController mouse1(myusb);


void setup() {
  pinMode(K1, INPUT_PULLUP);
  pinMode(K2, INPUT_PULLUP);
  pinMode(K3, INPUT_PULLUP);
  pinMode(K4, INPUT_PULLUP);
  pinMode(K5, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 0);
  myusb.begin();
}

// There's a simple state machine to handle debouncing: a key press must be stable for
// 100 milliseconds before it is handled. The variables below implement this.
// When a key is pressed, keyValue takes on the value, keyStartMillis records the time,
// and keyHandled is cleared.
// If the keypress value remains the same for 100 ms, the key is sent over USB and
// keyHandled is set.
// When the key is released, keyValue is set to 0.
// This algorithm handles press-and-release as well as rollover.
int keyValue = 0; // Current keypress value, or 0 if no press
unsigned long keyStartMillis = 0; // Time when keypress started
bool keyHandled = false; // Indicates that keypress has been handled


void loop() {
    myusb.Task();

  if (mouse1.available()) {
    Serial.print("Mouse: buttons = ");
    Serial.print(mouse1.getButtons());
    Serial.print(",  mouseX = ");
    Serial.print(mouse1.getMouseX());
    Serial.print(",  mouseY = ");
    Serial.print(mouse1.getMouseY());
    Serial.print(",  wheel = ");
    Serial.print(mouse1.getWheel());
    Serial.print(",  wheelH = ");
    Serial.print(mouse1.getWheelH());
    Serial.println();
    mouse1.mouseDataClear();
  }

  int newKeyValue = (digitalRead(K1) * 16 | digitalRead(K2) * 8 | digitalRead(K3) * 4 | digitalRead(K4) * 2 | digitalRead(K5)) ^ 0x1f;
  if (newKeyValue != keyValue) {
    keyValue = newKeyValue;
    keyStartMillis = millis();
    keyHandled = false;
    digitalWrite(LED, 0);
  } else if (millis() - keyStartMillis > 100 && !keyHandled) {
    if (keyValue == 0) return; // Key up so ignore
    // Detected a key pattern for more than 100 ms
    keyHandled = true;
    digitalWrite(LED, 1);
    Serial.print(digitalRead(K1), DEC);
    Serial.print(digitalRead(K2), DEC);
    Serial.print(digitalRead(K3), DEC);
    Serial.print(digitalRead(K4), DEC);
    Serial.println(digitalRead(K5), DEC);

    Serial.print(keyValue, HEX);
    Serial.println(keys0[keyValue]);
    // USB keyboard emulation info: https://www.pjrc.com/teensy/td_keyboard.html 
    Keyboard.print(keys0[keyValue]);
  }


}
