#include <systemc.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include "cpu_goalkeeper.h"
#include "assembler.h"

// ============================================================
// eval_goalkeeper: ensambla y ejecuta un programa .asm en el
// GOALKEEPER-8 real. No simula resultados: usa CPU_Goalkeeper.
//
// Uso:
//   ./eval_goalkeeper archivo.asm [--max-ciclos N] [--mem N]
// ============================================================

// Periodo del reloj: debe coincidir con el sc_clock (10 ns).
static const double PERIODO_NS = 10.0;

// Registra las escrituras OUT de la CPU para mostrarlas (E/S real).
SC_MODULE(MonitorIO)
{
    sc_in<bool> clk;
    sc_in<bool> io_write;
    sc_in<sc_uint<8>> io_addr;
    sc_in<sc_uint<8>> io_wdata;

    std::vector<std::pair<unsigned int, unsigned int>> outs;

    void capturar()
    {
        if (io_write.read())
            outs.push_back(std::make_pair(io_addr.read().to_uint(),
                                          io_wdata.read().to_uint()));
    }

    SC_CTOR(MonitorIO)
    {
        SC_METHOD(capturar);
        sensitive << clk.pos();
        dont_initialize();
    }
};

static std::string basename(const std::string& path)
{
    size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

int sc_main(int argc, char* argv[])
{
    std::string archivo;
    unsigned int max_ciclos = 100000;
    unsigned int mem_volcado = 32;

    for (int i = 1; i < argc; i++)
    {
        std::string a = argv[i];
        if (a == "--max-ciclos" && i + 1 < argc) max_ciclos = (unsigned)std::atoi(argv[++i]);
        else if (a == "--mem" && i + 1 < argc) mem_volcado = (unsigned)std::atoi(argv[++i]);
        else if (archivo.empty()) archivo = a;
    }

    if (archivo.empty())
    {
        std::cerr << "Uso: " << argv[0]
                  << " archivo.asm [--max-ciclos N] [--mem N]\n";
        return 1;
    }

    std::ifstream f(archivo.c_str());
    if (!f)
    {
        std::cerr << "No se pudo abrir el archivo: " << archivo << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string codigo = ss.str();

    assembler::Resultado ens = assembler::ensamblar(codigo);
    if (!ens.ok)
    {
        std::cerr << "Error de ensamblado: " << ens.error << "\n";
        return 1;
    }

    // ---------------- Mostrar palabras ensambladas ----------------
    std::cout << "Programa: " << basename(archivo) << "\n";
    std::cout << "Instrucciones: " << ens.instrucciones << "\n";
    std::cout << "Palabras de 16 bits:\n";
    for (size_t i = 0; i < ens.palabras.size(); i++)
    {
        std::cout << "  [" << std::setw(3) << i << "] 0x"
                  << std::hex << std::uppercase << std::setw(4)
                  << std::setfill('0') << ens.palabras[i].to_uint()
                  << std::dec << std::setfill(' ') << "\n";
    }

    // ---------------- Instanciar el computador real ----------------
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

    MonitorIO mon("mon");
    mon.clk(clk);
    mon.io_write(io_write);
    mon.io_addr(io_addr);
    mon.io_wdata(io_wdata);

    io_rdata.write(0);   // sin dispositivo externo conectado, IN lee 0

    cpu.cargar_programa(ens.palabras);

    // ---------------- Reset real y ejecucion ----------------
    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);

    cpu.iniciar_traza();

    unsigned int ciclos = 0;
    bool fin_halt = false;
    while (ciclos < max_ciclos && !halted.read())
    {
        sc_start(PERIODO_NS, SC_NS);
        ciclos++;
    }
    fin_halt = halted.read();

    cpu.detener_traza();

    // ---------------- Resultados ----------------
    std::cout << "\nCiclos ejecutados: " << ciclos << "\n";
    std::cout << "Estado final: " << (fin_halt ? "HALT" : "LIMITE DE CICLOS") << "\n";

    std::cout << "\nRegistros:\n";
    for (int i = 0; i < 8; i++)
        std::cout << "  R" << i << " = " << std::dec
                  << cpu.valor_registro(i).to_uint() << "\n";

    std::cout << "\nMemoria de datos (primeras " << mem_volcado << " posiciones):\n";
    for (unsigned int i = 0; i < mem_volcado && i < 256; i++)
    {
        unsigned int v = cpu.dmem.memoria[i].read().to_uint();
        if (v != 0)
            std::cout << "  [" << std::hex << std::setw(2) << std::setfill('0')
                      << i << "] = " << std::dec << v << "\n";
    }
    std::cout << "  (el resto es 0)\n";

    std::cout << "\nContadores del pipeline:\n";
    std::cout << "  stalls = " << cpu.stalls_totales << "\n";
    std::cout << "  branches tomados = " << cpu.branches_tomados << "\n";
    std::cout << "  flushes = " << cpu.flushes_totales << "\n";

    std::cout << "\nEventos OUT (bus de E/S real):\n";
    if (mon.outs.empty())
        std::cout << "  (ninguno)\n";
    for (size_t i = 0; i < mon.outs.size(); i++)
        std::cout << "  OUT 0x" << std::hex << std::setw(2) << std::setfill('0')
                  << mon.outs[i].first << " = " << std::dec
                  << mon.outs[i].second << "\n";

    return 0;
}
