#include <math.h>
#include "av.h"

int au_cursor;

// Объявление данных
SDL_AudioSpec sdl_audio = {44100, AUDIO_U8, 2, 0, 2048};

// Обработчик
void audio_callback(void *data, unsigned char *stream, int len)
{
    for (int i = 0; i < len; i += 2) {

        float v1 = 128;
        float v2 = 128;

        stream[i]   = v1;
        stream[i+1] = v2;

        au_cursor += 1;
    }
}

// -----------------------------------------------------------------------------
// ОБЩИЕ МЕТОДЫ
// -----------------------------------------------------------------------------

C65::C65(int w, int h, int scale, int fps) {

    unsigned format = SDL_PIXELFORMAT_BGRA32;

    _scale  = scale;
    _width  = w;
    _height = h;

    width   = w * scale;
    height  = h * scale;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        exit(1);
    }

    SDL_ClearError();

    // Создать окно
    sdl_window          = SDL_CreateWindow("FOX C65 EMULATOR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    sdl_renderer        = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_PRESENTVSYNC);
    sdl_pixel_format    = SDL_AllocFormat(format);
    sdl_screen_texture  = SDL_CreateTexture(sdl_renderer, format, SDL_TEXTUREACCESS_STREAMING, _width, _height);
    SDL_SetTextureBlendMode(sdl_screen_texture, SDL_BLENDMODE_NONE);

    sdl_audio.callback = audio_callback;
    SDL_OpenAudio(&sdl_audio, 0);
    SDL_PauseAudio(0);

    screen_buffer       = (Uint32*)malloc(width * height * sizeof(Uint32));
    frame_length        = 1000 / (fps ? fps : 1);
    frame_prev_ticks    = SDL_GetTicks();
}

