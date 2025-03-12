/*
 * Interface between Engelbart keyset and USB
 * Also supports a regular USB mouse so the buttons can be used with the keyset.
 * The tricky part is passing mouse operations through so the mouse is still usable.
 *
 * Ken Shirriff, righto.com
 *
 * Note: to compile, set Tools->USB Type to Keyboard + Mouse + Joystick
 * (Joystick is unused but there's no option for just Keyboard + Mouse.)
 */

#include "USBHost_t36.h"

#undef DEBUG  // Log to Serial

// DB-25 connector wiring to keyset
// Colors are arbitrary, corresponding to my cable
#define K5 0  // black, p14
#define K2 1  // gray, p10
#define K1 2  // purple, p11
#define K3 3  // blue, p12
#define K4 4  // green, p13
// ground = white, p18

// Keyset characters for no mouse button, middle mouse button, and left mouse button
const char keys0[33] = " abcdefghijklmnopqrstuvwxyz,.;? ";
const char keys1[33] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ<>:\\\t";
const char keys2[33] = " !\"#$%&'()@+-*/^0123456789=[]\x08\x1b\r";

#define LED 13  // Light LED on each character

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
int keyValue = 0;                  // Current keypress value, or 0 if no press
int buttonValue = 0;               // Current USB mouse button value
unsigned long keyStartMillis = 0;  // Time when keypress started
bool keyHandled = false;           // Indicates that keypress has been handled

/*
 * Left button: 1<<0, middle button 1<<2, right button 1<<1
 * See "Button Status" at https://wiki.osdev.org/USB_Human_Interface_Devices
 */
#define BUTTON_L 1  // Left mouse button, USB code
#define BUTTON_M 4  // Middle mouse button, USB code
#define BUTTON_R 2  // Right mouse button, USB code

// Log a debug message to the Serial port
void debugMsg(const char *msg) {
#ifdef DEBUG
  Serial.println(msg);
#endif
}

// Sends a character using the USB Keyboard
// This handles special characters and debug logging
void send(char c) {
  if (c == '\r') {  // return
#ifdef DEBUG
    Serial.println("Sending KEY_ENTER");
#endif
    Keyboard.press(KEY_ENTER);
    Keyboard.release(KEY_ENTER);
  } else if (c == '\t') {  // tab
#ifdef DEBUG
    Serial.println("Sending KEY_TAB");
#endif
    Keyboard.press(KEY_TAB);
    Keyboard.release(KEY_TAB);
  } else if (c == '\x1b') {  // escape
#ifdef DEBUG
    Serial.println("Sending KEY_ESC");
#endif
    Keyboard.press(KEY_ESC);
    Keyboard.release(KEY_ESC);
  } else if (c == '\b') {  // backspace
#ifdef DEBUG
    Serial.println("Sending KEY_BACKSPACE");
#endif
    Keyboard.press(KEY_BACKSPACE);
    Keyboard.release(KEY_BACKSPACE);
  } else if (c <= 0x1a) {  // Control character
#ifdef DEBUG
    Serial.print("Sending control-");
    Serial.println((char)(c + 64));
#endif
    Keyboard.press(MODIFIERKEY_CTRL);
    Keyboard.press(c - 1 + KEY_A);  // Will work for CTRL-alpha but probably not others
    Keyboard.release(c - 1 + KEY_A);
    Keyboard.release(MODIFIERKEY_CTRL);
  } else {
#ifdef DEBUG
    Serial.print("Sending: x");
    Serial.print(c, HEX);
    Serial.print(" ");
    Serial.println(c);
#endif
    Keyboard.print(c);
  }
}

