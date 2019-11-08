#include "global.h"

#include "mon.hpp"
#include "sys.hpp"
#include "app.hpp"

#ifdef GLOBAL_MONITOR

namespace u1_mon {

    // declarations and definitions

    int8_t cmd_info (uint8_t argc, char *argv[]);
    int8_t cmd_test (uint8_t argc, char *argv[]);
    int8_t cmd_set (uint8_t argc, char *argv[]);
    int8_t cmd_pwm (uint8_t argc, char *argv[]);
    int8_t cmd_eep (uint8_t argc, char *argv[]);
    int8_t cmd_trim (uint8_t argc, char *argv[]);
    int8_t cmd_log (uint8_t argc, char *argv[]);

    const char LINE_WELCOME[] PROGMEM = "Line-Mode: CTRL-X, CTRL-Y, CTRL-C, Return  \n\r";
    const char PMEM_CMD_INFO[] PROGMEM = "info\0Application infos\0info";
    const char PMEM_CMD_TEST[] PROGMEM = "test\0commando for test\0test";
    const char PMEM_CMD_SET[] PROGMEM = "set\0set function/port\0set {k1|k2|f|v|i|t} {0|1|value}";
    const char PMEM_CMD_PWM[] PROGMEM = "pwm\0set PWM duty cycle for CP/port\0pwm {0-100}";
    const char PMEM_CMD_EEP[] PROGMEM = "eep\0eeprom ...\0eep [c/r/w] [hex-address] [number|value]";
    const char PMEM_CMD_TRIM[] PROGMEM = "trim\0trim table\0trim [reset]";
    const char PMEM_CMD_LOG[] PROGMEM = "log\0log\0log";

    const char HEADER_1[] PROGMEM = "L1 |   Up-Time | A0 A1 A2 A6 A7 A8 |  Temp |   VCP |     CP | STATUS";
    const char HEADER_2[] PROGMEM = "L2 | Voltage\n   | +/- TH ADC MIN MAX VPP |  PERIOD |   FREQU |     VAC | ";
    const char HEADER_3[] PROGMEM = "L3 | Current (s + -)\n   | +/- TH ADC MIN MAX IPP | DPH | SIM |   CURR";
    const char HEADER_4[] PROGMEM = "L4 | Power (s + -)\n   |   FREQU |     VAC |   CURR | DPH |      S |      P |        Q |";

    const struct u1_sys::MonCmdInfo PMEMSTR_CMDS[] PROGMEM = {
          { PMEM_CMD_INFO, cmd_info }
        , { PMEM_CMD_TEST, cmd_test }
        , { PMEM_CMD_SET, cmd_set }
        , { PMEM_CMD_PWM, cmd_pwm }
        , { PMEM_CMD_EEP, cmd_eep }
        , { PMEM_CMD_TRIM, cmd_trim }
        , { PMEM_CMD_LOG, cmd_log }
    };

    // definitions and declarations
    struct Mon mon;
    
    void clearLogTable ();
    plogtable_t getNextLogRecord (plogtable_t p);
    plogtable_t getLogLatestRecord ();
    void saveLog ();

    // ------------------------------------------------------------------------

    void init () {
        memset((void *)&mon, 0, sizeof(mon));
        uint8_t *p = (uint8_t *)EEP_LOG_START;
        struct LogRecordHeader pHeader;
        mon.log.pLatestRecord = getLogLatestRecord();
        // printf("%u log records found\n", cnt);

    }

    void main () {
        
    }

    uint8_t getCmdCount () {
        return sizeof(PMEMSTR_CMDS) / sizeof(struct u1_sys::MonCmdInfo);
    }

    // --------------------------------------------------------
    // Monitor commands of the application
    // --------------------------------------------------------

    int8_t cmd_info (uint8_t argc, char *argv[]) {
        return 0;
    }


    int8_t cmd_test (uint8_t argc, char *argv[]) {
        return 0;
    }


    int8_t cmd_set (uint8_t argc, char *argv[]) {
        if (argc > 1 && argc < 4 && argv[1][1] == 0) {
            int v = 0;
            if (argc == 3) {
                if (sscanf(argv[2], "%d", &v) != 1) { return -2; }
            }
            switch (argv[1][0]) {
                case 'f': u1_app::app.sim.f = v; return 0;
                case 'v': u1_app::app.sim.v = v; return 0;
                case 'i': u1_app::app.sim.i = v; return 0;
                case 't': u1_app::app.sim.t = v; return 0;
            }
        }        

        if (argc != 3 || argv[2][1] != 0) { return -1; }
        uint8_t v = argv[2][0] - '0';
        if (strcmp("k1", argv[1]) == 0) {
            DDRC |= (1 << PC4); // PC4 - K1
            if (v) {
                PORTC |= (1 << PC4);
            } else {
                PORTC &= ~(1 << PC4);
            }
            return 0;
    
        } else if (strcmp("k2", argv[1]) == 0 ) {
           DDRC |= (1 << PC5); // PC5 - K2
            if (v) {
                PORTC |= (1 << PC5);
            } else {
                PORTC &= ~(1 << PC5);
            }
            return 0;
        }

        return -10;
    }

