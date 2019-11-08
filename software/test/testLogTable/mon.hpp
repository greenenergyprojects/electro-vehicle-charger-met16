#ifndef MON_HPP_
#define MON_HPP_

#define EEP_LOG_START 128


namespace u1_mon {

    typedef uint16_t plogtable_t;

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

}

#endif // MON_HPP_