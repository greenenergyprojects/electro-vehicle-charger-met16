#ifndef UC1_APP_HPP_
#define UC1_APP_HPP_


#include "sys.hpp"

// declarations

// defines

#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 3


namespace u1_app {

    #define LOG_HISTORY_SIZE 4

    #define EVENT_REFRESH_LCD       0x01
    #define EVENT_LOG_STARTCHARGING 0x02
    #define EVENT_LOG_CHARGING      0x04
    #define EVENT_LOG_STOPCHARGING  0x08
    #define EVENT_4                 0x10
    #define EVENT_5                 0x20
    #define EVENT_6                 0x40
    #define EVENT_7                 0x80

    enum State { Init, Test, Start, NotConnected, Connected, EVReady, Charging };


    // ***********************************************************************

    struct Trim {
        uint32_t magic;
        uint16_t startCnt; // only 15 bits used -> log record
        uint8_t  vcpK;
        uint8_t  vcpD;
        uint8_t  currK;
        int8_t   tempK;
        int8_t   tempOffs;
    };


    union Debug {
        int8_t s8[12];
        int16_t s16[6];
        int32_t s32[3];
        uint8_t u8[12];
        uint16_t u16[6];
        uint32_t u32[3];
    };

    struct AdcSineTmp {
        int8_t  area;        // =0 -> not known, =1 -> positive half, =-1 -> negative half
        uint8_t min;
        uint8_t max;
        uint8_t valueAt150;
        uint8_t valueAt210;
        uint8_t timer;       // number of 200us counts since phase angle == 0
    };

    struct AdcSine {
        uint8_t  th;          // adc threshhold value between positive and negative half
        uint8_t  min, max;    // adc min and max value of current period
        uint16_t peakToPeak;  // adc peak to peak value (=0 -> not detected)
    };
    
    struct AdcVoltage {
        uint16_t timer;
        int8_t  area;        // =0 -> not known, =1 -> positive half, =-1 -> negative half
        uint8_t th;          // adc threshhold value between positive and negative half
        uint8_t min, max;    // adc min and max value of current period
        uint8_t hMin, hMx;   // adc values on sine(30)=0.5*max and sine(-30)=0.5*min
        uint8_t period;      // 0x64 = 100 -> 50Hz
        uint16_t periodEwma; // 0x3200 = 12600 -> 50Hz (ewma filtered value)
        uint8_t peakToPeak;
    };

    struct AdcCurrent {
        struct AdcSineTmp tmp;
        struct AdcSine    sine;
        // uint8_t simAdcK;     // =0 take adc from sensor, >0 take voltage adc*simAdcK/32;
    };


    struct Adc {
        uint8_t cnt;
        uint8_t adc0, adc1, adc2, adc6, adc7, adc8;
        uint8_t recentAdc1[16];
        uint8_t recentAdc1Index;
        int16_t phAngX100us;
        struct AdcVoltage voltage;
        struct AdcCurrent current;
    };

    struct Clock {
        uint16_t ms;
        uint8_t  sec;
        uint8_t  min;
        uint8_t  hrs;
    };

    struct Sim {
        uint8_t currentAdcK; // =0 take current adc from sensor, >0 take voltage adc*simAdcK/32;
        int f;
        int v;
        int i; 
        int t;
    };

    struct LogChargingHistory {
        uint8_t typ;
        uint32_t time;
        struct u1_mon::LogDataCharging data;
    };

    struct App {
        enum State state;
        struct Sim sim;
        struct Clock clock;
        union Debug debug;
        struct Trim trim;
        struct Adc adc;
        struct u1_mon::LogDataCharging logDataCharging;
        struct LogChargingHistory logChargingHistory[LOG_HISTORY_SIZE];
        uint8_t disableStatusLED;
        uint8_t maxAmps;
        uint8_t vcpX16;
        uint16_t frequX256;
        uint16_t vphX256;
        uint16_t currX256;
        int16_t apparantPower;
        int16_t activePower;
        int16_t reactivePower;
        uint8_t protection;
        int8_t  temp;
        uint32_t energyKwhX256;
    };


    extern struct App app;

    void init ();
    void initTrim (uint8_t startup);
    void main ();

    void task_1ms   ();
    void task_2ms   ();
    void task_4ms   ();
    void task_8ms   ();
    void task_16ms  ();
    void task_32ms  ();
    void task_64ms  ();
    void task_128ms ();

    void clearLogHistory ();
    void addLogCharging (uint8_t typ, uint32_t time, uint8_t logIndex);
    uint8_t handleADCValue_adc100us (uint8_t channel, uint8_t result);

}

#endif  // UC1_APP_HPP_
