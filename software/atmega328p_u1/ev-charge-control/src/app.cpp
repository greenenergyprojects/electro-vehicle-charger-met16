#include "global.h"
#include "sys.hpp"
#include "mon.hpp"
#include "app.hpp"

#include <string.h>

namespace u1_app {

    struct App app;

    // declarations and definitions
    void app_refreshLcd ();

    // ------------------------------------------------------------------------

    void init () {
        memset(&app, 0, sizeof app);
        initTrim(1);
        u1_mon::startupLog();
        // u1_mon::clearTrim();
        // u1_mon::clearLogTable();
        app.state = Init;
        app.protection = '-'; // no protection active
        DDRD |= (1 << PD6); // PWM output
    }

    void initTrim (uint8_t startup) {
        if (startup) {
            struct Trim t;
            eeprom_busy_wait();
            eeprom_read_block(&t, 0, sizeof t);
            if (t.magic == 0x00030201) {
                app.trim = t;
                app.trim.startCnt++;
                if (app.trim.startCnt > 0x7fff) {
                    u1_mon::clearLogTable();
                    app.trim.startCnt = 1;
                }
                eeprom_write_word((uint16_t *)4, app.trim.startCnt);
                eeprom_busy_wait();
                if (app.trim.vcpK < 8) { // bug??
                    app.trim.vcpK = 13;

                }
                return;
            }
        }
        app.trim.magic = 0x00030201;
        app.trim.startCnt = 1;
        app.trim.vcpK = 13; app.trim.vcpD = 0;
        app.trim.currK = 149;
        app.trim.tempK = 64; app.trim.tempOffs = 0x58;
    }

    void main () {
        if (u1_sys::clearEvent(EVENT_REFRESH_LCD)) {
            app_refreshLcd();
        }
        if (u1_sys::clearEvent(EVENT_LOG_STARTCHARGING)) {
            struct u1_mon::LogDataStartCharge data = { app.maxAmps };
            u1_mon::saveLog(LOG_TYPE_STARTCHARGE, &data, sizeof (data));
        }
        if (u1_sys::clearEvent(EVENT_LOG_CHARGING)) {
            u1_mon::saveLog(LOG_TYPE_CHARGING, &app.logDataCharging, sizeof (app.logDataCharging));
        }
        if (u1_sys::clearEvent(EVENT_LOG_STOPCHARGING)) {
            u1_mon::saveLog(LOG_TYPE_STOPCHARGING, &app.logDataCharging, sizeof (app.logDataCharging));
        }
    }

    void incClockMs (struct Clock *p) {
        if (p->ms < 999) {
            p->ms++;
        } else {
            p->ms = 0;
            if (p->sec < 59) {
                p->sec++;
            } else {
               p->sec = 0;
                if (p->min < 59) {
                    p->min++;
                } else {
                    p->min = 0;
                    if (p->hrs < 0xff) {
                        p->hrs++;
                    } else {
                        p->hrs++;
                        p->min = 59;
                        p->sec = 59;
                        p->ms = 999;
                    }
                }
            }
        } 
    }

