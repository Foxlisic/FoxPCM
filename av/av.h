#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

#include "ansi.h"

// Операнды
enum OperandType
{
    ___ = 0,
    NDX = 1,  // (b8,X)
    ZP  = 2,  // b8
    IMM = 3,  // #b8
    ABS = 4,  // b16
    NDY = 5,  // (b8),Y
    ZPX = 6,  // b8,X
    ABY = 7,  // b16,Y
    ABX = 8,  // b16,X
    REL = 9,  // b8 (адрес)
    ACC = 10, // A
    IMP = 11, // -- нет --
    ZPY = 12, // b8,Y
    IND = 13  // (b16)
};

// Инструкции
enum InstructNames
{
    BRK = 1,  ORA = 2,  AND = 3,  EOR = 4,  ADC = 5,  STA = 6,  LDA = 7,
    CMP = 8,  SBC = 9,  BPL = 10, BMI = 11, BVC = 12, BVS = 13, BCC = 14, BCS = 15,
    BNE = 16, BEQ = 17, JSR = 18, RTI = 19, RTS = 20, LDY = 21, CPY = 22, CPX = 23,
    ASL = 24, PHP = 25, CLC = 26, BIT = 27, ROL = 28, PLP = 29, SEC = 30, LSR = 31,
    PHA = 32, PLA = 33, JMP = 34, CLI = 35, ROR = 36, SEI = 37, STY = 38, STX = 39,
    DEY = 40, TXA = 41, TYA = 42, TXS = 43, LDX = 44, TAY = 45, TAX = 46, CLV = 47,
    TSX = 48, DEC = 49, INY = 50, DEX = 51, CLD = 52, INC = 53, INX = 54, NOP = 55,
    SED = 56, AAC = 57, SLO = 58, RLA = 59, RRA = 60, SRE = 61, DCP = 62, ISC = 63,
    LAX = 64, AAX = 65, ASR = 66, ARR = 67, ATX = 68, AXS = 69, XAA = 70, AXA = 71,
    SYA = 72, SXA = 73, DOP = 74
};

// Имена инструкции
static const int opcode_names[256] = {

    /*        00  01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F */
    /* 00 */ BRK, ORA, ___, SLO, DOP, ORA, ASL, SLO, PHP, ORA, ASL, AAC, DOP, ORA, ASL, SLO,
    /* 10 */ BPL, ORA, ___, SLO, DOP, ORA, ASL, SLO, CLC, ORA, NOP, SLO, DOP, ORA, ASL, SLO,
    /* 20 */ JSR, AND, ___, RLA, BIT, AND, ROL, RLA, PLP, AND, ROL, AAC, BIT, AND, ROL, RLA,
    /* 30 */ BMI, AND, ___, RLA, DOP, AND, ROL, RLA, SEC, AND, NOP, RLA, DOP, AND, ROL, RLA,
    /* 40 */ RTI, EOR, ___, SRE, DOP, EOR, LSR, SRE, PHA, EOR, LSR, ASR, JMP, EOR, LSR, SRE,
    /* 50 */ BVC, EOR, ___, SRE, DOP, EOR, LSR, SRE, CLI, EOR, NOP, SRE, DOP, EOR, LSR, SRE,
    /* 60 */ RTS, ADC, ___, RRA, DOP, ADC, ROR, RRA, PLA, ADC, ROR, ARR, JMP, ADC, ROR, RRA,
    /* 70 */ BVS, ADC, ___, RRA, DOP, ADC, ROR, RRA, SEI, ADC, NOP, RRA, DOP, ADC, ROR, RRA,
    /* 80 */ DOP, STA, DOP, AAX, STY, STA, STX, AAX, DEY, DOP, TXA, XAA, STY, STA, STX, AAX,
    /* 90 */ BCC, STA, ___, AXA, STY, STA, STX, AAX, TYA, STA, TXS, AAX, SYA, STA, SXA, AAX,
    /* A0 */ LDY, LDA, LDX, LAX, LDY, LDA, LDX, LAX, TAY, LDA, TAX, ATX, LDY, LDA, LDX, LAX,
    /* B0 */ BCS, LDA, ___, LAX, LDY, LDA, LDX, LAX, CLV, LDA, TSX, LAX, LDY, LDA, LDX, LAX,
    /* C0 */ CPY, CMP, DOP, DCP, CPY, CMP, DEC, DCP, INY, CMP, DEX, AXS, CPY, CMP, DEC, DCP,
    /* D0 */ BNE, CMP, ___, DCP, DOP, CMP, DEC, DCP, CLD, CMP, NOP, DCP, DOP, CMP, DEC, DCP,
    /* E0 */ CPX, SBC, DOP, ISC, CPX, SBC, INC, ISC, INX, SBC, NOP, SBC, CPX, SBC, INC, ISC,
    /* F0 */ BEQ, SBC, ___, ISC, DOP, SBC, INC, ISC, SED, SBC, NOP, ISC, DOP, SBC, INC, ISC
};

