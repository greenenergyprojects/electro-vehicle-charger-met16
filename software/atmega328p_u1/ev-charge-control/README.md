# Software *ev-charge-control*

This software is written for the Arduino nano (Atmega 328P).

* Ready to use Intel-Hex file: [released/arduino.elf.hex](released/atmega328p_u1.hex).
* IDE: Visual Studio Code with C/C++ addons
* Use `make` (inside shell) to rebuild the project. The result is written to subdirectory *dist*.

## Flashing the software into microcontroller (Arduino Nano)

Use a Atmel ISP programming device and a programming software to program the hex file into the microcontroller flash.

For example:
* Programmer device [ebay USB-ISP-USBASP-Programmer](https://www.ebay.com/itm/USB-ISP-USBASP-Programmer-for-ATMEL-51-AVR-Programmer/181927074757?hash=item2a5bb2dbc5:g:9dIAAOSwhcJWQCim)
* Programming software [avrdude](https://www.nongnu.org/avrdude/)

Shell commands on Linux shell bash:

```bash
$ avrdude -c usbasp -p atmega328p -U lock:w:0x2f:m -U efuse:w:0x06:m -U hfuse:w:0xda:m -U lfuse:w:0xde:m
$ avrdude -c usbasp -p atmega328p -e -U flash:w:released/atmega328p_u1.hex:i
```

**Alternative way via Bootloader**

Program Bootloader into device:

```bash
$ avrdude -c usbasp -p atmega328p -U lock:w:0x2f:m -U efuse:w:0x06:m -U hfuse:w:0xda:m -U lfuse:w:0xde:m
$ avrdude -c usbasp -p atmega328p -e -U flash:w:../bootloader_u1/bootloader_atmega328p_57600_16MHz.hex:i
```

User [Easyprogrammer](../easyprogrammer_2.26.jar) to program the hex-file [released/atmega328p_u1.hex](released/atmega328p_u1.hex) into device.
You can use the Mini-USB cable or a USB/UART converter via connector J8.


## Usage of the software

?






