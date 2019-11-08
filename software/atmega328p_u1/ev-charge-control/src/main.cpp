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
const char MAIN_WELCOME[] = "\n\rEV charger MET16 V0.3.1";
const char MAIN_DATE[] = __DATE__;
const char MAIN_TIME[] = __TIME__;
const char MAIN_HELP[] = "\r\n";


int main () {
    MCUSR = 0;
    wdt_disable();
    
    u1_sys::init();
    u1_mon::init();
    u1_app::init();
    printf("%s %s %s %s", MAIN_WELCOME, MAIN_DATE, MAIN_TIME, MAIN_HELP);
    printf("\r\n");

    #ifdef GLOBAL_U1_LCD
    printf("LCD ");
    if (u1_sys::sys.lcd.status == 1) {
        printf("detected and ready to use\n");
        u1_sys::lcd_putString("EVC V0.3.1 ");
        u1_sys::lcd_putString(__TIME__);
    } else {
        printf("not ready (status=%d)\n", u1_sys::sys.lcd.status);
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
