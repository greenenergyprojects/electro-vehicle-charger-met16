#include "global.h"

#include <stdio.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "app.h"
#include "mon.h"
#include "sys.h"

// defines
// ...


// declarations and definations
void    app_refreshLcd (void);
uint8_t app_ewmaVcp (uint8_t value);
void    app_calcRms (struct App_AdcRms *p);

struct App app;


// functions

void app_init (void) {
    memset((void *)&app, 0, sizeof(app));
    app.disableStatusLED = 0;
    DDRD |= (1 << PD6);
    app.trim.vcpK = 13; app.trim.vcpD = 0;
    app.adc.voltage.corrK = 1630;
    app.adc.voltage.corrD = 0;
    app.adc.voltage.zeroAdcRange = 10;
    app.adc.current.corrK = 42;
    app.adc.current.corrD = 0;
    app.adc.current.zeroAdcRange = 0;
}


//--------------------------------------------------------

void app_main (void)
{
    //printf("SW1: %d     SW2: %d  \r", sys_getSw1Value(), sys_isSw2On(), TCNT1, app.cnt);
    app_calcRms(&app.adc.current);
    app_calcRms(&app.adc.voltage);
    
    if (!sys_isSw2On()) {
        sys_enablePWM(0);
        OCR0A = 0;
        app.maxAmps = 0;
        app.evStatus = APP_EVSTATUS_OFF;
    } else {
        sys_enablePWM(1);
        switch (sys_getSw1Value()) {
            case 7:  app.maxAmps =  7; break;
            case 8:  app.maxAmps =  8; break;
            case 9:  app.maxAmps =  9; break;
            case 0:  app.maxAmps = 10; break;
            case 1:  app.maxAmps = 11; break;
            case 2:  app.maxAmps = 12; break;
            case 3:  app.maxAmps = 13; break;
            case 4:  app.maxAmps = 14; break;
            case 5:  app.maxAmps = 15; break;
            case 6:  app.maxAmps = 16; break;
            default: app.maxAmps = 0; break;
        }
        if (app.vcpX16 >= 10 * 16) { // VCP >= 10V
            OCR0A = 255; // CP is 12V without PWM
            app.evStatus = APP_EVSTATUS_NOTCONNECTED;
        } else if (app.vcpX16 >= (75 * 16 / 10)) { // VCP >= 7.5V
            // EV connected, switch on PWM on CP
            OCR0A = app.pwmOn ? ((app.maxAmps * 255) / 6 + 5) / 10 : 255;
            app.evStatus = APP_EVSTATUS_CONNECTED;
        } else {
            // EV loading, switch on PWM on CP
            OCR0A = app.pwmOn ? ((app.maxAmps * 255) / 6 + 5) / 10 : 255;
            app.evStatus = APP_EVSTATUS_LOADING;
        }
        
    }

    if (sys_clearEvent(APP_EVENT_REFRESH_LCD))
        app_refreshLcd();
    
}

void app_refreshLcd (void) { // called from main, 6ms
    sys_lcd_setCursorPosition(0, 0);
    sys_lcd_putString("IMAX=");
    if (!sys_isSw2On()) {
        sys_lcd_putString(" 0A");
    
    } else {
        char s[5];
        snprintf(s, sizeof(s), "%2dA", app.maxAmps);
        sys_lcd_putString(s);
    }
    
    sys_lcd_putString("  ");
    switch (app.evStatus) {
        case APP_EVSTATUS_OFF:          sys_lcd_putString("OFF       "); break;
        case APP_EVSTATUS_NOTCONNECTED: sys_lcd_putString("NO EV     "); break;
        case APP_EVSTATUS_CONNECTED:    sys_lcd_putString("EV OK     "); break;
        case APP_EVSTATUS_LOADING:      sys_lcd_putString("  E=???.?kWh"); break;
    }
    
    sys_lcd_setCursorPosition(1, 0);
    {
        char s[21];
        snprintf(s, sizeof(s), "U=%3dV I=%2dA P=%4dW",
            (app.adc.voltage.rmsX256Ewma + 128) / 256,
            (app.adc.current.rmsX256Ewma + 128) / 256,
            (int)(((uint32_t)app.adc.voltage.rmsX256Ewma * app.adc.current.rmsX256Ewma + 32768) / 65536)
        );
        sys_lcd_putString(s);
    }
}

uint8_t app_ewmaVcp (uint8_t value) {
    static uint16_t s = 0;
    s = s - (s >> 3);    // s = 0..63488 (0xf800), real 0..63160 (0xf6b8)
    s += value * 32;      // s = 0..65528 (0xfff8), real 0..65200 (0xfeb0)
    return (s + 128) >> 8;
}

void app_reinitRms (struct App_AdcRms *p) { // called in ISR, 1.5us
    p->acc = 0;
    p->accSquare = 0;
}

void app_accumulateRms (struct App_AdcRms *p, uint8_t value) { // called in ISR, 10us
    if (value < p->adcMin[1]) p->adcMin[1] = value;
    if (value > p->adcMax[1]) p->adcMax[1] = value;
    p->acc += value;
    int32_t valueX256 = ((uint32_t)value) * 256 - p->medX256;
    p->accSquare += (valueX256 * valueX256) / 256;
}

