#include <string>
#include <stdint.h>
#include "mon.hpp"

namespace std {
    #define E2END 1023

    uint8_t eep [E2END + 1];

    void cli() {}
    void sei() {}
    
    void eeprom_init () {
        for (int i = 0; i < sizeof(eep); i++) {
            eep[i] = 0xff;
        }
    }

    void eeprom_busy_wait() {

    }

    void eeprom_update_byte (u1_mon::plogtable_t p, uint8_t v) {
        if (p >= 0 && p < sizeof(eep)) {
            eep[p] = v;
        }
    }

    void eeprom_write_byte (u1_mon::plogtable_t p, uint8_t v) {
        if (p >= 0 && p < sizeof(eep)) {
            eep[p] = v;
        }
    }

    uint8_t eeprom_read_byte (u1_mon::plogtable_t p) {
        if (p >= 0 && p < sizeof(eep)) {
            return eep[p];
        }
        return 0xff;
    }

    void eeprom_read_block (void *dest, u1_mon::plogtable_t src, int size) {
        if (dest == NULL) {
            return;
        }
        while (size > 0) {
            if (src >= 0 && src < sizeof(eep) ) {
                *((uint8_t *)dest) = eep[src++];
            }
            size--;
        }
    }

    void eeprom_update_block (void *src, u1_mon::plogtable_t dst, int size) {
        uint8_t *pFrom = (uint8_t *)src;
        while (size-- > 0) {
            uint8_t b = *pFrom++;
            eeprom_write_byte(dst++, b);
        }
    }

}

namespace u1_app {
    struct Clock {
        uint16_t ms;
        uint8_t  sec;
        uint8_t  min;
        uint8_t  hrs;
    };
    
    struct Trim {
        uint32_t magic;
        uint16_t startCnt; // only 15 bits used -> log record
        uint8_t  vcpK;
        uint8_t  vcpD;
        uint8_t  currK;
        int8_t   tempK;
        int8_t   tempOffs;
    };
    
    struct App {
        struct Trim trim;
        struct Clock clock;
    } app;
}

using namespace std;

namespace u1_mon {
    struct Mon mon;

    void clearLogTable () {
        plogtable_t p = (plogtable_t)EEP_LOG_START;
        while ((uint16_t)p < E2END) {
            eeprom_busy_wait();
            eeprom_update_byte(p, 0xff);
            p++;
        }
        mon.log.pLatestRecord = 0;
    }

    plogtable_t getNextLogRecord (plogtable_t p) {
        if (p == 0 || (uint16_t)p > E2END) {
            // illegal argument p, clear all records and return start of table
            clearLogTable();
            return (plogtable_t)EEP_LOG_START;
        } 
        eeprom_busy_wait();
        uint8_t rSize = eeprom_read_byte(p);
        if (rSize == 0xff) {
            return p;
        } else if (rSize < sizeof (struct LogRecordHeader)) {
            // failure in table, clear all records and return start of table
            clearLogTable();
            return (plogtable_t)EEP_LOG_START;
        }
        p = p + rSize;
        if ((uint16_t)p > E2END) {
            return (plogtable_t)EEP_LOG_START;
        }
        return p;
    }

     plogtable_t getLogLatestRecord () {
        plogtable_t prv = (plogtable_t) EEP_LOG_START;
        struct LogRecordHeader h; 
        uint32_t timeLatest = 0;

        plogtable_t p = (plogtable_t)EEP_LOG_START;
        while (1) {
            eeprom_busy_wait();
            eeprom_read_block(&h, p, sizeof(struct LogRecordHeader));
            if (h.size == 0xff) {
                return (plogtable_t)NULL;
            }
            uint32_t t = *( (uint32_t *)&h.time );
            if (t == 0 || h.size < sizeof(struct LogRecordHeader)) {
                // failure in table, clear all records and return start of table
                clearLogTable();
                return (plogtable_t)NULL;
            }
            if (t > timeLatest)     {
                timeLatest = t;
                prv = p;
            }
            p = (plogtable_t)(p + h.size);
            if ((uint16_t)p > E2END) {
                return prv;
            }
        }
    }

    void saveLog (void *pData, uint8_t typ, uint8_t size) {
        plogtable_t p;
        if (mon.log.pLatestRecord == (plogtable_t)NULL) {
            mon.log.pLatestRecord = getLogLatestRecord();
        }
        if (mon.log.pLatestRecord == (plogtable_t)NULL) {
            p = (plogtable_t)EEP_LOG_START;
        } else {
            eeprom_busy_wait();
            uint8_t lrSize = eeprom_read_byte(mon.log.pLatestRecord);
            p = p + lrSize;
            if ( ((uint16_t)p + sizeof(struct LogRecordHeader) + size) > E2END ) {
                p = (plogtable_t)EEP_LOG_START;
            }
        }
        
        struct LogRecordHeader h;
        h.size = size + sizeof(LogRecordHeader);
        h.typ = 0;
        h.time.systemStartCnt = u1_app::app.trim.startCnt;
        cli();
        h.time.sec = u1_app::app.clock.sec;
        h.time.min = u1_app::app.clock.min;
        h.time.hrs = u1_app::app.clock.hrs;
        sei();
        eeprom_busy_wait();
        eeprom_update_block(&h, p, sizeof(h));
        eeprom_busy_wait();
        eeprom_update_block(pData, p + sizeof(h), size);
        eeprom_busy_wait();
        eeprom_write_byte((p + 1), typ);
        eeprom_busy_wait();
        mon.log.pLatestRecord = p;
    }
}

int main () {
    u1_app::app.trim.startCnt = 0x08;
    u1_app::app.clock.hrs = 1;
    u1_app::app.clock.min = 0x02;
    u1_app::app.clock.sec = 0x03;


    eeprom_init();
    u1_mon::plogtable_t pLatest = u1_mon::getLogLatestRecord();
    u1_mon::mon.log.pLatestRecord = pLatest;

    struct Data {
        uint16_t cnt1;
        uint16_t cnt2;
    } x = { 0x0102, 0x0304 };

    u1_mon::saveLog(&x, 1, sizeof x);
    
    x.cnt1 = 0x0506;
    x.cnt2 = 0x0708;
    u1_mon::saveLog(&x, 1, sizeof x);

    printf("%lu\n", (long unsigned)u1_mon::mon.log.pLatestRecord);
    
    return 0;
}
