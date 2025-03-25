# Interfacing a vintage keyset to USB

In 1968, Douglas Engelbart gave "The Mother of All Demos", introducing many of the features of modern computing:
the mouse, hypertext, shared documents, and a graphical user interface.
One of his inventions—the keyset—didn't become popular.
The 5-finger keyset lets you type without moving your hand, entering characters by pressing multiple keys at a time as a chord.

Christina Englebart, his daughter, loaned me a vintage keyset.
I constructed an interface to connect the keyset to USB, using a Teensy microcontroller.
Now I can type with the keyset, using the mouse buttons to select upper case and special characters.

[Link to video](https://youtu.be/DpshKBKt_os?si=hEbSdSmSjGin9laS)

## Using the keyset

The following sheet shows the characters corresponding to each keyset and mouse button combination.

<img src="https://github.com/shirriff/keyset-to-usb-interface/blob/main/keyset-sheet-front.jpg" height="500" />

## The interface

I used a [Teensy 3.6](https://www.pjrc.com/teensy/index.html) board, which is similar to an Arduino but can act both as a USB host and a USB device. 
The Teensy emulates a USB keyboard and sends the appropriate character when the keyset is operated.

The tricky part is that the keyset is designed to uses three mouse buttons to select upper case, control characters, and other special functions.
Thus, the Teensy also reads a USB mouse to get the button states. To make the mouse work as a mouse, the Teensy re-sends mouse movements and
button presses over the USB interface, if the button presses aren't in combination with the keyset.

The keyset has its five buttons wired to a DB-25 connector so it is straightforward to connect to five inputs on the Teensy.
One complication is that the keyset apparently has a 1.5 KΩ between the leftmost button and ground, maybe to indicate that the device is
plugged in. To counteract this and allow the Teensy to read the pin, I connected a 1 KΩ pullup resistor.

## Blog post

For more information on the keyset, see my blog post [A USB interface to the "Mother of All Demos" keyset](https://www.righto.com/2025/03/mother-of-all-demos-usb-keyset-interface.html)

## 3D replica

Eric Schlaepfer has plans for an exact replica [here](https://github.com/schlae/engelbart-keyset).