void loop() {
  myusb.Task();

  int newButtonValue = buttonValue;  // USB mouse value stays the same unless event received

  if (mouse1.available()) {
#ifdef DEBUG_MOUSE
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
#endif
    newButtonValue = mouse1.getButtons();
    // Don't pass mouse buttons through here because we need to merge them with the keyset
    // This results in a 100ms delay before mouse buttons take effect, which is a bit annoying.
    // Pass mouse movement and wheel through immediately
    if (mouse1.getMouseX() != 0 || mouse1.getMouseY() != 0) {
      Mouse.move(mouse1.getMouseX(), mouse1.getMouseY());
    }
    if (mouse1.getWheel()) {
      Mouse.scroll(mouse1.getWheel(), mouse1.getWheelH());
    }
    mouse1.mouseDataClear();
  }

  int newKeyValue = (digitalRead(K1) * 16 | digitalRead(K2) * 8 | digitalRead(K3) * 4 | digitalRead(K4) * 2 | digitalRead(K5)) ^ 0x1f;
  if (newKeyValue != keyValue || newButtonValue != buttonValue) {
    keyValue = newKeyValue;
    buttonValue = newButtonValue;
    keyStartMillis = millis();
    keyHandled = false;
    if (buttonValue == 0) {
      Mouse.set_buttons(0, 0, 0);  // Release button presses if we're sending any
    }
    digitalWrite(LED, 0);
  } else if (millis() - keyStartMillis > 100 && !keyHandled) {
    if (keyValue == 0 && buttonValue == 0) return;  // Key up so ignore
    // Detected a key pattern for more than 100 ms
    keyHandled = true;
    digitalWrite(LED, 1);
#ifdef DEBUG
    Serial.print("keyset buttons: ");
    Serial.print(digitalRead(K1) ^ 1, DEC);
    Serial.print(digitalRead(K2) ^ 1, DEC);
    Serial.print(digitalRead(K3) ^ 1, DEC);
    Serial.print(digitalRead(K4) ^ 1, DEC);
    Serial.print(digitalRead(K5) ^ 1, DEC);
    Serial.print(", keyset value: x");
    Serial.print(keyValue, HEX);
    Serial.print(" mouse button value: ");
    Serial.println(buttonValue, DEC);
#endif
    if (keyValue == 0) {
      // Special handling for mouse buttons without keyset.
      if (buttonValue == BUTTON_M) {  // Middle button
        // Should be <CD>, but pass mouse button through
        debugMsg("Sending middle button");
        Mouse.set_buttons(0, 1, 0);
      } else if (buttonValue == BUTTON_L) {  // Left button
                                             // Should be <BC>, but pass mouse button through
        debugMsg("Sending left button");
        Mouse.set_buttons(1, 0, 0);
      } else if (buttonValue == BUTTON_R) {  // Right button
        // Should be <OK>, but pass mouse button through
        debugMsg("Sending right button");
        Mouse.set_buttons(0, 0, 1);
      } else if (buttonValue == (BUTTON_L | BUTTON_M)) {  // Left, middle buttons
        send('\b');                                       // Should be <BW> but send a backspace
      } else if (buttonValue == (BUTTON_M | BUTTON_R)) {  // Middle, right buttons
        debugMsg("Sending <RC>");
        Keyboard.print("<RC>");
      } else if (buttonValue == (BUTTON_L | BUTTON_R)) {  // Left, right buttons
        send('\x1b');
      } else if (buttonValue == (BUTTON_L | BUTTON_M | BUTTON_R)) {  // All buttons
        debugMsg("Ignoring 3-button press");
        // No action
      }
    } else if (buttonValue == 0) {
      // USB keyboard emulation info: https://www.pjrc.com/teensy/td_keyboard.html
      send(keys0[keyValue]);
    } else if (buttonValue == BUTTON_M) {
      send(keys1[keyValue]);
    } else if (buttonValue == BUTTON_L) {
      send(keys2[keyValue]);
    } else if (buttonValue == BUTTON_R) {
      // "Has no meaning with keyset input"
      debugMsg("Ignoring right + keyset");
    } else if (buttonValue == (BUTTON_L | BUTTON_M)) {
      // "Take each keyset code as a lowercase viewspec"
      // Ignore for now.
      debugMsg("Ignoring left/middle + keyset");
    } else if (buttonValue == (BUTTON_M | BUTTON_R)) {
      send(keys0[keyValue] & 0x1f);
    } else if (buttonValue == (BUTTON_L | BUTTON_R)) {
      // "Search for marker named by keyset combination"
      // Ignore for now.
      debugMsg("Ignoring left/right + keyset");
    } else if (buttonValue == (BUTTON_L | BUTTON_M | BUTTON_R)) {
      // "Take each keyset code as a capital viewspec."
      // Ignore for now.
      debugMsg("Ignoring left/middle/right + keyset");
    }
  }
}