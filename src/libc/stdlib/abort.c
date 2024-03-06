#include <stdlib.h>
#include <tic80.h>

void abort(void) {
    trace("[abort]\n", -1);
    tic80_exit();
    // wasm doesn't support signals, so just trap to halt the program.
    __builtin_trap();
}

void exit(__attribute__((unused)) int status) {
    // just ignore the status
    tic80_exit();
    while (1);
}
