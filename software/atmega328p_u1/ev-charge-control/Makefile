.PHONY: clean all dist

$(shell mkdir -p dist >/dev/null)
$(shell mkdir -p build >/dev/null)

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

all: dist/atmega328p_u1.hex
	@avr-size --mcu=atmega328p --format=avr dist/atmega328p_u1.elf

dist/atmega328p_u1.hex: dist/atmega328p_u1.elf
	avr-objcopy -O ihex $< $@

dist/atmega328p_u1.elf: build/main.o build/sys.o build/mon.o build/app.o
	avr-gcc -o $@ -mmcu=atmega328p build/main.o build/sys.o build/mon.o build/app.o

build/main.o: src/main.cpp src/global.h src/sys.hpp src/app.hpp
	avr-g++ -o $@ -mmcu=atmega328p -Os -c -DCOMPILE $<

build/sys.o: src/sys.cpp src/global.h src/mon.hpp src/app.hpp src/sys.hpp
	avr-g++ -o $@ -mmcu=atmega328p -Os -c -DCOMPILE $<

build/mon.o: src/mon.cpp src/global.h src/global.h src/mon.hpp src/app.hpp src/sys.hpp
	avr-g++ -o $@ -mmcu=atmega328p -Os -c -DCOMPILE $<

build/app.o: src/app.cpp src/global.h src/global.h src/mon.hpp src/app.hpp src/sys.hpp
	avr-g++ -o $@ -mmcu=atmega328p -Os -c -DCOMPILE $<

u1: dist/atmega328p_u1.hex
	avrdude -c usbasp -p atmega328p -e -U flash:w:$<:i

u1bl: dist/atmega328p_u1.hex ../bootloader_u1/bootloader_atmega328p_57600_16MHz.hex
	avrdude -c usbasp -p atmega328p -e -U flash:w:$<:i
	avrdude -c usbasp -p atmega328p -D -U flash:w:../bootloader_u1/bootloader_atmega328p_57600_16MHz.hex:i

easy: dist/atmega328p_u1.hex
	nohup /bin/bash htl ep dist/atmega328p_u1.hex atmega328p 57600 /dev/ttyUSB0 &

clean:
	-@rm -r dist
	-@rm -r build
