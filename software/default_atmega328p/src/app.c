#include "global.h"

#include <stdio.h>
#include <stdlib.h>
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
void    app_calcVoltage ();

struct App app;


// functions

void app_init (void) {
    memset((void *)&app, 0, sizeof(app));
    app.state = Init;
    DDRD |= (1 << PD6); // PWM output
    // app.adc.volt.thValue = 0x80;
    app.adc.voltage.thValue = 0x80;
    app.adc.voltage.thWidth = 0x4;
    app.adc.current.thValue = 0x80;
    app.adc.current.thWidth = 0x4;
    
    
//    app.trim.vcpK = 13; app.trim.vcpD = 0;
//    app.adc.voltage.corrK = 1630;
//    app.adc.voltage.corrD = 0;
//    app.adc.voltage.zeroAdcRange = 10;
//    app.adc.current.corrK = 42;
//    app.adc.current.corrD = 0;
//    app.adc.current.zeroAdcRange = 0;
}


//--------------------------------------------------------

void app_main (void)
{
    // app_calcRms(&app.adc.current);
    // app_calcRms(&app.adc.voltage);
    if (sys_clearEvent(APP_EVENT_REFRESH_LCD)) {
        app_refreshLcd();
    }
    if (sys_clearEvent(APP_EVENT_CALC_VOLT_UPP)) {
        
    }
}

void app_refreshLcd (void) { // called from main, 6ms
    char s[21];    
    sys_lcd_setCursorPosition(0, 0);
    {
        snprintf(s, sizeof(s), "%dA", app.maxAmps);
        sys_lcd_putString(s);
        if (app.maxAmps < 10) {
            sys_lcd_putString(" ");
        }
        sys_lcd_putString(" ");
    
        switch (app.state) {
            case Init:         sys_lcd_putString("INIT   "); break;
            case Test:         sys_lcd_putString("TEST   "); break;
            case Start:        sys_lcd_putString("START  "); break;
            case NotConnected: sys_lcd_putString("12V/---"); break;
            case Connected:    sys_lcd_putString(" 9V/EV "); break;
            case EVReady:      sys_lcd_putString(" 6V/RDY"); break; 
            case Charging:     sys_lcd_putString(" 6V/CHG"); break;
            default:           sys_lcd_putString("?      "); break;
        }
        if (app.energy < 0) {
            s[0] = 0;
        } else if (app.energy < 1000) {
            snprintf(s, sizeof(s), "E=%dWh", (int)app.energy);
        } else {
            div_t d = div((int)app.energy, 1000);
            snprintf(s, sizeof(s), "%d.%02dkWh", d.quot, d.rem / 10);
        }
        int8_t x = 9 - strlen(s);
        while (x-- > 0) {
            sys_lcd_putString(" ");
        }
        sys_lcd_putString(s);
        
    }
    
    sys_lcd_setCursorPosition(1, 0);
    {
//        snprintf(s, sizeof(s), "U=%3dV I=%2dA P=%4dW",
//            (app.adc.voltage.rmsX256Ewma + 128) / 256,
//            (app.adc.current.rmsX256Ewma + 128) / 256,
//            (int)(((uint32_t)app.adc.voltage.rmsX256Ewma * app.adc.current.rmsX256Ewma + 32768) / 65536)
//        );
        sys_lcd_putString(s);
    }
}

uint16_t app_ewmaPeriod (uint8_t value) {
    
    static uint16_t s = 25600;
    if (value == 0) {
        s = 0;
    } else if (s == 0) {
        s = 25600; // = 200 (value@50Hz) * 128
    } else {
        // s = k*a*value + (1-1/a)*s -> k=128, a=1/64, s(t>>tau)=k*value -> tau=256ms(@dt=4ms), s=25600(@f=50Hz)
        s = s - (s >> 6);           // s = 0..64512 (0xfc00), real 0..64007 (0xfa07)
        s += (uint16_t)value * 2;   // s = 0..65022 (0xfdfe), real 0..64517 (0xfc05)
    }
    return s;
}

