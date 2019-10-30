#include "global.h"

#include <stdio.h>
#include <string.h>

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "mon.h"
#include "app.h"
#include "sys.h"

#ifdef GLOBAL_MONITOR

// defines
// ...

// declarations and definations

int8_t mon_cmd_info (uint8_t argc, char *argv[]);
int8_t mon_cmd_test (uint8_t argc, char *argv[]);
int8_t mon_cmd_set (uint8_t argc, char *argv[]);
int8_t mon_cmd_pwm (uint8_t argc, char *argv[]);

const char MON_LINE_WELCOME[] PROGMEM = "Line-Mode: CTRL-X, CTRL-Y, CTRL-C, Return  \n\r";
const char MON_PMEM_CMD_INFO[] PROGMEM = "info\0Application infos\0info";
const char MON_PMEM_CMD_TEST[] PROGMEM = "test\0commando for test\0test";
const char MON_PMEM_CMD_SET[] PROGMEM = "set\0set function/port\0set {k1|k2}  {0|1}";
const char MON_PMEM_CMD_PWM[] PROGMEM = "pwm\0set PWM duty cycle for CP/port\0pwm {0-100}";

const struct Sys_MonCmdInfo MON_PMEMSTR_CMDS[] PROGMEM =
{
    { MON_PMEM_CMD_INFO, mon_cmd_info }
  , { MON_PMEM_CMD_TEST, mon_cmd_test }
  , { MON_PMEM_CMD_SET, mon_cmd_set }
  , { MON_PMEM_CMD_PWM, mon_cmd_pwm }
};

volatile struct Mon mon;

// functions

void mon_init (void)
{
  memset((void *)&mon, 0, sizeof(mon));
}


//--------------------------------------------------------

inline void mon_main (void)
{
}

inline uint8_t mon_getCmdCount (void)
{
  return sizeof(MON_PMEMSTR_CMDS)/sizeof(struct Sys_MonCmdInfo);
}


// --------------------------------------------------------
// Monitor commands of the application
// --------------------------------------------------------

int8_t mon_cmd_info (uint8_t argc, char *argv[])
{
  printf("app.flags_u8  : ");
  sys_printHexBin8(sys.flags_u8);
  sys_newline();
  return 0;
}


int8_t mon_cmd_test (uint8_t argc, char *argv[])
{
  uint8_t i;

  for (i = 0; i<argc; i++)
    printf("%u: %s\n\r", i, argv[i]);

  return 0;
}


