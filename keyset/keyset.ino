/*
 * Interface between Engelbart keyset and USB
 * Also supports a regular USB mouse so the buttons can be used with the keyset.
 * One tricky part is passing mouse operations through so the mouse is still usable.
 *
 * Ken Shirriff, righto.com
 *
 * Note: to compile, set Tools->USB Type to Keyboard + Mouse + Joystick
 * (Joystick is unused but there's no option for just Keyboard + Mouse.)
 */

#include "USBHost_t36.h"

#undef DEBUG  // Log to Serial
#undef DEBUG_MOUSE

// DB-25 connector wiring to keyset
// Colors are arbitrary, corresponding to my cable
#define K5 0  // black, p15
#define K2 1  // gray, p10
#define K1 2  // purple, p11
#define K3 3  // blue, p12
#define K4 4  // green, p13
// ground = white, p18

// Keyset characters for no mouse button, middle mouse button, and left mouse button
#define SHIFT 0x100
#define CTRL 0x200
#define ALT 0x400
const uint16_t keys0[32] = {
  0,                  // 0
  KEY_A,              // 1
  KEY_B,              // 2
  KEY_C,              // 3
  KEY_D,              // 4
  KEY_E,              // 5
  KEY_F,              // 6
  KEY_G,              // 7
  KEY_H,              // 8
  KEY_I,              // 9
  KEY_J,              // 10
  KEY_K,              // 11
  KEY_L,              // 12
  KEY_M,              // 13
  KEY_N,              // 14
  KEY_O,              // 15
  KEY_P,              // 16
  KEY_Q,              // 17
  KEY_R,              // 18
  KEY_S,              // 19
  KEY_T,              // 20
  KEY_U,              // 21
  KEY_V,              // 22
  KEY_W,              // 23
  KEY_X,              // 24
  KEY_Y,              // 25
  KEY_Z,              // 26
  KEY_COMMA,          // 27
  KEY_PERIOD,         // 28
  KEY_SEMICOLON,      // 29
  KEY_SLASH | SHIFT,  // 30 "?"
  KEY_SPACE,          // 31
};
const uint16_t keys1[32] = {
  0,                      // 0
  KEY_A | SHIFT,          // 1
  KEY_B | SHIFT,          // 2
  KEY_C | SHIFT,          // 3
  KEY_D | SHIFT,          // 4
  KEY_E | SHIFT,          // 5
  KEY_F | SHIFT,          // 6
  KEY_G | SHIFT,          // 7
  KEY_H | SHIFT,          // 8
  KEY_I | SHIFT,          // 9
  KEY_J | SHIFT,          // 10
  KEY_K | SHIFT,          // 11
  KEY_L | SHIFT,          // 12
  KEY_M | SHIFT,          // 13
  KEY_N | SHIFT,          // 14
  KEY_O | SHIFT,          // 15
  KEY_P | SHIFT,          // 16
  KEY_Q | SHIFT,          // 17
  KEY_R | SHIFT,          // 18
  KEY_S | SHIFT,          // 19
  KEY_T | SHIFT,          // 20
  KEY_U | SHIFT,          // 21
  KEY_V | SHIFT,          // 22
  KEY_W | SHIFT,          // 23
  KEY_X | SHIFT,          // 24
  KEY_Y | SHIFT,          // 25
  KEY_Z | SHIFT,          // 26
  KEY_COMMA | SHIFT,      // 27, "<"
  KEY_PERIOD | SHIFT,     // 28, ">"
  KEY_SEMICOLON | SHIFT,  // 29
  KEY_BACKSLASH,          // 30
  KEY_TAB,                // 31
};
const uint16_t keys2[32] = {
  0,                  // 0
  KEY_1 | SHIFT,      // 1, "!"
  KEY_QUOTE | SHIFT,  // 2, quotation mark
  KEY_3 | SHIFT,      // 3, "#"
  KEY_4 | SHIFT,      // 4, "$"
  KEY_5 | SHIFT,      // 5, "%"
  KEY_6 | SHIFT,      // 6, "&"
  KEY_QUOTE,          // 7, "'"
  KEY_9 | SHIFT,      // 8, "("
  KEY_0 | SHIFT,      // 9, ")""
  KEY_2 | SHIFT,      // 10, "@"
  KEY_EQUAL | SHIFT,  // 11, "+"
  KEY_MINUS,          // 12, "-"
  KEY_8 | SHIFT,      // 13, "*"
  KEY_SLASH,          // 14, "/"
  KEY_6 | SHIFT,      // 15, "^"
  KEY_0,              // 16
  KEY_1,              // 17
  KEY_2,              // 18
  KEY_3,              // 19
  KEY_4,              // 20
  KEY_5,              // 21
  KEY_6,              // 22
  KEY_7,              // 23
  KEY_8,              // 24
  KEY_9,              // 25
  KEY_EQUAL,          // 26
  KEY_LEFT_BRACE,     // 27, "["
  KEY_RIGHT_BRACE,    // 28, "]"
  KEY_LEFT | SHIFT,   // 29, left arrow
  KEY_ESC,            // 30
  KEY_ENTER,          // 31
};

