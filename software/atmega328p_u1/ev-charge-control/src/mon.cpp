#include "global.h"

#include <stdio.h>
#include <string.h>

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

    const char LINE_WELCOME[] PROGMEM = "Line-Mode: CTRL-X, CTRL-Y, CTRL-C, Return  \n\r";
    const char PMEM_CMD_INFO[] PROGMEM = "info\0Application infos\0info";
    const char PMEM_CMD_TEST[] PROGMEM = "test\0commando for test\0test";
    const char PMEM_CMD_SET[] PROGMEM = "set\0set function/port\0set {k1|k2}  {0|1}";
    const char PMEM_CMD_PWM[] PROGMEM = "pwm\0set PWM duty cycle for CP/port\0pwm {0-100}";

    const struct u1_sys::MonCmdInfo PMEMSTR_CMDS[] PROGMEM = {
          { PMEM_CMD_INFO, cmd_info }
        , { PMEM_CMD_TEST, cmd_test }
        , { PMEM_CMD_SET, cmd_set }
        , { PMEM_CMD_PWM, cmd_pwm }
    };

    struct Mon mon;


    // ------------------------------------------------------------------------

    void init () {
        memset((void *)&mon, 0, sizeof(mon));
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

    // --------------------------------------------------------
    // Monitor-Line for continues output
    // --------------------------------------------------------

    int8_t printLineHeader (uint8_t lineIndex) {
        if (lineIndex==0) {
            u1_sys::printString_P(LINE_WELCOME);
        }
        
        switch (lineIndex) {
            case 0: printf("L1 | A0 A1 A2 A6 A7 | Volt | Amps |   VCP | SW1 | STATUS "); return 60;
            case 1: printf("L2 | Voltage\n   | +/- TH ADC MIN MAX VPP |  PERIOD |  FREQU |    VAC | "); return 60;
            case 2: printf("L3 | Current\n   | +/- TH ADC MIN MAX IPP |"); return 60;
            case 3: printf("L4 | Debug"); return 60;
            default: return -1; // this line index is not valid
        }
    }

    int8_t printLine (uint8_t lineIndex, char keyPressed) {
        switch (lineIndex) {
            case 0: {
                printf("%02d |", (int)u1_sys::getSw1Value());
                struct u1_app::AdcVoltage *p = &(u1_app::app.adc.voltage);
                printf("%2d %02x %02x %02x %02x | ", p->area, p->th, p->min, p->max, p->period);
                uint16_t frequX256 = u1_app::app.frequX256;
                printf(" %2u.%1uHz |", frequX256 / 256, ((frequX256 & 0xff) * 10) / 256);

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
            
            case 1: {
                struct u1_app::AdcVoltage *p = &(u1_app::app.adc.voltage);
                printf("   | %3d %02x  %02x  %02x  %02x  %02x ", p->area, p->th, u1_app::app.adc.adc1, p->min, p->max, p->peakToPeak);
                printf("| %02x %04x ", p->period, p->periodEwma);
                uint16_t frequX256 = u1_app::app.frequX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %2u.%1uHz ", frequX256 / 256, ((frequX256 & 0xff) * 10) / 256);
                uint16_t vphX256 = u1_app::app.vphX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %3u.%1uV ", vphX256 / 256, ((vphX256 & 0xff) * 10) / 256);
                return 4;
            }
            
            case 2: {
                struct u1_app::AdcSine *p = &(u1_app::app.adc.current.adcSine);
                printf("   | %3d %02x  %02x  %02x  %02x  %02x ", p->area, p->th, u1_app::app.adc.adc0, p->min, p->max, p->peakToPeak);
                printf("| %02x ", u1_app::app.adc.phAngX100us);
                uint16_t currX256 = u1_app::app.currX256 + 13; // 0.05*256 = 12.8 -> 13
                printf("| %3u.%1uA ", currX256 / 256, ((currX256 & 0xff) * 10) / 256);
                return 4;
            }
        
            case 3: {
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

}

#endif // GLOBAL_MONITOR
