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

const char MON_LINE_WELCOME[] PROGMEM = "Line-Mode: CTRL-X, CTRL-Y, CTRL-C, Return  \n\r";
const char MON_PMEM_CMD_INFO[] PROGMEM = "info\0Application infos\0info";
const char MON_PMEM_CMD_TEST[] PROGMEM = "test\0commando for test\0test";

const struct Sys_MonCmdInfo MON_PMEMSTR_CMDS[] PROGMEM =
{
    { MON_PMEM_CMD_INFO, mon_cmd_info }
  , { MON_PMEM_CMD_TEST, mon_cmd_test }
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


// --------------------------------------------------------
// Monitor-Line for continues output
// --------------------------------------------------------

int8_t mon_printLineHeader (uint8_t lineIndex)
{
  if (lineIndex==0)
    sys_printString_P(MON_LINE_WELCOME);
  
  switch (lineIndex)
  {
    case 0: printf("L0 | A0 A1 A2 A6 A7 | Volt | Amps |   VCP | STATUS "); return 40;
    case 1: printf("L1 | Voltage\n   |    RMS | ADC1 MIN MAX   MED   ->V   RMS^2x256"); return 60;
    case 2: printf("L2 | Current\n   |    RMS | ADC0 MIN MAX   MED   ->A   RMS^2x256"); return 60;
    case 3: printf("L3 | Debug"); return 60;
    default: return -1; // this line index is not valid
  }
}

int8_t mon_printLine (uint8_t lineIndex, char keyPressed) {

    switch (lineIndex)
    {
        case 0: {
            printf("   | %02x %02x %02x %02x %02x | %3dV |  %2dA | %2d.%01dV | ",
                app.adc.adc0, app.adc.adc1, app.adc.adc2, app.adc.adc6, app.adc.adc7,
                app.adc.voltage.rmsX256Ewma / 256,
                app.adc.current.rmsX256Ewma / 256,
                app.vcpX16 / 16, (app.vcpX16 & 0x0f) * 10 / 16
            );
            switch (app.evStatus) {
                case APP_EVSTATUS_OFF:          printf("OFF"); break;
                case APP_EVSTATUS_NOTCONNECTED: printf("EV not connected"); break;
                case APP_EVSTATUS_CONNECTED:    printf("EV connected"); break;
                case APP_EVSTATUS_LOADING:      printf("EV loading"); break;
                default:                        printf("?"); break;
            }
            return 20;
        }
        
        case 1: {
            cli();
            uint16_t v = app.adc.voltage.rmsX256Ewma;
            sei();
            printf("   | %3d.%01dV |   %02x  %02x  %02x %02x.%02x ",
                v / 256, ((v &0xff) * 10) / 256, app.adc.adc1, app.adc.voltage.adcMin[0], app.adc.voltage.adcMax[0],
                app.adc.voltage.medX256 >> 8, app.adc.voltage.medX256 & 0xff);
            printf(" %3dV    %04x%04x",
                app.adc.voltage.rmsX256 / 256, 
                (uint16_t)(app.adc.voltage.rmsPow2X256 >> 16), (uint16_t)(app.adc.voltage.rmsPow2X256 & 0xffff)
            );
            return 2;
        }
      
        case 2: {
            cli();
            uint16_t v = app.adc.current.rmsX256Ewma;
            sei();
            printf("   | %3d.%01dA |   %02x  %02x  %02x %02x.%02x ",
                v / 256, ((v &0xff) * 10) / 256, app.adc.adc0, app.adc.current.adcMin[0], app.adc.current.adcMax[0],
                app.adc.current.medX256 >> 8, app.adc.current.medX256 & 0xff);
            printf(" %3dA    %04x%04x",
                app.adc.current.rmsX256 / 256, 
                (uint16_t)(app.adc.current.rmsPow2X256 >> 16), (uint16_t)(app.adc.current.rmsPow2X256 & 0xffff)
            );
            return 2;
        }
    
        case 3: {
            for (int i = 0; i < 3; i++) {
                printf(" %04x%04x", (uint16_t)(app.debug[i] >> 16), (uint16_t)(app.debug[i] & 0xffff));
            }
            return 2;
        }
    
        default: return -1;
    }
}

#endif // GLOBAL_MONITOR



