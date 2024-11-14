
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

// struct Note;
// struct Note {
//     int frequency;
//     int duration;//inverse duration
// };
// struct Chime {
//     struct Note* notes;
//     size_t length;
//     int bpm;
// };

// void playChime(PIN_Handle hBuzzer, struct Chime* chime) {
//     int beatDuration = 60000000 / chime->bpm;

//     size_t i;
//     for (i = 0; i < chime->length; i++) {
//         struct Note note = chime->notes[i];
//         if (note.frequency == 0) {
//             buzzerClose();
//         }
//         else {
//             buzzerOpen(hBuzzer);
//             buzzerSetFrequency(note.frequency);
//         }
//         sleepms(beatDuration/note.duration);
//     }
// }
