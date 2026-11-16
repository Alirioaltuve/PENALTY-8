#include <systemc.h>
#include <vector>
#include <map>
#include <iostream>
#include "cpu_goalkeeper.h"
#include "isa.h"

using namespace isa;

// Monitor que captura las escrituras OUT de la CPU para poder
// verificar los resultados del programa.
SC_MODULE(MonitorES)
{
    sc_in<bool> clk;
    sc_in<bool> io_write;
    sc_in<sc_uint<8>> io_addr;
    sc_in<sc_uint<8>> io_wdata;
    sc_in<bool> halted;

    std::map<int, int> escrituras;
    bool detenido;

    void capturar()
    {
        if (halted.read() == true)
        {
            detenido = true;
        }
        if (io_write.read() == true)
        {
            escrituras[io_addr.read().to_uint()] = io_wdata.read().to_uint();
        }
    }

    SC_CTOR(MonitorES)
    {
        detenido = false;
        SC_METHOD(capturar);
        sensitive << clk.pos();
        dont_initialize();
    }
};

static void comprobar(const char* nombre, int obtenido, int esperado, int& fallos)
{
    bool ok = (obtenido == esperado);
    std::cout << (ok ? "[OK]   " : "[FALLO] ")
              << nombre << ": obtenido=" << obtenido
              << " esperado=" << esperado << std::endl;
    if (!ok) fallos++;
}

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> reset;
    sc_signal<bool> io_write;
    sc_signal<bool> io_read;
    sc_signal<sc_uint<8>> io_addr;
    sc_signal<sc_uint<8>> io_wdata;
    sc_signal<sc_uint<8>> io_rdata;
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

    MonitorES monitor("monitor");
    monitor.clk(clk);
    monitor.io_write(io_write);
    monitor.io_addr(io_addr);
    monitor.io_wdata(io_wdata);
    monitor.halted(halted);

    // Programa de prueba:
    //  0 LDI R0,20
    //  1 LDI R1,22
    //  2 ADD R2,R0,R1        -> 42
    //  3 OUT 0x30,R2         -> 42
    //  4 SUB R2,R1,R0        -> 2
    //  5 OUT 0x31,R2         -> 2
    //  6 CMP R0,R0           -> Z=1
    //  7 LDI R4,99           -> conserva banderas
    //  8 JEQ 12
    //  9 LDI R6,0x22         (se omite)
    // 10 OUT 0x34,R6         (se omite)
    // 11 JMP 14
    // 12 LDI R6,0x11
    // 13 OUT 0x34,R6         -> 0x11
    // 14 HALT
    std::vector<sc_uint<16>> p;
    p.push_back(formato_i(LDI, 0, 20));          // 0
    p.push_back(formato_i(LDI, 1, 22));          // 1
    p.push_back(formato_r(ADD, 2, 0, 1));        // 2
    p.push_back(formato_i(OUT, 2, 0x30));        // 3
    p.push_back(formato_r(SUB, 2, 1, 0));        // 4
    p.push_back(formato_i(OUT, 2, 0x31));        // 5
    p.push_back(formato_r(CMP, 0, 0, 0));        // 6
    p.push_back(formato_i(LDI, 4, 99));          // 7
    p.push_back(formato_j(JEQ, 12));             // 8
    p.push_back(formato_i(LDI, 6, 0x22));        // 9
    p.push_back(formato_i(OUT, 6, 0x34));        // 10
    p.push_back(formato_j(JMP, 14));             // 11
    p.push_back(formato_i(LDI, 6, 0x11));        // 12
    p.push_back(formato_i(OUT, 6, 0x34));        // 13
    p.push_back(formato_i(HALT, 0, 0));          // 14
    cpu.cargar_programa(p);

    io_rdata.write(0);

    // Reset inicial
    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);

    // Ejecutar hasta que se detenga o se agote el tiempo
    sc_start(500, SC_NS);

    int fallos = 0;
    std::cout << "\n===== RESULTADOS CPU GOALKEEPER =====" << std::endl;
    comprobar("OUT 0x30 (ADD 20+22)", monitor.escrituras.count(0x30) ? monitor.escrituras[0x30] : -1, 42, fallos);
    comprobar("OUT 0x31 (SUB 22-20)", monitor.escrituras.count(0x31) ? monitor.escrituras[0x31] : -1, 2, fallos);
    comprobar("OUT 0x34 (JEQ tomado)", monitor.escrituras.count(0x34) ? monitor.escrituras[0x34] : -1, 0x11, fallos);
    comprobar("HALT detectado", monitor.detenido ? 1 : 0, 1, fallos);

    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
