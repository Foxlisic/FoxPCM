#include "av.h"

// Сброс процессора
void C65::reset() {

    pc      = 0x0200;   // Сразу же за областью стека
    reg_a   = 0x00;
    reg_x   = 0x00;
    reg_y   = 0x00;
    reg_s   = 0x00;
}

// Получение эффективного адреса
int C65::effective(int addr) {

    int opcode, iaddr, tmp, rt, pt;

    opcode = read(addr++);  // Чтение опкода
    addr  &= 0xffff;        // Чтобы адрес не вышел за пределы

    // Разобрать операнд
    switch (operand_types[opcode]) {

        // PEEK( PEEK( (arg + X) % 256) + PEEK((arg + X + 1) % 256) * 256
        // Indirect, X (b8,X)
        case NDX: {

            tmp = read( addr );
            tmp = (tmp + reg_x) & 0xff;
            return read( tmp ) + ((read((1 + tmp) & 0xff) << 8));
        }

        // Indirect, Y (b8),Y
        case NDY: {

            tmp = read(addr);
            rt  = read( tmp      & 0xFF);
            rt |= read((tmp + 1) & 0xFF) << 8;
            pt  = rt;
            rt  = (rt + reg_y) & 0xFFFF;
            return rt;
        }

        // Zero Page
        case ZP:  return read( addr );

        // Zero Page, X
        case ZPX: return (read(addr) + reg_x) & 0x00FF;

        // Zero Page, Y
        case ZPY: return (read(addr) + reg_y) & 0x00FF;

        // Absolute
        case ABS: return readw(addr);

        // Absolute, X
        case ABX: {

            pt = readw(addr);
            rt = pt + reg_x;
            return rt & 0xFFFF;
        }

        // Absolute, Y
        case ABY: {

            pt = readw(addr);
            rt = pt + reg_y;
            return rt & 0xFFFF;
        }

        // Indirect
        case IND: {

            addr  = readw(addr);
            iaddr = read(addr) + 256*read((addr & 0xFF00) + ((addr + 1) & 0x00FF));
            return iaddr;
        }

        // Relative
        case REL: {

            iaddr = read(addr);
            return (iaddr + addr + 1 + (iaddr < 128 ? 0 : -256)) & 0xFFFF;
        }
    }

    return -1;
}

// Вычисление количества cycles для branch
int C65::branch(int addr, int iaddr) {

    if ((addr & 0xFF00) != (iaddr & 0xFF00))
        return 2;

    return 1;
}

// Вызов прерывания
void C65::brk() {

    push((pc >> 8) & 0xff);         // Вставка обратного адреса в стек
    push(pc & 0xff);
    set_break(1);                   // Установить BFlag перед вставкой
    reg_p |= 0b00100000;            // 1
    push(reg_p);
    set_interrupt(1);
}

// Немаскируемое прерывание
void C65::nmi() {

    push((pc >> 8) & 0xff);         // Вставка обратного адреса в стек
    push(pc & 0xff);
    set_break(1);                  // Установить BFlag перед вставкой
    reg_p |= 0b00100000;           // 1
    push(reg_p);
    set_interrupt(1);
    pc = readw(0xFFFA);
}

