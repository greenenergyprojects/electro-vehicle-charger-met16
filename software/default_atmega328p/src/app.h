#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED

// declarations

enum APP_STATE { Init, Test, Start, NotConnected, Connected, EVReady, Charging };

struct App_TrimParams {
  uint8_t vcpK, vcpD;
};

struct App_AdcRms {
    uint16_t rmsX256Ewma; // root mean square times 256, filtered by EWMA
    uint16_t rmsX256;     // root mean square times 256
    uint16_t corrK;       // correction k
    uint16_t corrD;       // correction d
    uint16_t adcrmsX256;  // root mean square of adc value times 256
    uint16_t medX256;     // average of last period (times 256)
    uint16_t acc;         // accumulation of 100 values
    uint32_t rmsPow2X256; // (root mean square value)^2 * 256
    uint32_t accSquare;   // accumulation of 100 square-values; square-value = (value*256 - medX256)^2
    uint8_t  adcMax[2], adcMin[2]; // maximal and mininmal ADC value inside period (index 0 changed one times per period)
    uint8_t  zeroAdcRange; // range of ADC which results in zero rms (avoid wrong value caused by noise)
};

struct App_ADC_MEAS_UPLO {
    uint8_t  accCnt;
    uint16_t accValue;
    uint32_t accQuadValue;
    uint8_t  cnt;
    uint16_t value;
    uint32_t quadValue;
};

struct App_ADC_MEAS {
    uint8_t thValue;
    uint8_t thWidth;
    uint8_t upperCurv;
    struct App_ADC_MEAS_UPLO upper;
    struct App_ADC_MEAS_UPLO lower;
};

struct App_ADC_VOLT {
    uint8_t upperCurv;
    int8_t  thCnt;
    uint8_t thValue;
    uint8_t cntUpper;
    uint8_t cntLower;
    uint32_t accVoltage;
};

struct App_ADC_FREQU {
    uint8_t period;
    uint16_t periodEwma;
};

struct App_ADC {
    uint8_t cnt;
    uint8_t adc0, adc1, adc2, adc6, adc7;
    struct App_ADC_MEAS  voltage;
    struct App_ADC_MEAS  current;
    struct App_ADC_VOLT  volt;
    struct App_ADC_FREQU frequ;
    uint16_t voltX256;
    uint16_t currX256;
    uint16_t activePowerX256;
    uint32_t energyX256;
    
    // struct App_AdcRms current, voltage;
};


struct App {
    uint32_t cnt;
    enum APP_STATE state;
    struct App_TrimParams trim;
    struct App_ADC adc;
    uint16_t frequX256;
    uint8_t maxAmps;
    uint8_t vcpX16; // in volt, comma between bit 4 and 3
    uint8_t disableStatusLED;
    uint32_t debug[3];
    uint8_t smTimer;
    int32_t energy;       // accumulatete energy in Wh
};

extern struct App app;

// defines


#define APP_EVENT_REFRESH_LCD    0x01
#define APP_EVENT_CALC_FREQU     0x02
#define APP_EVENT_CALC_VOLT_UPP  0x04
#define APP_EVENT_CALC_VOLT_LOW  0x08
#define APP_EVENT_CALC_CURR_UPP  0x10
#define APP_EVENT_CALC_CURR_LOW  0x20
#define APP_EVENT_POW_UPP        0x40
#define APP_EVENT_POW_LOW        0x80


// functions

void app_init (void);
void app_main (void);

uint8_t app_handleADCValue (uint8_t channel, uint8_t result);

void app_task_1ms   (void);
void app_task_2ms   (void);
void app_task_4ms   (void);
void app_task_8ms   (void);
void app_task_16ms  (void);
void app_task_32ms  (void);
void app_task_64ms  (void);
void app_task_128ms (void);

#endif // APP_H_INCLUDED
