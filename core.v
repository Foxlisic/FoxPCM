// vec = 00 RESET; 01 IRQ1; 10 IRQ2; 11 BRK
module core
(
    input               clock,
    input               reset_n,
    input               ce,
    output      [15:0]  address,
    input       [ 7:0]  in,
    output reg  [ 7:0]  out,
    output reg          rd,
    output reg          we
);

assign address = cp ? ab : pc;

localparam

    LDC = 0,
    ZP  = 1,    ZPX = 2,    ZPY = 3,
    ABS = 4,    ABX = 5,    ABY = 6,    ABZ = 7,
    IX  = 8,    IX2 = 9,    IX3 = 10,
    IY  = 11,   IY2 = 12,   IY3 = 13,
    RUN = 14,   BRA = 15,
    JP1 = 16,   JP2 = 17,
    JND = 18,   TRM = 19,   POP = 20,   JSR = 21,
    RTS = 22,   BRK = 23,   RTI = 24;

localparam
    ORA = 0,    AND = 1,    EOR = 2,    ADC = 3,
    STA = 4,    LDA = 5,    CMP = 6,    SBC = 7,
    ASL = 8,    ROL = 9,    LSR = 10,   ROR = 11,
    BIT = 13,   DEC = 14,   INC = 15;

localparam
    DST_A = 0,  DST_X = 1,  DST_Y = 2,  DST_S = 3,
    SRC_D = 0,  SRC_X = 1,  SRC_Y = 2,  SRC_A = 3;

// Регистры
// ------------------------------------------------------
reg [ 7:0]  a, x, y, s, p;
//

// Управление процессором
// ------------------------------------------------------
reg         cp;
reg [ 4:0]  t;          // Состояние процессора
reg [ 2:0]  m;          // Процедура RUN
reg [15:0]  ab, pc;
reg [ 7:0]  op, w;      // Временный регистр
reg [ 3:0]  alu;        // Режим работы АЛУ
reg [ 1:0]  dst, src, vec;

// Вычисления
// ------------------------------------------------------
wire [8:0] zpx = in + x;
wire [8:0] zpy = in + y;
wire [3:0] bra = {p[1], p[0], p[6], p[7]}; // 3=Z,C,V,0=N
wire [7:0] sn  = s + 1;

