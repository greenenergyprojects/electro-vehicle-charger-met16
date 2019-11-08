#include "global.h"
#include "mon.hpp"
#include "app.hpp"
#include "sys.hpp"


// defines


namespace u1_sys {

    // declarations and definitions

    #ifdef GLOBAL_MONITOR

        #define MONITOR_FLAG_LINEMODE 0x01
        #define MONITOR_FLAG_LINE     0x02
        #define MONITOR_FLAG_CONT     0x04

        #define MONITOR_CTRL_C        0x03
        #define MONITOR_CTRL_X        0x18
        #define MONITOR_CTRL_Y        0x19

        struct Monitor {
            uint8_t flags;
            uint8_t lineIndex;
            uint8_t cursorPos;
            char cmdLine[40];
        };
        struct Monitor mon;

        void mon_main ();
        int8_t cmd_help (uint8_t argc, char *argv[]);
        int8_t cmd_sinfo (uint8_t argc, char *argv[]);
        int8_t cmd_hexdump (uint8_t argc, char *argv[]);
        int8_t cmd_setmem (uint8_t argc, char *argv[]);

        // globale Konstante im Flash
        const char PMEM_LINESTART[] PROGMEM = "\n\r>";
        const char PMEM_ERR0[] PROGMEM = "Error ";
        const char PMEM_ERR1[] PROGMEM = "Error: Unknown command\n\r";
        const char PMEM_ERR2[] PROGMEM = " -> Syntax error\n\rUsage: ";
        const char PMEM_CMD_HELP[] PROGMEM = "help\0List of all commands\0help";
        const char PMEM_CMD_SINFO[] PROGMEM = "sinfo\0Systeminformation\0sinfo";
        #ifdef GLOBAL_MONITOR_CMD_HEXDUMP
            const char PMEM_CMD_HEXDUMP[] PROGMEM = "hexdump\0Hexdump of memory content\0hexdump {f|s|e} start [length]";
        #endif // GLOBAL_MONITOR_CMD_HEXDUMP
        #ifdef GLOBAL_MONITOR_CMD_SETMEM
            const char PMEM_CMD_SETMEM[] PROGMEM = "setmem\0Write bytes into SRAM/EEPROM\0setmem [s|e] address value";
        #endif // GLOBAL_MONITOR_CMD_SETMEM

        const struct MonCmdInfo PMEMSTR_CMDS[] PROGMEM = {
              { PMEM_CMD_HELP, cmd_help }
            , { PMEM_CMD_SINFO, cmd_sinfo }
            #ifdef GLOBAL_MONITOR_CMD_HEXDUMP
            , { PMEM_CMD_HEXDUMP, cmd_hexdump }
            #endif // GLOBAL_MONITOR_CMD_HEXDUMP
            #ifdef GLOBAL_MONITOR_CMD_SETMEM
            , { PMEM_CMD_SETMEM, cmd_setmem }
            #endif // GLOBAL_MONITOR_CMD_SETMEM
        };

    #endif // GLOBAL_MONITOR

    int uart_putch (char c, FILE *f);
    int uart_getch (FILE *f);

    static FILE mystdout = FDEV_SETUP_STREAM(FDEV_RETURN uart_putch, NULL, _FDEV_SETUP_WRITE);
    static FILE mystdin  = FDEV_SETUP_STREAM(NULL, FDEV_RETURN uart_getch, _FDEV_SETUP_READ);

    struct Sys sys;

    // ---------------------------------------------------------------------



