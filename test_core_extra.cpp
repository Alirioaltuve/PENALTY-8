#include <systemc.h>
#include <vector>
#include <iostream>
#include "cpu_goalkeeper.h"
#include "isa.h"

using namespace isa;

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const std::string& detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (!detalle.empty()) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

// Carga un programa, aplica reset y ejecuta una cantidad fija de ciclos.
static void ejecutar(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                     const std::vector<sc_uint<16>>& prog, unsigned int ciclos)
{
    cpu.detener_traza();
    cpu.limpiar_eventos();
    cpu.limpiar_stalls();
    cpu.limpiar_control();
    cpu.cargar_programa(prog);

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);
    sc_start((double)ciclos * 10.0, SC_NS);
}

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);
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

    // 1. OR: escritura real del resultado.
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 0x0F));
        p.push_back(formato_i(LDI, 1, 0xF0));
        p.push_back(formato_r(OR, 2, 0, 1));
        p.push_back(formato_i(HALT, 0, 0));
        ejecutar(cpu, reset, p, 50);
        comprobar("OR 0x0F|0xF0 = 0xFF", cpu.valor_registro(2) == 0xFF,
                  "R2=" + std::to_string(cpu.valor_registro(2).to_uint()));
        comprobar("OR llega a HALT", halted.read(), "");
    }

    // 2. AND: escritura real del resultado.
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 0xFF));
        p.push_back(formato_i(LDI, 1, 0x0F));
        p.push_back(formato_r(AND, 2, 0, 1));
        p.push_back(formato_i(HALT, 0, 0));
        ejecutar(cpu, reset, p, 50);
        comprobar("AND 0xFF&0x0F = 0x0F", cpu.valor_registro(2) == 0x0F,
                  "R2=" + std::to_string(cpu.valor_registro(2).to_uint()));
    }

    // 3. MOV: copia correcta entre registros.
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 42));
        p.push_back(formato_r(MOV, 3, 0, 0));
        p.push_back(formato_i(HALT, 0, 0));
        ejecutar(cpu, reset, p, 50);
        comprobar("MOV R3,R0 = 42", cpu.valor_registro(3) == 42,
                  "R3=" + std::to_string(cpu.valor_registro(3).to_uint()));
    }

    // 4. Bucle independiente con salto hacia atras.
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 5));    // R0 = 5 (contador)
        p.push_back(formato_i(LDI, 1, 0));    // R1 = 0 (acumulador)
        p.push_back(formato_i(LDI, 7, 1));    // R7 = 1 (constante)
        p.push_back(formato_r(ADD, 1, 1, 7)); // loop: R1 = R1 + 1
        p.push_back(formato_r(SUB, 0, 0, 7)); // R0 = R0 - 1
        p.push_back(formato_j(JNE, 3));       // si R0 != 0 -> loop
        p.push_back(formato_i(HALT, 0, 0));
        ejecutar(cpu, reset, p, 200);

        bool r0 = cpu.valor_registro(0) == 0;
        bool r1 = cpu.valor_registro(1) == 5;
        comprobar("bucle: R0 llega a 0", r0, "R0=" + std::to_string(cpu.valor_registro(0).to_uint()));
        comprobar("bucle: R1 = 5 iteraciones", r1, "R1=" + std::to_string(cpu.valor_registro(1).to_uint()));
        comprobar("bucle: branches tomados == 4", cpu.branches_tomados == 4,
                  "branches=" + std::to_string(cpu.branches_tomados));
        comprobar("bucle: HALT alcanzado", halted.read(), "");
    }

    // 5. Dependencias encadenadas (forwarding EX/MEM y MEM/WB).
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 1));
        p.push_back(formato_r(ADD, 1, 0, 0));  // R1 = R0+R0 = 2
        p.push_back(formato_r(ADD, 2, 1, 1));  // R2 = R1+R1 = 4
        p.push_back(formato_r(ADD, 3, 2, 2));  // R3 = R2+R2 = 8
        p.push_back(formato_i(HALT, 0, 0));
        ejecutar(cpu, reset, p, 50);
        comprobar("deps: R1=2", cpu.valor_registro(1) == 2, "");
        comprobar("deps: R2=4", cpu.valor_registro(2) == 4, "");
        comprobar("deps: R3=8", cpu.valor_registro(3) == 8, "");
        comprobar("deps: sin stalls (ADD no es load-use)", cpu.stalls_totales == 0, "");
    }

    std::cout << "\nFallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