// Исполнение инструкции
// ------------------------------------------------------
always @(posedge clock)
// Состояние сброса процессора
if (reset_n == 1'b0) begin

    //        NV1BDIZC
    p   <= 8'b01100001;
    t   <= 1'b0;
    cp  <= 1'b0;
    pc  <= 16'h0000;
    a   <= 8'h80;
    x   <= 8'hFE;
    y   <= 8'hDA;
    s   <= 8'h00;
    t   <= BRK;
    vec <= 2'b00;       // RESET
    m   <= 3;           // Без записи в стек

end
// Активное состояние
else if (ce) begin

    rd  <= 1'b0;
    we  <= 1'b0;

    case (t)

    // Считывание опкода
    LDC: begin

        pc  <= pc + 1;
        m   <= 0;
        op  <= in;
        dst <= DST_A;
        src <= SRC_D;

        // ---------------------------------------------------------------------
        // Выбор метода адресации
        // ---------------------------------------------------------------------

        casex (in)
        8'b000_000_00: t <= BRK; // 00
        8'b010_000_00: t <= RTI; // 40
        8'b010_011_00: t <= JP1; // 4C
        8'b011_011_00: t <= JND; // 6C
        8'b001_000_00: t <= JSR; // 20
        8'b011_000_00: t <= RTS; // 60
        8'bxxx_000_x1: t <= IX;
        8'bxxx_010_x1,
        8'b1xx_000_x0: t <= RUN; // IMM
        8'bxxx_100_x1: t <= IY;
        8'bxxx_110_x1: t <= ABY;
        8'bxxx_001_xx: t <= ZP;
        8'bxxx_011_xx,
        8'b001_000_00: t <= ABS;
        8'b10x_101_1x: t <= ZPY;
        8'bxxx_101_xx: t <= ZPX;
        8'b10x_111_1x: t <= ABY;
        8'bxxx_111_xx: t <= ABX;
        8'bxxx_100_00: t <= BRA;
        default:       t <= RUN;
        endcase

        // ---------------------------------------------------------------------
        // Подготовка к исполнению или выполнению
        // ---------------------------------------------------------------------

        casex (in)

        // Для BRK так работает обратный адрес
        8'b000_000_00: begin pc <= pc + 2; vec <= 3; end

        // Базовые
        8'bxxx_xxx_01: begin alu <= in[7:5]; end

        // STA, LDA, TAY, TXA, TAX, TYA
        8'b100_xx1_x0: begin alu <= STA; end
        8'b101_xx1_x0,
        8'b101_000_x0,
        8'b101_010_00,
        8'b10x_010_10,
        8'b100_110_00: begin alu <= LDA; end

        // CPY, CPX
        8'b11x_000_00,
        8'b11x_xx1_00: begin alu <= CMP; end

        // BIT
        8'b001_0x1_00: begin alu <= BIT; end

        // DEC, INC
        8'b110_xx1_10,
        8'b100_010_00,
        8'b110_010_10: begin alu <= DEC; end
        8'b111_xx1_10,
        8'b11x_010_00: begin alu <= INC; end

        // Флаги
        8'b00x_110_00: begin p[0] <= in[5]; t <= LDC; end // CLC, SEC
        8'b01x_110_00: begin p[2] <= in[5]; t <= LDC; end // CLI, SEI
        8'b101_110_00: begin p[6] <= 1'b0;  t <= LDC; end // CLV
        8'b11x_110_00: begin p[3] <= in[5]; t <= LDC; end // CLD, SED

        // ASL, ROL, LSR, ROR
        8'b0xx_010_10: begin alu <= {1'b1, in[7:5]}; src <= SRC_A; end
        8'b0xx_xx1_10: begin alu <= {1'b1, in[7:5]}; end

        // NOP, TXS, TSX
        8'b111_010_10: begin t <= LDC; end
        8'b100_110_10: begin t <= LDC; s <= x; end
        8'b101_110_10: begin x <= s; p[7] <= s[7]; p[1] <= s == 0; t <= LDC; end

        // PHA, PHP
        8'b0x0_010_00: begin

            t   <= TRM;
            we  <= 1;
            cp  <= 1;
            out <= in[6] ? a : (p | 8'h30);
            ab  <= {8'h01, s};
            s   <= s - 1;

        end

        // PLA, PLP
        8'b0x1_010_00: begin t <= POP; cp <= 1; ab <= {8'h01, sn}; s <= sn; end

        // RTS, RTI
        8'b01x_000_00: begin cp <= 1; ab <= {8'h01, sn}; end

        endcase

        // ---------------------------------------------------------------------
        // Выбор op1 (dst)
        // ---------------------------------------------------------------------

        casex (in)
        8'h86, 8'hA6, 8'hB6, 8'h96, 8'h8E, 8'hAE,
        8'hBE, 8'hA2, 8'hCA, 8'hE8, 8'h9A,
        8'hE0, 8'b111_xx1_00:
            dst <= DST_X;

        8'hA0, 8'h84, 8'hA4, 8'h88, 8'hC8,
        8'h8C, 8'hAC, 8'h94, 8'hB4, 8'hBC,
        8'hC0, 8'b110_xx1_00:
            dst <= DST_Y;
        endcase

        // ---------------------------------------------------------------------
        // Выбор op1 (src)
        // ---------------------------------------------------------------------

        case (in)
        8'hE8, 8'hCA, 8'h8A: src <= SRC_X; // INX, DEX, TXA
        8'h88, 8'hC8, 8'h98: src <= SRC_Y; // INY, DEY, TYA
        8'hA8, 8'hAA:        src <= SRC_A; // TAX, TAY
        endcase

    end

    // Декодирование адреса указателя на опкоды
    // -------------------------------------------------------------------------

    // Адресация по Zero Page
    ZP:  begin t <= RUN; ab <= in;       cp <= 1; rd <= 1; pc <= pc + 1; end
    ZPX: begin t <= RUN; ab <= zpx[7:0]; cp <= 1; rd <= 1; pc <= pc + 1; end
    ZPY: begin t <= RUN; ab <= zpy[7:0]; cp <= 1; rd <= 1; pc <= pc + 1; end

    // Абсолютная адресация
    ABS: begin t <= ABZ; pc <= pc + 1; ab <= in; end
    ABX: begin t <= ABZ; pc <= pc + 1; ab <= zpx; end
    ABY: begin t <= ABZ; pc <= pc + 1; ab <= zpy; end
    ABZ: begin t <= RUN; pc <= pc + 1; ab[15:8] <= ab[15:8] + in; cp <= 1; rd <= 1; end

    // Непрямая адресация
    IX:  begin t <= IX2; pc <= pc + 1; cp <= 1; ab <= zpx[7:0]; end
    IY:  begin t <= IY2; pc <= pc + 1; cp <= 1; ab <= in; end
    IX2,
    IY2: begin t <= IX3; w  <= in; ab[7:0] <= ab[7:0] + 1; end
    IX3: begin t <= RUN; rd <= 1; ab <= {in, w}; end
    IY3: begin t <= RUN; rd <= 1; ab <= {in, w} + y; end

    // Переход по условию
    BRA: begin t <= LDC; pc <= pc + 1 + ((bra[op[7:6]] == op[5]) ? {{8{in[7]}}, in[7:0]} : 0); end

    // Специальные инструкции
    // -------------------------------------------------------------------------

    // JMP ABS
    JP1: begin t <= JP2; pc <= pc + 1; w <= in; end
    JP2: begin t <= LDC; pc <= {in, w}; end

    // JMP (IND)
    JND: case (m)

        0: begin m <= 1;   ab[ 7:0] <= in; pc <= pc + 1; end
        1: begin m <= 2;   ab[15:8] <= in; pc <= pc + 1; cp <= 1; end
        2: begin m <= 3;   pc[ 7:0] <= in; ab[7:0] <= ab[7:0] + 1; end
        3: begin t <= LDC; pc[15:8] <= in; cp <= 0; end

    endcase

    // PLA, PLP
    POP: begin t <= LDC; cp <= 0; if (op[6]) a <= in; else p <= in; end

    // Jump Sub Routine
    JSR: case (m)

        0: begin m <= 1; w  <= in; pc <= pc + 1; end
        1: begin m <= 2; we <= 1; out <= pc[15:8]; ab      <= {8'h01, s};  pc[15:8] <= in; cp <= 1;end
        2: begin m <= 3; we <= 1; out <= pc[ 7:0]; ab[7:0] <= ab[7:0] - 1; pc[7:0]  <= w;  end
        3: begin t <= LDC; cp <= 0; s <= s - 2; end

    endcase

    // Return from Subroutine
    RTS: case (m)

        0: begin m <= 1;   pc[7:0] <= in; ab[7:0] <= ab[7:0] + 1; end
        1: begin t <= LDC; pc <= {in, pc[7:0]} + 1; s <= s + 2; end

    endcase

    // Вызов BRK или прерывания
    BRK: case (m)

        // BRK|IRQ стадия
        0: begin m <= 1;    we <= 1; ab      <= {8'h01, s};  out <= pc[15:8]; cp <= 1; s <= s - 3; end
        1: begin m <= 2;    we <= 1; ab[7:0] <= ab[7:0] - 1; out <= pc[ 7:0]; end
        2: begin m <= 3;    we <= 1; ab[7:0] <= ab[7:0] - 1; out <= p | 8'h10; p[2] <= 1'b1; end
        // RESET стадия
        3: begin m <= 4;    ab <= {13'h1FFF, vec, 1'b0}; end
        4: begin m <= 5;    pc[ 7:0] <= in; ab[0] <= 1'b1; end
        5: begin t <= LDC;  pc[15:8] <= in; cp <= 0; end

    endcase

    // Возврат из процедуры
    RTI: case (m)

        0: begin m <= 1;   p        <= in; ab[7:0] <= ab[7:0] + 1; end
        1: begin m <= 2;   pc[ 7:0] <= in; ab[7:0] <= ab[7:0] + 1; end
        2: begin t <= LDC; pc[15:8] <= in; s <= s + 3; cp <= 0; end

    endcase

    // Выполнение инструкции
    // -------------------------------------------------------------------------
    RUN: begin

        // По умолчанию, перейти к считыванию опкода
        cp <= 0;
        t  <= LDC;

        // Immediate
        casex (op) 8'bxxx_010_x1, 8'b1xx_000_x0: pc <= pc + 1; endcase

        // Разбор операции
        casex (op)

            // STA,STY,STX
            8'b100_xxx_01,
            8'b100_xx1_x0: begin t <= TRM; out <= R; we <= 1'b1; cp <= 1'b1; end
            // ORA, AND, ADC, EOR, SBC, LDA
            // ASL, ROL, LSR, ROR <ACC>
            // TXA, TYA
            8'bxxx_xxx_01,
            8'b0xx_010_10,
            8'b100_010_10,
            8'b100_110_00: begin a <= R; p <= F; end
            // LDY, INY, DEY, TAY
            8'b101_xx1_00,
            8'b101_000_00,
            8'b1x0_010_00,
            8'b101_010_00: begin y <= R; p <= F; end
            // LDX, INX, DEX, TAX
            8'b101_xx1_10,
            8'b101_000_10,
            8'b111_010_00,
            8'b110_010_10,
            8'b101_010_10: begin x <= R; p <= F; end
            // ASL, ROL, LSR, ROR
            // DEC, INC
            8'b0xx_xx1_10,
            8'b11x_xx1_10: begin t <= TRM; out <= R; we <= 1'b1; cp <= 1'b1; p <= F; end
            // CMP, CPY, CPX, BIT
            8'b110_xxx_01,
            8'b11x_000_00,
            8'b11x_xx1_00,
            8'b001_0x1_00: begin p <= F; end

        endcase

    end

    // Завершение цикла записи в память
    TRM: begin cp <= 0; t <= LDC; end

    endcase

end

// Арифметико-логическое устройство
// -----------------------------------------------------------------------------

// Левый операнд
wire [7:0] op1 =
    dst == DST_A ? a :
    dst == DST_X ? x :
    dst == DST_Y ? y : s;

// Правый операнд
wire [7:0] op2 =
    src == SRC_A ? a :
    src == SRC_X ? x :
    src == SRC_Y ? y : in;

// Результат
wire [8:0] R =
    // Базовые операции
    alu == ORA ? op1 | op2 :
    alu == AND ? op1 & op2 :
    alu == EOR ? op1 ^ op2 :
    alu == ADC ? op1 + op2 + cin :
    alu == STA ? op1 :
    alu == LDA ? op2 :
    alu == CMP ? op1 - op2 :
    alu == SBC ? op1 - op2 - !cin :
    // Расширенные
    alu == ASL ? {op2[6:0], 1'b0} :
    alu == ROL ? {op2[6:0], cin} :
    alu == LSR ? {1'b0, op2[7:1]} :
    alu == ROR ? {cin, op2[7:1]} :
    alu == BIT ? op1 & op2 :
    alu == DEC ? op2 - 1 :
    alu == INC ? op2 + 1 : 0;

// Вычисление флагов
wire sign  =  R[7];        // Флаг знака
wire zero  = ~|R[7:0];     // Тест на Zero
wire oadc  = (op1[7] ^ op2[7] ^ 1'b1) & (op1[7] ^ R[7]);
wire osbc  = (op1[7] ^ op2[7] ^ 1'b0) & (op2[7] ^ R[7]);
wire cin   =  p[0];
wire carry =  R[8];

// Новые флаги
wire [7:0] F =
    alu == ADC ? {sign, oadc, p[5:2], zero,  carry} :
    alu == CMP ? {sign,       p[6:2], zero, ~carry} :
    alu == SBC ? {sign, osbc, p[5:2], zero, ~carry} :
    alu == ASL || alu == ROL ? {sign, p[6:2], zero, op2[7]} :
    alu == LSR || alu == ROR ? {sign, p[6:2], zero, op2[0]} :
    alu == BIT ? {op2[7:6], p[5:2], zero, p[0]} :
                 {sign,     p[6:2], zero, p[0]};

endmodule