#define LED 13  // Light LED on each activity

#define KEYSET_DELAY_MS 100   // Delay to collect presses
#define MOUSE_DELAY_MS 100    // Delay before sending a mouse button press
#define KEYSET_RELEASE_MS 50  // Delay after releasing all keys before processing next character, to account for release bounce

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

// State machine definitions
#define IDLE 1
#define MOUSE_ACTIVITY 2
#define MOUSE_HELD 3
#define KEYSET_ACTIVITY 4
#define KEY_SENT 5

int state = IDLE;

int keyValue = 0;                   // Previous keyset value, or 0 if no press
int accumKeyValue = 0;              // Accumulated keyset value, collecting the key presses
int buttonValue = 0;                // Previous USB mouse button value
int accumButtonValue = 0;           // Accumulated button press value, collecting the buttons.
unsigned long timerFireMillis = 0;  // Time when the timer expires

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
void send(uint16_t c) {
  Serial.print("KEY_A: ");
  Serial.println(KEY_A, HEX);
  Serial.print("send(");
  Serial.print(c, HEX);
  Serial.println(")");
  if (c & SHIFT) {
    debugMsg("Sending SHIFT");
    Keyboard.press(MODIFIERKEY_SHIFT);
  }
  if (c & CTRL) {
    debugMsg("Sending CTRL");
    Keyboard.press(MODIFIERKEY_CTRL);
  }
  if (c & ALT) {
    debugMsg("Sending ALT");
    Keyboard.press(MODIFIERKEY_GUI);  // Mac Command key
  }
  Keyboard.press(c & 0xf0ff);
  Keyboard.releaseAll();
}

void debugInfo(int newKeyValue, int newButtonValue) {
#ifdef DEBUG
  Serial.print("updateKeysetActivity: accumKeyValue: ");
  Serial.print(accumKeyValue, DEC);
  Serial.print(", newKeyValue: ");
  Serial.print(newKeyValue, DEC);
  Serial.print(", accumButtonValue: ");
  Serial.print(accumButtonValue, DEC);
  Serial.print(", newButtonValue: ");
  Serial.println(newButtonValue, DEC);
#endif
}

// Move state machine to IDLE.
// A separate function to avoid duplicated code.
void moveToIdleState() {
  state = IDLE;  // Debounce delay on key release is over.
  debugMsg("Moved to IDLE state");
  accumKeyValue = 0;
  accumButtonValue = 0;
}

// Update KEYSET_ACTIVITY key, button, and timers
void updateKeysetActivity(int newKeyValue, int newButtonValue) {
  debugInfo(newKeyValue, newButtonValue);
  accumKeyValue |= newKeyValue;        // Accumulate keyset key
  accumButtonValue |= newButtonValue;  // Accumulate the mouse button (if any)
  timerFireMillis = millis() + KEYSET_DELAY_MS;
}

