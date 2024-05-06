#!/bin/bash
avrdude -p m168p -c usbasp -P usb -U eeprom:r:"../_build/kitchenTimer.ino.eep":i &&
avrdude -p m168p -c usbasp -P usb -U flash:w:"../_build/kitchenTimer.ino.hex":a &&
avrdude -p m168p -c usbasp -P usb -U eeprom:w:"../_build/kitchenTimer.ino.eep":a