    int8_t cmd_pwm (uint8_t argc, char *argv[]) {
        if (argc != 2) { return -1; }
        int v;
        if (sscanf(argv[1], "%d", &v) != 1) { return -2; }
        if (v < 0 || v > 100) { return -3; }
        if (v == 0) { 
            u1_sys::enablePWM(0);
        } else if (v == 100) {
            u1_sys::enablePWM(1);
            OCR0A = 255; // CP is 12V without PWM
        } else {
            u1_sys::enablePWM(1);
            OCR0A = (255 * v) / 100;
        }
        return 0;
    }

    int8_t cmd_eep (uint8_t argc, char *argv[]) {
        uint8_t *pAddr = 0;
        uint16_t number = 32;
        char type = 'r';
        if (argc > 1) {
            if (argv[1][1] != 0) { return -1; }
            type = argv[1][0];
            if (type == 'c' && argc == 2) {
                clearLogTable();
                return 0;
            }

            if (type != 'r' && type != 'w') { return -2; }
            if (type == 'w' && argc != 4) { return -3; }
        }
        if (argc > 2) {
            int a;
            if (sscanf(argv[2], "%d", &a) != 1) { return -4; }
            if (a < 0 || a > 1023) { return - 5; }
            pAddr = (uint8_t *)a;
        }
        if (argc > 3) {
            int n;
            if (sscanf(argv[3], "%d", &n) != 1) { return -6; }
            if (type == 'w') {
                if (n < 0 || n > 0xff) { return -7; }
            } else if (n < 1 || n > 1024) { 
                return - 8;
            }
            number = n;
        }
        if (argc > 4) {
            return -8;
        }
        

        if (type == 'w') {
            eeprom_write_byte(pAddr, number);
            eeprom_busy_wait();
            number = 1;
        }
        
        while ((uint16_t)pAddr < 1024 && number > 0) {
            uint8_t b = eeprom_read_byte(pAddr);
            printf(" %04x: %02x\n", (uint16_t)pAddr, b);
            pAddr++; number--;
        }
        printf("\n");
        // ueeprom_read_byte (const uint8_t *__p) __ATTR_PURE__;
        return 0;
    }

    void printTrim (struct u1_app::Trim *p) {
        printf(" magic: %08lx\n", p->magic);
        printf(" startCnt: %u\n", p->startCnt);
        printf(" vcpK: %d\n", p->vcpK);
        printf(" vcpD: %d\n", p->vcpD);
        printf(" currK: %d\n", p->currK);
        printf(" tempK: %d\n", p->tempK);
        printf(" tempOffs: %d\n\n", p->tempOffs);
    }

    int8_t cmd_trim (uint8_t argc, char *argv[]) {
        if (argc == 2) {
            if (strcmp(argv[1], "clear") == 0) {
                uint8_t *p = 0;
                for (uint8_t i = 0; i < sizeof(u1_app::app.trim); i++) {
                    eeprom_write_byte(p++, 0xff);
                    eeprom_busy_wait();
                }
                
            } else if (strcmp(argv[1], "reset") == 0) {
                u1_app::initTrim(0);
                eeprom_update_block(&u1_app::app.trim, (void *)0, sizeof(u1_app::app.trim));
                eeprom_busy_wait();
            } else {
                return -1;
            }
            printf("OK\n\n");
        }

        printf("EE2:\n");
        struct u1_app::Trim t;
        uint8_t *p = (uint8_t *)&t;
        for (uint8_t i = 0; i < sizeof(t); i++) {
            *(p++) = eeprom_read_byte((uint8_t *)(uint16_t)i);
        }
        printTrim(&t);
        printf("TRIM:\n");
        printTrim(&u1_app::app.trim);
        printf("\n");

        return 0;
    }
    

    int8_t cmd_log (uint8_t argc, char *argv[]) {
        return -1;
    }


    // --------------------------------------------------------
    // Monitor-Line for continues output
    // --------------------------------------------------------

    int8_t printLineHeader (uint8_t lineIndex) {
        if (lineIndex==0) {
            u1_sys::printString_P(LINE_WELCOME);
        }
        
        switch (lineIndex) {
            case 0: u1_sys::printString_P(HEADER_1); return 75;
            case 1: u1_sys::printString_P(HEADER_2); return 60;
            case 2: u1_sys::printString_P(HEADER_3); return 50;
            case 3: u1_sys::printString_P(HEADER_4); return 75;
            default: return -1; // this line index is not valid
        }
    }

