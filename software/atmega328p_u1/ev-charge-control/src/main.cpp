//***********************************************************************
// EV Charger
// ----------------------------------------------------------------------
// Description:
// 
// ----------------------------------------------------------------------
// Created: Oct 30, 2019 (C) Manfred.Steiner@gmx.at
// ----------------------------------------------------------------------
// Program with ...
// mkII:   avrdude -c avrisp2 -p m328p -P usb -U flash:w:arduino.elf.hex:i
// STK500: avrdude -c stk500v2 -p m328p -P /dev/ttyUSBx -U flash:w:arduino.elf.hex:i
// USBASP: avrdude -c usbasp -p m328p -U flash:w:arduino.elf.hex:i
//***********************************************************************

#include "global.h"
#include "sys.hpp"
#include "mon.hpp"
#include "app.hpp"

// defines
// ...

// declarations and definations
// ...

// constants located in program flash and SRAM
const char MAIN_WELCOME[] PROGMEM = "\n\rEV charger MET16 V0.3.2";
const char MAIN_DATE[] PROGMEM = __DATE__;
const char MAIN_TIME[] PROGMEM = __TIME__;
const char MAIN_HELP[] PROGMEM = "\r\n";
const char MAIN_LCD_OK[] PROGMEM = "detected and ready to use\r\n";
const char MAIN_LCD_ERR[] PROGMEM = "not ready, status=";


int main () {
    MCUSR = 0;
    wdt_disable();
    
    u1_sys::init();
    u1_mon::init();
    u1_app::init();
    u1_sys::printString_P(MAIN_WELCOME); printf(" ");
    u1_sys::printString_P(MAIN_DATE); printf(" ");
    u1_sys::printString_P(MAIN_TIME); printf("\r\n");
    u1_sys::printString_P(MAIN_HELP);

    // printf("%s %s %s %s", MAIN_WELCOME, MAIN_DATE, MAIN_TIME, MAIN_HELP);
    printf("\r\n");

    #ifdef GLOBAL_U1_LCD
    printf("LCD ");
    if (u1_sys::sys.lcd.status == 1) {
        u1_sys::printString_P(MAIN_LCD_OK);
        u1_sys::lcd_putString("EVC V0.3.2 ");
        u1_sys::lcd_putString(__TIME__);
    } else {
        u1_sys::printString_P(MAIN_LCD_ERR);
        printf("%d\r\n", u1_sys::sys.lcd.status);
    }
    #endif // GLOBAL_U1_LCD

    // enable interrupt system
    sei();

    while (1) {
        u1_sys::main();
        u1_mon::main();
        u1_app::main();
    }
    return 0;
}
