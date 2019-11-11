#ifndef U1_MON_HPP_
#define U1_MON_HPP_

#include "global.h"

#ifdef GLOBAL_MONITOR

#include "sys.hpp"
#include <avr/pgmspace.h>

namespace u1_mon {
    typedef uint8_t * plogtable_t;

    #define EEP_LOG_START 256
    #define EEP_LOG_DESCRIPTORS 64
    #define EEP_LOG_SLOT_SIZE ((E2END + 1 - EEP_LOG_START - EEP_LOG_DESCRIPTORS * 5) / EEP_LOG_DESCRIPTORS)
    #define EEP_LOG_SLOTS_START (EEP_LOG_START + EEP_LOG_DESCRIPTORS * sizeof(LogDescriptor))

    #define LOG_TYPE_SYSTEMSTART   0
    #define LOG_TYPE_STARTCHARGE   1
    #define LOG_TYPE_CHARGING      2
    #define LOG_TYPE_STOPCHARGING  3
    #define LOG_TYPE_INVALID      15

    #if EEP_LOG_SLOT_SIZE < 6
        #error log table too large
    #endif


    struct LogDescriptor {
        uint8_t typ;     // Bit 7:4=subindex, Bit 3:0=typ (0x0f = invalid descriptor, 0x00=startup system)
        uint32_t time;   // Bit 31:17=startupCnt, 16:12=hrs, 11:6=min, 5:0=sec
    };

    struct LogDataStartCharge { // start charge
        uint8_t maxAmps;
    };
    
    struct LogDataCharging {
        uint8_t chgTimeHours;
        uint8_t chgTimeMinutes;
        uint16_t energyKwhX256;
    };;

    struct Log {
        uint8_t index;
        uint8_t lastTyp;
    };

    struct Mon {
        struct Log log;
    };

    extern struct Mon mon;
    extern const struct u1_sys::MonCmdInfo PMEMSTR_CMDS[] PROGMEM;

    void init ();
    void main ();
    void clearTrim ();
    void clearLogTable();
    void startupLog ();
    uint8_t saveLog (uint8_t typ, void *pData, uint8_t size);
    uint8_t getCmdCount ();
    int8_t  printLineHeader (uint8_t lineIndex);
    int8_t  printLine (uint8_t lineIndex, char keyPressed);
}

#endif // GLOBAL_MONITOR
#endif // U1_MON_HPP_