    void init () {
        memset((void *)&sys, 0, sizeof(sys));
        _delay_ms(10);

        // Life LED PB5 (Arduino Nano)
        // DDRB |= (1 << PB5); // not available, Port used for LCD
        
        // Timer for state-machine
        OCR2A  = (F_CPU+4)/8/10000-1;
        TCCR2A = (1 << WGM01);
        TCCR2B = (1 << CS01);
        TIMSK2 = (1 << OCIE0A);
        TIFR2  = (1 << OCF0A);
        
        // Timer for ADC
        TCCR1A = 0;
        TCCR1B = (1 << WGM12) | (1 << CS10); // CTC, f = 16MHz
        OCR1A = 1600; // CTC -> 100us (1600)
        OCR1B = 1600; // ADC Trigger -> 100us (1600)
        TIMSK1 = (1 << OCIE1B);
        
        // Timer for 1kHz control pilot signal
        // TCCR0A = (1 << COM0A1) | (0 << COM0A0) | (1 << WGM01) | (1 << WGM00);
        // TCCR0B = (1 << CS01) | (1 << CS00);
        // OCR0A = 0xff;
        
        // Uart for FTDI connector J8
        UBRR0L = (F_CPU / GLOBAL_UART0_BITRATE + 4)/8 - 1;
        UBRR0H = 0x00;
        UCSR0A = (1<<U2X0);
        UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
        UCSR0B = (1 << RXCIE0) | (1 << TXEN0) | (1 << RXEN0);

        #ifdef GLOBAL_U1_LCD  
        PORTB  &= ~0x3f;        // RS, RW, D7, D6, D5, D4
        PORTD  &= ~(1 << PD7);  // E
        DDRB   |=  0x3f;        // RS, RW, D7, D6, D5, D4
        DDRD   |= (1 << PD7);   // E
        lcd_init();
        #endif

        // connect libc functions printf(), gets()... to UART
        // fdevopen(sys_uart_putch, sys_uart_getch);
        stdout = &mystdout;
        stdin  = &mystdin;
        
        // settings for ev-charger-met
        DDRC |= (1 << PC3); // LED D3 (PC3) - Life LED (green)
        DDRC |= (1 << PC4); // LED D7 - K1 relais (LED blue) (Nano A4)
        DDRC |= (1 << PC5); // LED D2 - K2 SSR relais (LED red) (Nano A5)
        DDRD &= ~(0x3c); // SW1 (rotary switch 0-9) PD2/PD3/PD4/PD5
        PORTD |= 0x3c;   // pull up on rotary switch
        
        // ADC: ADC0=Current 0A..16A, ADC1=230V Voltage, 
        //      ADC2=-12V, ADC6=Voltage CP, ADC7=+12V
        ADMUX = (1 << REFS1) | (1 << REFS0) | (1 << ADLAR); // Vref = internal 1.1V
        ADCSRA = (1 << ADEN) | (1 << ADATE) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1);
        ADCSRB = (1 << ADTS2) | (1 << ADTS0); // ADC Trigger: Timer 1 Compare Match B
    }


    void main () {
        #ifdef GLOBAL_MONITOR
            mon_main();
        #endif
    }


    // ----------------------------------------------------------------------------

    uint8_t inc8BitCnt (uint8_t count) {
        return count<0xff ? count+1 : count;
    }


    uint16_t inc16BitCnt (uint16_t count) {
        return count<0xffff ? count+1 : count;
    }

    

    // ****************************************************************************
    // Event Handling
    // ****************************************************************************

    Event_t setEvent (Event_t event) {
        uint8_t eventIsPending = 0;
        pushSREGAndCli();
        if (sys.eventFlag & event) {
            eventIsPending = 1;
        }
        sys.eventFlag |= event;
        popSREG();

        return eventIsPending;
    }


    Event_t clearEvent (Event_t event) {
        uint8_t eventIsPending = 0;
        pushSREGAndCli();
        if (sys.eventFlag & event) {
            eventIsPending = 1;
        }
        sys.eventFlag &= ~event;
        popSREG();

        return eventIsPending;
    }


    Event_t isEventPending (Event_t event) {
        return (sys.eventFlag & event) != 0;
    }

    // ****************************************************************************
    // Monitor Functions
    // ****************************************************************************

    void newline () {
        printf("\n\r");
    }

    void printString_P (PGM_P str) {
        char c;
        while (1) {
            memcpy_P(&c, str++, 1);
            if (!c) { 
                break;
            }
            putchar(c);
        }
    }

    void puts_P (PGM_P str) {
        printString_P(str);
        newline();
    }

    void printBin (uint8_t value, char sep) {
        uint8_t i;
        for (i = 0; i < 8; i++, value <<= 1) {
            putchar(value & 0x80 ? '1' : '0');
            if (i == 3 && sep) {
                putchar(sep);
            }
        } 
    }

    void printHexBin8 (uint8_t value) {
        printf("0x%02x (", value);
        printBin(value, ' ');
        putchar(')');
    }

    void printHexBin16 (uint16_t value) {
        printf("0x%04x (", value);
        printBin(value >> 8, ' ');
        putchar(' ');
        printBin(value & 0xff, ' ');
        putchar(')');
    }

    int16_t readArgument (char *str, int16_t max) {
        int16_t value;
        if (str[0] == '0' && str[1] == 'x') {
            value = strtol(str, NULL, 16);
        } else {
            value = strtol(str, NULL, 10);
        }
        if (value > max) {
            return -1;
        }
        return value;
    }

    int16_t getByte (char typ, uint16_t add) {
        uint8_t value = 0;
        switch (typ) {
            case 'f': { // flash
                if (add > FLASHEND) {
                    return -1;
                }
                memcpy_P(&value, (PGM_P) add, 1);
                break;
            }

            case 's': { // SRAM
                if (add > RAMEND ) { 
                    return -1;
                }
                value = *((uint8_t *)add);
                break;
            }

            case 'e': { // EEPROM
                if (add > E2END) { 
                    return -1;
                }
                value = eeprom_read_byte ((uint8_t *)add);
                break;
            }

            default: {
                return -1;
            }
        }

        return value;
    }


    // ****************************************************************************
    // UART Functions
    // ****************************************************************************

    int uart_getch (FILE *f) {
        if (f != stdin) {
            return _FDEV_EOF;
        }
        if (sys.uart.wpos_u8 == sys.uart.rpos_u8) {
            return _FDEV_EOF;
        }
        uint8_t c = sys.uart.rbuffer_u8[sys.uart.rpos_u8++];
        if (sys.uart.rpos_u8 >= GLOBAL_UART0_RECBUFSIZE) {
            sys.uart.rpos_u8 = 0;
        }
        return (int) c;
    }


    int uart_putch (char c, FILE *f) {
        if (f != stdout) {
            return _FDEV_EOF;
        }
        while (!(UCSR0A & (1<<UDRE0))) {
        }
        UDR0 = c;
        return (int)c;
    }

    uint8_t uart_available () {
        return sys.uart.wpos_u8 >= sys.uart.rpos_u8
           ? sys.uart.wpos_u8 - sys.uart.rpos_u8
           : ((int16_t)sys.uart.wpos_u8) + GLOBAL_UART0_RECBUFSIZE - sys.uart.rpos_u8;
    }

    int16_t uart_getBufferByte (uint8_t pos) {
        int16_t value;
        pushSREGAndCli();

        if (pos >= uart_available()) {
            value = -1;
        } else {
            uint8_t bufpos = sys.uart.rpos_u8 + pos;
            if (bufpos >= GLOBAL_UART0_RECBUFSIZE) {
                bufpos -= GLOBAL_UART0_RECBUFSIZE;
                value = sys.uart.rbuffer_u8[bufpos];
            }
        }

        popSREG();
        return value;
    }

    void uart_flush () {
        pushSREGAndCli();
        while ((UCSR0A & (1 << RXC0))) {
            sys.uart.rbuffer_u8[0] = UDR0;
        }
        sys.uart.rpos_u8 = 0;
        sys.uart.wpos_u8 = 0;
        sys.uart.errcnt_u8 = 0;
        popSREG();
    }

    // ****************************************************************************
    // UART Functions
    // ****************************************************************************

    #ifdef GLOBAL_U1_LCD

    void lcd_init () {
        uint8_t i;

        //_delay_ms(16);
        sys.lcd.status = 0;
        for (i = 0; i < 4; i++) {
            lcd_setRegister(LCD_CMD_SET_FUNCTION | 0x08); // 4 Bit, 2 Zeilen, 5x7
            if (i == 0) {
                _delay_ms(5);
             } else {
                _delay_us(100);
             }
        }

        lcd_setRegister(LCD_CMD_DISPLAY_ON_OFF | 0x04); // display on, cursor off
        if (!lcd_isReady(50)) {
            sys.lcd.status = -1;
            return;
        }

        lcd_setRegister(LCD_CMD_DISPLAY_ON_OFF | 0x04); // display on, cursor off
        if (!lcd_isReady(50)) {
            sys.lcd.status = -3;
            return;
        }

        lcd_setRegister(LCD_CMD_DISPLAY_CLEAR);
        if (!lcd_isReady(1200)) {
            sys.lcd.status = -4;
            return;
        }

        sys.lcd.status = 1;
    }


    void lcd_setRegister (uint8_t cmd) {
        LCD_CLR_E;
        LCD_CLR_RW;
        LCD_CLR_RS;
        DDRB |= 0x0f;  // LCD-Data (D7,D6,D5,D4) to Output
        
        PORTB &= ~0x0f;
        PORTB |= cmd >> 4; // send High-Byte
        LCD_SET_E;
        _delay_us(LCD_PULSE_LENGTH);
        LCD_CLR_E;
        _delay_us(1);

        PORTB &= ~0x0f;
        PORTB |= cmd & 0x0f; // send Low-Byte
        LCD_SET_E;
        _delay_us(LCD_PULSE_LENGTH);
        LCD_CLR_E;
        _delay_us(1);
    }

    void lcd_setDRAddr (uint8_t address) {
        lcd_waitOnReady();
        lcd_setRegister(LCD_CMD_SET_DDRAM_ADDR | address);
        lcd_waitOnReady();
    }

    void lcd_waitOnReady () {
        if (lcd_isReady(50) == 0) {
            sys.lcd.status = -6;
        }
    }


    uint8_t lcd_isReady (uint16_t us) {
        if (sys.lcd.status < 0) {
            return 0;
        }

        uint8_t busy;
        PORTB = 0xff;
        DDRB &= ~0x0f;  // LCD-Data (D7,D6,D5,D4) to Input
        do {
            sys.lcd.data = 0;
            LCD_SET_RW;
            LCD_CLR_RS;
            
            _delay_us(1);
            LCD_SET_E;
            _delay_us(LCD_PULSE_LENGTH);
            sys.lcd.data = PINB << 4; // High Byte
            LCD_CLR_E;
            
            _delay_us(1);
            LCD_SET_E;
            _delay_us(LCD_PULSE_LENGTH);
            sys.lcd.data |= (PINB & 0x0f); // Low Byte
            
            LCD_CLR_E;
            _delay_us(1);
            LCD_CLR_RW;
            
            busy = sys.lcd.data & LCD_BUSY_FLAG;
            us = (us >= 11) ? us - 11 : 0;
        } while (us > 0 && busy);

        if (sys.lcd.status == 1 && busy) {
            sys.lcd.status = -5;
        }

        DDRB |= 0x0f;  // LCD-Data (D7,D6,D5,D4) to Output

        return busy == 0;   
    }


    void lcd_setDisplayOn () {
        lcd_waitOnReady();
        lcd_setRegister(LCD_CMD_DISPLAY_ON_OFF | 0x04); // display on
        lcd_waitOnReady();
    }

    void lcd_setDisplayOff () {
        lcd_waitOnReady();
        lcd_setRegister(LCD_CMD_DISPLAY_ON_OFF); // display off
        lcd_waitOnReady();
    }

    void lcd_clear () {
        lcd_waitOnReady();
        lcd_setRegister(LCD_CMD_DISPLAY_CLEAR);
        while (!lcd_isReady(1200)) {
        }
    }

    void lcd_setCursorPosition (uint8_t rowIndex, uint8_t columnIndex) {
        if (sys.lcd.status != 1 || rowIndex > 1) {
            return;
        }
        if (rowIndex) {
            lcd_setDRAddr(0x40 + columnIndex);
        } else {
            lcd_setDRAddr(columnIndex);
        }
    }

    void lcd_putchar (int character) {
        if (sys.lcd.status != 1) {
            return;
        }
        lcd_waitOnReady();

        LCD_CLR_E;
        LCD_CLR_RW;
        LCD_SET_RS;
        DDRB |= 0x0f;  // LCD-Data (D7,D6,D5,D4) to Output
        
        PORTB &= ~0x0f;
        PORTB |= character >> 4; // send High-Byte
        LCD_SET_E;
        _delay_us(LCD_PULSE_LENGTH);
        LCD_CLR_E;
        _delay_us(1);

        PORTB &= ~0x0f;
        PORTB |= character & 0x0f; // send Low-Byte
        LCD_SET_E;
        _delay_us(LCD_PULSE_LENGTH);
        LCD_CLR_E;
        _delay_us(1);
        LCD_CLR_RS;
        _delay_us(1);
    }

    void lcd_putString (const char * str) {
        while (*str && sys.lcd.status == 1) {
            lcd_putchar(*str++);
        }
        lcd_waitOnReady();
    }

    #endif // GLOBAL_U1_LCD


    // ****************************************************************************
    // Monitor Handling
    // ****************************************************************************

    #ifdef GLOBAL_MONITOR

    inline uint8_t getMonCmdCount () {
        return sizeof(PMEMSTR_CMDS) / sizeof(struct MonCmdInfo);
    }


    struct MonCmdInfo getMonCmdInfo (uint8_t index) {
        struct MonCmdInfo ci = { NULL, NULL} ;
        PGM_P p;

        if (index < (sizeof(PMEMSTR_CMDS) / sizeof(struct MonCmdInfo)) ) {
            p = (PGM_P)&(PMEMSTR_CMDS[index]);
        } else {
            index -= (sizeof(PMEMSTR_CMDS) / sizeof(struct MonCmdInfo) );
            if (index >= u1_mon::getCmdCount()) {
                return ci;
            }
            p = (PGM_P)&(u1_mon::PMEMSTR_CMDS[index]);
        }

        memcpy_P(&ci, p, sizeof(struct MonCmdInfo));
        return ci;
    }


    void mon_printUsageInfo (struct MonCmdInfo *pCmdInfo) {
        PGM_P p = pCmdInfo->pInfo;

        uint8_t i;
        for (i = 0; i < 2; i++) {
            char c;
            do {
                memcpy_P(&c, p++, 1);
            }
            while (c != 0);
        }
        printString_P(p);
    }


    void mon_ExecuteCmd () {
        struct Monitor *pmon = (struct Monitor *)&mon;
        newline();

        uint8_t i;
        char *argv[GLOBAL_MONITOR_MAXARGV];
        uint8_t argc;

        char *ps = pmon->cmdLine;
        argc = 0;

        for (i = 0; i < GLOBAL_MONITOR_MAXARGV && *ps; i++) {
            while (*ps == ' ') {
                ps++; // ignore leading spaces
            }
            if (*ps == 0) {
                break;
            }
            argv[i] = ps;
            argc++;
            while (*ps != ' ' && *ps) {
                ps++;
            }
            if (*ps==' ') {
                *ps++ = 0;
            }
        }
        for (;i < GLOBAL_MONITOR_MAXARGV; i++) {
            argv[i] = NULL;
        }
        
        // printf("\n\rargc=%d\n\r", argc);
        // for (i=0; i < GLOBAL_MONITOR_MAXARGV; i++) {
        //     printf("  argv[%d]=0x%04x", i, (uint16_t)argv[i]);
        //     if (argv[i]) {
        //         printf(" len=%d cmd='%s'", strlen(argv[i]), argv[i]);
        //     }
        //     newline();
        // }
        
        if (argc > 0 && *argv[0]) {
            i = 0;
            while (1) {
                struct MonCmdInfo ci = getMonCmdInfo(i++);
                if (ci.pFunction == NULL) {
                    printString_P(PMEM_ERR1);
                    break;
                } else if (strcmp_P(pmon->cmdLine, ci.pInfo) == 0) {
                    if (argc == 2 && strcmp_P(argv[1], PMEM_CMD_HELP) == 0) {
                        mon_printUsageInfo(&ci);
                    } else {
                        int8_t retCode = (*ci.pFunction)(argc, argv);
                        if (retCode) {
                            printString_P(PMEM_ERR0);
                            printf("%d", retCode);
                            if (retCode < 0) {
                                printString_P(PMEM_ERR2);
                                mon_printUsageInfo(&ci);
                            }
                        }
                    }
                    newline();
                    break;
                }
            }
        }

        printString_P(PMEM_LINESTART);
        pmon->cursorPos = 0;
        pmon->cmdLine[0] = 0;
    }


    void mon_CmdLineBack () {
        struct Monitor *pmon = (struct Monitor *)&mon;
        if (pmon->cursorPos == 0) {
            return;
        }
        printf("\b \b");
        pmon->cmdLine[--pmon->cursorPos] = 0;
    }

    void putnchar (char c, uint8_t count) {
        for (; count > 0; count--) {
            putchar(c);
        }
    }


    void mon_main () {
        struct Monitor *pmon = (struct Monitor *)&mon;
        char c = 0;
        int8_t incLineIndex = 0;
        static uint8_t lastbyte = 0;

        if (mon.flags & MONITOR_FLAG_LINEMODE) {
            while (uart_available() > 0) {
                c = getchar();
                if (lastbyte != '\n' && c == '\r') {
                    c = '\n';
                } else {
                    lastbyte = c;
                }
                if (c=='\n') {
                    mon.flags &= ~(MONITOR_FLAG_LINEMODE | MONITOR_FLAG_LINE);
                    printf("\n\n\r>");
                    return;
                }
            }
            if (c == MONITOR_CTRL_X) {
                incLineIndex = 1;
                mon.flags &= ~MONITOR_FLAG_LINE;

            } else if (c == MONITOR_CTRL_Y) {
                incLineIndex = -1;
                mon.flags &= ~MONITOR_FLAG_LINE;

            } else if (c == MONITOR_CTRL_C) {
                if (mon.flags & MONITOR_FLAG_CONT) {
                    mon.flags &= ~(MONITOR_FLAG_CONT | MONITOR_FLAG_LINE);
                } else {
                    mon.flags |= MONITOR_FLAG_CONT;
                }
            }

            if (mon.flags & MONITOR_FLAG_LINE) {
                int8_t lenSpaces = u1_mon::printLine(mon.lineIndex, c);
                putnchar(' ', lenSpaces);
                printf("\r");
                if (mon.flags & MONITOR_FLAG_CONT) {
                    printf("\n");
                }
            } else {
                int8_t len;
                printf("\n\n\r");
                do {
                    if (incLineIndex) {
                        mon.lineIndex = (incLineIndex > 0) ? mon.lineIndex + 1 : mon.lineIndex - 1;
                    }
                    len = u1_mon::printLineHeader(mon.lineIndex);
                } while (len < 0 && mon.lineIndex != 0);
                newline();
                if (len > 0) {
                    putnchar('-', len);
                }
                newline();
                mon.flags |= MONITOR_FLAG_LINE;
            }

        } else { // monitor in command mode
            while (uart_available() > 0) {
                c = getchar();
                if (lastbyte != '\n' && c == '\r') {
                    c = '\n';
                } else {
                    lastbyte = c;
                }

                if (c == MONITOR_CTRL_X || c == MONITOR_CTRL_Y) {
                    mon.flags |= MONITOR_FLAG_LINEMODE;
                } else if (c == '\n') {
                    mon_ExecuteCmd();
                } else if (c == 127) {
                    // ignore Taste 'Entf' -> maybe implemented later
                } else if (c == '\b') {
                    mon_CmdLineBack();
                } else if (c < ' ' || c > 126) {
                    // ignore control codes
                } else if (pmon->cursorPos < (sizeof(pmon->cmdLine)-1) ) {
                    #ifdef GLOBAL_MONiTOR_ONLYLOCASE
                    c = tolower(c);
                    #endif
                    pmon->cmdLine[pmon->cursorPos++] = c;
                    pmon->cmdLine[pmon->cursorPos] = 0;
                    putchar(c);
                }
            }
        }
    }
    


    // ****************************************************************************
    // Monitor Commands
    // ****************************************************************************

    int8_t cmd_help (uint8_t argc, char *argv[]) {
        // struct Monitor *pmon = (struct Monitor *)&mon;
        uint8_t i, j, max;

        i = 0; max = 0;
        while (1) {
            struct MonCmdInfo ci = getMonCmdInfo(i++);
            if (ci.pFunction == NULL) {
                break;
            }
            uint8_t len = strlen_P(ci.pInfo);
            max = (len>max) ? len : max;
        }

        i = 0;
        while (1) {
            struct MonCmdInfo ci = getMonCmdInfo(i++);
            if (ci.pFunction == NULL) {
                break;
            }

            printString_P(ci.pInfo);
            for (j = strlen_P(ci.pInfo); j < (max + 2); j++) {
                putchar(' ');
            }
            puts_P(ci.pInfo + strlen_P(ci.pInfo) + 1);
        }

        return 0;
    }

    int8_t cmd_sinfo (uint8_t argc, char *argv[]) {
        printf("sys.errcnt.task: ");
        printHexBin8(sys.errcnt.task);
        newline();
        return 0;
    }

    #ifdef GLOBAL_MONITOR_CMD_SETMEM
    int8_t cmd_setmem (uint8_t argc, char *argv[]) {
        char     typ;    // 1st parameter (s='SRAM | 'e'=EEPROM)
        uint16_t add;    // 2nd parameter (address)
        uint16_t value;  // 3rd parameter (value)
        char     *padd, *pval;

        if (argc == 3) {
            typ = 's'; // default is set of SRAM-Byte
            padd = argv[1];
            pval = argv[2];
        } else if (argc == 4) {
            typ = argv[1][0];
            padd = argv[2];
            pval = argv[3];
        } else {
            return -1; // Syntax Error
        }

        add = readArgument(padd, RAMEND);
        value = readArgument(pval, 255);
        if (value < 0) {
            return -2;
        }

        printf("set 0x%02x to address 0x%04x\n\r", value, add);

        switch (typ) {
            case 's': {
                *((uint8_t *)add) = value;
                break;
            }

            case 'e': {
                eeprom_write_byte((uint8_t *)add, value);
                eeprom_busy_wait();
                break;
            }
        }

        return 0;
    }
    #endif // GLOBAL_MONITOR_CMD_SETMEM


    #ifdef GLOBAL_MONITOR_CMD_HEXDUMP
    int8_t cmd_hexdump (uint8_t argc, char *argv[]) {
        if (argc < 3 || argc > 4) {
            return -1;
        }

        char     typ; // 1st parameter ('s'=SRAM | 'f'=FLASH | 'e'=EEPROM)
        uint16_t add; // 2nd parameter (start address of hexdump)
        uint16_t len; // [3rd parameter] (number of bytes to dump)

        typ = argv[1][0];
        if (typ != 'f' && typ != 's' && typ != 'e') {
            return -1;
        }
        if (argv[2][0] == '0' && argv[2][1] == 'x') {
            add = strtol(&argv[2][2], NULL, 16);
        } else {
            add = strtol(argv[2], NULL, 10);
        }

        if (argc == 4) {
            if (argv[3][0] == '0' && argv[3][1] == 'x') {
                len = strtol(&argv[3][2], NULL, 16);
            } else {
                len = strtol(&argv[3][0], NULL, 10);
            }
        } else {
            len = 32;
        }

        char s[19] = "  ";
        s[18] = 0;
        uint16_t i;

        for (i = 0; i < len; add++, i++) {
            int16_t i16 = getByte(typ, add);
            if (i16 < 0) {
                break;
            }
            uint8_t value = (uint8_t)i16;

            if ( (i % 16) == 0) {
                printf("0x%04x: ", add);
            } else if (i % 4 == 0) {
                putchar(' ');
            }

            s[ (i % 16) + 2 ] = (value >= 32 && value < 127) ? value : '.';
            printf("%02x ", value);
            if (i % 16 == 15) {
                printf(s);
                newline();
            }
        }

        if ( (i % 16) != 0) {
            for (; (i % 16) != 0; i++) {
                printf("   ");
                s[ (i % 16) +2 ] = ' ';
                if ( (i % 4) == 0) {
                    putchar(' ');
                }
            }
            printf(s);
            newline();
        }

        return 0;
    }
    #endif // GLOBAL_MONITOR_CMD_HEXDUMP

    #endif // GLOBAL_MONITOR


    // ****************************************************************************
    // Application functions
    // ****************************************************************************

    // x=0..200 (0..360°) => sin(x) * 256 (with error: -0,52% ... 0,63%)
    int16_t sinX256 (uint8_t x) {
        // while (x < 0) {
        //     x += 200;
        // }
        while (x > 200) {
            x -= 200;
        }

        if (x >= 173) {
            return -sinX256(200 - x);
        } else if (x >= 150) {
            return -cosX256(x - 150);
        } else if (x >= 128) {
            return -cosX256(150 - x);
        } else if (x >= 100) {
            return -sinX256(x - 100);
        } else if (x >= 73) {
            return sinX256(100 - x);
        } else if ( x >= 50) {
            return cosX256(x - 50);
        } else if (x > 27) {
            return cosX256(50 - x);
        }

        // sin(x)= x - x³/6
        uint16_t a = x * x ;                 // 0 .. 729
        uint16_t b = 86 * a;                 // 0 .. 62694
        uint16_t c = 2048 - (b + 128) / 256; // 2048 .. 1804
        uint32_t d = x * c;                  // 0 .. 55296
        uint16_t e = (uint16_t)d;
        e = e + 64 + x * 16;
        return (uint8_t)(e >> 8);
    }
    
    // x=0..200 (0..360°) => cos(x) * 256 (with error: -0,53% ... 0,63%)
    int16_t cosX256 (uint8_t x) {
        // while (x < 0) {
        //     x += 200;
        // }
        while (x > 200) {
            x -= 200;
        }
        if (x >= 178) {
            return cosX256(200 - x);
        } else if (x >= 150) {
            return sinX256(x - 150);
        } else if (x >= 123) {
            return -sinX256(150 - x);
        } else if (x >= 100) {
            return -cosX256(x - 100);
        } else if (x >= 78) {
            return -cosX256(100 - x);
        } else if ( x >= 50) {
            return -sinX256(x - 50);
        } else if (x > 22) {
            return sinX256(50 - x);
        }
        
        // cos(x)= 1 - x²/2
        uint16_t a = x * x + 4 ;     // 0 .. 488
        uint16_t b = 256 - a / 8;    // 0 .. 134
        b = b + (x > 16 ? 1 : 0);
        return b;
    }

    void enablePWM (uint8_t enable) {
        if (enable) {
            if (TCCR0A == 0) {
                OCR0A = 0xff;
                TCCR0A = (1 << COM0A1) | (0 << COM0A0) | (1 << WGM01) | (1 << WGM00);
                TCCR0B = (1 << CS01) | (1 << CS00);
            }
        } else {
            TCCR0A = 0;
            TCCR0B = 0;
            OCR0A = 0;
            DDRD |= (1 << PD6);
            PORTD &= ~(1 << PD6);
        }
    }

    void setK1 (uint8_t on) {
        DDRC |= (1 << PC4); // PC4 - K1 (Relais 230V/16A)
        if (on) {
            PORTC |= (1 << PC4);
        } else {
            PORTC &= ~(1 << PC4);
        }
    }

    void setK2 (uint8_t on) {
        DDRC |= (1 << PC5); // PC5 - K2 (Fotek SSR 100A)
        if (on) {
            PORTC |= (1 << PC5);
        } else {
            PORTC &= ~(1 << PC5);
        }
    }

    void setLedD3 (uint8_t ledState) {
        if (ledState)
            PORTC |= (1 << PC3);
        else
            PORTC &= ~(1 << PC3);
    }

    void toggleLedD3 (void) {
        PORTC ^= (1 << PC3);
    }

    uint8_t getSw1Value () {
        return 15 - ((PIND >> 2) & 0x0f);
    }


}   



