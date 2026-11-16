#include <systemc.h>
#include <iostream>
#include "unidad_control.h"
#include "isa.h"

using namespace isa;

static int fallos = 0;

static void comprobar(const char* nombre, int obtenido, int esperado)
{
    bool ok = (obtenido == esperado);
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre
              << ": obtenido=" << obtenido
              << " esperado=" << esperado << std::endl;
    if (!ok) fallos++;
}

int sc_main(int argc, char* argv[])
{
    sc_signal<sc_uint<16>> instruccion;
    sc_signal<bool> flag_c, flag_z;

    sc_signal<bool> reg_write;
    sc_signal<sc_uint<3>> reg_a_sel, reg_b_sel, reg_w_sel;
    sc_signal<sc_uint<3>> alu_op;
    sc_signal<bool> alu_b_inm;
    sc_signal<sc_uint<3>> write_back_sel;
    sc_signal<bool> mem_write;
    sc_signal<sc_uint<8>> mem_addr;
    sc_signal<bool> io_write, io_read;
    sc_signal<sc_uint<8>> io_addr;
    sc_signal<bool> write_flags;
    sc_signal<bool> salto;
    sc_signal<sc_uint<8>> direccion_salto;
    sc_signal<bool> halt;

    UnidadControl uc("uc");
    uc.instruccion(instruccion);
    uc.flag_c(flag_c);
    uc.flag_z(flag_z);
    uc.reg_write(reg_write);
    uc.reg_a_sel(reg_a_sel);
    uc.reg_b_sel(reg_b_sel);
    uc.reg_w_sel(reg_w_sel);
    uc.alu_op(alu_op);
    uc.alu_b_inm(alu_b_inm);
    uc.write_back_sel(write_back_sel);
    uc.mem_write(mem_write);
    uc.mem_addr(mem_addr);
    uc.io_write(io_write);
    uc.io_read(io_read);
    uc.io_addr(io_addr);
    uc.write_flags(write_flags);
    uc.salto(salto);
    uc.direccion_salto(direccion_salto);
    uc.halt(halt);

    flag_c.write(false);
    flag_z.write(false);

    auto set_instr = [&](sc_uint<16> instr) {
        instruccion.write(instr);
        sc_start(1, SC_NS);
    };

    // ---------------- NOP ----------------
    set_instr(formato_i(NOP, 0, 0));
    comprobar("NOP reg_write", reg_write.read(), 0);
    comprobar("NOP mem_write", mem_write.read(), 0);
    comprobar("NOP io_write", io_write.read(), 0);
    comprobar("NOP salto", salto.read(), 0);
    comprobar("NOP halt", halt.read(), 0);

    // ---------------- ADD R2,R0,R1 ----------------
    set_instr(formato_r(ADD, 2, 0, 1));
    comprobar("ADD reg_write", reg_write.read(), 1);
    comprobar("ADD alu_op", alu_op.read().to_uint(), ALU_ADD);
    comprobar("ADD write_flags", write_flags.read(), 1);
    comprobar("ADD ra", reg_a_sel.read().to_uint(), 0);
    comprobar("ADD rb", reg_b_sel.read().to_uint(), 1);
    comprobar("ADD rd", reg_w_sel.read().to_uint(), 2);
    comprobar("ADD wb=ALU", write_back_sel.read().to_uint(), WB_ALU);

    // ---------------- SUB ----------------
    set_instr(formato_r(SUB, 2, 1, 0));
    comprobar("SUB alu_op", alu_op.read().to_uint(), ALU_SUB);
    comprobar("SUB write_flags", write_flags.read(), 1);

    // ---------------- AND ----------------
    set_instr(formato_r(AND, 2, 0, 1));
    comprobar("AND alu_op", alu_op.read().to_uint(), ALU_AND);

    // ---------------- OR ----------------
    set_instr(formato_r(OR, 2, 0, 1));
    comprobar("OR alu_op", alu_op.read().to_uint(), ALU_OR);

    // ---------------- CMP R0,R7 (no escribe registro) -------------
    set_instr(formato_r(CMP, 0, 0, 7));
    comprobar("CMP alu_op", alu_op.read().to_uint(), ALU_CMP);
    comprobar("CMP write_flags", write_flags.read(), 1);
    comprobar("CMP reg_write", reg_write.read(), 0);

    // ---------------- LDI R3,42 ----------------
    set_instr(formato_i(LDI, 3, 42));
    comprobar("LDI reg_write", reg_write.read(), 1);
    comprobar("LDI wb=INM", write_back_sel.read().to_uint(), WB_INM);
    comprobar("LDI rd", reg_w_sel.read().to_uint(), 3);
    comprobar("LDI write_flags (conservar)", write_flags.read(), 0);

    // ---------------- MOV R5,R1 ----------------
    set_instr(formato_r(MOV, 5, 1, 0));
    comprobar("MOV reg_write", reg_write.read(), 1);
    comprobar("MOV wb=RA", write_back_sel.read().to_uint(), WB_RA);
    comprobar("MOV ra", reg_a_sel.read().to_uint(), 1);
    comprobar("MOV rd", reg_w_sel.read().to_uint(), 5);
    comprobar("MOV write_flags", write_flags.read(), 0);

    // ---------------- LD R2,0x10 ----------------
    set_instr(formato_i(LD, 2, 0x10));
    comprobar("LD reg_write", reg_write.read(), 1);
    comprobar("LD wb=MEM", write_back_sel.read().to_uint(), WB_MEM);
    comprobar("LD mem_addr", mem_addr.read().to_uint(), 0x10);

    // ---------------- ST R1,0x20 ----------------
    set_instr(formato_i(ST, 1, 0x20));
    comprobar("ST mem_write", mem_write.read(), 1);
    comprobar("ST mem_addr", mem_addr.read().to_uint(), 0x20);
    comprobar("ST fuente", reg_a_sel.read().to_uint(), 1);

    // ---------------- IN R4,0x11 ----------------
    set_instr(formato_i(IN, 4, 0x11));
    comprobar("IN io_read", io_read.read(), 1);
    comprobar("IN io_addr", io_addr.read().to_uint(), 0x11);
    comprobar("IN wb=IO", write_back_sel.read().to_uint(), WB_IO);

    // ---------------- OUT 0x20,R5 ----------------
    set_instr(formato_i(OUT, 5, 0x20));
    comprobar("OUT io_write", io_write.read(), 1);
    comprobar("OUT io_addr", io_addr.read().to_uint(), 0x20);
    comprobar("OUT fuente", reg_a_sel.read().to_uint(), 5);

    // ---------------- JMP 20 ----------------
    set_instr(formato_j(JMP, 20));
    comprobar("JMP salto", salto.read(), 1);
    comprobar("JMP dir", direccion_salto.read().to_uint(), 20);

    // ---------------- JEQ segun Z ----------------
    flag_z.write(false);
    set_instr(formato_j(JEQ, 30));
    comprobar("JEQ Z=0 no salta", salto.read(), 0);
    flag_z.write(true);
    sc_start(1, SC_NS);
    comprobar("JEQ Z=1 salta", salto.read(), 1);
    comprobar("JEQ dir", direccion_salto.read().to_uint(), 30);

    // ---------------- JLT segun C ----------------
    flag_c.write(true);
    set_instr(formato_j(JLT, 40));
    comprobar("JLT C=1 no salta", salto.read(), 0);
    flag_c.write(false);
    sc_start(1, SC_NS);
    comprobar("JLT C=0 salta", salto.read(), 1);

    // ---------------- HALT ----------------
    set_instr(formato_i(HALT, 0, 0));
    comprobar("HALT halt", halt.read(), 1);
    comprobar("HALT reg_write", reg_write.read(), 0);

    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