    void calcPower () {
        pushSREGAndCli();
        volatile uint8_t vac = (uint8_t)((app.vphX256 + 128) / 256);
        volatile uint8_t iH = (uint8_t)(app.currX256 / 256);
        volatile uint8_t iL = (uint8_t)app.currX256;
        volatile int16_t ph = app.adc.phAngX100us; // 0..199 -> phi= 0°.. 358.2°
        popSREG();

        if (iH >= 32 || iH == 0 || ph < 0 || ph > 199 || vac == 0 || iH == 0) {
            app.apparantPower =  0;
            app.activePower =  0;
            app.reactivePower = 0;
        
        } else {
            uint8_t xI;
            if (iH >= 16) {
                xI = (iH << 3) | ((iL + 2) >> 5);
            } else if (iH >= 8) {
                xI = (iH << 4) | ((iL + 4) >> 4);
            } else {
                xI = (iH << 5) | ((iL + 8) >> 3);
            }

            // without volatile -> conversion error on int32_t s = xS (!!!)
            volatile uint16_t xS = vac * xI;
            int32_t s = (int32_t)xS;
            int32_t xP;
            int32_t xQ; 
            switch (ph) {
                // case 0:   xP = s; xQ = 0; break;
                // case 50:  xP = 0; xQ = s; break;
                // case 100: xP = -s; xQ = 0; break;
                // case 150: xP = 0; xQ = -s; break;
                default: {
                    int16_t kCos = u1_sys::cosX256(ph);
                    int16_t kSin = u1_sys::sinX256(ph);
                    xP = s * kCos;
                    xQ = s * kSin;
                    break;
                }
            }

            if (iH >= 16) {
                xS = xS / 8;
                xP = xP / 256 / 8;
                xQ = xQ / 256 / 8;
            } else if (iH >= 8) {
                xS = xS / 16;
                xP = xP / 256 / 16;
                xQ = xQ / 256 / 16;
            } else {
                xS = xS / 32;
                xP = xP / 256 / 32;
                xQ = xQ / 256 / 32;
            }

            app.apparantPower =  xS > 32767 ? 32767 : xS;
            app.activePower =  xP > 32767 ? 32767 : ( (xP < -32768) ? -32768 : xP );
            app.reactivePower = xQ > 32767 ? 32767 : ( (xQ < -32768) ? -32768 : xQ );
        }
    }


    void app_refreshLcd () { // called from main, 6ms
        static uint8_t timer = 0;;
        char s[21];    
        u1_sys::lcd_setCursorPosition(0, 0);
        {
            
            snprintf(s, sizeof(s), "%d|%2dA|%c|%2dC|", u1_sys::getSw1Value(), app.maxAmps, app.protection, app.temp);
            u1_sys::lcd_putString(s);
            uint8_t len = strlen(s);    
            switch (app.state) {
                case Init:         snprintf(s, sizeof(s), "INIT"); break;
                case Test:         snprintf(s, sizeof(s), "TEST"); break;
                case Start:        snprintf(s, sizeof(s), "START"); break;
                case NotConnected: snprintf(s, sizeof(s), "12V/---"); break;
                case Connected:    snprintf(s, sizeof(s), " 9V/EV "); break;
                case EVReady:      snprintf(s, sizeof(s), " 6V/RDY"); break; 
                case Charging:     snprintf(s, sizeof(s), " 6V/CHG"); break;
                default:           snprintf(s, sizeof(s), "?"); break;
            }
            
            for (uint8_t i = 0; i < 20 - len - strlen(s); i++) {
                u1_sys::lcd_putchar(' ');
            }
            u1_sys::lcd_putString(s);   
        }
        // div_t d = div((int)app.energy, 1000);
        // snprintf(s, sizeof(s), "%d.%02dkWh", d.quot, d.rem / 10);    
        
        u1_sys::lcd_setCursorPosition(1, 0);
        uint16_t currX256 = u1_app::app.currX256 + 13; // 0.05*256 = 12.8 -> 13
        snprintf(s, sizeof(s), "%2d.%1dA ", (currX256 / 256), ((currX256 & 0xff) * 10) / 256);
        u1_sys::lcd_putString(s);   
        timer = timer < 15 ? timer + 1 : 0;
        int8_t size = 14;
        switch (timer / 4) {
            case 0: {
                struct u1_app::AdcVoltage *p = &(u1_app::app.adc.voltage);
                uint16_t vphX256 = u1_app::app.vphX256 + 128; // 0.5*256 = 128
                uint16_t frequX256 = u1_app::app.frequX256 + 13; // 0.05*256 = 12.8 -> 13
                snprintf(s, sizeof(s), "%4dV %2d.%1dHz",
                    vphX256 / 256,
                    (frequX256 / 256), ((frequX256 & 0xff) * 10) / 256
                );
                break;
            }

            case 1: {
                int16_t appPower = u1_app::app.apparantPower;
                int16_t p = u1_app::app.activePower;
                snprintf(s, sizeof(s), "%4dVA %5dW", appPower, p);
                break;
            }
            
            case 2: {
                // div_t d = div((int)app.energy, 1000);
                // snprintf(s, sizeof(s), "%d.%02dkWh", d.quot, d.rem / 10);
                cli();
                uint32_t e = app.energyKwhX256;
                sei();
                snprintf(s, sizeof(s), "%ld.%02dkWh", e / 256, ((uint8_t)e * 100 / 256));
                break;
            }
            
            case 3: {
                char sim[6] = "-----";
                if (app.sim.currentAdcK > 0) {
                    sim[0] = 'S';
                }
                if (app.sim.f > 0) {
                    sim[1] = 'f';
                }
                if (app.sim.v > 0) {
                    sim[2] = 'v';
                }
                if (app.sim.t > 0) {
                    sim[3] = 't';
                }
                if (app.sim.i > 0) {
                    sim[4] = 'i';
                }
                u1_sys::lcd_putString(sim);
                size = size - 5;
                snprintf(s, sizeof(s), "%d:%02d:%02d", app.clock.hrs, app.clock.min, app.clock.sec);
                break;
            }

        }
        for (uint8_t i = 0; i < size - strlen(s); i++) {
            u1_sys::lcd_putchar(' ');
        }
        u1_sys::lcd_putString(s);   
    }