void app_calcRmsPow2X256 (struct App_AdcRms *p) { // called in ISR, 11us
    p->rmsPow2X256 = ((uint32_t)(p->accSquare / 256) * 655 + 128);
    p->medX256 = (uint16_t)((((uint32_t)p->acc) * 655 + 32768) / 256);
    
    app.debug[0] = p->medX256;
    app.debug[1] = p->accSquare;
    app.debug[2] = p->rmsPow2X256;
}

void app_calcRms (struct App_AdcRms *p) { // called from main, 1.5ms
    p->adcMin[0] = p->adcMin[1]; p->adcMin[1] = 255;
    p->adcMax[0] = p->adcMax[1]; p->adcMax[1] = 0;
    if (p->adcMax[0] - p->adcMin[0] < p->zeroAdcRange) {
        p->rmsX256 = 0;
    } else {
        cli();
        uint32_t tmp = p->rmsPow2X256 / 256;
        sei();
        tmp = tmp * 655 + 128;
    
        // calculate square root by calculating power of 2 and compare with target value
        for (uint8_t i = 0; i < 254; i++) {
            uint32_t x1 = p->adcrmsX256;
            uint32_t x2 = x1 * x1;
            if (tmp > x2) {
                if ((tmp / 4) > x2 && p->adcrmsX256 < 0x8000) {
                    p->adcrmsX256 = p->adcrmsX256 * 2 + 1;
                } else if (p->adcrmsX256 < 0xffff) {
                    p->adcrmsX256++; 
                } else {
                    break;
                }
            } else if (tmp < x2) {
                if (tmp < (x2 / 4) && p->adcrmsX256) {
                    p->adcrmsX256 /= 2;
                } else if (p->adcrmsX256 > 0) {
                    p->adcrmsX256--; 
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        // calculate correct value with linear correction (factor k,d)
        p->rmsX256 = (uint16_t)(((uint32_t)p->adcrmsX256) * p->corrK / 256 + p->corrD);
    }    
    
    // use ewma filter to get quiet value
    int32_t d = (int32_t)p->rmsX256Ewma - p->rmsX256;
    d = d < 0 ? -d : d;
    if (d > (10 * 256)) {
        // filtered value completly wrong, reinit filter
        p->rmsX256Ewma = p->rmsX256;
    } else {
        int32_t x = (p->rmsX256Ewma - p->rmsX256Ewma / 32) + p->rmsX256 / 32;
        if (x > 0xffff) p->rmsX256Ewma = x;
        else if (x < 0) p->rmsX256Ewma = 0;
        else p->rmsX256Ewma = (uint16_t)x;
    }
}

uint8_t app_handleADCValue (uint8_t channel, uint8_t result) {
    app.cnt++;
    switch(channel) {
        case 0: { // current 
            app.adc.cnt++;
            app_accumulateRms(&app.adc.current, result);
            app.adc.adc0 = result;
            return 1;
        }
            
        case 1: { // voltage
            app_accumulateRms(&app.adc.voltage, result);
            app.adc.adc1 = result;
            if (app.adc.cnt < 100) {
                return 0;
            }
            return 2;
        }
            
        case 2: { // -12V verification
            app.adc.adc2 = result;
            // handle current measurement
            app_calcRmsPow2X256(&app.adc.current);
            return 6;
        }
            
        case 6: { // CP voltage level
            app.adc.adc6 = result;
            app.vcpX16 = app_ewmaVcp( (result * app.trim.vcpK + (uint16_t)app.trim.vcpD) / 16 );
            // handle voltage measurement
            app_calcRmsPow2X256(&app.adc.voltage);
            return 7;    
        }

        case 7: // +12V verification
            app.adc.adc7 = result;
            
            // prepare current/voltage measurement
            app.adc.cnt = 0;
            app_reinitRms(&app.adc.current);
            app_reinitRms(&app.adc.voltage);
            return 0;    
            
    }
    return 0;
}

//--------------------------------------------------------

void app_task_1ms (void) {
}

void app_task_2ms (void) {}
void app_task_4ms (void) {}

void app_task_8ms (void) {
    static uint8_t timer = 0;
    
    timer++;
    if (timer > 125) {
        timer = 0;
    }
    
    if (app.disableStatusLED)
        return;
    
    if (timer < 62) {
        sys_setLedD3(1);
    } else {
        sys_setLedD3(0);
    }
    
    switch (app.evStatus) {
        case APP_EVSTATUS_OFF:          sys_setLedD2(0); break;
        case APP_EVSTATUS_NOTCONNECTED: sys_setLedD2(0); break;
        case APP_EVSTATUS_CONNECTED:    sys_setLedD2(0); break;
        case APP_EVSTATUS_LOADING:      sys_setLedD2(1); break;
        default:                        sys_setLedD2(0); break;
    }

}

void app_task_16ms (void) {}

void app_task_32ms (void) {
    static uint8_t switchTimer = 0;
    if (!sys_isSw2On()) {
        switchTimer = 0;
    } else if (switchTimer < 255) {
       switchTimer++; 
    }
   app.pwmOn = switchTimer > 30;
}

void app_task_64ms (void) {
}

void app_task_128ms (void) {
    static uint8_t timer = 0;
    timer++;
    if (timer >= 5) {
        timer = 0;
        sys_setEvent(APP_EVENT_REFRESH_LCD);
    }
}

