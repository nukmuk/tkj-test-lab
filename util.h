
#include <xdc/std.h>
#include <xdc/runtime/System.h>

void print(char* s, ...) {
    va_list args;
    va_start(args, s);

    System_printf(s, args);
    System_flush();

    va_end(args);
}