void loop() {
  digitalWrite(LED, 0);  // Clear indicator
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
    // Don't pass mouse buttons through here because we need to merge them with the keyset.
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
  // State machine processing
  if (state == IDLE) {
    if ((newButtonValue == BUTTON_L || newButtonValue == BUTTON_M || newButtonValue == BUTTON_R) && newKeyValue == 0
        && accumButtonValue != (accumButtonValue | newButtonValue)) {
      // Simple mouse button press
      state = MOUSE_ACTIVITY;
      debugMsg("Moved to MOUSE_ACTIVITY state due to mouse button");
      debugInfo(newKeyValue, newButtonValue);
      timerFireMillis = millis() + MOUSE_DELAY_MS;  // Delay before handling button press in case the keyset is used
    } else if (newKeyValue != 0 || accumButtonValue != (accumButtonValue | newButtonValue)) {
      // Key pressed or a new mouse button pressed (ignore a "holdover" mouse button)
      state = KEYSET_ACTIVITY;
      debugMsg("Moved to KEYSET_ACTIVITY state due to keyset activation");
      updateKeysetActivity(newKeyValue, newButtonValue);
    }
    accumButtonValue = newButtonValue;  // Set to current button state, which could add or remove from accumButtonState
  } else if (state == MOUSE_ACTIVITY) {
    if (newButtonValue == 0) {
      debugMsg("Mouse button released");
      mouseButtonDown(accumButtonValue);  // Button released, so send a short-duration click
      mouseButtonUp();
      moveToIdleState();
    } else if (accumButtonValue != (accumButtonValue | newButtonValue)) {
      // A second mouse button pressed, so now it must be handled by the keyset logic
      state = KEYSET_ACTIVITY;
      debugMsg("Moved to KEYSET_ACTIVITY state due to second mouse button");
      updateKeysetActivity(newKeyValue, newButtonValue);
    } else if (millis() > timerFireMillis) {
      debugMsg("Mouse button timer fired: sending button");
      mouseButtonDown(accumButtonValue);  // Too long to be a keyset+mouse action so send mouse button
      state = MOUSE_HELD;
      debugMsg("Moved to MOUSE_HELD state");
    }
  } else if (state == MOUSE_HELD) {
    if (newButtonValue == 0) {
      debugMsg("Mouse button released: sending up");
      mouseButtonUp();  // Send mouse button up; down was sent when moving to this state.
      moveToIdleState();
      state = IDLE;
    } else if (newKeyValue != 0 || accumButtonValue != (accumButtonValue | newButtonValue)) {
      // Key pressed or a new mouse button pressed, so becomes a keyset action.
      // This is an unwanted path, since we already sent a mouse button down.
      // This happens if the user holds a mouse button too long before using the keyset.
      debugMsg("Unexpected key/button in MOUSE_HELD");
      mouseButtonUp();  // Have to release the mouse now.
      state = KEYSET_ACTIVITY;
      debugMsg("Moved to KEYSET_ACTIVITY state");
      updateKeysetActivity(newKeyValue, newButtonValue);
    }
  } else if (state == KEYSET_ACTIVITY) {
    debugInfo(newKeyValue, newButtonValue);
    if (millis() > timerFireMillis && newKeyValue == 0) {
      // Timer expired and all keys up
      debugMsg("Timer expired and all keys up, so sending key");
      sendKey();
      state = KEY_SENT;
      debugMsg("Moved to KEY_SENT state");
      timerFireMillis = millis() + KEYSET_RELEASE_MS;
    } else if (accumKeyValue != (accumKeyValue | newKeyValue) || accumButtonValue != (accumButtonValue | newButtonValue)) {
      // Another key or button pressed
      updateKeysetActivity(newKeyValue, newButtonValue);
    }
  } else if (state == KEY_SENT) {
    if (millis() > timerFireMillis) {
      debugMsg("Debounce delay expired");
      moveToIdleState();                  // Release debounce finished
      accumButtonValue = newButtonValue;  // Reset accumButtonValue; there could still be a button held
    }
  } else {
    Serial.println("Bad state");
  }

  // Update current key/button
  keyValue = newKeyValue;
  buttonValue = newButtonValue;

  // Show status on LED
  if (keyValue == 0 && buttonValue == 0) {
    digitalWrite(LED, 0);
  } else {
    digitalWrite(LED, 1);
  }
}