C65::~C65() {

    SDL_DestroyTexture(sdl_screen_texture);
    SDL_FreeFormat(sdl_pixel_format);
    SDL_DestroyRenderer(sdl_renderer);

    free(screen_buffer);

    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

void C65::load(int argc, char** argv) {

    int i = 1;
    while (i < argc) {

        if (argv[i][0] == '-') {

            switch (argv[i][1]) {
                case 'd': debug = 1; target = 1000; break;
            }
        }
        // Загрузка файла, если он задан
        else {

            FILE* fp = fopen(argv[i], "rb");
            if (fp) {

                // fread(program, 2, 65536, fp);
                fclose(fp);

            } else {
                printf("File not found\n");
                exit(1);
            }
        }

        i++;
    }

    if (argc > 1) {


    }
}

// Один фрейм
void C65::frame() {

    uint32_t instr = 0;

    Uint32 TM = SDL_GetTicks();
    while (instr < target) {

        // Отладка
        if (debug) {

            // if (dbg_file == NULL) dbg_file = fopen("debug.log", "w");
            // ds_decode(pc);
            // fprintf(dbg_file, "%05X | %s\n", 2*pc, ds_line);
        }

        // Извлечь следующую клавишу
        /*
        if (kb_id > 0 && eoi == 0) {

            kb_code = kb[0];
            for (int i = 0; i < kb_id - 1; i++) kb[i] = kb[i+1];
            kb_id = (kb_id - 1) & 255;

            // if (intr_mask & 2) { eoi = 1; interruptcall(2); }
        }
        */

        // @TODO instr += step();
    }

    // TIMER IRQ
    // if (flag.i && (intr_mask & 1) && eoi == 0) { eoi = 1; interruptcall(1); }

    TM = SDL_GetTicks() - TM;

    // Корректировать максимальную скорость
    if (debug == 0 && TM < frame_length) {
        target = (target * frame_length) / (TM ? TM : 1);
    }

    // Не более 25 Мгц
    if (target > 1000000) {
        target = 1000000;
    }

    // Мигание курсора
    cur_flash = (cur_flash < 25) ? cur_flash + 1 : 0;

    // Обновить экран при каждом мигании
    if (cur_flash == 0 || cur_flash == 12) {
        // switch_vm(videomode);
    }
}

// Ожидание событий
int C65::main() {

    for (;;) {

        Uint32 ticks = SDL_GetTicks();

        // Ожидать наступления события
        while (SDL_PollEvent(& evt)) {

            switch (evt.type) {

                // Выход из программы по нажатии "крестика"
                case SDL_QUIT: {
                    return 0;
                }

                // https://wiki.machinesdl.org/SDL_Scancode
                case SDL_KEYDOWN:

                    kbd_scancode(evt.key.keysym.scancode, 0);
                    break;

                case SDL_KEYUP:

                    kbd_scancode(evt.key.keysym.scancode, 1);
                    break;

                // Движение мыши
                case SDL_MOUSEMOTION: {

                    mx = evt.motion.x;
                    my = evt.motion.y;
                    break;
                }

                // Движение мыши
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {

                    // SDL_BUTTON_LEFT | SDL_BUTTON_MIDDLE | SDL_BUTTON_RIGHT
                    // SDL_PRESSED | SDL_RELEASED
                    mb = evt.button.button;
                    ms = evt.button.state;

                    break;
                }

                // Все другие события
                default: break;
            }
        }

        // Истечение таймаута: обновление экрана
        if (ticks - frame_prev_ticks >= frame_length) {

            frame_prev_ticks = ticks;
            update();
            return 1;
        }

        SDL_Delay(1);
    }
}

// Обновить окно
void C65::update() {

    SDL_Rect dstRect;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = width;
    dstRect.h = height;

    SDL_UpdateTexture       (sdl_screen_texture, NULL, screen_buffer, _width * sizeof(Uint32));
    SDL_SetRenderDrawColor  (sdl_renderer, 0, 0, 0, 0);
    SDL_RenderClear         (sdl_renderer);
    SDL_RenderCopy          (sdl_renderer, sdl_screen_texture, NULL, &dstRect);
    SDL_RenderPresent       (sdl_renderer);
}

// -----------------------------------------------------------------------------
// ФУНКЦИИ РИСОВАНИЯ
// -----------------------------------------------------------------------------

// Установка точки
void C65::pset(int x, int y, Uint32 cl) {

    if (x < 0 || y < 0 || x >= _width || y >= _height)
        return;

    screen_buffer[y*_width + x] = cl;
}

// Печать символа в указанном месте
void C65::pchr(int x, int y, uint8_t ch) {

    x *= 8;
    y *= 16;

    for (int i = 0; i < 16; i++) {

        int m = ansi[ch][i];
        for (int j = 0; j < 8; j++) {

            int t = m;

            // Подчеркивание
            if (cur_x == loc_x && cur_y == loc_y && i >= 14 && cur_flash < 12)
                t |= 0xFF;

            int cl = t & (1 << (7-j)) ? fore : back;
            if (cl >= 0) pset(x + j, y + i, cl);
        }
    }

    loc_x++;
}

// Сканирование нажатой клавиши
// https://ru.wikipedia.org/wiki/Скан-код
void C65::kbd_scancode(int scancode, int release) {

    switch (scancode) {

        // Коды клавиш A-Z
        case SDL_SCANCODE_A: if (release) kbd_push(0xF0); kbd_push(0x1C); break;
        case SDL_SCANCODE_B: if (release) kbd_push(0xF0); kbd_push(0x32); break;
        case SDL_SCANCODE_C: if (release) kbd_push(0xF0); kbd_push(0x21); break;
        case SDL_SCANCODE_D: if (release) kbd_push(0xF0); kbd_push(0x23); break;
        case SDL_SCANCODE_E: if (release) kbd_push(0xF0); kbd_push(0x24); break;
        case SDL_SCANCODE_F: if (release) kbd_push(0xF0); kbd_push(0x2B); break;
        case SDL_SCANCODE_G: if (release) kbd_push(0xF0); kbd_push(0x34); break;
        case SDL_SCANCODE_H: if (release) kbd_push(0xF0); kbd_push(0x33); break;
        case SDL_SCANCODE_I: if (release) kbd_push(0xF0); kbd_push(0x43); break;
        case SDL_SCANCODE_J: if (release) kbd_push(0xF0); kbd_push(0x3B); break;
        case SDL_SCANCODE_K: if (release) kbd_push(0xF0); kbd_push(0x42); break;
        case SDL_SCANCODE_L: if (release) kbd_push(0xF0); kbd_push(0x4B); break;
        case SDL_SCANCODE_M: if (release) kbd_push(0xF0); kbd_push(0x3A); break;
        case SDL_SCANCODE_N: if (release) kbd_push(0xF0); kbd_push(0x31); break;
        case SDL_SCANCODE_O: if (release) kbd_push(0xF0); kbd_push(0x44); break;
        case SDL_SCANCODE_P: if (release) kbd_push(0xF0); kbd_push(0x4D); break;
        case SDL_SCANCODE_Q: if (release) kbd_push(0xF0); kbd_push(0x15); break;
        case SDL_SCANCODE_R: if (release) kbd_push(0xF0); kbd_push(0x2D); break;
        case SDL_SCANCODE_S: if (release) kbd_push(0xF0); kbd_push(0x1B); break;
        case SDL_SCANCODE_T: if (release) kbd_push(0xF0); kbd_push(0x2C); break;
        case SDL_SCANCODE_U: if (release) kbd_push(0xF0); kbd_push(0x3C); break;
        case SDL_SCANCODE_V: if (release) kbd_push(0xF0); kbd_push(0x2A); break;
        case SDL_SCANCODE_W: if (release) kbd_push(0xF0); kbd_push(0x1D); break;
        case SDL_SCANCODE_X: if (release) kbd_push(0xF0); kbd_push(0x22); break;
        case SDL_SCANCODE_Y: if (release) kbd_push(0xF0); kbd_push(0x35); break;
        case SDL_SCANCODE_Z: if (release) kbd_push(0xF0); kbd_push(0x1A); break;

        // Цифры
        case SDL_SCANCODE_0: if (release) kbd_push(0xF0); kbd_push(0x45); break;
        case SDL_SCANCODE_1: if (release) kbd_push(0xF0); kbd_push(0x16); break;
        case SDL_SCANCODE_2: if (release) kbd_push(0xF0); kbd_push(0x1E); break;
        case SDL_SCANCODE_3: if (release) kbd_push(0xF0); kbd_push(0x26); break;
        case SDL_SCANCODE_4: if (release) kbd_push(0xF0); kbd_push(0x25); break;
        case SDL_SCANCODE_5: if (release) kbd_push(0xF0); kbd_push(0x2E); break;
        case SDL_SCANCODE_6: if (release) kbd_push(0xF0); kbd_push(0x36); break;
        case SDL_SCANCODE_7: if (release) kbd_push(0xF0); kbd_push(0x3D); break;
        case SDL_SCANCODE_8: if (release) kbd_push(0xF0); kbd_push(0x3E); break;
        case SDL_SCANCODE_9: if (release) kbd_push(0xF0); kbd_push(0x46); break;

        // Keypad
        case SDL_SCANCODE_KP_0: if (release) kbd_push(0xF0); kbd_push(0x70); break;
        case SDL_SCANCODE_KP_1: if (release) kbd_push(0xF0); kbd_push(0x69); break;
        case SDL_SCANCODE_KP_2: if (release) kbd_push(0xF0); kbd_push(0x72); break;
        case SDL_SCANCODE_KP_3: if (release) kbd_push(0xF0); kbd_push(0x7A); break;
        case SDL_SCANCODE_KP_4: if (release) kbd_push(0xF0); kbd_push(0x6B); break;
        case SDL_SCANCODE_KP_5: if (release) kbd_push(0xF0); kbd_push(0x73); break;
        case SDL_SCANCODE_KP_6: if (release) kbd_push(0xF0); kbd_push(0x74); break;
        case SDL_SCANCODE_KP_7: if (release) kbd_push(0xF0); kbd_push(0x6C); break;
        case SDL_SCANCODE_KP_8: if (release) kbd_push(0xF0); kbd_push(0x75); break;
        case SDL_SCANCODE_KP_9: if (release) kbd_push(0xF0); kbd_push(0x7D); break;

        // Специальные символы
        case SDL_SCANCODE_GRAVE:        if (release) kbd_push(0xF0); kbd_push(0x0E); break;
        case SDL_SCANCODE_MINUS:        if (release) kbd_push(0xF0); kbd_push(0x4E); break;
        case SDL_SCANCODE_EQUALS:       if (release) kbd_push(0xF0); kbd_push(0x55); break;
        case SDL_SCANCODE_BACKSLASH:    if (release) kbd_push(0xF0); kbd_push(0x5D); break;
        case SDL_SCANCODE_LEFTBRACKET:  if (release) kbd_push(0xF0); kbd_push(0x54); break;
        case SDL_SCANCODE_RIGHTBRACKET: if (release) kbd_push(0xF0); kbd_push(0x5B); break;
        case SDL_SCANCODE_SEMICOLON:    if (release) kbd_push(0xF0); kbd_push(0x4C); break;
        case SDL_SCANCODE_APOSTROPHE:   if (release) kbd_push(0xF0); kbd_push(0x52); break;
        case SDL_SCANCODE_COMMA:        if (release) kbd_push(0xF0); kbd_push(0x41); break;
        case SDL_SCANCODE_PERIOD:       if (release) kbd_push(0xF0); kbd_push(0x49); break;
        case SDL_SCANCODE_SLASH:        if (release) kbd_push(0xF0); kbd_push(0x4A); break;
        case SDL_SCANCODE_BACKSPACE:    if (release) kbd_push(0xF0); kbd_push(0x66); break;
        case SDL_SCANCODE_SPACE:        if (release) kbd_push(0xF0); kbd_push(0x29); break;
        case SDL_SCANCODE_TAB:          if (release) kbd_push(0xF0); kbd_push(0x0D); break;
        case SDL_SCANCODE_CAPSLOCK:     if (release) kbd_push(0xF0); kbd_push(0x58); break;
        case SDL_SCANCODE_LSHIFT:       if (release) kbd_push(0xF0); kbd_push(0x12); break;
        case SDL_SCANCODE_LCTRL:        if (release) kbd_push(0xF0); kbd_push(0x14); break;
        case SDL_SCANCODE_LALT:         if (release) kbd_push(0xF0); kbd_push(0x11); break;
        case SDL_SCANCODE_RSHIFT:       if (release) kbd_push(0xF0); kbd_push(0x59); break;
        case SDL_SCANCODE_RETURN:       if (release) kbd_push(0xF0); kbd_push(0x5A); break;
        case SDL_SCANCODE_ESCAPE:       if (release) kbd_push(0xF0); kbd_push(0x76); break;
        case SDL_SCANCODE_NUMLOCKCLEAR: if (release) kbd_push(0xF0); kbd_push(0x77); break;
        case SDL_SCANCODE_KP_MULTIPLY:  if (release) kbd_push(0xF0); kbd_push(0x7C); break;
        case SDL_SCANCODE_KP_MINUS:     if (release) kbd_push(0xF0); kbd_push(0x7B); break;
        case SDL_SCANCODE_KP_PLUS:      if (release) kbd_push(0xF0); kbd_push(0x79); break;
        case SDL_SCANCODE_KP_PERIOD:    if (release) kbd_push(0xF0); kbd_push(0x71); break;
        case SDL_SCANCODE_SCROLLLOCK:   if (release) kbd_push(0xF0); kbd_push(0x7E); break;

        // F1-F12 Клавиши
        case SDL_SCANCODE_F1:   if (release) kbd_push(0xF0); kbd_push(0x05); break;
        case SDL_SCANCODE_F2:   if (release) kbd_push(0xF0); kbd_push(0x06); break;
        case SDL_SCANCODE_F3:   if (release) kbd_push(0xF0); kbd_push(0x04); break;
        case SDL_SCANCODE_F4:   if (release) kbd_push(0xF0); kbd_push(0x0C); break;
        case SDL_SCANCODE_F5:   if (release) kbd_push(0xF0); kbd_push(0x03); break;
        case SDL_SCANCODE_F6:   if (release) kbd_push(0xF0); kbd_push(0x0B); break;
        case SDL_SCANCODE_F7:   if (release) kbd_push(0xF0); kbd_push(0x83); break;
        case SDL_SCANCODE_F8:   if (release) kbd_push(0xF0); kbd_push(0x0A); break;
        case SDL_SCANCODE_F9:   if (release) kbd_push(0xF0); kbd_push(0x01); break;
        case SDL_SCANCODE_F10:  if (release) kbd_push(0xF0); kbd_push(0x09); break;
        case SDL_SCANCODE_F11:  if (release) kbd_push(0xF0); kbd_push(0x78); break;
        case SDL_SCANCODE_F12:  if (release) kbd_push(0xF0); kbd_push(0x07); break;

        // Расширенные клавиши
        case SDL_SCANCODE_LGUI:         kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x1F); break;
        case SDL_SCANCODE_RGUI:         kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x27); break;
        case SDL_SCANCODE_APPLICATION:  kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x2F); break;
        case SDL_SCANCODE_RCTRL:        kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x14); break;
        case SDL_SCANCODE_RALT:         kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x11); break;
        case SDL_SCANCODE_KP_DIVIDE:    kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x4A); break;
        case SDL_SCANCODE_KP_ENTER:     kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x5A); break;

        case SDL_SCANCODE_INSERT:       kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x70); break;
        case SDL_SCANCODE_HOME:         kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x6C); break;
        case SDL_SCANCODE_END:          kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x69); break;
        case SDL_SCANCODE_PAGEUP:       kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x7D); break;
        case SDL_SCANCODE_PAGEDOWN:     kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x7A); break;
        case SDL_SCANCODE_DELETE:       kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x71); break;

        case SDL_SCANCODE_UP:           kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x75); break;
        case SDL_SCANCODE_DOWN:         kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x72); break;
        case SDL_SCANCODE_LEFT:         kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x6B); break;
        case SDL_SCANCODE_RIGHT:        kbd_push(0xE0); if (release) kbd_push(0xF0); kbd_push(0x74); break;

        // Клавиша PrnScr
        case SDL_SCANCODE_PRINTSCREEN: {

            if (release == 0) {

                kbd_push(0xE0); kbd_push(0x12);
                kbd_push(0xE0); kbd_push(0x7C);

            } else {

                kbd_push(0xE0); kbd_push(0xF0); kbd_push(0x7C);
                kbd_push(0xE0); kbd_push(0xF0); kbd_push(0x12);
            }

            break;
        }

        // Клавиша Pause
        case SDL_SCANCODE_PAUSE: {

            kbd_push(0xE1);
            kbd_push(0x14); if (release) kbd_push(0xF0); kbd_push(0x77);
            kbd_push(0x14); if (release) kbd_push(0xF0); kbd_push(0x77);
            break;
        }
    }
}

// Добавить вызов прерывания клавиатуры в очередь
void C65::kbd_push(uint8_t code) {

    kb[kb_id] = code;
    kb_id = (kb_id + 1) & 255;
}
