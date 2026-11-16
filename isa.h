#ifndef ISA_H
#define ISA_H

#include <systemc.h>

// ============================================================
// Definicion comun de la ISA de 16 bits para GOALKEEPER-8.
// Este archivo centraliza los opcodes y los helpers para
// construir instrucciones. Lo usan la Unidad de Control y
// el ensamblador del programa del portero, garantizando que
// ambos hablan exactamente el mismo lenguaje.
// ============================================================

namespace isa
{

// Opcode de instruccion: ocupa los 5 bits superiores [15:11]
enum Opcode
{
    NOP   = 0,
    LDI   = 1,
    MOV   = 2,
    ADD   = 3,
    SUB   = 4,
    AND   = 5,
    OR    = 6,
    CMP   = 7,
    JMP   = 8,
    JEQ   = 9,
    JNE   = 10,
    JGE   = 11,
    JLT   = 12,
    JGT   = 13,
    JLE   = 14,
    LD    = 15,
    ST    = 16,
    IN    = 17,
    OUT   = 18,
    HALT  = 19
};

// Opcode interno de la ALU: 3 bits
enum AluOp
{
    ALU_ADD = 0,
    ALU_SUB = 1,
    ALU_AND = 2,
    ALU_OR  = 3,
    ALU_CMP = 4
};

// ------------------------------------------------------------
// Formato R: [15:11]=opcode, [10:8]=RD, [7:5]=RA, [4:2]=RB
// ------------------------------------------------------------
inline sc_uint<16> formato_r(Opcode op, unsigned int rd,
                             unsigned int ra, unsigned int rb)
{
    sc_uint<16> instr = 0;
    instr.range(15, 11) = op;
    instr.range(10, 8)  = rd & 0x7;
    instr.range(7, 5)   = ra & 0x7;
    instr.range(4, 2)   = rb & 0x7;
    return instr;
}

// ------------------------------------------------------------
// Formato I: [15:11]=opcode, [10:8]=registro, [7:0]=inmediato
// ------------------------------------------------------------
inline sc_uint<16> formato_i(Opcode op, unsigned int reg, unsigned int inm)
{
    sc_uint<16> instr = 0;
    instr.range(15, 11) = op;
    instr.range(10, 8)  = reg & 0x7;
    instr.range(7, 0)   = inm & 0xFF;
    return instr;
}

// ------------------------------------------------------------
// Formato J: [15:11]=opcode, [10:3]=direccion, [2:0]=reservados
// ------------------------------------------------------------
inline sc_uint<16> formato_j(Opcode op, unsigned int direccion)
{
    sc_uint<16> instr = 0;
    instr.range(15, 11) = op;
    instr.range(10, 3)  = direccion & 0xFF;
    return instr;
}

} // namespace isa

#endif // ISA_H
