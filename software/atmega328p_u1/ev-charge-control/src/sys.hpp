#ifndef U1_SYS_HPP_
#define U1_SYS_HPP_

#include "global.h"

#define F_CPU 16000000UL
#ifndef __AVR_ATmega328P__
    #define __AVR_ATmega328P__
#endif

#if GLOBAL_U1_SYS_UART0_RECBUFSIZE > 255
  #error "Error: GLOBAL_U1_SYS_UART0_RECBUFSIZE value over maximum (255)"
#endif

#if !defined(COMPILE)
    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned long uint32_t;
    typedef signed char int8_t;
    typedef signed short int16_t;
    typedef signed long int32_t;
    #define pgm_read_byte(address_short) address_short
    #define boot_page_erase(address_short) address_short
    #define boot_page_write(address_short) address_short
    #define boot_page_fill(address_short, value) address_short
    #define _FDEV_EOF -1
    // #define FDEV_SETUP_STREAM(handler, mem, type) {}
    // #define fdev_setup_stream(stream, put, get, rwflag)
    // #define fdev_setup_stream(stream, put, get)
    #include <stdio.h>
    #include <string.h>
    // #include <ctype.h>
    #include <stdlib.h>
    #include <avr/io.h>
    #include <avr/interrupt.h>
    #include <avr/wdt.h>
    #include <util/delay.h>
    #include <avr/pgmspace.h>
    #include <avr/eeprom.h>
    #undef FDEV_SETUP_STREAM
    #define FDEV_SETUP_STREAM(p, g, f) { 0, 0, f, 0, 0, p, g, 0 }
    #define FDEV_RETURN (char *)

#else 
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <avr/io.h>
    #include <avr/interrupt.h>
    #include <avr/wdt.h>
    #include <avr/pgmspace.h>
    #include <avr/eeprom.h>
    #include <util/delay.h>    
    #undef FDEV_SETUP_STREAM
    #define FDEV_SETUP_STREAM(p, g, f) { 0, 0, f, 0, 0, p, g, 0 }
    #define FDEV_RETURN

#endif

#define popSREG() __asm__("pop r0\n\t" "out 0x3f,r0\n\t");
#define pushSREGAndCli() __asm__("in r0,0x3f\n\t" "cli\n\t" "push r0\n\t");



// declarations

namespace u1_sys {

    typedef uint8_t Event_t;

    struct Errcnt {
        uint8_t task;
    };

    struct Uart {
        uint8_t rpos_u8;
        uint8_t wpos_u8;
        uint8_t errcnt_u8;
        uint8_t rbuffer_u8[GLOBAL_UART0_RECBUFSIZE];
    };

    struct Sys_Lcd {
        int8_t   status;  // 0=not initialized, 1=ready, <0->error
        uint8_t  data;
    };


    struct Sys {
        struct Errcnt errcnt;
        Event_t eventFlag;
        struct Uart uart;
        struct Sys_Lcd lcd;
    };

    struct MonCmdInfo {
        PGM_P pInfo;
        int8_t (*pFunction)(uint8_t, char *[]);
    };


    
    // defines
    #define LCD_PULSE_LENGTH 15
    #define LCD_SET_RS   PORTB |=  (1 << PB5); // Signal RS=1
    #define LCD_CLR_RS   PORTB &= ~(1 << PB5); // Signal RS=0
    #define LCD_SET_RW   PORTB |=  (1 << PB4); // Signal RW=1
    #define LCD_CLR_RW   PORTB &= ~(1 << PB4); // Signal RW=0
    #define LCD_SET_E    PORTD |=  (1 << PD7); // Signal E=1 
    #define LCD_CLR_E    PORTD &= ~(1 << PD7); // Signal E=0 

    #define LCD_CMD_DISPLAY_CLEAR  0x01  // Display clear
    #define LCD_CMD_CURSOR_HOME    0x02  // Move cursor digit 1
    #define LCD_CMD_SET_ENTRY_MODE 0x04  // Entry Mode Set
    #define LCD_CMD_DISPLAY_ON_OFF 0x08  // Display on/off
    #define LCD_CMD_SHIFT          0x10  // Display shift
    #define LCD_CMD_SET_FUNCTION   0x20  // 4/8 Bits...
    #define LCD_CMD_SET_CGRAM_ADDR 0x40  // Character Generator ROM
    #define LCD_CMD_SET_DDRAM_ADDR 0x80  // Display Data RAM
    #define LCD_BUSY_FLAG          0x80



    // FLAG_SREG_I must have same position as I-Bit in Status-Register!!
    #define FLAG_SREG_I          0x80

    extern struct Sys sys;

    // functions

    void init ();
    void main ();

    uint8_t inc8BitCnt (uint8_t count);
    uint16_t inc16BitCnt (uint16_t count);

    Event_t setEvent (Event_t event);
    Event_t clearEvent (Event_t event);
    Event_t isEventPending (Event_t event);

    void newline ();
    void printString_P (PGM_P str);
    void puts_P (PGM_P str);
    void printBin (uint8_t value, char sep);
    void printHexBin8 (uint8_t value);
    void printHexBin16 (uint16_t value);
    int16_t readArgument (char *str, int16_t max);
    int16_t getByte (char typ, uint16_t add);

    uint8_t uart_available ();
    int16_t uart_getBufferByte (uint8_t pos);
    void uart_flush ();

    #ifdef GLOBAL_U1_LCD
        void lcd_init ();
        void lcd_setRegister (uint8_t cmd);
        void lcd_setDRAddr (uint8_t address);
        void lcd_waitOnReady ();
        uint8_t lcd_isReady (uint16_t us);
        void lcd_setDisplayOn ();
        void lcd_setDisplayOff ();
        void lcd_clear ();
        void lcd_setCursorPosition (uint8_t rowIndex, uint8_t columnIndex);
        void lcd_putchar (int character);
        void lcd_putString (const char * str);
    #endif

    int16_t cosX256 (uint8_t x);
    int16_t sinX256 (uint8_t x);
    void enablePWM (uint8_t enable);
    void setK1 (uint8_t on);
    void setK2 (uint8_t on);
    void setLedD3 (uint8_t on);
    void toggleLedD3 ();
    uint8_t getSw1Value ();


}

#endif // U1_SYS_HPP_
