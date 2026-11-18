#include <systemc.h>
#include <iostream>
#include <string>
#include <vector>
#include "cpu_goalkeeper.h"
#include "assembler.h"

// ============================================================
// Verificacion automatica de los programas de aceptacion.
// Ensambla texto .asm (mismo contenido que examples/*.asm), lo
// ejecuta en la CPU real y comprueba registros y memoria.
// ============================================================

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const std::string& detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (!detalle.empty()) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

static const double PERIODO_NS = 10.0;

struct Estado
{
    bool halted;
    unsigned int regs[8];
    unsigned int mem[256];
    unsigned int stalls;
    unsigned int branches;
};

// Ensambla y ejecuta un programa en la CPU real.
static Estado ejecutar_asm(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                           sc_signal<bool>& halted,
                           const std::string& texto, unsigned int max_ciclos)
{
    Estado e;
    e.halted = false;
    for (int i = 0; i < 8; i++) e.regs[i] = 0;
    for (int i = 0; i < 256; i++) e.mem[i] = 0;
    e.stalls = 0;
    e.branches = 0;

    assembler::Resultado r = assembler::ensamblar(texto);
    if (!r.ok)
    {
        e.regs[0] = 0xFFFFFFFF;  // marcador de error
        return e;
    }

    cpu.detener_traza();
    cpu.limpiar_eventos();
    cpu.limpiar_stalls();
    cpu.limpiar_control();
    cpu.cargar_programa(r.palabras);

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);

    cpu.iniciar_traza();
    unsigned int ciclos = 0;
    while (ciclos < max_ciclos && !halted.read())
    {
        sc_start(PERIODO_NS, SC_NS);
        ciclos++;
    }
    cpu.detener_traza();

    e.halted = halted.read();
    for (int i = 0; i < 8; i++) e.regs[i] = cpu.valor_registro(i).to_uint();
    for (int i = 0; i < 256; i++) e.mem[i] = cpu.dmem.memoria[i].read().to_uint();
    e.stalls = cpu.stalls_totales;
    e.branches = cpu.branches_tomados;
    return e;
}

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", PERIODO_NS, SC_NS);
    sc_signal<bool> reset;
    sc_signal<bool> io_write, io_read;
    sc_signal<sc_uint<8>> io_addr, io_wdata, io_rdata;
    sc_signal<bool> halted;

    CPU_Goalkeeper cpu("cpu");
    cpu.clk(clk);
    cpu.reset(reset);
    cpu.io_write(io_write);
    cpu.io_read(io_read);
    cpu.io_addr(io_addr);
    cpu.io_wdata(io_wdata);
    cpu.io_rdata(io_rdata);
    cpu.halted(halted);

    io_rdata.write(0);

    // 1. Aritmetica (ADD/SUB).
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R1, 10\nLDI R2, 20\nADD R3, R1, R2\nSUB R4, R3, R2\nHALT\n", 100);
        comprobar("suma: R3=30", e.regs[3] == 30, "R3=" + std::to_string(e.regs[3]));
        comprobar("suma: R4=10", e.regs[4] == 10, "R4=" + std::to_string(e.regs[4]));
        comprobar("suma: HALT", e.halted, "");
    }

    // 2. Logica (AND/OR).
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 10\nLDI R1, 6\nAND R4, R0, R1\nOR R5, R0, R1\nHALT\n", 100);
        comprobar("logica: R4=2", e.regs[4] == 2, "R4=" + std::to_string(e.regs[4]));
        comprobar("logica: R5=14", e.regs[5] == 14, "R5=" + std::to_string(e.regs[5]));
    }

    // 3. MOV.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 42\nMOV R6, R0\nHALT\n", 100);
        comprobar("mov: R6=42", e.regs[6] == 42, "R6=" + std::to_string(e.regs[6]));
    }

    // 4. CMP + JEQ.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 7\nLDI R1, 7\nCMP R0, R1\nJEQ iguales\nLDI R5, 0\nJMP fin\niguales: LDI R5, 1\nfin: HALT\n", 100);
        comprobar("cmp: R5=1", e.regs[5] == 1, "R5=" + std::to_string(e.regs[5]));
    }

    // 5. Saltos (JEQ,JNE,JGE,JLT,JGT,JLE).
    {
        std::string asm_ =
            "LDI R0, 5\n"
            "LDI R1, 5\nCMP R0, R1\nJEQ a1\nLDI R2, 0\nJMP a2\na1: LDI R2, 1\na2:\n"
            "LDI R1, 4\nCMP R0, R1\nJNE b1\nLDI R3, 0\nJMP b2\nb1: LDI R3, 1\nb2:\n"
            "LDI R1, 5\nCMP R0, R1\nJGE c1\nLDI R4, 0\nJMP c2\nc1: LDI R4, 1\nc2:\n"
            "LDI R1, 6\nCMP R0, R1\nJLT d1\nLDI R5, 0\nJMP d2\nd1: LDI R5, 1\nd2:\n"
            "LDI R1, 4\nCMP R0, R1\nJGT e1\nLDI R6, 0\nJMP e2\ne1: LDI R6, 1\ne2:\n"
            "LDI R1, 5\nCMP R0, R1\nJLE f1\nLDI R7, 0\nJMP f2\nf1: LDI R7, 1\nf2:\n"
            "HALT\n";
        Estado e = ejecutar_asm(cpu, reset, halted, asm_, 200);
        comprobar("saltos: R2..R7 = 1",
                  e.regs[2] == 1 && e.regs[3] == 1 && e.regs[4] == 1 &&
                  e.regs[5] == 1 && e.regs[6] == 1 && e.regs[7] == 1,
                  "R2=" + std::to_string(e.regs[2]) + " R3=" + std::to_string(e.regs[3]) +
                  " R4=" + std::to_string(e.regs[4]) + " R5=" + std::to_string(e.regs[5]) +
                  " R6=" + std::to_string(e.regs[6]) + " R7=" + std::to_string(e.regs[7]));
    }

    // 6. Memoria (ST/LD).
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 55\nST R0, 0x10\nLD R1, 0x10\nHALT\n", 100);
        comprobar("memoria: mem[0x10]=55", e.mem[0x10] == 55, "");
        comprobar("memoria: R1=55", e.regs[1] == 55, "");
    }

    // 7. Forwarding.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 1\nADD R1, R0, R0\nADD R2, R1, R1\nADD R3, R2, R2\nHALT\n", 100);
        comprobar("forwarding: R1=2", e.regs[1] == 2, "");
        comprobar("forwarding: R2=4", e.regs[2] == 4, "");
        comprobar("forwarding: R3=8", e.regs[3] == 8, "");
    }

    // 8. Hazard load-use.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 7\nST R0, 0x20\nLD R1, 0x20\nADD R2, R1, R1\nHALT\n", 100);
        comprobar("hazard: R1=7", e.regs[1] == 7, "");
        comprobar("hazard: R2=14", e.regs[2] == 14, "");
        comprobar("hazard: hubo stall", e.stalls >= 1, "stalls=" + std::to_string(e.stalls));
    }

    // 9. Bucle.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 5\nLDI R1, 0\nLDI R7, 1\nbucle: ADD R1, R1, R7\nSUB R0, R0, R7\nJNE bucle\nHALT\n", 300);
        comprobar("bucle: R0=0", e.regs[0] == 0, "R0=" + std::to_string(e.regs[0]));
        comprobar("bucle: R1=5", e.regs[1] == 5, "R1=" + std::to_string(e.regs[1]));
    }

    // 10. Branch flush.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 1\nJMP destino\nLDI R0, 99\ndestino: HALT\n", 100);
        comprobar("flush: R0=1", e.regs[0] == 1, "R0=" + std::to_string(e.regs[0]));
    }

    // 11. HALT con instrucciones pendientes.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 5\nLDI R1, 10\nADD R2, R0, R1\nHALT\nLDI R3, 99\n", 100);
        comprobar("halt: R2=15", e.regs[2] == 15, "");
        comprobar("halt: R3=0 (no ejecutada)", e.regs[3] == 0, "R3=" + std::to_string(e.regs[3]));
    }

    // 12. E/S (OUT observable; IN lee 0 sin dispositivo).
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 42\nOUT R0, 0x30\nIN R1, 0x11\nHALT\n", 100);
        comprobar("io: R0=42", e.regs[0] == 42, "");
        comprobar("io: R1=0 (IN sin dispositivo)", e.regs[1] == 0, "");
    }

    // 13. Combinado.
    {
        Estado e = ejecutar_asm(cpu, reset, halted,
            "LDI R0, 10\nLDI R1, 6\nADD R2, R0, R1\nSUB R3, R0, R1\nAND R4, R0, R1\n"
            "OR R5, R0, R1\nMOV R6, R0\nST R2, 0x10\nLD R7, 0x10\nHALT\n", 100);
        comprobar("combinado: R2=16", e.regs[2] == 16, "");
        comprobar("combinado: R3=4", e.regs[3] == 4, "");
        comprobar("combinado: R4=2", e.regs[4] == 2, "");
        comprobar("combinado: R5=14", e.regs[5] == 14, "");
        comprobar("combinado: R6=10", e.regs[6] == 10, "");
        comprobar("combinado: R7=16", e.regs[7] == 16, "");
        comprobar("combinado: mem[0x10]=16", e.mem[0x10] == 16, "");
    }

    std::cout << "\nFallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