// ------------------------------------
// Interrupt Service Routinen
// ------------------------------------

ISR (USART_RX_vect) {
    static uint8_t lastChar;
    uint8_t c = UDR0;

    if (c=='R' && lastChar=='@') {
        wdt_enable(WDTO_15MS);
        wdt_reset();
        while(1) {};
    }
    lastChar = c;

    u1_sys::sys.uart.rbuffer_u8[u1_sys::sys.uart.wpos_u8++] = c;
    if (u1_sys::sys.uart.wpos_u8 >= GLOBAL_UART0_RECBUFSIZE) {
        u1_sys::sys.uart.wpos_u8 = 0;
    }
    if (u1_sys::sys.uart.wpos_u8 == u1_sys::sys.uart.rpos_u8) {
        u1_sys::sys.uart.wpos_u8 == 0 ? u1_sys::sys.uart.wpos_u8 = GLOBAL_UART0_RECBUFSIZE-1 : u1_sys::sys.uart.wpos_u8--;
        u1_sys::sys.uart.errcnt_u8 = u1_sys::inc8BitCnt(u1_sys::sys.uart.errcnt_u8);
    }
    u1_sys::sys.uart.rbuffer_u8[u1_sys::sys.uart.wpos_u8] = 0;
}

ISR (USART_TX_vect) {
}

