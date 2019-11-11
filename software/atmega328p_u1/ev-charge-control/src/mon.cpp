#include "global.h"

#include "mon.hpp"
#include "sys.hpp"
#include "app.hpp"

#ifdef GLOBAL_MONITOR

namespace u1_mon {

    // declarations and definitions

    int8_t cmd_info (uint8_t argc, const char **argv);
    int8_t cmd_test (uint8_t argc, const char **argv);
    int8_t cmd_set (uint8_t argc, const char **argv);
    int8_t cmd_pwm (uint8_t argc, const char **argv);
    int8_t cmd_eep (uint8_t argc, const char **argv);
    int8_t cmd_trim (uint8_t argc, const char **argv);
    int8_t cmd_log (uint8_t argc, const char **argv);

    const char LINE_WELCOME[] PROGMEM = "Line-Mode: CTRL-X, CTRL-Y, CTRL-C, Return  \n\r";
    const char PMEM_CMD_INFO[] PROGMEM = "info\0Application infos\0info";
    const char PMEM_CMD_TEST[] PROGMEM = "test\0commando for test\0test";
    const char PMEM_CMD_SET[] PROGMEM = "set\0set function/port\0set {k1|k2|f|v|i|t} {0|1|value}";
    const char PMEM_CMD_PWM[] PROGMEM = "pwm\0set PWM duty cycle for CP/port\0pwm {0-100}";
    const char PMEM_CMD_EEP[] PROGMEM = "eep\0eeprom ...\0eep [c/r/w] [hex-address] [number|value]";
    const char PMEM_CMD_TRIM[] PROGMEM = "trim\0trim table\0trim [reset]";
    const char PMEM_CMD_LOG[] PROGMEM = "log\0log\0log [clear]";

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
    void startupLog ();
    

    // ------------------------------------------------------------------------

    void init () {
        memset((void *)&mon, 0, sizeof(mon));
        // startupLog();
    }

    void main () {
        
    }

    uint8_t getCmdCount () {
        return sizeof(PMEMSTR_CMDS) / sizeof(struct u1_sys::MonCmdInfo);
    }

    // --------------------------------------------------------
    // Monitor commands of the application
    // --------------------------------------------------------

    int8_t cmd_info (uint8_t argc, const char **argv) {
        return 0;
    }


    int8_t cmd_test (uint8_t argc, const char **argv) {
        return 0;
    }


