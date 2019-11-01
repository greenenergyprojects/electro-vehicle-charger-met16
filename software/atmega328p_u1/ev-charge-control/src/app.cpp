#include "global.h"
#include "sys.hpp"
#include "app.hpp"

#include <string.h>

namespace u1_app {

    struct App app;

    // declarations and definitions
    void app_refreshLcd ();

    // ------------------------------------------------------------------------

    void init () {
        memset(&app, 0, sizeof app);
        app.state = Init;
        DDRD |= (1 << PD6); // PWM output
    }

    void main () {
        if (u1_sys::clearEvent(EVENT_REFRESH_LCD)) {
            app_refreshLcd();
        }
    }

    void app_refreshLcd () { // called from main, 6ms
        char s[21];    
        u1_sys::lcd_setCursorPosition(0, 0);
        {
            snprintf(s, sizeof(s), "%dA", app.maxAmps);
            u1_sys::lcd_putString(s);
            if (app.maxAmps < 10) {
                u1_sys::lcd_putString(" ");
            }
            u1_sys::lcd_putString(" ");
    
            switch (app.state) {
                case Init:         u1_sys::lcd_putString("INIT   "); break;
                case Test:         u1_sys::lcd_putString("TEST   "); break;
                case Start:        u1_sys::lcd_putString("START  "); break;
                case NotConnected: u1_sys::lcd_putString("12V/---"); break;
                case Connected:    u1_sys::lcd_putString(" 9V/EV "); break;
                case EVReady:      u1_sys::lcd_putString(" 6V/RDY"); break; 
                case Charging:     u1_sys::lcd_putString(" 6V/CHG"); break;
                default:           u1_sys::lcd_putString("?      "); break;
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
                u1_sys::lcd_putString(" ");
            }
            u1_sys::lcd_putString(s);
        
        }
    
        u1_sys::lcd_setCursorPosition(1, 0);
        {
            struct u1_app::AdcVoltage *p = &(u1_app::app.adc.voltage);
            uint16_t vphX256 = u1_app::app.vphX256 + 128; // 0.5*256 = 128
            uint16_t frequX256 = u1_app::app.frequX256 + 13; // 0.05*256 = 12.8 -> 13
            uint16_t currX256 = u1_app::app.currX256 + 13; // 0.05*256 = 12.8 -> 13
            snprintf(s, sizeof(s), "U:%3dV/%2d.%1dHz  %2d.%1dA",
                vphX256 / 256,
                (frequX256 / 256), ((frequX256 & 0xff) * 10) / 256,
                (currX256 / 256), ((currX256 & 0xff) * 10) / 256
            );
            // snprintf(s, sizeof(s), "U=%3dV I=%2dA P=%4dW",
            //     (app.adc.voltage.rmsX256Ewma + 128) / 256,
            //     (app.adc.current.rmsX256Ewma + 128) / 256,
            //     (int)(((uint32_t)app.adc.voltage.rmsX256Ewma * app.adc.current.rmsX256Ewma + 32768) / 65536)
            // );
            u1_sys::lcd_putString(s);
        }
    }

    uint16_t calcVoltagePeriodEwma_4ms (uint8_t value) {
        static uint16_t s = 25600;
        if (value == 0) {
            s = 0;
        } else if (s == 0) {
            s = 25600; // = 200 (value@50Hz) * 128
        } else {
            // s = k*a*value + (1-1/a)*s -> k=256, a=1/64, s(t>>tau)=k*value -> tau=256ms(@dt=4ms), s=25600(@f=50Hz)
            s = s - (s >> 6);           // s = 0..64512 (0xfc00), real 0..64007 (0xfa07)
            s += (uint16_t)value * 4;   // s = 0..65022 (0xfdfe), real 0..64517 (0xfc05)
        }
        return s;
    }

