#include <string>
#include <stdint.h>
#include <string.h>

#include "mon.hpp"

namespace std {

    uint8_t eep [E2END + 1];

    void cli() {}
    void sei() {}
    void popSREG () {}
    void pushSREGAndCli () {}

    void eeprom_init () {
        for (int i = 0; i < sizeof(eep); i++) {
            eep[i] = 0xff;
        }
    }

    void eeprom_busy_wait() {

    }

    void eeprom_update_byte (u1_mon::plogtable_t p, uint8_t v) {
        if (p >= 0 && p < sizeof(eep)) {
            // printf(" eep %04x <- %02x\n", p, v);
            eep[p] = v;
        }
    }

    void eeprom_write_byte (u1_mon::plogtable_t p, uint8_t v) {
        if (p >= 0 && p < sizeof(eep)) {
            // printf(" eep %04x <- %02x\n", p, v);
            eep[p] = v;
        }
    }

    uint8_t eeprom_read_byte (u1_mon::plogtable_t p) {
        if (p >= 0 && p < sizeof(eep)) {
            return eep[p];
        }
        return 0xff;
    }

    uint16_t eeprom_read_word (u1_mon::plogtable_t p) {
        return (eeprom_read_byte(p + 1) << 8) | eeprom_read_byte(p);
    }