    int8_t cmd_set (uint8_t argc, const char **argv) {
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

    int8_t cmd_pwm (uint8_t argc, const char **argv) {
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

    int8_t cmd_eep (uint8_t argc, const char **argv) {
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

    int8_t cmd_trim (uint8_t argc, const char **argv) {
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
    

    // ***********************************************************************
    // log functions
    // ***********************************************************************

    int16_t findLogDescriptorIndex (uint32_t time) {
        int16_t rv = -1;
        uint32_t rvTimeDiff = 0xffffffff;
        uint8_t index = 0;
        plogtable_t p = (plogtable_t)EEP_LOG_START + sizeof (struct LogDescriptor) * index;
        struct LogDescriptor d;
        while (index < EEP_LOG_DESCRIPTORS) {
            eeprom_busy_wait();
            eeprom_read_block(&d, p, sizeof (d));
            if ((d.typ & 0x0f) != 0x0f) {
                uint32_t diff = d.time > time ? d.time - time : time - d.time;  
                if (diff < rvTimeDiff) {
                    rvTimeDiff = diff;
                    rv = index;
                }
            }
            index++;
            p += sizeof(struct LogDescriptor);
        }
        return rv;
    }

    int16_t findNewestLogDescriptorIndex () {
        return findLogDescriptorIndex(0xffffffff);
    }

    int16_t findOldestLogDescriptorIndex () {
        return findLogDescriptorIndex(0);
    }

    uint8_t saveLog (uint8_t typ, void *pData, uint8_t size) {
        if (typ >= 0x0f) { 
            return 0xff; // error
        }
        struct LogDescriptor d;
        d.typ = 0x0f;
        pushSREGAndCli(); {
            uint16_t tHigh = (u1_app::app.trim.startCnt << 1) | ((u1_app::app.clock.hrs >> 4) & 0x01);
            uint16_t tLow = ((u1_app::app.clock.hrs & 0x0f) << 12) | ((u1_app::app.clock.min & 0x3f) << 6) | (u1_app::app.clock.sec & 0x3f);
            d.time = (((uint32_t)tHigh) << 16) | tLow;
        } popSREG();

        uint8_t index = 0;
        plogtable_t p = (plogtable_t)EEP_LOG_START;
        plogtable_t pTo = (plogtable_t)EEP_LOG_SLOTS_START;
        uint8_t lastTyp = typ;
        if (mon.log.index < EEP_LOG_DESCRIPTORS) {
            index = mon.log.index;
            p += index * sizeof (struct LogDescriptor);
            pTo += index * EEP_LOG_SLOT_SIZE;
            lastTyp = mon.log.lastTyp == 0 ? 0xff : mon.log.lastTyp;
        }
        uint8_t rv;
        int16_t bytes = size;
        uint8_t slotCnt = 0;

        do {    
            if (lastTyp != typ || slotCnt > 0) {
                index++;
                p += sizeof (struct LogDescriptor);
                pTo += EEP_LOG_SLOT_SIZE;
                if (index >= EEP_LOG_DESCRIPTORS) { 
                    p = (plogtable_t)EEP_LOG_START;
                    pTo = (plogtable_t)EEP_LOG_SLOTS_START;
                    index = 0;
                }
            }
            if (slotCnt == 0) {
                rv = index;
            }
            d.typ = (d.typ & 0x0f) | (slotCnt++ << 4);
            eeprom_busy_wait();
            eeprom_update_block(&d, p, sizeof(d));
            uint8_t l = bytes < EEP_LOG_SLOT_SIZE ? bytes : EEP_LOG_SLOT_SIZE;
            eeprom_busy_wait();
            if (pData != NULL && size > 0) {
                eeprom_update_block(pData, pTo, l);
                
                bytes -= EEP_LOG_SLOT_SIZE;
                pData = ((uint8_t *)pData) + l;
            }
        } while (bytes > 0 && slotCnt < 16);

        index = rv;
        p = (plogtable_t)EEP_LOG_START + index * sizeof (struct LogDescriptor);
        d.typ = typ;
        uint8_t slot = 0;
        while (slotCnt-- > 0) {
            d.typ = (d.typ & 0x0f) | (slot << 4);
            eeprom_busy_wait();
            eeprom_write_byte(p, *((uint8_t *)&d));
            p += sizeof (struct LogDescriptor);
            mon.log.index = index++;
            slot++;
            if (index >= EEP_LOG_DESCRIPTORS) { 
                p = (plogtable_t)EEP_LOG_START;
                index = 0;
            }
        }
        mon.log.lastTyp = typ;

        return rv;
    }

    void startupLog () {
        int16_t startIndex = findNewestLogDescriptorIndex();
        mon.log.index = startIndex < 0 ? EEP_LOG_DESCRIPTORS : startIndex + 1;
        if (mon.log.index >= EEP_LOG_DESCRIPTORS) {
            mon.log.index = 0;
        }
        mon.log.lastTyp = 0xff;
        saveLog(LOG_TYPE_SYSTEMSTART, NULL, 0);
    }

    int8_t cmd_log (uint8_t argc, const char **argv) {
        if (argc == 2 && strcmp("clear", argv[1]) == 0) {
            clearLogTable();
            return 0;
        }
        if (argc > 1) { return -1; }

        struct LogDescriptor d;
        int16_t startIndex = findOldestLogDescriptorIndex();
        uint8_t cnt = 0;
        if (startIndex >= 0) {
            uint8_t index = (uint8_t)startIndex;
            plogtable_t p = (plogtable_t)EEP_LOG_START + sizeof (struct LogDescriptor) * index;
            struct LogDescriptor d;
            do {
                eeprom_busy_wait();
                eeprom_read_block(&d, p, sizeof (d));
                if ((d.typ & 0x0f) != 0x0f) {
                    cnt++;
                    uint8_t typ = d.typ & 0x0f;
                    uint8_t subIndex = d.typ >> 4;
                    uint16_t startupCnt = d.time >> 17;
                    uint8_t hrs = (d.time >> 12) & 0x1f;
                    uint8_t min = (d.time >> 6) & 0x3f;
                    uint8_t sec = d.time & 0x3f;
                    printf("  %2d(%01x/%01x) %5d-%02d:%02d:%02d -> ", index, typ, subIndex, startupCnt, hrs, min, sec);
                    uint8_t slot[EEP_LOG_SLOT_SIZE];
                    eeprom_busy_wait();
                    eeprom_read_block(&slot, (plogtable_t)EEP_LOG_SLOTS_START + index * EEP_LOG_SLOT_SIZE, sizeof (slot));
                    switch (d.typ) {
                        case LOG_TYPE_SYSTEMSTART: {
                            printf("system start");
                            break;
                        }

                        case LOG_TYPE_STARTCHARGE: {
                            struct LogDataStartCharge *pData = (struct LogDataStartCharge *)&slot;
                            printf("start charge maxAmps=%u", pData->maxAmps);
                            break;
                        }

                        case LOG_TYPE_CHARGING: case LOG_TYPE_STOPCHARGING: {
                            struct LogDataCharging *pData = (struct LogDataCharging *)&slot;
                            uint8_t e = pData->energyKwhX256 >> 8;
                            uint8_t nk = ((pData->energyKwhX256 & 0xff) * 100 + 128) / 256;
                            if (d.typ == LOG_TYPE_STOPCHARGING) {
                                printf("stop ");
                            }
                            printf("charge %u:%02u, E=%u.%02ukWh", pData->chgTimeHours, pData->chgTimeMinutes, e, nk);
                            break;
                        }

                        default: {
                            printf("? (");
                            for (uint8_t i = 0; i < sizeof slot; i++) {
                                printf(" %02x", slot[i]);
                            }
                            printf(")");
                            break;
                        }
                    }
                    printf("\n");
                }
                index++;
                p += sizeof(struct LogDescriptor);
                if (index >= EEP_LOG_DESCRIPTORS) {
                    index = 0;
                    p = (plogtable_t)EEP_LOG_START;
                }
            } while (index != startIndex);
        }
        printf("%d valid log records\n", cnt);

        return 0;
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
        mon.log.index = 0xff;
        mon.log.lastTyp = 0x0f;
    }

}

#endif // GLOBAL_MONITOR