    uint16_t calcVoltageAmplitudeEwma_4ms (uint8_t value) {
        static uint16_t s = 58880; // =230V ( = 230 * 256)
        if (value == 0) {
            s = 0;
        } else if (s == 0) {
            s = 58880; // = 230V ( = 230 * 256)
        } else {
            // adc-peakpeak= 149 -> s = k * a * 149 = 57216 -> 57216/256 = 223.5V 
            // s = k*a*value + (1-1/a)*s -> k=5.8046875, a=1/64, s(t>>tau)=k*value -> tau=256ms(@dt=4ms), s=25600(@f=50Hz)
            s = s - (s >> 6); // s = 0..64512 (0xfc00)
            if (value <= 170) {
                s += (uint16_t)value * 0x06;  // s = 0..65532 (0xfffc);
            } else {
                s += 1023; // s = 0..65535 (0xffff)
            }
            
        }
        return s;
    }

    uint16_t calcCurrentAmplitudeEwma_4ms (uint8_t value) {
        static uint16_t s = 0; // =0A
        if (value == 0) {
            s = 0;
        } else if (s == 0) {
            s = 1;
        } else {
            // adc-peakpeak= 94 -> s = k * a * 94 = 1536 -> 1536/256 = 6A 
            // s = k*a*value + (1-1/a)*s -> k=0.2553, a=1/64, s(t>>tau)=k*value -> tau=256ms(@dt=4ms)
            s = s - (s >> 4); // s = 0..61440 (0xf000)
            if (value <= 244) {
                s += ((uint16_t)value * 264) / 256;  // s = 0..???? (0x????);
            } else {
                s += 255;
            }
        }
        return s;
    }


