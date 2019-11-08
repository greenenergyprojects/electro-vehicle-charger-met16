#ifndef U1_MON_HPP_
#define U1_MON_HPP_

#include "global.h"

#ifdef GLOBAL_MONITOR

#include "sys.hpp"
#include <avr/pgmspace.h>

namespace u1_mon {

    #define EEP_LOG_START 128
    typedef uint8_t * plogtable_t;

    struct LogTime { // 32 Bit
        unsigned int sec: 6;
        unsigned int min: 6;
        unsigned int hrs: 5;
        unsigned int systemStartCnt: 15;
    };

    struct LogRecordHeader {
        uint8_t size;            // size in bytes, 0xff -> invalid (end of table)
        uint8_t typ;             // =0 -> invalid (save in progress), >0 -> valid record
        struct LogTime time;
    };

    struct LogRecordValue1 {
        struct LogRecordHeader header;
        uint16_t chgTimeMinutes;
        uint32_t energyKwhX256;
    };

    struct Log {
        plogtable_t pLatestRecord; // pointer of latest record in EEPROM log table
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
    uint8_t getCmdCount ();
    int8_t  printLineHeader (uint8_t lineIndex);
    int8_t  printLine (uint8_t lineIndex, char keyPressed);
}

#endif // GLOBAL_MONITOR
#endif // U1_MON_HPP_