uint8_t app_ewmaVcp (uint8_t value) {
    static uint16_t s = 0;
    s = s - (s >> 3);    // s = 0..63488 (0xf800), real 0..63160 (0xf6b8)
    s += (uint16_t)value * 32;      // s = 0..65528 (0xfff8), real 0..65200 (0xfeb0)
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

void app_task2ms_calcVoltage () {
    sys_setLedD3(1);
    static uint32_t s = 230L << 16;
    cli();
    uint16_t value = (volatile uint16_t)app.adc.voltage.lower.value;
    uint8_t cnt = (volatile uint8_t)app.adc.voltage.lower.cnt;
    sei();
    if (cnt > 0) {
        uint16_t oldValue =  value;
        value = value < 0xfff ? value << 4 : 0xfff0;
        value = value / cnt;
        s = s - (s / 64) + (uint32_t)256 * value;
        app.debug[0] = s;
        app.debug[1] = ((uint32_t)oldValue << 16) | value;
        app.debug[2] = cnt;
        app.adc.voltX256 = (uint16_t)(s / 256);
        app.adc.voltage.lower.cnt = 0;
    } else {
        app.adc.voltX256 = 0;
    }
    sys_setLedD3(0);
}

int8_t app_100us_calcAdcMeas (struct App_ADC_MEAS *pm, uint8_t adcValue) {
    int8_t rv = 0; // 0 -> no change of curve side (upper or lower)
    // sys_setLedD3(1);
    if (pm->upperCurv) {
        uint8_t th = pm->thValue - pm->thWidth;
        rv = (adcValue < th) ? -1 : 0; // change to lower curve side
    } else {
        uint8_t th = pm->thValue + pm->thWidth;
        rv = (adcValue > th) ? 1 : 0; // change to upper curve side
    }
    
    if (rv == 0) {
        // no change from curve side
        struct App_ADC_MEAS_UPLO *pul = (pm->upperCurv) ? &(pm->upper) : &(pm->lower);
        if (pul->accCnt < 0xff) {
            pul->accCnt++;
            uint8_t x = (adcValue >= pm->thValue) ? adcValue - pm->thValue : pm->thValue - adcValue;
            pul->accValue += x;
            pul->accQuadValue += (x * x);
        }
        
    } else {
        // adjust threshhold to the middle between lower and upper curve side
        if (pm->lower.accCnt < pm->upper.accCnt) {
            // upper curve sider too long -> increase threshhold
            if (pm->thValue < 0xff) {
                pm->thValue++;
            }
        } else if (pm->lower.accCnt > pm->upper.accCnt){
            // upper curve sider too short -> decrease threshhold
            if (pm->thValue > 0) {
                pm->thValue--;
            }
        }
        
        struct App_ADC_MEAS_UPLO *pul = (rv > 0) ? &(pm->lower) : &(pm->upper);
        pul->cnt = pul->accCnt;
        pul->value = pul->accValue;
        pul->quadValue = pul->accQuadValue;
        
//        sys_setLedD3(1);
//        struct App_ADC_MEAS_UPLO *pul = (rv > 0) ? &(pm->lower) : &(pm->upper);
//        pul->cnt = pul->accCnt;
//        uint32_t x1 = pul->accCnt * (uint32_t)pul->value;
//        uint32_t x2 = (uint32_t)(x1 / 256);
//        int32_t diff = ((int32_t)x2) - pul->accValue;
//        if (diff < -256) {
//            pul->value += 128;
//        } else if (diff >= 256) {
//            pul->value -= 128;
//        } else if (diff < 0) {
//            pul->value++;
//        } else if (diff > 0) {
//            pul->value--;
//        }
//        sys_setLedD3(0);
        //pul->value = pul->accValue;
        //pul->quadValue = pul->accQuadValue;
        //pul->quadValue = pul->accValue;
        //pul->quadValue = (uint32_t)x1;
        
        pm->upperCurv = (rv > 0);
        
        // reset accisition values for starting half
        pul = (rv > 0) ? &(pm->upper) : &(pm->lower);
        pul->accCnt = 0;
        pul->accValue = 0;
        pul->accQuadValue = 0;
    }
    
    // sys_setLedD3(0);
    return rv;
}

uint8_t app_handleADCValue (uint8_t channel, uint8_t result) {
    static uint8_t frequCnt = 0;
    if (frequCnt < 0xff) {
        frequCnt++;
    } else {
        app.adc.frequ.period = 0;
        sys_setEvent(APP_EVENT_CALC_FREQU);
    }
    app.cnt++;
    uint8_t nextChannel;

    switch(channel) {
        case 0: { // current 
            app.adc.cnt++;
            app.adc.adc0 = result;
            int8_t change = app_100us_calcAdcMeas(&app.adc.current, result);
            if (change > 0) {
                sys_setEvent(APP_EVENT_CALC_CURR_LOW);
            } else if (change < 0) {
                sys_setEvent(APP_EVENT_CALC_CURR_UPP);
            }
            nextChannel = 1;
            break;
        }
            
        case 1: { // voltage
            // app_accumulateRms(&app.adc.voltage, result);
            app.adc.adc1 = result;
            int8_t change = app_100us_calcAdcMeas(&app.adc.voltage, result);
            if (change > 0) {
                sys_setK2(1);
                sys_setEvent(APP_EVENT_CALC_VOLT_LOW);
                app.adc.frequ.period = frequCnt;
                sys_setEvent(APP_EVENT_CALC_FREQU);
                frequCnt = 0;
            } else if (change < 0) {
                sys_setK2(0);
                sys_setEvent(APP_EVENT_CALC_VOLT_UPP);
            }

            nextChannel = 0;
            break;
//            
//            if (app.adc.volt.upperCurv == (result <= app.adc.volt.thValue)) {
//                // change between upper and lower curve range detected
//                app.adc.volt.thCnt = 2;
//                app.adc.volt.upperCurv = (result > app.adc.volt.thValue);
//                if (result > app.adc.volt.thValue) {
//                    app.adc.volt.cntUpper = 0;
//                    app.adc.frequ.period = frequCnt;
//                    sys_setEvent(APP_EVENT_CALC_FREQU);
//                    frequCnt = 0;
//                    
//                } else {
//                    app.adc.volt.cntLower = 0;
//                }
//
//            }                    
//
//            static uint32_t accVoltage;
//            static uint8_t accCnt;
//            if (app.adc.volt.thCnt > 0) {
//                app.adc.volt.thCnt--;
//            } else if (app.adc.volt.thCnt == 0) {
//                app.adc.volt.thCnt--;
//                if (app.adc.volt.cntLower < app.adc.volt.cntUpper) {
//                    app.adc.volt.thValue++;
//                } else if (app.adc.volt.cntLower > app.adc.volt.cntUpper){
//                    app.adc.volt.thValue--;
//                }
//                if (accCnt > 0) {
//                    app.adc.volt.accVoltage = accVoltage;
//                    accCnt = 0;
//                }
//                accVoltage = 0;
//                
//            } else {
//                sys_toggleLedD3();
//                if (result > app.adc.volt.thValue) {
//                    if (app.adc.volt.cntUpper < 0xff) {
//                        sys_setK2(1);
//                        app.adc.volt.cntUpper++;
//                    }
//                } else {
//                    if (app.adc.volt.cntLower < 0xff) {
//                        sys_setK2(0);
//                        app.adc.volt.cntLower++;
//                    }
//                    accCnt++;
//                    uint8_t x = app.adc.volt.thValue - result;
//                    accVoltage +=  x * x;
//                }                
//            } 
            
        }
            
        case 2: { // -12V verification
            app.adc.adc2 = result;
            // handle current measurement
            // app_calcRmsPow2X256(&app.adc.current);
            nextChannel = 6;
            break;
        }
            
        case 6: { // CP voltage level
            app.adc.adc6 = result;
            app.vcpX16 = app_ewmaVcp( (result * app.trim.vcpK + (uint16_t)app.trim.vcpD) / 16 );
            // handle voltage measurement
            // app_calcRmsPow2X256(&app.adc.voltage);
            nextChannel = 7;
            break;
        }

        case 7: // +12V verification
            app.adc.adc7 = result;
            
            // prepare current/voltage measurement
            app.adc.cnt = 0;
            // app_reinitRms(&app.adc.current);
            // app_reinitRms(&app.adc.voltage);
            nextChannel = 0;
            break;
        
        default:
            nextChannel = 0;
            break;
            
    }
    
    return nextChannel;
}

//--------------------------------------------------------

void app_task_1ms (void) {
    static uint16_t timer = 0;
    if (timer > 0) {
        timer--;
    }
    
    enum APP_STATE nextState = app.state;
    switch (sys_getSw1Value()) {
        case 0:  app.maxAmps =  0; nextState = Test; break;
        case 1:  app.maxAmps =  8; break;
        case 2:  app.maxAmps =  9; break;
        case 3:  app.maxAmps = 10; break;
        case 4:  app.maxAmps = 12; break;
        case 5:  app.maxAmps = 13; break;
        case 6:  app.maxAmps = 14; break;
        case 7:  app.maxAmps = 15; break;
        case 8:  app.maxAmps = 16; break;
        case 9:  app.maxAmps = 17; break;
        default: app.maxAmps = 0; break;
    }

    uint8_t pwmOC = ((app.maxAmps * 255) / 6 + 5) / 10;
    app.disableStatusLED = 0;
    
    switch (nextState) {
        case Init: {
            sys_setK1(0);
            sys_enablePWM(0);
            OCR0A = 0;
            app.maxAmps =  0;
            timer = 1500;
            nextState = Start;
            break;
        }

        case Test: {
            sys_setK1(1);
            app.disableStatusLED = 1;
            sys_enablePWM(0);
            OCR0A = 0;
            app.maxAmps =  0;
            if (sys_getSw1Value() != 0) {
                timer = 1500;
                nextState = Start;
            }
            break;
        }

        case Start: {
            sys_setK1(0);
            sys_enablePWM(1);
            OCR0A = 255;
            if (timer == 0) {
                if (app.vcpX16 >= (100 * 16 / 10)) { // 7.5V
                    nextState = NotConnected;
                } else if (app.vcpX16 >= (75 * 16 / 10)) {
                    nextState = Connected;
                } else {
                    nextState = Init; // error
                }
            }
            break;
        }
        
        case NotConnected: {
            sys_setK1(0);
            sys_enablePWM(1);
            OCR0A = pwmOC;
            if (app.vcpX16 >= (100 * 16 / 10)) { // 10V
                nextState = NotConnected;
            } else if (app.vcpX16 >= (75 * 16 / 10)) { // 7.5V
                nextState = Connected;
            } else {
                nextState = Init; // error
            }
            break;
        }

        case Connected: {
            sys_setK1(0);
            sys_enablePWM(1);
            OCR0A = pwmOC;
            if (app.vcpX16 >= (100 * 16 / 10)) { // 10.5V
                // typ. 12V
                nextState = NotConnected; 
            } else if (app.vcpX16 >= (75 * 16 / 10)) { // 7.5V
                // typ. 9V
                nextState = Connected;
            } else if (app.vcpX16 >= (40 * 16 / 10)) { // 4.0V
                // typ. 6V
                timer = 1000;
                nextState = EVReady;
            } else {
                // typ. 3V (Charging with air ventilation)
                nextState = Init; // error
            }
            break;
        }
        
        case EVReady: {
            sys_setK1(0);
            sys_enablePWM(1);
            OCR0A = pwmOC;
            if (app.vcpX16 >= (100 * 16 / 10)) { // 10.5V
                // typ. 12V
                if (timer == 0) {
                    nextState = NotConnected; 
                }
            } else if (app.vcpX16 >= (75 * 16 / 10)) { // 7.5V
                // typ. 9V
                if (timer == 0) {
                    nextState = Connected;
                }
            } else if (app.vcpX16 >= (40 * 16 / 10)) { // 4.0V
                // typ. 6V
                if (timer == 0) {
                    sys_setK1(1);
                    timer =  500;
                    nextState = Charging;
                }
            } else {
                // typ. 3V (Charging with air ventilation)
                if (timer == 0) {
                    nextState = Init; // error
                }
            }
            break;
        }
        
        case Charging: {
            sys_enablePWM(1);
            OCR0A = pwmOC;
            if (app.vcpX16 >= (100 * 16 / 10)) { // 10.5V
                // typ. 12V
                if (timer == 0) {
                    sys_setK1(0);
                    nextState = NotConnected; 
                }
            } else if (app.vcpX16 >= (75 * 16 / 10)) { // 7.5V
                // typ. 9V
                if (timer == 0) {
                    sys_setK1(0);
                    nextState = Connected;
                }
            } else if (app.vcpX16 >= (40 * 16 / 10)) { // 4.0V
                // typ. 6V
                sys_setK1(1);
                timer =  500;
                nextState = Charging;
            } else {
                // typ. 3V (Charging with air ventilation)
                if (timer == 0) {
                    sys_setK1(0);
                    nextState = Init; // error
                }
            }
            break;
        }
        
        
        default: {
            nextState = Init;
            break;
        }
    }
    app.state = nextState;
}

void app_task_2ms (void) {
    static uint8_t timer = 0;
    timer = timer > 0 ? timer - 1: 0;
    if (sys_clearEvent(APP_EVENT_CALC_VOLT_LOW) || timer == 0) {
        app_task2ms_calcVoltage();
        timer = 50;
    }
}

void app_task_4ms (void) {
    uint16_t periodEwma = app_ewmaPeriod(app.adc.frequ.period);
    app.adc.frequ.periodEwma = periodEwma;
    if (periodEwma == 0) {
        app.frequX256 = 0;
    } else {
        int32_t diff_i32 = (int32_t)periodEwma - 25600;
        int8_t d_i8 = (diff_i32 < -128) ? -128 : ( (diff_i32 > 127) ? 127 : (int8_t)diff_i32 );
        app.frequX256 = 0x3200 - (d_i8 / 2); // 0x32 --> 50Hz
    }
    if (sys_clearEvent(APP_EVENT_CALC_FREQU)) {
        
    }
}

void app_task_8ms (void) {
    static uint8_t timer = 0;
    
    timer++;
    if (timer > 125) {
        timer = 0;
    }
    
    switch (app.state) {
        case Init:         break;
        case Test:         break;
        case Start:        break;
        case NotConnected: break;
        case Connected:    break;
        case EVReady:      break; 
        case Charging:     break;
        default:           break;
    }
    
    if (app.disableStatusLED)
        return;
    
    if (timer < 62) {
        sys_setLedD3(1);
    } else {
        sys_setLedD3(0);
    }

}

void app_task_16ms (void) {}

void app_task_32ms (void) {}

void app_task_64ms (void) {
//    if (app.state == Charging) {
//        if (app.energy < 0x7ff0) {
//            app.energy += 10;
//        }
//    } else {
//        app.energy = 0;;
//    }
}

void app_task_128ms (void) {
    static uint8_t timer = 0;
    timer++;
    if (timer >= 5) {
        timer = 0;
        sys_setEvent(APP_EVENT_REFRESH_LCD);
    }
    if (app.smTimer > 0) {
        app.smTimer--;
    }
}

