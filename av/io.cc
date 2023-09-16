#include "av.h"

uint8_t C65::read(int x) {
    return sram[x];
}

// E000-EFFF 4k FONT
// F000-FFFF 4k VRAM | 2K + 2K
void C65::write(int x, uint8_t y) {

    sram[x] = y;
    updatevm(x);
}

void C65::updatevm(int x) {

    // Обновить символ
    if (x >= 0xF000) {

        int ad = x - 0xF000;
        int si = ad & 0x7FF;
        int x_ = si % 80,
            y_ = si / 80;
        int ch = sram[0xF000 + si];
        int at = sram[0xF800 + si];

        fore = DOS16[at & 0x0F];
        back = DOS16[at >> 4];
        pchr(x_, y_, ch);
    }
}

void C65::refresh() {

    for (int i = 0xF000; i <= 0xFFFF; i++)
        updatevm(i);
}