// Send over USB the mouse button stored in accumButtonValue
void mouseButtonDown(int accumButtonValue) {
  // Special handling for mouse buttons without keyset.
  if (accumButtonValue == BUTTON_M) {  // Middle button
    // Should be <CD>, but pass mouse button through
    debugMsg("Sending middle button");
    Mouse.set_buttons(0, 1, 0);
  } else if (accumButtonValue == BUTTON_L) {  // Left button
                                              // Should be <BC>, but pass mouse button through
    debugMsg("Sending left button");
    Mouse.set_buttons(1, 0, 0);
  } else if (accumButtonValue == BUTTON_R) {  // Right button
    // Should be <OK>, but pass mouse button through
    debugMsg("Sending right button");
    Mouse.set_buttons(0, 0, 1);
  } else {
#ifdef DEBUG
    Serial.print("*** Bad mouse button send: ");
    Serial.println(accumButtonValue, DEC);
#endif
  }
}

// Send a mouse buttons up over USB
void mouseButtonUp() {
  Mouse.set_buttons(0, 0, 0);
}

// Sends the appropriate key over USB based on accumKeyValue and accumButtonValue
// This is where the mapping logic is implemented, using the three tables keys0, keys1, and keys2
void sendKey() {
#ifdef DEBUG
  Serial.print("Send: keyset buttons: ");
  Serial.print(digitalRead(K1) ^ 1, DEC);
  Serial.print(digitalRead(K2) ^ 1, DEC);
  Serial.print(digitalRead(K3) ^ 1, DEC);
  Serial.print(digitalRead(K4) ^ 1, DEC);
  Serial.print(digitalRead(K5) ^ 1, DEC);
  Serial.print(", accumKeyValue value: x");
  Serial.print(accumKeyValue, HEX);
  Serial.print(" accumButtonValue: ");
  Serial.println(accumButtonValue, DEC);
#endif
  if (accumButtonValue == 0) {
    // USB keyboard emulation info: https://www.pjrc.com/teensy/td_keyboard.html
    send(keys0[accumKeyValue]);
  } else if (accumButtonValue == BUTTON_M) {
    debugMsg("Sending with middle button: caps");
    send(keys1[accumKeyValue]);
  } else if (accumButtonValue == BUTTON_L) {
    debugMsg("Sending with right button: special key");
    send(keys2[accumKeyValue]);
  } else if (accumButtonValue == BUTTON_R) {
    // "Has no meaning with keyset input"
    debugMsg("Ignoring right + keyset");
  } else if (accumButtonValue == (BUTTON_L | BUTTON_M)) {
    if (accumKeyValue == 0) {
      send(KEY_W | CTRL);
    } else {
      // "Take each keyset code as a lowercase viewspec"
      debugMsg("Sending ALT + char");
      send(keys0[accumKeyValue] | ALT);
    }
  } else if (accumButtonValue == (BUTTON_M | BUTTON_R)) {
    if (accumKeyValue == 0) {
      send(KEY_B | CTRL);  // Should be <RC> but send ^B
    } else {
      debugMsg("Sending CTRL + char");
      send(keys0[accumKeyValue] | CTRL);  // Control character
    }
  } else if (accumButtonValue == (BUTTON_L | BUTTON_R)) {
    if (accumKeyValue == 0) {
      send(KEY_ESC);  // Send Escape
    } else {
      // "Search for marker named by keyset combination"
      // Ignore for now.
      debugMsg("Ignoring left/right + keyset");
    }
  } else if (accumButtonValue == (BUTTON_L | BUTTON_M | BUTTON_R)) {
    // "Take each keyset code as a capital viewspec."
    debugMsg("Sending SHIFT + ALT + char");
    send(keys1[accumKeyValue] | ALT);
  }
}