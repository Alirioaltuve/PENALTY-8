#ifndef CPU_GOALKEEPER_H
#define CPU_GOALKEEPER_H

#include <systemc.h>
#include <vector>
#include "pc.h"
#include "memoria_instrucciones.h"
#include "unidad_control.h"
#include "banco_registros.h"
#include "alu.h"
#include "registro_banderas.h"
#include "memoria_datos.h"

// ============================================================
// CPU_Goalkeeper
// Computador RISC de 8 bits, una instruccion por ciclo.
// Conecta: PC, memoria de instrucciones, unidad de control,
// banco de registros, ALU, registro de banderas y memoria de
// datos, ademas de la interfaz de entrada/salida.
//
// Todos los elementos de estado (PC, registros, banderas,
// memoria de datos) se actualizan en el flanco positivo.
// El camino de datos (memoria de instrucciones, control,
// lectura de registros, ALU, writeback) es combinacional.
// ============================================================

SC_MODULE(CPU_Goalkeeper)
{
    // -------------------- Reloj y reset ----------------
    sc_in<bool> clk;
    sc_in<bool> reset;

    // -------------------- Bus de E/S -------------------
    sc_out<bool> io_write;
    sc_out<bool> io_read;
    sc_out<sc_uint<8>> io_addr;
    sc_out<sc_uint<8>> io_wdata;
    sc_in<sc_uint<8>> io_rdata;

    // -------------------- Estado ------------------------
    sc_out<bool> halted;

    // -------------------- Senales internas --------------
    sc_signal<sc_uint<8>> pc_addr;
    sc_signal<sc_uint<16>> instr;

    sc_signal<bool> ctrl_reg_write;
    sc_signal<sc_uint<3>> ctrl_reg_a_sel;
    sc_signal<sc_uint<3>> ctrl_reg_b_sel;
    sc_signal<sc_uint<3>> ctrl_reg_w_sel;

    sc_signal<sc_uint<3>> ctrl_alu_op;
    sc_signal<bool> ctrl_alu_b_inm;
    sc_signal<sc_uint<3>> ctrl_wb_sel;

    sc_signal<bool> ctrl_mem_write;
    sc_signal<sc_uint<8>> ctrl_mem_addr;

    sc_signal<bool> ctrl_io_write;
    sc_signal<bool> ctrl_io_read;
    sc_signal<sc_uint<8>> ctrl_io_addr;

    sc_signal<bool> ctrl_write_flags;
    sc_signal<bool> ctrl_salto;
    sc_signal<sc_uint<8>> ctrl_dir_salto;
    sc_signal<bool> ctrl_halt;

    sc_signal<sc_uint<8>> reg_a_out;
    sc_signal<sc_uint<8>> reg_b_out;

    sc_signal<sc_uint<8>> alu_b;
    sc_signal<sc_uint<8>> alu_result;
    sc_signal<sc_uint<8>> mem_read;

    sc_signal<bool> alu_carry;
    sc_signal<bool> alu_zero;

    sc_signal<bool> flag_c;
    sc_signal<bool> flag_z;

    sc_signal<sc_uint<8>> writeback;

    // -------------------- Submodulos --------------------
    PC pc;
    MemoriaInstrucciones imem;
    UnidadControl uc;
    BancoRegistros regs;
    ALU alu;
    RegistroBanderas flags;
    MemoriaDatos dmem;

    // -------------------- Glue combinacional -----------
    // Conecta las senales internas con los puertos del modulo
    // y resuelve los multiplexores del camino de datos.
    void enlazar()
    {
        // Puertos de salida de E/S
        io_write.write(ctrl_io_write.read());
        io_read.write(ctrl_io_read.read());
        io_addr.write(ctrl_io_addr.read());
        io_wdata.write(reg_a_out.read());
        halted.write(ctrl_halt.read());

        // Mux del operando B de la ALU
        if (ctrl_alu_b_inm.read())
            alu_b.write(instr.read().range(7, 0));
        else
            alu_b.write(reg_b_out.read());

        // Mux de writeback hacia el banco de registros
        switch (ctrl_wb_sel.read().to_uint())
        {
            case WB_INM:
                writeback.write(instr.read().range(7, 0));
                break;
            case WB_MEM:
                writeback.write(mem_read.read());
                break;
            case WB_IO:
                writeback.write(io_rdata.read());
                break;
            case WB_RA:
                writeback.write(reg_a_out.read());
                break;
            default: // WB_ALU
                writeback.write(alu_result.read());
                break;
        }
    }

    // Carga un programa en la memoria de instrucciones.
    void cargar_programa(const std::vector<sc_uint<16>>& programa)
    {
        imem.cargar_programa(programa);
    }

    SC_CTOR(CPU_Goalkeeper)
        : pc("pc"),
          imem("imem"),
          uc("uc"),
          regs("regs"),
          alu("alu"),
          flags("flags"),
          dmem("dmem")
    {
        // ---------------- PC ----------------
        pc.clk(clk);
        pc.reset(reset);
        pc.salto(ctrl_salto);
        pc.direccion_salto(ctrl_dir_salto);
        pc.direccion_actual(pc_addr);

        // ---------------- Memoria de instrucciones ----
        imem.direccion(pc_addr);
        imem.instruccion(instr);

        // ---------------- Unidad de control -----------
        uc.instruccion(instr);
        uc.flag_c(flag_c);
        uc.flag_z(flag_z);

        uc.reg_write(ctrl_reg_write);
        uc.reg_a_sel(ctrl_reg_a_sel);
        uc.reg_b_sel(ctrl_reg_b_sel);
        uc.reg_w_sel(ctrl_reg_w_sel);

        uc.alu_op(ctrl_alu_op);
        uc.alu_b_inm(ctrl_alu_b_inm);
        uc.write_back_sel(ctrl_wb_sel);

        uc.mem_write(ctrl_mem_write);
        uc.mem_addr(ctrl_mem_addr);

        uc.io_write(ctrl_io_write);
        uc.io_read(ctrl_io_read);
        uc.io_addr(ctrl_io_addr);

        uc.write_flags(ctrl_write_flags);
        uc.salto(ctrl_salto);
        uc.direccion_salto(ctrl_dir_salto);
        uc.halt(ctrl_halt);

        // ---------------- Banco de registros ----------
        regs.clk(clk);
        regs.reset(reset);
        regs.write_enable(ctrl_reg_write);
        regs.read_register_1(ctrl_reg_a_sel);
        regs.read_register_2(ctrl_reg_b_sel);
        regs.write_register(ctrl_reg_w_sel);
        regs.write_data(writeback);
        regs.read_data_1(reg_a_out);
        regs.read_data_2(reg_b_out);

        // ---------------- ALU -------------------------
        alu.A(reg_a_out);
        alu.B(alu_b);
        alu.operacion(ctrl_alu_op);
        alu.resultado(alu_result);
        alu.carry(alu_carry);
        alu.zero(alu_zero);

        // ---------------- Registro de banderas --------
        flags.clk(clk);
        flags.reset(reset);
        flags.actualizar(ctrl_write_flags);
        flags.carry_in(alu_carry);
        flags.zero_in(alu_zero);
        flags.carry_out(flag_c);
        flags.zero_out(flag_z);

        // ---------------- Memoria de datos ------------
        dmem.clk(clk);
        dmem.reset(reset);
        dmem.write_enable(ctrl_mem_write);
        dmem.direccion(ctrl_mem_addr);
        dmem.write_data(reg_a_out);
        dmem.read_data(mem_read);

        // ---------------- Glue combinacional ----------
        SC_METHOD(enlazar);
        sensitive << ctrl_io_write;
        sensitive << ctrl_io_read;
        sensitive << ctrl_io_addr;
        sensitive << reg_a_out;
        sensitive << ctrl_halt;
        sensitive << ctrl_alu_b_inm;
        sensitive << instr;
        sensitive << reg_b_out;
        sensitive << ctrl_wb_sel;
        sensitive << mem_read;
        sensitive << io_rdata;
        sensitive << alu_result;
    }
};

#endif // CPU_GOALKEEPER_H
