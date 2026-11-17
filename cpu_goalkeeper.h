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
#include "isa.h"

// ============================================================
// CPU_Goalkeeper
// Pipeline RISC de 5 etapas: IF -> ID -> EX -> MEM -> WB.
//
// Cada etapa tiene su registro intermedio (IF/ID, ID/EX, EX/MEM,
// MEM/WB). Los registros son sc_signal y se actualizan en un unico
// proceso sensible al flanco positivo. Las etapas consumen el valor
// del registro del ciclo anterior: las escrituras hechas en el
// flanco no son visibles hasta el siguiente delta, evitando que una
// etapa lea datos recien producidos por otra en el mismo flanco.
//
// Esta fase NO incluye forwarding, deteccion de hazards, stalls ni
// flush de saltos.
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

    // -------------------- Señales del camino de datos ---
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

    sc_signal<sc_uint<8>> id_reg_a;
    sc_signal<sc_uint<8>> id_reg_b;

    sc_signal<sc_uint<8>> alu_b;
    sc_signal<sc_uint<8>> alu_result;
    sc_signal<sc_uint<8>> mem_read;

    sc_signal<bool> alu_carry;
    sc_signal<bool> alu_zero;

    sc_signal<bool> flag_c_arch;
    sc_signal<bool> flag_z_arch;

    sc_signal<bool> wb_enable;
    sc_signal<bool> wb_flags_enable;
    sc_signal<bool> mem_write_enable;

    // El PC no se salta en esta fase (flush de saltos pendiente).
    sc_signal<bool> nunca_salto;
    sc_signal<sc_uint<8>> dir_ignorada;

    // ---------- Pipeline IF/ID ----------
    sc_signal<bool> ifid_valid;
    sc_signal<sc_uint<16>> ifid_instr;
    sc_signal<sc_uint<8>> ifid_pc;

    // ---------- Pipeline ID/EX ----------
    sc_signal<bool> idex_valid;
    sc_signal<sc_uint<16>> idex_instr;
    sc_signal<sc_uint<8>> idex_pc;
    sc_signal<sc_uint<8>> idex_a;
    sc_signal<sc_uint<8>> idex_b;
    sc_signal<sc_uint<3>> idex_rd;
    sc_signal<sc_uint<3>> idex_rs1;
    sc_signal<sc_uint<3>> idex_rs2;
    sc_signal<sc_uint<8>> idex_imm;
    sc_signal<sc_uint<3>> idex_alu_op;
    sc_signal<bool> idex_alu_b_imm;
    sc_signal<sc_uint<3>> idex_wb_sel;
    sc_signal<bool> idex_reg_write;
    sc_signal<bool> idex_mem_write;
    sc_signal<sc_uint<8>> idex_mem_addr;
    sc_signal<bool> idex_io_write;
    sc_signal<bool> idex_io_read;
    sc_signal<sc_uint<8>> idex_io_addr;
    sc_signal<bool> idex_write_flags;

    // ---------- Pipeline EX/MEM ----------
    sc_signal<bool> exmem_valid;
    sc_signal<sc_uint<16>> exmem_instr;
    sc_signal<sc_uint<8>> exmem_alu_result;
    sc_signal<sc_uint<8>> exmem_a;
    sc_signal<sc_uint<8>> exmem_imm;
    sc_signal<sc_uint<3>> exmem_rd;
    sc_signal<sc_uint<3>> exmem_wb_sel;
    sc_signal<bool> exmem_reg_write;
    sc_signal<bool> exmem_mem_write;
    sc_signal<sc_uint<8>> exmem_mem_addr;
    sc_signal<sc_uint<8>> exmem_mem_wdata;
    sc_signal<bool> exmem_io_read;
    sc_signal<sc_uint<8>> exmem_io_data;
    sc_signal<bool> exmem_write_flags;
    sc_signal<bool> exmem_flag_c;
    sc_signal<bool> exmem_flag_z;

    // ---------- Pipeline MEM/WB ----------
    sc_signal<bool> memwb_valid;
    sc_signal<sc_uint<16>> memwb_instr;
    sc_signal<sc_uint<8>> memwb_wb_data;
    sc_signal<sc_uint<3>> memwb_rd;
    sc_signal<bool> memwb_reg_write;
    sc_signal<bool> memwb_write_flags;
    sc_signal<bool> memwb_flag_c;
    sc_signal<bool> memwb_flag_z;

    // -------------------- Observabilidad ---------------
    struct TrazaCiclo
    {
        int ciclo;
        sc_uint<8> pc;
        sc_uint<16> if_instr;
        bool ifid_valid;
        sc_uint<16> ifid_instr;
        bool idex_valid;
        sc_uint<16> idex_instr;
        bool exmem_valid;
        sc_uint<16> exmem_instr;
        bool memwb_valid;
        sc_uint<16> memwb_instr;
        bool wb_escribe;
        sc_uint<3> wb_rd;
        sc_uint<8> wb_dato;
    };

    struct EventoEscritura
    {
        sc_uint<3> rd;
        sc_uint<8> dato;
        sc_uint<16> instr;
    };

    std::vector<TrazaCiclo> traza;
    std::vector<EventoEscritura> escrituras;
    bool trazando;
    int ciclo_traza;

    void iniciar_traza()
    {
        traza.clear();
        ciclo_traza = 1;
        trazando = true;
    }

    void detener_traza() { trazando = false; }

    void limpiar_eventos() { escrituras.clear(); }

    sc_uint<8> valor_registro(int i) { return regs.registros[i & 0x7].read(); }

    // -------------------- Submodulos --------------------
    PC pc;
    MemoriaInstrucciones imem;
    UnidadControl uc;
    BancoRegistros regs;
    ALU alu;
    RegistroBanderas flags;
    MemoriaDatos dmem;

    // -------------------- Combinacional: mux B ALU ----
    void comb_alu_b()
    {
        if (idex_alu_b_imm.read())
            alu_b.write(idex_imm.read());
        else
            alu_b.write(idex_b.read());
    }

    // -------------------- Combinacional: escritura WB --
    void comb_wb()
    {
        wb_enable.write(memwb_valid.read() && memwb_reg_write.read());
        wb_flags_enable.write(memwb_valid.read() && memwb_write_flags.read());
    }

    // -------------------- Combinacional: escritura MEM -
    void comb_mem_we()
    {
        mem_write_enable.write(exmem_valid.read() && exmem_mem_write.read());
    }

    // -------------------- Salidas de E/S y HALT --------
    void salidas()
    {
        bool ex_valida = idex_valid.read();
        bool ex_out = ex_valida && idex_io_write.read();
        bool ex_in = ex_valida && idex_io_read.read();

        io_write.write(ex_out);
        io_read.write(ex_in);

        if (ex_out || ex_in)
            io_addr.write(idex_io_addr.read());
        else
            io_addr.write(0);

        io_wdata.write(idex_a.read());

        bool h = memwb_valid.read() &&
                 (memwb_instr.read().range(15, 11).to_uint() == isa::HALT);
        halted.write(h);
    }

    // -------------------- Proceso secuencial -----------
    // Un unico proceso actualiza los cuatro registros intermedios.
    // Lee el estado del ciclo anterior y solo entonces escribe el
    // siguiente. Las señales sc_signal difieren la escritura al
    // siguiente delta, por lo que nunca se mezclan ambos estados.
    void reloj()
    {
        bool rst = reset.read();

        // --- Estado previo (ciclo que termina) ---
        bool v_ifid = ifid_valid.read();
        sc_uint<16> instr_ifid = ifid_instr.read();
        sc_uint<8> ifid_pc_old = ifid_pc.read();

        bool v_idex = idex_valid.read();
        sc_uint<16> instr_idex = idex_instr.read();
        sc_uint<8> idex_a_old = idex_a.read();
        sc_uint<8> idex_b_old = idex_b.read();
        sc_uint<3> idex_rd_old = idex_rd.read();
        sc_uint<3> idex_rs1_old = idex_rs1.read();
        sc_uint<3> idex_rs2_old = idex_rs2.read();
        sc_uint<8> idex_imm_old = idex_imm.read();
        sc_uint<3> idex_alu_op_old = idex_alu_op.read();
        bool idex_alu_b_imm_old = idex_alu_b_imm.read();
        sc_uint<3> idex_wb_sel_old = idex_wb_sel.read();
        bool idex_reg_write_old = idex_reg_write.read();
        bool idex_mem_write_old = idex_mem_write.read();
        sc_uint<8> idex_mem_addr_old = idex_mem_addr.read();
        bool idex_io_write_old = idex_io_write.read();
        bool idex_io_read_old = idex_io_read.read();
        sc_uint<8> idex_io_addr_old = idex_io_addr.read();
        bool idex_write_flags_old = idex_write_flags.read();

        bool v_exmem = exmem_valid.read();
        sc_uint<16> instr_exmem = exmem_instr.read();
        sc_uint<8> exmem_alu_result_old = exmem_alu_result.read();
        sc_uint<8> exmem_a_old = exmem_a.read();
        sc_uint<8> exmem_imm_old = exmem_imm.read();
        sc_uint<3> exmem_rd_old = exmem_rd.read();
        sc_uint<3> exmem_wb_sel_old = exmem_wb_sel.read();
        bool exmem_reg_write_old = exmem_reg_write.read();
        sc_uint<8> exmem_io_data_old = exmem_io_data.read();
        bool exmem_write_flags_old = exmem_write_flags.read();
        bool exmem_flag_c_old = exmem_flag_c.read();
        bool exmem_flag_z_old = exmem_flag_z.read();

        bool v_memwb = memwb_valid.read();
        sc_uint<16> instr_memwb = memwb_instr.read();
        sc_uint<8> memwb_wb_data_old = memwb_wb_data.read();
        sc_uint<3> memwb_rd_old = memwb_rd.read();
        bool memwb_reg_write_old = memwb_reg_write.read();
        bool memwb_write_flags_old = memwb_write_flags.read();
        bool memwb_flag_c_old = memwb_flag_c.read();
        bool memwb_flag_z_old = memwb_flag_z.read();

        // --- Entradas combinacionales del ciclo ---
        sc_uint<16> instr_fetch = instr.read();
        sc_uint<8> pc_actual = pc_addr.read();

        sc_uint<8> a_id = id_reg_a.read();
        sc_uint<8> b_id = id_reg_b.read();

        sc_uint<8> res_alu = alu_result.read();
        bool c_alu = alu_carry.read();
        bool z_alu = alu_zero.read();

        sc_uint<8> dato_mem = mem_read.read();
        sc_uint<8> dato_io = io_rdata.read();

        bool c_reg_write = ctrl_reg_write.read();
        sc_uint<3> c_reg_w_sel = ctrl_reg_w_sel.read();
        sc_uint<3> c_reg_a_sel = ctrl_reg_a_sel.read();
        sc_uint<3> c_reg_b_sel = ctrl_reg_b_sel.read();
        sc_uint<3> c_alu_op = ctrl_alu_op.read();
        bool c_alu_b_inm = ctrl_alu_b_inm.read();
        sc_uint<3> c_wb_sel = ctrl_wb_sel.read();
        bool c_mem_write = ctrl_mem_write.read();
        sc_uint<8> c_mem_addr = ctrl_mem_addr.read();
        bool c_io_write = ctrl_io_write.read();
        bool c_io_read = ctrl_io_read.read();
        sc_uint<8> c_io_addr = ctrl_io_addr.read();
        bool c_write_flags = ctrl_write_flags.read();

        // --- Traza real del ciclo (estado previo al flanco) ---
        if (trazando && !rst)
        {
            TrazaCiclo t;
            t.ciclo = ciclo_traza++;
            t.pc = pc_actual;
            t.if_instr = instr_fetch;
            t.ifid_valid = v_ifid;
            t.ifid_instr = instr_ifid;
            t.idex_valid = v_idex;
            t.idex_instr = instr_idex;
            t.exmem_valid = v_exmem;
            t.exmem_instr = instr_exmem;
            t.memwb_valid = v_memwb;
            t.memwb_instr = instr_memwb;
            t.wb_escribe = v_memwb && memwb_reg_write_old;
            t.wb_rd = memwb_rd_old;
            t.wb_dato = memwb_wb_data_old;
            traza.push_back(t);
        }

        // --- Escritura arquitectonica que ocurre en este flanco ---
        if (v_memwb && memwb_reg_write_old)
        {
            EventoEscritura ev;
            ev.rd = memwb_rd_old;
            ev.dato = memwb_wb_data_old;
            ev.instr = instr_memwb;
            escrituras.push_back(ev);
        }

        // --- Reset sincrono: invalida todo el pipeline ---
        if (rst)
        {
            ifid_valid.write(false);
            ifid_instr.write(0);
            ifid_pc.write(0);

            idex_valid.write(false);
            idex_instr.write(0);
            idex_pc.write(0);
            idex_a.write(0);
            idex_b.write(0);
            idex_rd.write(0);
            idex_rs1.write(0);
            idex_rs2.write(0);
            idex_imm.write(0);
            idex_alu_op.write(isa::ALU_ADD);
            idex_alu_b_imm.write(false);
            idex_wb_sel.write(WB_ALU);
            idex_reg_write.write(false);
            idex_mem_write.write(false);
            idex_mem_addr.write(0);
            idex_io_write.write(false);
            idex_io_read.write(false);
            idex_io_addr.write(0);
            idex_write_flags.write(false);

            exmem_valid.write(false);
            exmem_instr.write(0);
            exmem_alu_result.write(0);
            exmem_a.write(0);
            exmem_imm.write(0);
            exmem_rd.write(0);
            exmem_wb_sel.write(WB_ALU);
            exmem_reg_write.write(false);
            exmem_mem_write.write(false);
            exmem_mem_addr.write(0);
            exmem_mem_wdata.write(0);
            exmem_io_read.write(false);
            exmem_io_data.write(0);
            exmem_write_flags.write(false);
            exmem_flag_c.write(false);
            exmem_flag_z.write(false);

            memwb_valid.write(false);
            memwb_instr.write(0);
            memwb_wb_data.write(0);
            memwb_rd.write(0);
            memwb_reg_write.write(false);
            memwb_write_flags.write(false);
            memwb_flag_c.write(false);
            memwb_flag_z.write(false);
            return;
        }

        // --- IF -> IF/ID ---
        ifid_valid.write(true);
        ifid_instr.write(instr_fetch);
        ifid_pc.write(pc_actual);

        // --- ID -> ID/EX (decodifica IF/ID previo) ---
        if (v_ifid)
        {
            idex_valid.write(true);
            idex_instr.write(instr_ifid);
            idex_pc.write(ifid_pc_old);
            idex_a.write(a_id);
            idex_b.write(b_id);
            idex_rd.write(c_reg_w_sel);
            idex_rs1.write(c_reg_a_sel);
            idex_rs2.write(c_reg_b_sel);
            idex_imm.write(instr_ifid.range(7, 0));
            idex_alu_op.write(c_alu_op);
            idex_alu_b_imm.write(c_alu_b_inm);
            idex_wb_sel.write(c_wb_sel);
            idex_reg_write.write(c_reg_write);
            idex_mem_write.write(c_mem_write);
            idex_mem_addr.write(c_mem_addr);
            idex_io_write.write(c_io_write);
            idex_io_read.write(c_io_read);
            idex_io_addr.write(c_io_addr);
            idex_write_flags.write(c_write_flags);
        }
        else
        {
            idex_valid.write(false);
            idex_instr.write(0);
            idex_reg_write.write(false);
            idex_mem_write.write(false);
            idex_io_write.write(false);
            idex_io_read.write(false);
            idex_write_flags.write(false);
        }

        // --- EX -> EX/MEM (usa ALU de ID/EX previo) ---
        if (v_idex)
        {
            exmem_valid.write(true);
            exmem_instr.write(instr_idex);
            exmem_alu_result.write(res_alu);
            exmem_a.write(idex_a_old);
            exmem_imm.write(idex_imm_old);
            exmem_rd.write(idex_rd_old);
            exmem_wb_sel.write(idex_wb_sel_old);
            exmem_reg_write.write(idex_reg_write_old);
            exmem_mem_write.write(idex_mem_write_old);
            exmem_mem_addr.write(idex_mem_addr_old);
            exmem_mem_wdata.write(idex_a_old);
            exmem_io_read.write(idex_io_read_old);
            exmem_io_data.write(dato_io);
            exmem_write_flags.write(idex_write_flags_old);
            exmem_flag_c.write(c_alu);
            exmem_flag_z.write(z_alu);
        }
        else
        {
            exmem_valid.write(false);
            exmem_instr.write(0);
            exmem_reg_write.write(false);
            exmem_mem_write.write(false);
            exmem_write_flags.write(false);
        }

        // --- MEM -> MEM/WB (usa memoria de EX/MEM previo) ---
        if (v_exmem)
        {
            sc_uint<8> dato = 0;
            switch (exmem_wb_sel_old.to_uint())
            {
                case WB_INM: dato = exmem_imm_old; break;
                case WB_MEM: dato = dato_mem; break;
                case WB_IO:  dato = exmem_io_data_old; break;
                case WB_RA:  dato = exmem_a_old; break;
                default:     dato = exmem_alu_result_old; break;
            }

            memwb_valid.write(true);
            memwb_instr.write(instr_exmem);
            memwb_wb_data.write(dato);
            memwb_rd.write(exmem_rd_old);
            memwb_reg_write.write(exmem_reg_write_old);
            memwb_write_flags.write(exmem_write_flags_old);
            memwb_flag_c.write(exmem_flag_c_old);
            memwb_flag_z.write(exmem_flag_z_old);
        }
        else
        {
            memwb_valid.write(false);
            memwb_instr.write(0);
            memwb_reg_write.write(false);
            memwb_write_flags.write(false);
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
        trazando = false;
        ciclo_traza = 1;

        nunca_salto.write(false);
        dir_ignorada.write(0);

        // ---------------- PC ----------------
        pc.clk(clk);
        pc.reset(reset);
        pc.salto(nunca_salto);
        pc.direccion_salto(dir_ignorada);
        pc.direccion_actual(pc_addr);

        // ---------------- Memoria de instrucciones ----
        imem.direccion(pc_addr);
        imem.instruccion(instr);

        // ---------------- Unidad de control (ID) -------
        uc.instruccion(ifid_instr);
        uc.flag_c(flag_c_arch);
        uc.flag_z(flag_z_arch);

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
        regs.write_enable(wb_enable);
        regs.read_register_1(ctrl_reg_a_sel);
        regs.read_register_2(ctrl_reg_b_sel);
        regs.write_register(memwb_rd);
        regs.write_data(memwb_wb_data);
        regs.read_data_1(id_reg_a);
        regs.read_data_2(id_reg_b);

        // ---------------- ALU (EX) --------------------
        alu.A(idex_a);
        alu.B(alu_b);
        alu.operacion(idex_alu_op);
        alu.resultado(alu_result);
        alu.carry(alu_carry);
        alu.zero(alu_zero);

        // ---------------- Registro de banderas (WB) ---
        flags.clk(clk);
        flags.reset(reset);
        flags.actualizar(wb_flags_enable);
        flags.carry_in(memwb_flag_c);
        flags.zero_in(memwb_flag_z);
        flags.carry_out(flag_c_arch);
        flags.zero_out(flag_z_arch);

        // ---------------- Memoria de datos (MEM) ------
        dmem.clk(clk);
        dmem.reset(reset);
        dmem.write_enable(mem_write_enable);
        dmem.direccion(exmem_mem_addr);
        dmem.write_data(exmem_mem_wdata);
        dmem.read_data(mem_read);

        // ---------------- Procesos --------------------
        SC_METHOD(reloj);
        sensitive << clk.pos();
        dont_initialize();

        SC_METHOD(comb_alu_b);
        sensitive << idex_alu_b_imm << idex_imm << idex_b;

        SC_METHOD(comb_wb);
        sensitive << memwb_valid << memwb_reg_write << memwb_write_flags;

        SC_METHOD(comb_mem_we);
        sensitive << exmem_valid << exmem_mem_write;

        SC_METHOD(salidas);
        sensitive << idex_valid << idex_io_write << idex_io_read
                  << idex_io_addr << idex_a
                  << memwb_valid << memwb_instr;
    }
};

#endif // CPU_GOALKEEPER_H