    int8_t printLine (uint8_t lineIndex, char keyPressed) {
        
        // handle keypressed
        switch (lineIndex) {
            case 2: case 3: {
                uint8_t k = u1_app::app.sim.currentAdcK;
                if (keyPressed == 's') {
                    if (k == 0) {
                        // enable current simulation
                        // current adc taken from voltage sensor
                        k = 32;
                    } else {
                        // disable current simulation
                        // current adc taken from current sensor
                        k = 0;
                    }
                }
                if (k > 0) {
                    // adjust current adc simulation amplitude
                    if (keyPressed =='+') {
                        k = (k < 0xff) ? k + 1 : k;
                    } else if (keyPressed == '-') {
                        k = (k < 0xff) ? k - 1 : k;
                    }
                }
                u1_app::app.sim.currentAdcK = k;
                break;
            }
        }

        // create line output
        printf("%02d | ", (int)u1_sys::getSw1Value());
        switch (lineIndex) {
            case 0: { // line L1
                struct u1_app::Adc *p = &(u1_app::app.adc);
                printf("%3d:%02d:%02d ", u1_app::app.clock.hrs, u1_app::app.clock.min, u1_app::app.clock.sec);
                printf("| %02x %02x %02x %02x %02x %02x ", p->adc0, p->adc1, p->adc2, p->adc6, p->adc7, p->adc8);
                printf("| %c%3dC ", u1_app::app.sim.t > 0 ? 'S' : ' ', (int)u1_app::app.temp);
                printf("| %2d.%1dV ", u1_app::app.vcpX16 >> 4, ((u1_app::app.vcpX16 & 0x0f) * 10) / 16 ); 
                uint16_t cpPwmX256 = ((uint16_t)OCR0A) * 100;
                printf ("| %3d.%1d%% ", cpPwmX256 >> 8, ((cpPwmX256 & 0xff) * 10) / 256 );
                printf("| "); 
                
                switch (u1_app::app.state) {
                    case u1_app::Init:         printf("Init"); break;
                    case u1_app::Test:         printf("Test"); break;
                    case u1_app::Start:        printf("Start"); break;
                    case u1_app::NotConnected: printf("NotConnected"); break;
                    case u1_app::Connected:    printf("Connected"); break;
                    case u1_app::EVReady:      printf("EVReady"); break; 
                    case u1_app::Charging:     printf("Charging"); break;
                    default: printf("? (%d)", u1_app::app.state);
                }
                return 10;
            }
            
            case 1: { // line L2
                struct u1_app::AdcVoltage *p = &(u1_app::app.adc.voltage);
                printf("%3d %02x  %02x  %02x  %02x  %02x ", p->area, p->th, u1_app::app.adc.adc1, p->min, p->max, p->peakToPeak);
                printf("| %02x %04x ", p->period, p->periodEwma);
                uint16_t frequX256 = u1_app::app.frequX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %c%2u.%1uHz ", u1_app::app.sim.f > 0 ? 'S' : ' ', frequX256 / 256, ((frequX256 & 0xff) * 10) / 256);
                uint16_t vphX256 = u1_app::app.vphX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %c%3u.%1uV ", u1_app::app.sim.v > 0 ? 'S' : ' ', vphX256 / 256, ((vphX256 & 0xff) * 10) / 256);
                return 4;
            }
            
            case 2: { // line L3
                struct u1_app::AdcSine *p = &(u1_app::app.adc.current.sine);
                printf("%3d %02x  %02x  %02x  %02x %03x ", u1_app::app.adc.current.tmp.area, p->th, u1_app::app.adc.adc0, p->min, p->max, p->peakToPeak);
                printf("| %3d ", u1_app::app.adc.phAngX100us);
                printf("| %3d ", u1_app::app.sim.currentAdcK);
                uint16_t currX256 = u1_app::app.currX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %c%2u.%1uA ", u1_app::app.sim.i > 0 || u1_app::app.sim.currentAdcK > 0 ? 'S' : ' ', currX256 / 256, ((currX256 & 0xff) * 10) / 256);                
                return 4;
            }
        
            case 3: { // line L4
                struct u1_app::AdcVoltage *pv = &(u1_app::app.adc.voltage);
                struct u1_app::AdcSine *pc = &(u1_app::app.adc.current.sine);
                uint16_t frequX256 = u1_app::app.frequX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("%c%2u.%1uHz ", u1_app::app.sim.f > 0 || u1_app::app.sim.currentAdcK > 0 ? 'S' : ' ', frequX256 / 256, ((frequX256 & 0xff) * 10) / 256);
                uint16_t vphX256 = u1_app::app.vphX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %c%3u.%1uV ", u1_app::app.sim.v > 0 ? 'S' : ' ', vphX256 / 256, ((vphX256 & 0xff) * 10) / 256);
                uint16_t currX256 = u1_app::app.currX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %c%2u.%1uA ", u1_app::app.sim.i > 0 ? 'S' : ' ', currX256 / 256, ((currX256 & 0xff) * 10) / 256);
                printf("| %3d ", u1_app::app.adc.phAngX100us);
                
                cli();
                int16_t s = u1_app::app.apparantPower;
                int16_t p = u1_app::app.activePower;
                int16_t q = u1_app::app.reactivePower;
                uint32_t e = u1_app::app.energyKwhX256;
                sei();

                printf("| %4dVA ", s); 
                printf("| %5dW ", p); 
                printf("| %5dvar ", q);
                printf("| %ld.%02dkWh", e / 256, ((uint8_t)e) * 100 / 256);
                return 2;
            }
        
            case 4: {
                for (int i = 0; i < 6; i++) {
                    printf(" %04x", u1_app::app.debug.u16[i] );
                }
                return 2;
            }
        
            default: return -1;
        }
    }