// Типы операндов для каждого опкода
static const int operand_types[256] =
{
    /*       00   01   02   03   04   05   06   07   08   09   0A   0B   0C   0D   0E   0F */
    /* 00 */ IMP, NDX, ___, NDX, ZP , ZP , ZP , ZP , IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS,
    /* 10 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    /* 20 */ ABS, NDX, ___, NDX, ZP , ZP , ZP , ZP , IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS,
    /* 30 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    /* 40 */ IMP, NDX, ___, NDX, ZP , ZP , ZP , ZP , IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS,
    /* 50 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    /* 60 */ IMP, NDX, ___, NDX, ZP , ZP , ZP , ZP , IMP, IMM, ACC, IMM, IND, ABS, ABS, ABS,
    /* 70 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    /* 80 */ IMM, NDX, IMM, NDX, ZP , ZP , ZP , ZP , IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    /* 90 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPY, ZPY, IMP, ABY, IMP, ABY, ABX, ABX, ABY, ABX,
    /* A0 */ IMM, NDX, IMM, NDX, ZP , ZP , ZP , ZP , IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    /* B0 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPY, ZPY, IMP, ABY, IMP, ABY, ABX, ABX, ABY, ABY,
    /* C0 */ IMM, NDX, IMM, NDX, ZP , ZP , ZP , ZP , IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    /* D0 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX,
    /* E0 */ IMM, NDX, IMM, NDX, ZP , ZP , ZP , ZP , IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS,
    /* F0 */ REL, NDY, ___, NDY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX
};

static const int cycles_basic[256] =
{
    7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 3, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

// -----------------------------------------------------------------------------
// Основное ядро эмулятора
// -----------------------------------------------------------------------------

class C65 {
protected:

    SDL_Surface*        screen_surface  = NULL;
    SDL_Window*         sdl_window      = NULL;
    SDL_Renderer*       sdl_renderer;
    SDL_PixelFormat*    sdl_pixel_format;
    SDL_Texture*        sdl_screen_texture;
    SDL_Event           evt;

    Uint32* screen_buffer;
    Uint32  width, height, _scale, _width, _height;
    Uint32  frame_length;
    Uint32  frame_prev_ticks;

public:

    // Открытые поля
    int     kb_code, mx, my, ms, mb;
    int     kb[256], kb_id = 0;
    int     fore  = 0xFFFFFF, back = 0x000000;
    int     loc_x = 0, loc_y = 0;
    int     cur_x = 0, cur_y, cur_flash = 0;
    int     debug = 0, target = 10000;

    // Информация о SPI
    uint8_t     spi_data, spi_st = 2, spi_status, spi_command, spi_crc, spi_resp;
    uint32_t    spi_arg, spi_lba;
    uint16_t    spi_phase;
    uint8_t     spi_sector[512];
    FILE*       spi_file;

    // Процессор
    uint8_t     sram[65536];
    uint8_t     reg_p, reg_s, reg_x, reg_y, reg_a;
    uint16_t    pc;
    int         cycles_ext;

    // Конструктор и деструктор
     C65(int w, int h, int scale, int fps);
    ~C65();

    // Главные свойства окна
    int         main();
    void        update();
    void        destroy();
    void        frame();
    void        pset(int x, int y, Uint32 cl);
    void        pchr(int x, int y, uint8_t ch);
    void        load(int, char**);
    void        kbd_scancode(int scancode, int release);
    void        kbd_push(uint8_t code);
    void        spi_cmd(uint8_t data);

    // CPU
    uint8_t read(int x);
    void    write(int x, uint8_t y);

    // Чтение слова
    uint16_t readw(int addr) {

        int l = read(addr);
        int h = read(addr+1);
        return 256*h + l;
    }

    void    reset();
    int     effective(int addr);
    int     branch(int addr, int iaddr);
    void    brk();
    void    nmi();
    int     step();

    // Установка флагов
    void set_zero(int x)      { reg_p = x ? (reg_p & 0xFD) : (reg_p | 0x02); }
    void set_overflow(int x)  { reg_p = x ? (reg_p | 0x40) : (reg_p & 0xBF); }
    void set_carry(int x)     { reg_p = x ? (reg_p | 0x01) : (reg_p & 0xFE); }
    void set_decimal(int x)   { reg_p = x ? (reg_p | 0x08) : (reg_p & 0xF7); }
    void set_break(int x)     { reg_p = x ? (reg_p | 0x10) : (reg_p & 0xEF); }
    void set_interrupt(int x) { reg_p = x ? (reg_p | 0x04) : (reg_p & 0xFB); }
    void set_sign(int x)      { reg_p = !!(x & 0x80) ? (reg_p | 0x80) : (reg_p & 0x7F); };

    // Получение значений флагов
    int if_carry()      { return !!(reg_p & 0x01); }
    int if_zero()       { return !!(reg_p & 0x02); }
    int if_interrupt()  { return !!(reg_p & 0x04); }
    int if_overflow()   { return !!(reg_p & 0x40); }
    int if_sign()       { return !!(reg_p & 0x80); }

    // Работа со стеком
    void push(int x) { write(0x100 + reg_s, x & 0xff); reg_s = ((reg_s - 1) & 0xff); }
    int  pull()      { reg_s = (reg_s + 1) & 0xff; return read(0x100 + reg_s); }

};