ISR (TIMER1_COMPA_vect) {
}

ISR (TIMER1_COMPB_vect) {
}

// Timer 2 Output/Compare Interrupt
// called every 100us
ISR (TIMER2_COMPA_vect) {
    static uint8_t cnt100us = 0;
    static uint8_t cnt500us = 0;
    static uint8_t busy = 0;

    cnt100us++;
    if (cnt100us >= 5) {
        cnt100us = 0;
        cnt500us++;
        if (busy) {
            u1_sys::sys.errcnt.task = u1_sys::inc8BitCnt(u1_sys::sys.errcnt.task);
        } else {
            busy = 1;
            sei();
            wdt_reset();
            if      (cnt500us & 0x01) { u1_app::task_1ms(); }
            else if (cnt500us & 0x02) { u1_app::task_2ms(); }
            else if (cnt500us & 0x04) { u1_app::task_4ms(); }
            else if (cnt500us & 0x08) { u1_app::task_8ms(); }
            else if (cnt500us & 0x10) { u1_app::task_16ms(); }
            else if (cnt500us & 0x20) { u1_app::task_32ms(); }
            else if (cnt500us & 0x40) { u1_app::task_64ms(); }
            else if (cnt500us & 0x80) { u1_app::task_128ms(); }
            busy = 0;
        }
    }
}

ISR (ADC_vect) {
    uint8_t channel = ADMUX & 0x0f;
    channel = u1_app::handleADCValue_adc100us(channel, ADCH);
    ADMUX = (1 << REFS1) | (1 << REFS0) | (1 << ADLAR) | (channel & 0x0f); // Vref = internal 1.1V
}


ISR (SPI_STC_vect) {
}