    // --------------------------------------------------------
    // Log functions
    // --------------------------------------------------------

    void clearEEP (uint8_t *pStartAddr, uint16_t size) {
        while (size-- > 0 && (uint16_t)pStartAddr <= E2END) {
            eeprom_busy_wait();
            eeprom_write_byte(pStartAddr++, 0xff);
        }
        eeprom_busy_wait();
    }
    
    void clearTrim () {
        clearEEP(0, sizeof(struct u1_app::Trim));
    }

    void clearLogTable () {
        clearEEP((uint8_t *)EEP_LOG_START, E2END + 1 - EEP_LOG_START);
        mon.log.pLatestRecord = 0;
    }

    plogtable_t getNextLogRecord (plogtable_t p) {
        if (p == 0 || (uint16_t)p > E2END) {
            // illegal argument p, clear all records and return start of table
            clearLogTable();
            return (plogtable_t)EEP_LOG_START;
        } 
        eeprom_busy_wait();
        uint8_t rSize = eeprom_read_byte(p);
        if (rSize == 0xff) {
            return p;
        } else if (rSize < sizeof (struct LogRecordHeader)) {
            // failure in table, clear all records and return start of table
            clearLogTable();
            return (plogtable_t)EEP_LOG_START;
        }
        p = p + rSize;
        if ((uint16_t)p > E2END) {
            return (plogtable_t)EEP_LOG_START;
        }
        return p;
    }

    plogtable_t getLogLatestRecord () {
        plogtable_t prv = (plogtable_t) EEP_LOG_START;
        struct LogRecordHeader h; 
        uint32_t timeLatest = 0;

        plogtable_t p = (plogtable_t)EEP_LOG_START;
        while (1) {
            eeprom_busy_wait();
            eeprom_read_block(&h, p, sizeof(struct LogRecordHeader));
            if (h.size == 0xff) {
                return (plogtable_t)NULL;
            }
            uint32_t t = *( (uint32_t *)&h.time );
            if (t == 0 || h.size < sizeof(struct LogRecordHeader)) {
                // failure in table, clear all records and return start of table
                clearLogTable();
                return (plogtable_t)NULL;
            }
            if (t > timeLatest)     {
                timeLatest = t;
                prv = p;
            }
            p = (plogtable_t)(p + h.size);
            if ((uint16_t)p > E2END) {
                return prv;
            }
        }
    }

    void saveLog (void *pData, uint8_t typ, uint8_t size) {
        plogtable_t p;
        if (mon.log.pLatestRecord == (plogtable_t)NULL) {
            mon.log.pLatestRecord = getLogLatestRecord();
        }
        if (mon.log.pLatestRecord == (plogtable_t)NULL) {
            p = (plogtable_t)EEP_LOG_START;
        } else {
            eeprom_busy_wait();
            uint8_t lrSize = eeprom_read_byte(mon.log.pLatestRecord);
            p = p + lrSize;
            if ( ((uint16_t)p + sizeof(struct LogRecordHeader) + size) > E2END ) {
                p = (plogtable_t)EEP_LOG_START;
            }
        }
        
        struct LogRecordHeader h;
        h.size = size + sizeof(LogRecordHeader);
        h.typ = 0;
        h.time.systemStartCnt = u1_app::app.trim.startCnt;
        cli();
        h.time.sec = u1_app::app.clock.sec;
        h.time.min = u1_app::app.clock.min;
        h.time.hrs = u1_app::app.clock.hrs;
        sei();
        eeprom_busy_wait();
        eeprom_update_block(&h, p, sizeof(h));
        eeprom_busy_wait();
        eeprom_update_block(pData, p + sizeof(h), size);
        eeprom_busy_wait();
        eeprom_write_byte((p + 1), typ);
        eeprom_busy_wait();
        mon.log.pLatestRecord = p;
    }

}

#endif // GLOBAL_MONITOR
