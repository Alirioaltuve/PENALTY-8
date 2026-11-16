#ifndef UNIDAD_CONTROL_H
#define UNIDAD_CONTROL_H

#include <systemc.h>
#include "isa.h"

// ============================================================
// Unidad de Control de GOALKEEPER-8
// - Totalmente combinacional.
// - Recibe la instruccion de 16 bits y las banderas actuales.
// - Genera todas las senales de control del camino de datos.
// - Todas las salidas tienen un valor definido en todas las rutas
//   para evitar latches accidentales.
// ============================================================

// Selector del origen del dato que se escribe en el banco de registros
#define WB_ALU 0   // resultado de la ALU (R-format)
#define WB_INM 1   // inmediato (LDI)
#define WB_MEM 2   // dato leido de memoria (LD)
#define WB_IO  3   // dato leido de un puerto de E/S (IN)
#define WB_RA  4   // copia directa del registro fuente (MOV)

SC_MODULE(UnidadControl)
{
    // -------------------- Entradas --------------------
    sc_in<sc_uint<16>> instruccion;
    sc_in<bool> flag_c;
    sc_in<bool> flag_z;

    // -------------------- Salidas ---------------------
    // Banco de registros
    sc_out<bool> reg_write;
    sc_out<sc_uint<3>> reg_a_sel;
    sc_out<sc_uint<3>> reg_b_sel;
    sc_out<sc_uint<3>> reg_w_sel;

    // ALU
    sc_out<sc_uint<3>> alu_op;
    sc_out<bool> alu_b_inm;       // 1 = operando B viene del inmediato
    sc_out<sc_uint<3>> write_back_sel;

    // Memoria de datos
    sc_out<bool> mem_write;
    sc_out<sc_uint<8>> mem_addr;

    // Entrada/Salida
    sc_out<bool> io_write;
    sc_out<bool> io_read;
    sc_out<sc_uint<8>> io_addr;

    // Banderas
    sc_out<bool> write_flags;

    // Control del PC
    sc_out<bool> salto;
    sc_out<sc_uint<8>> direccion_salto;

    // HALT
    sc_out<bool> halt;

    void decodificar()
    {
        sc_uint<16> instr = instruccion.read();

        unsigned int op = instr.range(15, 11).to_uint();
        unsigned int rd = instr.range(10, 8).to_uint();
        unsigned int ra = instr.range(7, 5).to_uint();
        unsigned int rb = instr.range(4, 2).to_uint();

        sc_uint<8> inm = instr.range(7, 0);
        sc_uint<8> dir = instr.range(10, 3);

        // ------- Valores seguros por defecto -------
        reg_write.write(false);
        reg_a_sel.write(0);
        reg_b_sel.write(0);
        reg_w_sel.write(rd);
        alu_op.write(isa::ALU_ADD);
        alu_b_inm.write(false);
        write_back_sel.write(WB_ALU);
        mem_write.write(false);
        mem_addr.write(inm);
        io_write.write(false);
        io_read.write(false);
        io_addr.write(inm);
        write_flags.write(false);
        salto.write(false);
        direccion_salto.write(0);
        halt.write(false);

        switch (op)
        {
            case isa::NOP:
                break;

            case isa::LDI:
                reg_write.write(true);
                reg_a_sel.write(rd);
                reg_w_sel.write(rd);
                write_back_sel.write(WB_INM);
                break;

            case isa::MOV:
                reg_write.write(true);
                reg_a_sel.write(ra);
                reg_w_sel.write(rd);
                write_back_sel.write(WB_RA);
                break;

            case isa::ADD:
            case isa::SUB:
            case isa::AND:
            case isa::OR:
                reg_write.write(true);
                reg_a_sel.write(ra);
                reg_b_sel.write(rb);
                reg_w_sel.write(rd);
                // ADD=3->0, SUB=4->1, AND=5->2, OR=6->3
                alu_op.write(op - isa::ADD);
                write_back_sel.write(WB_ALU);
                write_flags.write(true);
                break;

            case isa::CMP:
                // Solo actualiza banderas, no escribe registro
                reg_a_sel.write(ra);
                reg_b_sel.write(rb);
                alu_op.write(isa::ALU_CMP);
                write_flags.write(true);
                break;

            case isa::LD:
                reg_write.write(true);
                reg_w_sel.write(rd);
                mem_addr.write(inm);
                write_back_sel.write(WB_MEM);
                break;

            case isa::ST:
                reg_a_sel.write(rd);   // fuente en [10:8]
                mem_addr.write(inm);
                mem_write.write(true);
                break;

            case isa::IN:
                reg_write.write(true);
                reg_w_sel.write(rd);
                io_read.write(true);
                io_addr.write(inm);
                write_back_sel.write(WB_IO);
                break;

            case isa::OUT:
                reg_a_sel.write(rd);   // fuente en [10:8]
                io_write.write(true);
                io_addr.write(inm);
                break;

            case isa::JMP:
                salto.write(true);
                direccion_salto.write(dir);
                break;

            case isa::JEQ:
                direccion_salto.write(dir);
                if (flag_z.read()) salto.write(true);
                break;

            case isa::JNE:
                direccion_salto.write(dir);
                if (!flag_z.read()) salto.write(true);
                break;

            case isa::JGE:
                direccion_salto.write(dir);
                if (flag_c.read()) salto.write(true);
                break;

            case isa::JLT:
                direccion_salto.write(dir);
                if (!flag_c.read()) salto.write(true);
                break;

            case isa::JGT:
                direccion_salto.write(dir);
                if (flag_c.read() && !flag_z.read()) salto.write(true);
                break;

            case isa::JLE:
                direccion_salto.write(dir);
                if (!flag_c.read() || flag_z.read()) salto.write(true);
                break;

            case isa::HALT:
                halt.write(true);
                break;

            default:
                // Opcode ilegal: se mantienen los valores seguros.
                break;
        }
    }

    SC_CTOR(UnidadControl)
    {
        SC_METHOD(decodificar);
        sensitive << instruccion;
        sensitive << flag_c;
        sensitive << flag_z;
    }
};

#endif // UNIDAD_CONTROL_H
