#include "av.h"

uint8_t C65::read(int x) {
    return sram[x];
}

void C65::write(int x, uint8_t y) {
    sram[x] = y;
}
