# Code for the interface

This directory has the code for use with the Arduino IDE.
I used a [Teensy 3.6](https://www.pjrc.com/store/teensy36.html) because I had one handy, but a [Teensy 4.1](https://www.pjrc.com/store/teensy41.html)
would probably make more sense if you're building this from scratch.
The important feature is that the Teensy can act as a USB client to simulate the keyboard and also as a USB host to
read the buttons from a USB mouse.

The code contains three tables that define the mapping from keyset presses to USB keycodes, matching the table in the
parent README.
The first table (keys0) defines the keycodes when the keyset is used without the mouse buttons: mostly lower-case letters.
The second table (keys1) defines the keycodes with the middle mouse button: mostly upper-case letters.
The third table (keys2) defines the keycodes with the left mouse button: special characters and numbers.
Note that the USB keycodes have nothing to do with ASCII. They map to physical keys, so, for example,
an exclamation point is the "1" key and the SHIFT modifier.

### Unusual key mappings

Using the keyset with the middle and right mouse buttons generates a control character. I'm simply sending the
keycode along with the CTRL key modifier. This works with letters, but doesn't make much sense for control-comma, for instance.

Using the keyset with the left and middle buttons generates a "lowercase viewspec" according to the keyset sheet.
I'm sending the original character
with the GUI modifier, which corresponds to the Command (⌘) key on a MacBook. I'm not sure what happens if you
send this to a PC.

Using the keyset with all three buttons generates a "capital viewspec". I'm sending the original character with
the SHIFT modifier and the GUI modifier.

Using the keyset with the right button has no defined meaning, so I ignore this.
Using the keyset with the left and right buttons is defined as "search for marker". I'm ignoring this.

### Unusual button presses

The keyset sheet defines special actions for button presses without the keyset. I'm sending single button
presses unchanged, because otherwise the mouse is unusable with normal applications.
Pressing left and middle buttons is supposed to send a <BW>, but I'm sending control-W.
Pressing middle and right buttons is supposed to send a <RC>, but I'm sending control-B.
Pressing left and right buttons is supposed to send <ESC>, so I'm sending Escape.
There is no action specified for all three buttons at once, so I don't send anything.


## The state machine for debouncing

Debouncing the keyset is trickier than I expected. Moreover, the keyset is used in combination with mouse buttons, but the keys and buttons are unlikely to be
pressed simultaneously. Finally, I wanted the mouse buttons to be usable as mouse buttons independent of the keyset. In particular, handling a double-click
limits the possibility of using a time delay. To handle these requirements, I
came up with the state machine below.

<img src="https://raw.githubusercontent.com/shirriff/keyset-to-usb-interface/refs/heads/main/keyset/state-diagram.jpg" height="300" />

The upper states handle mouse buttons when the keyset is not active. Because a mouse button may be followed by a keyset action, there is a time delay
between the MOUSE_ACTIVITY and the MOUSE_HELD state. If the button is immediately released, the click is sent exiting MOUSE_ACTIVITY.
But if the button is held, the button press isn't sent until the timer expires and MOUSE_HELD is reached.

The lower states handle keyset activity as well as generating a keyset-associated character when multiple mouse buttons are pressed.
The idea is to accumulate all the keyset and mouse buttons as they are pressed. Once nothing more has been pressed for a time interval and all keys are
released, the appropriate key character is sent over USB and the state machine moves to KEY_SENT.
(The time delay is necessary since a keyset press can bounce, resulting in an immediate release. The accumulation is used because keys probably won't
be released simultaneously, so you want to keep track of the most keys that were pressed. Sending on all-keys-up matches the behavior of the original
keyset implementation.)
After a short delay, the state moves from KEY_SENT to IDLE; this delay ensures that a bounce on key release doesn't trigger a new character.

### Some notes

1. Even a short keypress will trigger a character (after the timer expires). I tried other approaches that would drop short keypresses, which
is undesirable.

2. The dashed line is an undesirable case. If you hold the mouse button down for a while and then use the keyset, we want to treat it
as button+keyset, but we've already sent the button press over USB. All we can do at that point is send a button-up and continue, so there is an unwanted
button click.

3. To move to KEY_SENT, I require all the keyset keys to be up, but allow the mouse button to remain down. I don't know what the
"authentic" behavior is, but this behavior is useful if you're typing multiple numbers or upper-case letters. But it requires care in the IDLE state to
avoid treating the held-over mouse button as a click.