    void task_1ms () {
        static uint16_t timer = 0;
        if (timer > 0) {
            timer--;
        }
    
        enum State nextState = app.state;
        switch (u1_sys::getSw1Value()) {
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
                u1_sys::setK1(0);
                u1_sys::enablePWM(0);
                OCR0A = 0;
                app.maxAmps =  0;
                timer = 1500;
                nextState = Start;
                break;
            }

            case Test: {
                u1_sys::setK1(1);
                app.disableStatusLED = 1;
                u1_sys::enablePWM(0);
                OCR0A = 0;
                app.maxAmps =  0;
                if (u1_sys::getSw1Value() != 0) {
                    timer = 1500;
                    nextState = Start;
                }
                break;
            }

            case Start: {
                u1_sys::setK1(0);
                u1_sys::enablePWM(1);
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
                u1_sys::setK1(0);
                u1_sys::enablePWM(1);
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
                u1_sys::setK1(0);
                u1_sys::enablePWM(1);
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
                u1_sys::setK1(0);
                u1_sys::enablePWM(1);
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
                        u1_sys::setK1(1);
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
                u1_sys::enablePWM(1);
                OCR0A = pwmOC;
                if (app.vcpX16 >= (100 * 16 / 10)) { // 10.5V
                    // typ. 12V
                    if (timer == 0) {
                        u1_sys::setK1(0);
                        nextState = NotConnected; 
                    }
                } else if (app.vcpX16 >= (75 * 16 / 10)) { // 7.5V
                    // typ. 9V
                    if (timer == 0) {
                        u1_sys::setK1(0);
                        nextState = Connected;
                    }
                } else if (app.vcpX16 >= (40 * 16 / 10)) { // 4.0V
                    // typ. 6V
                    u1_sys::setK1(1);
                    timer =  500;
                    nextState = Charging;
                } else {
                    // typ. 3V (Charging with air ventilation)
                    if (timer == 0) {
                        u1_sys::setK1(0);
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

    void task_2ms () {
    }

    void task_4ms () {
        uint16_t valueEwma;
        
        valueEwma = calcVoltagePeriodEwma_4ms(app.adc.voltage.period);
        app.adc.voltage.periodEwma = valueEwma;
        if (valueEwma == 0) {
            app.frequX256 = 0;
        } else {
            int32_t diff_i32 = (int32_t)valueEwma - 25600;
            int8_t d_i8 = (diff_i32 < -128) ? -128 : ( (diff_i32 > 127) ? 127 : (int8_t)diff_i32 );
            app.frequX256 = 0x3200 - (d_i8 / 2); // 0x32 --> 50Hz
        }

        valueEwma = calcVoltageAmplitudeEwma_4ms(app.adc.voltage.peakToPeak);
        app.vphX256 = valueEwma;

        if (app.adc.current.adcSine.peakToPeak > 0x20) {
            u1_sys::setLedD3(1);
        }
        valueEwma = calcCurrentAmplitudeEwma_4ms(app.adc.current.adcSine.peakToPeak);
        app.currX256 = valueEwma;
        u1_sys::setLedD3(0);
    }

    
    void task_8ms () {
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
            u1_sys::setLedD3(1);
        } else {
            u1_sys::setLedD3(0);
        }
    }


    void task_16ms () {
    }

    void task_32ms () {
    }

    void task_64ms () {
    }

    void task_128ms () {
        static uint8_t timer = 0;
        timer++;
        if (timer >= 5) {
            timer = 0;
            u1_sys::setEvent(EVENT_REFRESH_LCD);
        }
    }

    // ****************************************************************************

    int8_t refreshAdcSine_adc100us (struct AdcSine *p, uint8_t adc) {
        p->tmpTimer = (p->tmpTimer < 0xff) ? p->tmpTimer + 1 : p->tmpTimer;

        p->tmpMin = (adc < p->tmpMin) ? adc : p->tmpMin;
        p->tmpMax = (adc > p->tmpMax) ? adc : p->tmpMax;
        
        if (p->area == 0) {
            if (p->tmpTimer == 128) {
                p->min = p->tmpMin;
                p->max = p->tmpMax;
                if ( (p->tmpMax <= p->tmpMin) || (p->tmpMax - p->tmpMin) < 0x20) {
                    // no voltage curve detected, try again
                    p->tmpMin = 0xff; p->tmpMax = 0; p->tmpTimer = 0;
                    return 0;                    
                }
                p->th = (uint8_t)( ((uint16_t)p->tmpMax + p->tmpMin) / 2);
            
            } else if (p->tmpTimer == 255) {
                // failure, reset values and try detection again
                p->min = 0; p->max = 0;
                p->tmpMin = 0xff; p->tmpMax = 0; p->tmpTimer = 0;
                return 0;

            } else if (p->tmpTimer > 128) {
                int16_t x;
                x = ( (int16_t)(p->th) ) + 0x08;
                if (adc > x) {
                    p->area = 1;
                    p->tmpTimer = 0;
                    return 1;
                } 
                x = ( (int16_t)(p->th) ) - 0x08;
                if (adc < x) {
                    p->area = -1;
                    p->tmpTimer = 0;
                    return -1;
                }
            }
            return 0;

        }

        if (p->tmpTimer >= 128) {
            // failure, reset values and try detection again
            p->area = 0; p->min = 0; p->max = 0; p->peakToPeak = 0;
            p->tmpMin = 0xff; p->tmpMax = 0; p->tmpTimer = 0;
            return 0;

        }

        int8_t rv = 0;
        if (p->area > 0 && (adc < p->th) ) {
            rv = -1;
        } else if (p->area < 0 && (adc > p->th) ) {
            rv = +1;
        }
        if (rv) {
            p->area = rv;
        }
        if (rv > 0) {
            if (p->tmpTimer < 0x30) {
                // measurement time too short
                p->peakToPeak = 0;
                p->th = 0x80;
            } else {
                p->peakToPeak = (p->tmpMax > p->tmpMin) ? p->tmpMax - p->tmpMin : 0;
                p->min = p->tmpMin;
                p->max = p->tmpMax;
                p->th = (uint8_t)( ((uint16_t)p->tmpMax + p->tmpMin) / 2);
            }
            p->tmpTimer = 0;
            p->tmpMin = 0xff;
            p->tmpMax = 0;
        }    
        return rv;
    }


    int8_t calcAdcVoltage_adc200us (uint8_t adc) {
        static uint8_t min = 0xff, max = 0;
        static uint8_t timer = 0;
        timer = (timer < 0xff) ? timer + 1 : timer;

        struct AdcVoltage *p = &app.adc.voltage;
        min = (adc < min) ? adc : min;
        max = (adc > max) ? adc : max;
        
        if (p->area == 0) {
            if (timer == 128) {
                p->min = min;
                p->max = max;
                if (max <= min || (max - min) < 0x20) {
                    // no voltage curve detected, try again
                    min = 0xff; max = 0; timer = 0;
                    return 0;                    
                }
                p->th = (uint8_t)( ((uint16_t)max + min) / 2);
            
            } else if (timer == 255) {
                // failure, reset values and try detection again
                p->min = 0; p->max = 0;
                min = 0xff; max = 0; timer = 0;
                return 0;

            } else if (timer > 128) {
                int16_t x;
                x = ( (int16_t)(p->th) ) + 0x08;
                if (adc > x) {
                    p->area = 1;
                    timer = 0;
                    return 1;
                } 
                x = ( (int16_t)(p->th) ) - 0x08;
                if (adc < x) {
                    p->area = -1;
                    timer = 0;
                    return -1;
                }
            }
            return 0;

        }

        if (timer >= 128) {
            // failure, reset values and try detection again
            p->area = 0; p->min = 0; p->max = 0; p->period = 0; p->peakToPeak = 0;
            min = 0xff; max = 0; timer = 0;
            return 0;

        }

        int8_t rv = 0;
        if (p->area > 0 && (adc < p->th) ) {
            rv = -1;
        } else if (p->area < 0 && (adc > p->th) ) {
            rv = +1;
        }
        if (rv) {
            p->area = rv;
        }
        if (rv > 0) {
            if (timer < 0x30) {
                // measurement time too short
                p->period = 0;
                p->peakToPeak = 0;
                p->th = 0x80;
            } else {
                p->period = timer;
                p->peakToPeak = (max > min) ? max - min : 0;
                p->min = min;
                p->max = max;
                p->th = (uint8_t)( ((uint16_t)max + min) / 2);
            }
            timer = 0;
            min = 0xff;
            max = 0;
        }    
        return rv;
    }

    uint8_t handleADCValue_adc100us (uint8_t channel, uint8_t result) {
        static int8_t phAngX100us = 0;
        uint8_t nextChannel;

        phAngX100us = (phAngX100us < 127) ? phAngX100us + 1 : phAngX100us;
        app.adc.cnt++;
        switch (channel) {
            case 0: { // current 
                app.adc.adc0 = result;
                int8_t change = refreshAdcSine_adc100us(&app.adc.current.adcSine, result);
                if (change > 0) {
                    app.adc.phAngX100us = phAngX100us;
                    phAngX100us = -128;
                }
                nextChannel = 1;
                break;
            }
                
            case 1: { // voltage
                app.adc.adc1 = result;
                app.adc.recentAdc1[app.adc.recentAdc1Index++] = result;
                if (app.adc.recentAdc1Index >= sizeof (app.adc.recentAdc1)) {
                    app.adc.recentAdc1Index = 0;
                }
                
                int8_t change = calcAdcVoltage_adc200us(result);
                if (change > 0) {
                    u1_sys::setK2(1);
                    if (phAngX100us > 0x0f) {
                        app.adc.phAngX100us = 0;
                    }
                    phAngX100us = 0;
                } else if (change < 0) {
                    u1_sys::setK2(0);
                }
                nextChannel = 0;
                break;
            }
            
            default: nextChannel = 0; break;
        }

        return nextChannel;

    }

}