// Исполнение шага инструкции
int C65::step() {

    int temp, optype, opcode, opname, ppurd = 1, src = 0;
    uint8_t  cycles_per_instr;
    uint16_t addr = pc;

    // Определение эффективного адреса
    uint16_t iaddr = effective(addr);

    // Прочесть информацию по опкодам
    opcode = read(addr);
    optype = operand_types[ opcode ];
    opname = opcode_names [ opcode ];

    // Эти инструкции НЕ ДОЛЖНЫ читать что-либо из памяти перед записью
    if (opname == STA || opname == STX || opname == STY) {
        ppurd = 0;
    }

    // Инкремент адреса при чтении опкода
    addr = (addr + 1) & 0xffff;

    // Базовые циклы + доп. циклы
    cycles_per_instr = cycles_basic[ opcode ];

    // --------------------------------
    // Чтение операнда из памяти
    // --------------------------------

    switch (optype) {

        // case ___: console.log(`Opcode ${opcode} error at ${pc}`); return;
        case NDX: // Indirect X (b8,X)
        case NDY: // Indirect, Y
        case ZP:  // Zero Page
        case ZPX: // Zero Page, X
        case ZPY: // Zero Page, Y
        case REL: // Relative

            addr = (addr + 1) & 0xffff;
            if (ppurd) src = read(iaddr);
            break;

        case ABS: // Absolute
        case ABX: // Absolute, X
        case ABY: // Absolute, Y
        case IND: // Indirect

            addr = (addr + 2) & 0xffff;
            if (ppurd) src = read(iaddr);
            break;

        case IMM: // Immediate

            if (ppurd) src = read(addr);
            addr = (addr + 1) & 0xffff;
            break;

        case ACC: // Accumulator source

            src = reg_a;
            break;
    }

    // --------------------------------
    // Разбор инструкции и исполнение
    // --------------------------------

    switch (opname) {

        // Сложение с учетом переноса
        case ADC: {

            temp = src + reg_a + (reg_p & 1);
            set_zero(temp & 0xff);
            set_sign(temp);
            set_overflow(((reg_a ^ src ^ 0x80) & 0x80) && ((reg_a ^ temp) & 0x80) );
            set_carry(temp > 0xff);
            reg_a = temp & 0xff;
            break;
        }

        // Логическое умножение
        case AND: {

            src &= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;
            break;
        }

        // Логический сдвиг вправо
        case ASL: {

            set_carry(src & 0x80);

            src <<= 1;
            src &= 0xff;
            set_sign(src);
            set_zero(src);

            if (optype == ACC) reg_a = src; else write(iaddr, src);
            break;
        }

        // Условные переходы
        case BCC: if (!if_carry())    { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BCS: if ( if_carry())    { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BNE: if (!if_zero())     { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BEQ: if ( if_zero())     { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BPL: if (!if_sign())     { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BMI: if ( if_sign())     { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BVC: if (!if_overflow()) { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;
        case BVS: if ( if_overflow()) { cycles_per_instr += branch(addr, iaddr); addr = iaddr; } break;

        // Копировать бит 6 в OVERFLOW флаг
        case BIT: {

            set_sign(src);
            set_overflow(0x40 & src);
            set_zero(src & reg_a);
            break;
        }

        // Программное прерывание
        case BRK: {

            pc = (pc + 2) & 0xffff;
            brk();
            addr = readw(0xFFFE);
            break;
        }

        // Флаги
        case CLC: set_carry(0);     break;
        case SEC: set_carry(1);     break;
        case CLD: set_decimal(0);   break;
        case SED: set_decimal(1);   break;
        case CLI: set_interrupt(0); break;
        case SEI: set_interrupt(1); break;
        case CLV: set_overflow(0);  break;

        // Сравнение A, X, Y с операндом
        case CMP:
        case CPX:
        case CPY: {

            src = (opname == CMP ? reg_a : (opname == CPX ? reg_x : reg_y)) - src;
            set_carry(src >= 0);
            set_sign(src);
            set_zero(src & 0xff);
            break;
        }

        // Уменьшение операнда на единицу
        case DEC: {

            src = (src - 1) & 0xff;
            set_sign(src);
            set_zero(src);
            write(iaddr, src);
            break;
        }

        // Уменьшение X на единицу
        case DEX: {

            reg_x = (reg_x - 1) & 0xff;
            set_sign(reg_x);
            set_zero(reg_x);
            break;
        }

        // Уменьшение Y на единицу
        case DEY: {

            reg_y = (reg_y - 1) & 0xff;
            set_sign(reg_y);
            set_zero(reg_y);
            break;
        }

        // Исключающее ИЛИ
        case EOR: {

            src ^= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;
            break;
        }

        // Увеличение операнда на единицу
        case INC: {

            src = (src + 1) & 0xff;
            set_sign(src);
            set_zero(src);
            write(iaddr, src);
            break;
        }

        // Уменьшение X на единицу
        case INX: {

            reg_x = (reg_x + 1) & 0xff;
            set_sign(reg_x);
            set_zero(reg_x);
            break;
        }

        // Увеличение Y на единицу
        case INY: {

            reg_y = (reg_y + 1) & 0xff;
            set_sign(reg_y);
            set_zero(reg_y);
            break;
        }

        // Переход по адресу
        case JMP: addr = iaddr; break;

        // Вызов подпрограммы
        case JSR: {

            addr = (addr - 1) & 0xffff;
            push((addr >> 8) & 0xff);   // Вставка обратного адреса в стек (-1)
            push(addr & 0xff);
            addr = iaddr;
            break;
        }

        // Загрузка операнда в аккумулятор
        case LDA: {

            set_sign(src);
            set_zero(src);
            reg_a = (src);
            break;
        }

        // Загрузка операнда в X
        case LDX: {

            set_sign(src);
            set_zero(src);
            reg_x = (src);
            break;
        }

        // Загрузка операнда в Y
        case LDY: {

            set_sign(src);
            set_zero(src);
            reg_y = (src);
            break;
        }

        // Логический сдвиг вправо
        case LSR: {

            set_carry(src & 0x01);
            src >>= 1;
            set_sign(src);
            set_zero(src);
            if (optype == ACC) reg_a = src; else write(iaddr, src);
            break;
        }

        // Логическое побитовое ИЛИ
        case ORA: {

            src |= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;
            break;
        }

        // Стек
        case PHA: push(reg_a); break;
        case PHP: push((reg_p | 0x30)); break;
        case PLP: reg_p = pull(); break;

        // Извлечение из стека в A
        case PLA: {

            src = pull();
            set_sign(src);
            set_zero(src);
            reg_a = src;
            break;
        }

        // Циклический сдвиг влево
        case ROL: {

            src <<= 1;
            if (if_carry()) src |= 0x1;
            set_carry(src > 0xff);

            src &= 0xff;
            set_sign(src);
            set_zero(src);

            if (optype == ACC) reg_a = src; else write(iaddr, src);
            break;
        }

        // Циклический сдвиг вправо
        case ROR: {

            if (if_carry()) src |= 0x100;
            set_carry(src & 0x01);

            src >>= 1;
            set_sign(src);
            set_zero(src);

            if (optype == ACC) reg_a = src; else write(iaddr, src);
            break;
        }

        // Возврат из прерывания
        case RTI: {

            reg_p = pull();
            src   = pull();
            src  |= (pull() << 8);
            addr  = src;
            break;
        }

        // Возврат из подпрограммы
        case RTS: {

            src  = pull();
            src += ((pull()) << 8) + 1;
            addr = (src);
            break;
        }

        // Вычитание
        case SBC: {

            temp = reg_a - src - (if_carry() ? 0 : 1);

            set_sign(temp);
            set_zero(temp & 0xff);
            set_overflow(((reg_a ^ temp) & 0x80) && ((reg_a ^ src) & 0x80));
            set_carry(temp >= 0);
            reg_a = (temp & 0xff);
            break;
        }

        // Запись содержимого A,X,Y в память
        case STA: write(iaddr, reg_a); break;
        case STX: write(iaddr, reg_x); break;
        case STY: write(iaddr, reg_y); break;

        // Пересылка содержимого аккумулятора в регистр X
        case TAX: {

            src = reg_a;
            set_sign(src);
            set_zero(src);
            reg_x = (src);
            break;
        }

        // Пересылка содержимого аккумулятора в регистр Y
        case TAY: {

            src = reg_a;
            set_sign(src);
            set_zero(src);
            reg_y = (src);
            break;
        }

        // Пересылка содержимого S в регистр X
        case TSX: {

            src = reg_s;
            set_sign(src);
            set_zero(src);
            reg_x = (src);
            break;
        }

        // Пересылка содержимого X в регистр A
        case TXA: {

            src = reg_x;
            set_sign(src);
            set_zero(src);
            reg_a = (src);
            break;
        }

        // Пересылка содержимого X в регистр S
        case TXS: {

            reg_s = reg_x;
            break;
        }

        // Пересылка содержимого Y в регистр A
        case TYA: {

            src = reg_y;
            set_sign(src);
            set_zero(src);
            reg_a = (src);
            break;
        }

        // -------------------------------------------------------------
        // Недокументированные расширенные инструкции
        // -------------------------------------------------------------

        case SLO: {

            // ASL
            set_carry(src & 0x80);
            src <<= 1;
            src &= 0xff;
            set_sign(src);
            set_zero(src);

            if (optype == ACC) reg_a = src;
            else write(iaddr, src);

            // ORA
            src |= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;
            break;
        }

        case RLA: {

            // ROL
            src <<= 1;
            if (if_carry()) src |= 0x1;
            set_carry(src > 0xff);
            src &= 0xff;
            set_sign(src);
            set_zero(src);
            if (optype == ACC) reg_a = src; else write(iaddr, src);

            // AND
            src &= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;
            break;
        }

        case RRA: {

            // ROR
            if (if_carry()) src |= 0x100;
            set_carry(src & 0x01);
            src >>= 1;
            set_sign(src);
            set_zero(src);
            if (optype == ACC) reg_a = src; else write(iaddr, src);

            // ADC
            temp = src + reg_a + (reg_p & 1);
            set_zero(temp & 0xff);
            set_sign(temp);
            set_overflow(((reg_a ^ src ^ 0x80) & 0x80) && ((reg_a ^ temp) & 0x80) );
            set_carry(temp > 0xff);
            reg_a = temp & 0xff;
            break;

        }

        case SRE: {

            // LSR
            set_carry(src & 0x01);
            src >>= 1;
            set_sign(src);
            set_zero(src);
            if (optype == ACC) reg_a = src; else write(iaddr, src);

            // EOR
            src ^= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;

            break;
        }

        case DCP: {

            // DEC
            src = (src - 1) & 0xff;
            set_sign(src);
            set_zero(src);
            write(iaddr, src);

            // CMP
            src = reg_a - src;
            set_carry(src >= 0);
            set_sign(src);
            set_zero(src & 0xff);
            break;
        }

        // Увеличить на +1 и вычесть из A полученное значение
        case ISC: {

            // INC
            src = (src + 1) & 0xff;
            set_sign(src);
            set_zero(src);
            write(iaddr, src);

            // SBC
            temp = reg_a - src - (if_carry() ? 0 : 1);

            set_sign(temp);
            set_zero(temp & 0xff);
            set_overflow(((reg_a ^ temp) & 0x80) && ((reg_a ^ src) & 0x80));
            set_carry(temp >= 0);
            reg_a = (temp & 0xff);
            break;
        }

        // A,X = src
        case LAX: {

            reg_a = (src);
            set_sign(src);
            set_zero(src);
            reg_x = (src);
            break;
        }

        case AAX: write(iaddr, reg_a & reg_x); break;

        // AND + Carry
        case AAC: {

            // AND
            src &= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;

            // Carry
            set_carry(reg_a & 0x80);
            break;
        }

        case ASR: {

            // AND
            src &= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;

            // LSR A
            set_carry(reg_a & 0x01);
            reg_a >>= 1;
            set_sign(reg_a);
            set_zero(reg_a);
            break;
        }

        case ARR: {

            // AND
            src &= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;

            // P[6] = A[6] ^ A[7]: Переполнение
            set_overflow((reg_a ^ (reg_a >> 1)) & 0x40);

            temp = (reg_a >> 7) & 1;
            reg_a >>= 1;
            reg_a |= (reg_p & 1) << 7;

            set_carry(temp);
            set_sign(reg_a);
            set_zero(reg_a);
            break;
        }

        case ATX: {

            reg_a |= 0xFF;

            // AND
            src &= reg_a;
            set_sign(src);
            set_zero(src);
            reg_a = src;
            reg_x = reg_a;
            break;

        }

        case AXS: {

            temp = (reg_a & reg_x) - src;
            set_sign(temp);
            set_zero(temp);
            set_carry(((temp >> 8) & 1) ^ 1);
            reg_x = temp;
            break;
        }

        // Работает правильно, а тесты все равно не проходят эти 2
        case SYA: {

            temp = read(pc + 2);
            temp = ((temp + 1) & reg_y);
            write(iaddr, temp & 0xff);
            break;
        }

        case SXA: {

            temp = read(pc + 2);
            temp = ((temp + 1) & reg_x);
            write(iaddr, temp & 0xff);
            break;
        }
    }

    // Установка нового адреса
    pc = addr;

    return cycles_per_instr;
}