    uint8_t calcVcpEwma (uint8_t value) {
        static uint16_t s = 0;
        s = s - (s >> 3);    // s = 0..63488 (0xf800), real 0..63160 (0xf6b8)
        s += (uint16_t)value * 32;      // s = 0..65528 (0xfff8), real 0..65200 (0xfeb0)
        return (s + 128) >> 8;
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

    // value 0 .. 511 (>255 in case of Ipeak > 20A)
    uint16_t calcCurrentAmplitudeEwma_4ms (uint16_t value) {
        static uint16_t s = 0; // =0A
        if (value == 0) {
            s = 0;
        } else if (s == 0) {
            s = 1;
        } else {
            // adc-peakpeak= 94 -> s = k * a * 94 = 1536 -> 1536/256 = 6A 
            // s = k*a*value + (1-1/a)*s -> k=0.2553, a=1/64, s(t>>tau)=k*value -> tau=256ms(@dt=4ms)
            s = s - (s >> 4); // s = 0..61439 (0xefff)
            if (value > 511) {
                value = 511;
            }
            s += ((uint16_t)value * app.trim.currK) / 128;  // s = 0..62461 (0xf3fd);
        }
        return s;
    }


    void task_1ms () {
        static struct Clock chgTime;
        incClockMs(&app.clock);

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

        int8_t dt = app.temp - 50;
        if (dt > 0) {
            int8_t maxAmps = 17 - dt / 2;
            app.maxAmps = maxAmps >= 8 ? maxAmps : 0;
            if (app.protection == '-') {
                app.protection = 'T';
            }
        } else if (app.protection == 'T') {
            app.protection = '-';
        }
        if (app.protection == 'F') {
            app.maxAmps =  0;

        }

        uint8_t pwmOC = ((app.maxAmps * 255) / 6 + 5) / 10;
        if (pwmOC < 13) {
            pwmOC = 13; // 5% duty cyle
        }
        
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

                incClockMs(&chgTime);
                app.logDataCharging.chgTimeHours = chgTime.hrs;
                app.logDataCharging.chgTimeMinutes = chgTime.min;
                app.logDataCharging.energyKwhX256 = app.energyKwhX256;
                if (nextState != Charging) {
                    u1_sys::setEvent(EVENT_LOG_STOPCHARGING);
                } else if (chgTime.ms == 0 && chgTime.sec == 0 && (chgTime.min) % 4 == 1) {
                    u1_sys::setEvent(EVENT_LOG_CHARGING);
                }
                break;
            }
            
            
            default: {
                nextState = Init;
                break;
            }
        }