int8_t mon_cmd_set (uint8_t argc, char *argv[]) {
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

int8_t mon_cmd_pwm (uint8_t argc, char *argv[]) {
    if (argc != 2) { return -1; }
    int v;
    if (sscanf(argv[1], "%d", &v) != 1) { return -2; }
    if (v < 0 || v > 100) { return -3; }
    if (v == 0) { 
        sys_enablePWM(0);
    } else if (v == 100) {
        sys_enablePWM(1);
        OCR0A = 255; // CP is 12V without PWM
    } else {
        sys_enablePWM(1);
        OCR0A = (255 * v) / 100;
    }
    return 0;

}


// --------------------------------------------------------
// Monitor-Line for continues output
// --------------------------------------------------------

int8_t mon_printLineHeader (uint8_t lineIndex)
{
  if (lineIndex==0)
    sys_printString_P(MON_LINE_WELCOME);
  
  switch (lineIndex)
  {
    case 0: printf("L0 | A0 A1 A2 A6 A7 | Volt | Amps |   VCP | SW1 | STATUS "); return 60;
    case 1: printf("L1 | Frequency\n | period"); return 60;  
    case 2: printf("L2 | Voltage\n   |    RMS | ADC1 MIN MAX   MED   ->V   RMS^2x256"); return 60;
    case 3: printf("L3 | Current\n   |    RMS | ADC0 MIN MAX   MED   ->A   RMS^2x256"); return 60;
    case 4: printf("L4 | Debug"); return 60;
    default: return -1; // this line index is not valid
  }
}

int8_t mon_printLine (uint8_t lineIndex, char keyPressed) {

    switch (lineIndex)
    {
        case 0: {
            cli();
            uint8_t cnt = (volatile uint8_t) app.adc.voltage.lower.cnt;
            uint16_t value = (volatile uint16_t) app.adc.voltage.lower.value;
            uint32_t quadValue = (volatile uint32_t) app.adc.voltage.lower.quadValue;
            uint16_t voltX256 = (volatile uint16_t) app.adc.voltX256;
            sei();
            printf("   |");
            printf(" %04x%04x |", (uint16_t)(app.debug[0] >> 16), (uint16_t)(app.debug[0] & 0xffff));
            printf(" %04x%04x |", (uint16_t)(app.debug[1] >> 16), (uint16_t)(app.debug[1] & 0xffff));
            printf(" %04x%04x |", (uint16_t)(app.debug[2] >> 16), (uint16_t)(app.debug[2] & 0xffff));
            //printf(" %02x %04x %04x%04x |", cnt, value, (uint16_t)(quadValue >> 16), (uint16_t)quadValue);
            printf(" %3dV |", (voltX256 + 128) >> 8);
            printf(" %02x %02x %02x %02x %02x", app.adc.adc0, app.adc.adc1, app.adc.adc2, app.adc.adc6, app.adc.adc7);
            
//            printf("   | %02x %02x %02x %02x %02x | %3dV |  %2dA | %2d.%01dV | ",
//                app.adc.adc0, app.adc.adc1, app.adc.adc2, app.adc.adc6, app.adc.adc7,
//                app.adc.voltage.rmsX256Ewma / 256,
//                app.adc.current.rmsX256Ewma / 256,
//                app.vcpX16 / 16, (app.vcpX16 & 0x0f) * 10 / 16
//            );
            
            
            printf(" %02d |", sys_getSw1Value());
            switch (app.state) {
                case Init:         printf("Init"); break;
                case Test:         printf("Test"); break;
                case Start:        printf("Start"); break;
                case NotConnected: printf("NotConnected"); break;
                case Connected:    printf("Connected"); break;
                case EVReady:      printf("EVReady"); break; 
                case Charging:     printf("Charging"); break;
                default:           printf("? (%d)", app.state);
             }
            return 10;
        }
        
        case 1: {
            cli();
            uint8_t period = app.adc.frequ.period;
            uint16_t periodEwma = app.adc.frequ.periodEwma;
            uint16_t frequX256 = app.frequX256;
            uint32_t accVoltage = app.adc.volt.accVoltage;
            sei();
            printf("   | %3u %5u %04x  %04x%04x", period, periodEwma, frequX256, (uint16_t)(accVoltage >> 16), (uint16_t)accVoltage);
            
            printf(" %2u.%03uHz ", frequX256 / 256, (uint16_t)( (((uint32_t)(frequX256 & 0xff)) * 1000) / 256) );
            
            frequX256 += 1;
            printf(" %2u.%02uHz ", frequX256 / 256, ((frequX256 & 0xff) * 100) / 256);
            frequX256 -= 1;
            
            frequX256 += 13;
            printf(" %2u.%1uHz ", frequX256 / 256, ((frequX256 & 0xff) * 10) / 256);
            
            return 10;
        }
        
        case 2: {
//            cli();
//            uint16_t v = app.adc.voltage.rmsX256Ewma;
//            sei();
//            printf("   | %3d.%01dV |   %02x  %02x  %02x %02x.%02x ",
//                v / 256, ((v &0xff) * 10) / 256, app.adc.adc1, app.adc.voltage.adcMin[0], app.adc.voltage.adcMax[0],
//                app.adc.voltage.medX256 >> 8, app.adc.voltage.medX256 & 0xff);
//            printf(" %3dV    %04x%04x",
//                app.adc.voltage.rmsX256 / 256, 
//                (uint16_t)(app.adc.voltage.rmsPow2X256 >> 16), (uint16_t)(app.adc.voltage.rmsPow2X256 & 0xffff)
//            );
            return 2;
        }
      
        case 3: {
//            cli();
//            uint16_t v = app.adc.current.rmsX256Ewma;
//            sei();
//            printf("   | %3d.%01dA |   %02x  %02x  %02x %02x.%02x ",
//                v / 256, ((v &0xff) * 10) / 256, app.adc.adc0, app.adc.current.adcMin[0], app.adc.current.adcMax[0],
//                app.adc.current.medX256 >> 8, app.adc.current.medX256 & 0xff);
//            printf(" %3dA    %04x%04x",
//                app.adc.current.rmsX256 / 256, 
//                (uint16_t)(app.adc.current.rmsPow2X256 >> 16), (uint16_t)(app.adc.current.rmsPow2X256 & 0xffff)
//            );
            return 2;
        }
    
        case 4: {
            for (int i = 0; i < 3; i++) {
                printf(" %04x%04x", (uint16_t)(app.debug[i] >> 16), (uint16_t)(app.debug[i] & 0xffff));
            }
            return 2;
        }
    
        default: return -1;
    }
}

#endif // GLOBAL_MONITOR