    void eeprom_read_block (void *dest, u1_mon::plogtable_t src, int size) {
        uint8_t *p = (uint8_t *)dest;
        if (p == NULL) {
            return;
        }
        while (size > 0) {
            if (src >= 0 && src < sizeof(eep) ) {
                *p++ = eep[src++];
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

    struct LogChargingHistory {
        uint8_t typ;
        uint32_t time;
        struct u1_mon::LogDataCharging data;
    };


    struct App {
        struct u1_mon::LogDataCharging logDataCharging;
        struct LogChargingHistory logChargingHistory[4];
        struct Trim trim;
        struct Clock clock;
    } app;

    void clearLogHistory ();
    void addLogCharging (uint8_t typ, uint32_t time, uint8_t logIndex);
}

using namespace std;

namespace u1_mon {
    struct Mon mon;

    // uint8_t nextLogIndex (int16_t index) {
    //     if (index < 0) {
    //         return 0;
    //     }
    //     return index >= EEP_LOG_DESCRIPTORS ? 0 : index + 1;
    // }

    int16_t findLogDescriptorIndex (uint32_t time) {
        int16_t rv = -1;
        uint32_t rvTimeDiff = 0xffffffff;
        uint8_t index = 0;
        plogtable_t p = (plogtable_t)EEP_LOG_START + sizeof (struct LogDescriptor) * index;
        struct LogDescriptor d;
        while (index < EEP_LOG_DESCRIPTORS) {
            eeprom_busy_wait();
            eeprom_read_block(&d, p, sizeof (d));
            uint8_t typ = d.typ & 0x0f;
            if (typ != 0x0f) {
                uint32_t diff = d.time > time ? d.time - time : time - d.time;  
                if (diff < rvTimeDiff) {
                    rvTimeDiff = diff;
                    rv = index;
                }
                u1_app::addLogCharging(typ, d.time, index);
            }
            index++;
            p += sizeof(struct LogDescriptor);
        }
        return rv;
    }

    int16_t findNewestLogDescriptorIndex () {
        return findLogDescriptorIndex(0xffffffff);
    }

    int16_t findOldestLogDescriptorIndex () {
        return findLogDescriptorIndex(0);
    }

    void readLogData (uint8_t index, uint8_t *pDest, uint8_t destSize) {
        if (index >= EEP_LOG_DESCRIPTORS) {
            return;
        }
        eeprom_busy_wait();
        plogtable_t pFrom = (plogtable_t)EEP_LOG_SLOTS_START + EEP_LOG_SLOT_SIZE * index;
        eeprom_read_block(pDest, pFrom, destSize);
        printf(" readLogData %d %p %d -> %02x %02x %02x %02x\n", index, (void *)(pDest), destSize, pDest[0], pDest[1], pDest[2], pDest[3]);
    }

    uint8_t saveLog (uint8_t typ, void *pData, uint8_t size) {
        if (typ >= 0x0f) { 
            return 0xff; // error
        }
        struct LogDescriptor d;
        d.typ = 0x0f;
        pushSREGAndCli(); {
            uint16_t tHigh = (u1_app::app.trim.startCnt << 1) | ((u1_app::app.clock.hrs >> 4) & 0x01);
            uint16_t tLow = ((u1_app::app.clock.hrs & 0x0f) << 12) | ((u1_app::app.clock.min & 0x3f) << 6) | (u1_app::app.clock.sec & 0x3f);
            d.time = (((uint32_t)tHigh) << 16) | tLow;
        } popSREG();

        uint8_t index = 0;
        plogtable_t p = (plogtable_t)EEP_LOG_START;
        plogtable_t pTo = (plogtable_t)EEP_LOG_SLOTS_START;
        uint8_t lastTyp = typ;
        if (mon.log.index < EEP_LOG_DESCRIPTORS) {
            index = mon.log.index;
            p += index * sizeof (struct LogDescriptor);
            pTo += index * EEP_LOG_SLOT_SIZE;
            lastTyp = mon.log.lastTyp == 0 ? 0xff : mon.log.lastTyp;
        }
        uint8_t rv;
        int16_t bytes = size;
        uint8_t slotCnt = 0;

        do {    
            if (lastTyp != typ || slotCnt > 0) {
                index++;
                p += sizeof (struct LogDescriptor);
                pTo += EEP_LOG_SLOT_SIZE;
                if (index >= EEP_LOG_DESCRIPTORS) { 
                    p = (plogtable_t)EEP_LOG_START;
                    pTo = (plogtable_t)EEP_LOG_SLOTS_START;
                    index = 0;
                }
            }
            if (slotCnt == 0) {
                rv = index;
            }
            d.typ = (d.typ & 0x0f) | (slotCnt++ << 4);
            eeprom_busy_wait();
            eeprom_update_block(&d, p, sizeof(d));
            uint8_t l = bytes < EEP_LOG_SLOT_SIZE ? bytes : EEP_LOG_SLOT_SIZE;
            eeprom_busy_wait();
            if (pData != NULL && size > 0) {
                eeprom_update_block(pData, pTo, l);
                
                bytes -= EEP_LOG_SLOT_SIZE;
                pData = ((uint8_t *)pData) + l;
            }
        } while (bytes > 0 && slotCnt < 16);

        index = rv;
        p = (plogtable_t)EEP_LOG_START + index * sizeof (struct LogDescriptor);
        d.typ = typ;
        uint8_t slot = 0;
        while (slotCnt-- > 0) {
            d.typ = (d.typ & 0x0f) | (slot << 4);
            eeprom_busy_wait();
            eeprom_write_byte(p, *((uint8_t *)&d));
            p += sizeof (struct LogDescriptor);
            mon.log.index = index++;
            slot++;
            if (index >= EEP_LOG_DESCRIPTORS) { 
                p = (plogtable_t)EEP_LOG_START;
                index = 0;
            }
        }
        mon.log.lastTyp = typ;
        u1_app::addLogCharging(typ, d.time, rv);

        return rv;
    }

    void startupLog () {
        int16_t startIndex = findNewestLogDescriptorIndex();
        mon.log.index = startIndex < 0 ? EEP_LOG_DESCRIPTORS : startIndex + 1;
        if (mon.log.index >= EEP_LOG_DESCRIPTORS) {
            mon.log.index = 0;
        }
        mon.log.lastTyp = 0xff;
        saveLog(LOG_TYPE_SYSTEMSTART, NULL, 0);
    }

    void clearEEP (plogtable_t pStartAddr, uint16_t size) {
        while (size-- > 0 && (uint16_t)pStartAddr <= E2END) {
            eeprom_busy_wait();
            eeprom_write_byte(pStartAddr++, 0xff);
        }
        eeprom_busy_wait();
    }

    void clearLogTable () {
        clearEEP((plogtable_t)EEP_LOG_START, E2END + 1 - EEP_LOG_START);
        mon.log.index = 0xff;
        mon.log.lastTyp = 0x0f;
    }


    int8_t cmd_log (uint8_t argc, const char **argv) {
        if (argc == 2 && strcmp("clear", argv[1]) == 0) {
            clearLogTable();
            return 0;
        }
        if (argc > 1) { return -1; }

        struct LogDescriptor d;
        int16_t startIndex = findOldestLogDescriptorIndex();
        uint8_t cnt = 0;
        if (startIndex >= 0) {
            uint8_t index = (uint8_t)startIndex;
            plogtable_t p = (plogtable_t)EEP_LOG_START + sizeof (struct LogDescriptor) * index;
            struct LogDescriptor d;
            do {
                eeprom_busy_wait();
                eeprom_read_block(&d, p, sizeof (d));
                if ((d.typ & 0x0f) != 0x0f) {
                    cnt++;
                    uint8_t typ = d.typ & 0x0f;
                    uint8_t subIndex = d.typ >> 4;
                    uint16_t startupCnt = d.time >> 17;
                    uint8_t hrs = (d.time >> 12) & 0x1f;
                    uint8_t min = (d.time >> 6) & 0x3f;
                    uint8_t sec = d.time & 0x3f;
                    printf("  %2d(%01x/%01x) %5d-%02d:%02d:%02d -> ", index, typ, subIndex, startupCnt, hrs, min, sec);
                    uint8_t slot[EEP_LOG_SLOT_SIZE];
                    eeprom_busy_wait();
                    eeprom_read_block(&slot, (plogtable_t)EEP_LOG_SLOTS_START + index * EEP_LOG_SLOT_SIZE, sizeof (slot));
                    switch (d.typ) {
                        case LOG_TYPE_SYSTEMSTART: {
                            printf("system start");
                            break;
                        }

                        case LOG_TYPE_STARTCHARGE: {
                            struct LogDataStartCharge *pData = (struct LogDataStartCharge *)&slot;
                            printf("start charge maxAmps=%u", pData->maxAmps);
                            break;
                        }

                        case LOG_TYPE_CHARGING: case LOG_TYPE_STOPCHARGING: {
                            struct LogDataCharging *pData = (struct LogDataCharging *)&slot;
                            uint8_t e = pData->energyKwhX256 >> 8;
                            uint8_t nk = ((pData->energyKwhX256 & 0xff) * 100 + 128) / 256;
                            if (d.typ == LOG_TYPE_STOPCHARGING) {
                                printf("stop ");
                            }
                            printf("charge %u:%02u, E=%u.%02ukWh", pData->chgTimeHours, pData->chgTimeMinutes, e, nk);
                            break;
                        }

                        default: {
                            printf("? (");
                            for (uint8_t i = 0; i < sizeof slot; i++) {
                                printf(" %02x", slot[i]);
                            }
                            printf(")");
                            break;
                        }
                    }
                    printf("\n");
                }
                index++;
                p += sizeof(struct LogDescriptor);
                if (index >= EEP_LOG_DESCRIPTORS) {
                    index = 0;
                    p = (plogtable_t)EEP_LOG_START;
                }
            } while (index != startIndex);
        }
        printf("%d valid log records\n", cnt);

        printf("\nLog history:\n");
        for (uint8_t i = 0; i < (sizeof (u1_app::app.logChargingHistory) / sizeof (u1_app::app.logChargingHistory[0])); i++) {
            struct u1_app::LogChargingHistory *p= &(u1_app::app.logChargingHistory[i]);
            printf("  %u: typ=%u  time=%04x%04x ", i, p->typ, (uint16_t)(p->time >> 16), (uint16_t)(p->time));
            printf("= %u-%u:%02u:%02u -> ", (uint16_t)(p->time >> 17), (uint16_t)((p->time >> 12) & 0x1f), (uint16_t)((p->time >> 6) & 0x3f), (uint16_t)(p->time & 0x3f));
            printf("  %u:%02umin", p->data.chgTimeHours, p->data.chgTimeMinutes);
            printf("  %u.%02ukWh", p->data.energyKwhX256 >> 8, ((p->data.energyKwhX256 & 0xff) * 100 + 128) / 256);
            printf("\n");
        }

        return 0;
    }



}

namespace u1_app {

    void clearLogHistory () {
        int s = sizeof (u1_app::app.logChargingHistory);
        memset(u1_app::app.logChargingHistory, 0, sizeof (u1_app::app.logChargingHistory));
    }

    void addLogCharging (uint8_t typ, uint32_t time, uint8_t logIndex) {
        static uint8_t index = 0;
        if (typ != LOG_TYPE_STOPCHARGING) {
            return;
        }
        struct u1_app::LogChargingHistory *px = &u1_app::app.logChargingHistory[index];
        px->typ = typ;
        px->time = time;
        u1_mon::readLogData(logIndex, (uint8_t*)&px->data, sizeof(px->data));
        printf("hist[%d]: set %p ... %u %04x%04x\n", index, (void *)px, logIndex, (uint16_t)(time >> 16), (uint16_t)time);
        index++;
        if (index >= (sizeof(app.logChargingHistory) / sizeof(app.logChargingHistory[0]))) {
            index
        }

    }
}


int main () {
    eeprom_init();
    u1_app::app.trim.startCnt = 0x01;
    u1_app::app.clock.hrs = 0;
    u1_app::app.clock.min = 0x00;
    u1_app::app.clock.sec = 0x00;
    u1_mon::mon.log.index = 0xff;
    u1_mon::startupLog();

    struct u1_mon::LogDataStartCharge x1 = { 10 };
    struct u1_mon::LogDataCharging x2 = { 0, 0, 0 };
    u1_mon::saveLog(1, &x1, sizeof x1);
    for (int i = 0; i < 100; i++) {
        if (i % 100 == 99) {
            uint8_t f[16];
            for (int i = 0; i < sizeof f; i++) {
                f[i] = i + 10;
            }
            u1_mon::saveLog(14, &f, sizeof f);
        } else if (i % 10 == 9) {
            u1_mon::saveLog(3, &x2, sizeof x2);
        } else {
            u1_mon::saveLog(2, &x2, sizeof x2);
        }
        
        u1_app::app.clock.sec++;
        if (u1_app::app.clock.sec == 60) {
            u1_app::app.clock.min++;
            u1_app::app.clock.sec = 0;
        }
        x2.chgTimeMinutes += 2;
        if (x2.chgTimeMinutes >= 60) {
            x2.chgTimeMinutes -= 60;
            x2.chgTimeHours++;
        }
        x2.energyKwhX256 += 200;
    }
    
    u1_app::clearLogHistory();
    u1_mon::startupLog();


    const char *argv[] = { "cmd_log" };
    u1_mon::cmd_log(1, argv);
    
    return 0;
}
