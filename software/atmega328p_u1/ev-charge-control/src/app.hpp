#ifndef UC1_APP_HPP_
#define UC1_APP_HPP_


#include "sys.hpp"

// declarations

// defines

#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 3


namespace u1_app {

    #define EVENT_REFRESH_LCD    0x01
    #define EVENT_CALC_FREQU     0x02
    #define EVENT_CALC_VOLT_UPP  0x04
    #define EVENT_CALC_VOLT_LOW  0x08
    #define EVENT_CALC_CURR_UPP  0x10
    #define EVENT_CALC_CURR_LOW  0x20
    #define EVENT_POW_UPP        0x40
    #define EVENT_POW_LOW        0x80

    enum State { Init, Test, Start, NotConnected, Connected, EVReady, Charging };

    union Debug {
        uint8_t u8[12];
        uint16_t u16[6];
        uint32_t u32[3];
    };

    struct AdcSine {
        uint8_t tmpMin, tmpMax, tmpTimer;
        int8_t  area;        // =0 -> not known, =1 -> positive half, =-1 -> negative half
        uint8_t th;          // adc threshhold value between positive and negative half
        uint8_t min, max;    // adc min and max value of current period
        uint8_t peakToPeak;  // adc peak to peak value (=0 -> not detected)
    };
    
    struct AdcVoltage {
        int8_t  area;        // =0 -> not known, =1 -> positive half, =-1 -> negative half
        uint8_t th;          // adc threshhold value between positive and negative half
        uint8_t min, max;    // adc min and max value of current period
        uint8_t period;      // 0x64 = 100 -> 50Hz
        uint16_t periodEwma; // 0x3200 = 12600 -> 50Hz (ewma filtered value)
        uint8_t peakToPeak;
    };

    struct AdcCurrent {
        struct AdcSine adcSine;
    };

    struct Adc {
        uint8_t cnt;
        uint8_t adc0, adc1;
        uint8_t recentAdc1[16];
        uint8_t recentAdc1Index;
        uint8_t phAngX100us;
        struct AdcVoltage voltage;
        struct AdcCurrent current;
    };


    struct App {
        enum State state;
        union Debug debug;
        struct Adc adc;
        uint8_t disableStatusLED;
        uint8_t maxAmps;
        uint8_t vcpX16;
        uint16_t frequX256;
        uint16_t vphX256;
        uint16_t currX256;
        uint16_t energy;
    };


    extern struct App app;

    void init ();
    void main ();

    void task_1ms   ();
    void task_2ms   ();
    void task_4ms   ();
    void task_8ms   ();
    void task_16ms  ();
    void task_32ms  ();
    void task_64ms  ();
    void task_128ms ();

    uint8_t handleADCValue_adc100us (uint8_t channel, uint8_t result);

}

#endif  // UC1_APP_HPP_
