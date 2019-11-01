#ifndef U1_MON_HPP_
#define U1_MON_HPP_

#include "global.h"

#ifdef GLOBAL_MONITOR

#include "sys.hpp"
#include <avr/pgmspace.h>

namespace u1_mon {

    struct Mon {
        uint8_t dummy;
    };

    extern struct Mon mon;
    extern const struct u1_sys::MonCmdInfo PMEMSTR_CMDS[] PROGMEM;

    void init ();
    void main ();
    uint8_t getCmdCount ();
    int8_t  printLineHeader (uint8_t lineIndex);
    int8_t  printLine (uint8_t lineIndex, char keyPressed);
}

#endif // GLOBAL_MONITOR
#endif // U1_MON_HPP_