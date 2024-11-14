
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include "buzzer.h"
#include "pitches.h"

void print(char* s, ...) {
    va_list args;
    va_start(args, s);

    System_printf(s, args);
    System_flush();

    va_end(args);
}

Void sleepms(int ms) {
    Task_sleep(ms * 1000 / Clock_tickPeriod);
}

Void playNoteForMs(PIN_Handle hBuzzer, int frequency, int ms) {
    if (frequency == 0)
        buzzerClose();
    else {
        buzzerOpen(hBuzzer);
        buzzerSetFrequency(frequency);
        sleepms(ms);
        buzzerClose();
    }
}