        if (app.state != nextState) {
            if (nextState == Charging) {
                app.energyKwhX256 = 0;
                memset(&chgTime, sizeof (chgTime), 0);
                u1_sys::setEvent(EVENT_LOG_STARTCHARGING);
            }
            app.state = nextState;
        }

    }

    void task_2ms () {
    }

    void task_4ms () {
        uint16_t valueEwma;
        
        valueEwma = calcVoltagePeriodEwma_4ms(app.adc.voltage.period);
        app.adc.voltage.periodEwma = valueEwma;
        uint16_t frequX256;
        if (valueEwma == 0) {
            frequX256 = 0;
        } else {
            int32_t diff_i32 = (int32_t)valueEwma - 25600;
            int8_t d_i8 = (diff_i32 < -128) ? -128 : ( (diff_i32 > 127) ? 127 : (int8_t)diff_i32 );
            frequX256 = 0x3200 - (d_i8 / 2); // 0x32 --> 50Hz
        }
        if (app.sim.f > 0) {
            frequX256 =  app.sim.f * 256;
        }
        if (frequX256 > (50 * 256 + 128) || frequX256 < (50 * 256 - 128)) {
            app.protection = 'F';
        } else {
            if (app.protection == 'F') {
                app.protection = '-';
            }
        }
        app.frequX256 =  frequX256;

        valueEwma = calcVoltageAmplitudeEwma_4ms(app.adc.voltage.peakToPeak);
        if (app.sim.v > 0) {
            valueEwma = app.sim.v * 256; 
        }
        app.vphX256 = valueEwma;

        valueEwma = calcCurrentAmplitudeEwma_4ms(app.adc.current.sine.peakToPeak);
        if (app.sim.i > 0) {
            valueEwma = app.sim.i * 256;
        }
        app.currX256 = valueEwma;

        calcPower();
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

    int8_t refreshAdcSine_adc100us (struct AdcSineTmp *p, struct AdcSine *pSine, uint8_t adc) {
        p->timer = (p->timer < 0xff) ? p->timer + 1 : p->timer;

        p->min = (adc < p->min) ? adc : p->min;
        p->max = (adc > p->max) ? adc : p->max;
        
        if (p->area == 0) {
            if (p->timer == 128) {
                pSine->min = p->min;
                pSine->max = p->max;
                if ( (p->max <= p->min) || (p->max - p->min) < 0x20) {
                    // no sine curve detected, try again
                    p->min = 0xff; p->max = 0; p->timer = 0;
                    return 0;                    
                }
                pSine->th = (uint8_t)( ((uint16_t)p->max + p->min) / 2);
            
            } else if (p->timer == 255) {
                // failure, reset values and try detection again
                pSine->min = 0; pSine->max = 0;
                p->min = 0xff; p->max = 0; p->timer = 0;
                return 0;

            } else if (p->timer > 128) {
                int16_t x;
                x = ( (int16_t)(pSine->th) ) + 0x08;
                if (adc > x) {
                    p->area = 1;
                    p->timer = 0;
                    return 1;
                } 
                x = ( (int16_t)(pSine->th) ) - 0x08;
                if (adc < x) {
                    p->area = -1;
                    p->timer = 0;
                    return -1;
                }
            }
            return 0;

        }

        if (p->timer >= 128) {
            // failure, reset values and try detection again
            p->area = 0; pSine->min = 0; pSine->max = 0; pSine->peakToPeak = 0;
            p->min = 0xff; p->max = 0; p->timer = 0;
            return 0;
        }
        if (p->timer == 43) {
            p->valueAt150 = adc;
        } else if (p->timer == 59) {
            p->valueAt210 = adc;
        }

        int8_t rv = 0;
        if (p->area > 0 && (adc < pSine->th) ) {
            rv = -1;
        } else if (p->area < 0 && (adc > pSine->th) ) {
            rv = +1;
        }
        if (rv) {
            p->area = rv;
        }
        if (rv > 0) {
            if (p->timer < 0x60) {
                // measurement time too short
                pSine->peakToPeak = 0;
                pSine->th = 0x80;
            } else {
                pSine->min = p->min;
                pSine->max = p->max;
                pSine->th = (uint8_t)( ((uint16_t)p->max + p->min) / 2);
                if (p->max >= 0xf8 || p->min <= 0x08) {
                    pSine->peakToPeak = 2 * (uint16_t)( (p->valueAt150 > p->valueAt210) ? p->valueAt150 - p->valueAt210 : 0);
                } else {
                    pSine->peakToPeak = (p->max > p->min) ? p->max - p->min : 0;
                }
            }
            p->timer = 0;
            p->min = 0xff;
            p->max = 0;
        }    
        return rv;
    }

    int8_t calcAdcVoltageOld_adc200us (uint8_t adc) {
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


    int8_t calcAdcVoltage_adc200us (uint8_t adc) {
        static uint8_t min = 0xff, max = 0;

        struct AdcVoltage *p = &app.adc.voltage;
        min = (adc < min) ? adc : min;
        max = (adc > max) ? adc : max;
        
        if (p->area == 0) {
            if (p->th == 0 && p->timer > 255) {
                p->min = min;
                p->max = max;
                if (max <= min || (max - min) < 0x20) {
                    // no voltage curve detected, try again
                    min = 0xff; max = 0; p->timer = 0;
                    return 0;                    
                }
                p->th = (uint8_t)( ((uint16_t)max + min) / 2);
            
            } else if (p->timer >= 512) {
                // failure, reset values and try detection again
                p->min = 0; p->max = 0;
                min = 0xff; max = 0; p->timer = 0; p->th = 0;
                return 0;

            } else if (p->timer > 255) {
                int16_t x;
                x = ( (int16_t)(p->th) ) + 0x08;
                if (adc > x) {
                    p->area = 1;
                    p->timer = 0;
                    return 1;
                } 
                x = ( (int16_t)(p->th) ) - 0x08;
                if (adc < x) {
                    p->area = -1;
                    p->timer = 0;
                    return -1;
                }
            }
            return 0;

        }

        if (p->timer >= 512) {
            // failure, reset values and try detection again
            p->area = 0; p->min = 0; p->max = 0; p->period = 0; p->peakToPeak = 0;
            min = 0xff; max = 0; p->timer = 0;
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
            if (p->timer < 0x60) {
                // measurement time too short
                p->period = 0;
                p->peakToPeak = 0;
                p->th = 0x80;
            } else {
                p->period = p->timer / 2;
                p->peakToPeak = (max > min) ? max - min : 0;
                p->min = min;
                p->max = max;
                p->th = (uint8_t)( ((uint16_t)max + min) / 2);
            }
            p->timer = 0;
            min = 0xff;
            max = 0;
        }    
        return rv;
    }


    uint8_t handleADCValue_adc100us (uint8_t channel, uint8_t result) {
        static int16_t phAngX100us = 0;
        static uint8_t timerChannel2 = 0;
        static int32_t accEnergy = 0;
        

        if (app.adc.voltage.timer < 512) {
            // used for period/frequency measurement
            // must be increased every 100us
            app.adc.voltage.timer++;
        }

        // 1kWh = 256 -> 1000W * 3600s * 200 (calls/period) * 50Hz = 36.000.000.000
        //          1 -> 36.000.000.000 / 256 = 140.625.000 (hex 0861 c468)
        accEnergy = accEnergy + app.activePower - 140625000L;
        if (accEnergy < 0) {
            accEnergy = accEnergy + 140625000L;
        } else {
            app.energyKwhX256 += 1;
        }

        if ( (phAngX100us >= 0) && (phAngX100us < 127) ) {
            phAngX100us++;
        }
        app.adc.cnt++;
        
        uint8_t nextChannel;
        switch (channel) {
            case 0: { // current 
                nextChannel = 1;
                if (app.sim.currentAdcK > 0) {
                    int16_t x = ( ( (int16_t)(app.adc.recentAdc1[(app.adc.recentAdc1Index + 1) % 16]) ) - 0x5c ) * app.sim.currentAdcK / 32 + 0x80;
                    result = (x < 0) ? 0 : ( (x > 0xff) ? 0xff : (uint8_t)x);
                }
                app.adc.adc0 = result;
                int8_t change = refreshAdcSine_adc100us(&app.adc.current.tmp, &app.adc.current.sine, result);
                if (change > 0) {
                    if (phAngX100us > 0 && phAngX100us < 211) {
                        phAngX100us = phAngX100us - 12;
                        app.adc.phAngX100us = phAngX100us < 0 ? phAngX100us + 200 : phAngX100us;
                    } else {
                        app.adc.phAngX100us = -1;
                    }
                    phAngX100us = -32768;
                    
                } else if (change < 0) {
                }
                break;
            }
                
            case 1: { // voltage
                nextChannel = 0;
                app.adc.adc1 = result;
                app.adc.recentAdc1[app.adc.recentAdc1Index++] = result;
                if (app.adc.recentAdc1Index >= sizeof (app.adc.recentAdc1)) {
                    app.adc.recentAdc1Index = 0;
                }
                
                int8_t change = calcAdcVoltage_adc200us(result);
                if (change > 0) {
                    // u1_sys::setK2(1);
                    if (phAngX100us > 0x0f) {
                        app.adc.phAngX100us = 0;
                    }
                    phAngX100us = 0;
                } else if (change < 0) {
                    // u1_sys::setK2(0);
                    timerChannel2 = 0;
                }
                break;
            }
            
            case 2: { // -12V verification
                app.adc.adc2 = result;
                nextChannel = 0;
                break;
            }

            case 6: { // CP voltage level
                app.adc.adc6 = result;
                app.vcpX16 = calcVcpEwma( (result * app.trim.vcpK + (uint16_t)app.trim.vcpD) / 16 );
                nextChannel = 0;
                break;
            }

            case 7: { // +12V verification
                app.adc.adc7 = result;
                nextChannel = 0;
                break;
            }

            case 8: { // temperature
                app.adc.adc8 = result;                
                int16_t x = result;
                int8_t temp = 25 + (x - app.trim.tempOffs) * app.trim.tempK / 16;
                if (app.sim.t > 0) {
                    temp = app.sim.t;
                }
                app.temp = temp;
                nextChannel = 0;
                break;
            }

            default: nextChannel = 0; break;
        }

        // next statement not working, current sometimes zero -> investigation needed
        // if (nextChannel == ) { // measure temp, VCP, ... instead of current
        //     timerChannel2++;
        //     if (timerChannel2 & 0x01) {
        //         uint8_t ch = 2 + (timerChannel2 >> 1);
        //         if (ch <= 5) {
        //             nextChannel = ch;
        //         }
        //     }
        // }

        // fast fix, use voltage instead of current time slot
        if (nextChannel == 1) { // measure temp, VCP, ... instead of voltage
            timerChannel2++;
            if (timerChannel2 & 0x01) {
                uint8_t ch = 2 + (timerChannel2 >> 1);
                switch (ch) {
                    case 2: nextChannel = 2; break;
                    case 3: nextChannel = 6; break;
                    case 4: nextChannel = 7; break;
                    case 5: nextChannel = 8; break;
                }
            }
        }
        
        return nextChannel;

    }

}
