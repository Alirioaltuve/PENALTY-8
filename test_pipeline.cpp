#include <systemc.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "cpu_goalkeeper.h"
#include "isa.h"

using namespace isa;

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const std::string& detalle)
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (!detalle.empty()) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

static std::string etiqueta(sc_uint<16> i)
{
    if (i.to_uint() == 0) return "NOP";
    unsigned int op = i.range(15, 11).to_uint();
    unsigned int rd = i.range(10, 8).to_uint();
    if (op == LDI)
        return std::string("I") + std::to_string(rd);
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << i.to_uint();
    return os.str();
}

static std::string etapa(bool valida, sc_uint<16> instr)
{
    return valida ? etiqueta(instr) : "-";
}

static void imprimir_traza(const std::vector<CPU_Goalkeeper::TrazaCiclo>& t)
{
    std::cout << "\n===== TRAZA REAL DEL PIPELINE (SystemC) =====\n";
    std::cout << "Ciclo | PC | IF/ID.v IF/ID | ID/EX.v ID/EX | EX/MEM.v EX/MEM | MEM/WB.v MEM/WB | WB.esc\n";
    std::cout << "------+----+---------------+-------------+----------------+----------------+-------\n";
    for (const auto& c : t)
    {
        std::cout << std::setw(5) << c.ciclo << " | "
                  << std::setw(2) << c.pc.to_uint() << " | "
                  << std::setw(6) << (c.ifid_valid ? "1" : "0") << " "
                  << std::setw(5) << etapa(c.ifid_valid, c.ifid_instr) << " | "
                  << std::setw(6) << (c.idex_valid ? "1" : "0") << " "
                  << std::setw(5) << etapa(c.idex_valid, c.idex_instr) << " | "
                  << std::setw(7) << (c.exmem_valid ? "1" : "0") << " "
                  << std::setw(5) << etapa(c.exmem_valid, c.exmem_instr) << " | "
                  << std::setw(8) << (c.memwb_valid ? "1" : "0") << " "
                  << std::setw(5) << etapa(c.memwb_valid, c.memwb_instr) << " | "
                  << (c.wb_escribe ? "SI" : "no") << "\n";
    }

    std::cout << "\n===== ETAPAS POR CICLO (IF=fetch en PC, resto=registro) =====\n";
    std::cout << "Ciclo | IF | ID | EX | MEM | WB\n";
    std::cout << "------+----+----+----+-----+----\n";
    for (const auto& c : t)
    {
        std::cout << std::setw(5) << c.ciclo << " | "
                  << std::setw(3) << etiqueta(c.if_instr) << " | "
                  << std::setw(3) << etapa(c.ifid_valid, c.ifid_instr) << " | "
                  << std::setw(3) << etapa(c.idex_valid, c.idex_instr) << " | "
                  << std::setw(3) << etapa(c.exmem_valid, c.exmem_instr) << " | "
                  << std::setw(3) << etapa(c.memwb_valid, c.memwb_instr) << "\n";
    }
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

    io_rdata.write(0);

    // =====================================================
    // A. Cinco instrucciones independientes (sin RAW, sin
    //    saltos, sin perifericos): LDI R1,11 ... LDI R5,55
    // =====================================================
    std::vector<sc_uint<16>> prog5;
    for (int i = 1; i <= 5; i++)
        prog5.push_back(formato_i(LDI, i, i * 11));
    cpu.cargar_programa(prog5);

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);

    cpu.iniciar_traza();
    int guarda = 0;
    while (cpu.traza.size() < 9 && guarda < 1000)
    {
        sc_start(1, SC_NS);
        guarda++;
    }
    sc_start(1, SC_NS);
    cpu.detener_traza();

    imprimir_traza(cpu.traza);

    bool lleno_c5 = false, wb_i1_c5 = false, wb_i5_c9 = false;
    if (cpu.traza.size() >= 5)
    {
        const auto& c5 = cpu.traza[4];
        lleno_c5 = c5.ifid_valid && c5.idex_valid && c5.exmem_valid && c5.memwb_valid;
        wb_i1_c5 = c5.memwb_valid && etiqueta(c5.memwb_instr) == "I1";
    }
    if (cpu.traza.size() >= 9)
    {
        const auto& c9 = cpu.traza[8];
        wb_i5_c9 = c9.memwb_valid && etiqueta(c9.memwb_instr) == "I5";
    }

    std::cout << "\n===== COMPROBACIONES DE LA TRAZA =====\n";
    comprobar("Pipeline lleno en ciclo 5", lleno_c5, "los 4 registros con valid=1");
    comprobar("I1 en WB en ciclo 5", wb_i1_c5, "");
    comprobar("I5 en WB en ciclo 9", wb_i5_c9, "");

    bool escrituras_ok = (cpu.escrituras.size() == 5);
    std::cout << "Escrituras arquitectonicas observadas: " << cpu.escrituras.size() << "\n";
    if (escrituras_ok)
    {
        for (int i = 0; i < 5; i++)
        {
            unsigned int rd = cpu.escrituras[i].rd.to_uint();
            unsigned int dato = cpu.escrituras[i].dato.to_uint();
            bool ok = (rd == (unsigned)(i + 1)) && (dato == (unsigned)((i + 1) * 11));
            if (!ok) escrituras_ok = false;
            std::cout << "  escritura " << (i + 1) << ": R" << rd
                      << " = " << dato << (ok ? "  [OK]" : "  [MAL]") << std::endl;
        }
    }
    comprobar("Una escritura por instruccion (5 en total)", escrituras_ok, "");

    bool regs_ok = true;
    for (int i = 1; i <= 5; i++)
    {
        unsigned int v = cpu.valor_registro(i).to_uint();
        if (v != (unsigned)(i * 11)) regs_ok = false;
        std::cout << "  R" << i << " = " << v << " (esperado " << i * 11 << ")\n";
    }
    comprobar("Resultados correctos en WB (R1..R5)", regs_ok, "");

    // =====================================================
    // B. Prueba aislada de R0: solo LDI R0,20
    // =====================================================
    std::cout << "\n===== B. PRUEBA AISLADA DE R0 =====\n";
    cpu.limpiar_eventos();
    std::vector<sc_uint<16>> prog_r0;
    prog_r0.push_back(formato_i(LDI, 0, 20));
    cpu.cargar_programa(prog_r0);

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);
    sc_start(120, SC_NS);

    unsigned int r0_final = cpu.valor_registro(0).to_uint();
    std::cout << "R0 observado tras WB = " << r0_final << " (esperado 20)\n";
    std::cout << "Escrituras observadas = " << cpu.escrituras.size() << "\n";
    comprobar("R0 contiene 20 tras WB", r0_final == 20,
              "lectura directa del banco de registros");
    comprobar("R0 se escribe exactamente una vez", cpu.escrituras.size() == 1,
              "eventos de escritura registrados");

    // =====================================================
    // C. Reset: invalida los cuatro registros e impide que
    //    una instruccion en vuelo escriba.
    // =====================================================
    std::cout << "\n===== C. PRUEBA DE RESET =====\n";
    cpu.limpiar_eventos();
    cpu.cargar_programa(prog_r0);

    // Limpia el estado con reset.
    reset.write(true);
    sc_start(35, SC_NS);
    cpu.limpiar_eventos();

    // Deja el LDI en vuelo pero antes de WB (pocos ciclos).
    reset.write(false);
    sc_start(25, SC_NS);
    std::cout << "Escrituras antes del reset en vuelo = " << cpu.escrituras.size() << "\n";

    // Reset con la instruccion en vuelo.
    reset.write(true);
    sc_start(35, SC_NS);

    bool v_ifid = cpu.ifid_valid.read();
    bool v_idex = cpu.idex_valid.read();
    bool v_exmem = cpu.exmem_valid.read();
    bool v_memwb = cpu.memwb_valid.read();

    std::cout << "valid observados con reset: IF/ID=" << v_ifid
              << " ID/EX=" << v_idex
              << " EX/MEM=" << v_exmem
              << " MEM/WB=" << v_memwb << "\n";
    comprobar("Reset invalida IF/ID", !v_ifid, "");
    comprobar("Reset invalida ID/EX", !v_idex, "");
    comprobar("Reset invalida EX/MEM", !v_exmem, "");
    comprobar("Reset invalida MEM/WB", !v_memwb, "");

    unsigned int r0_reset = cpu.valor_registro(0).to_uint();
    std::cout << "R0 tras reset = " << r0_reset << " (esperado 0)\n";
    comprobar("Reset limpia R0", r0_reset == 0, "");

    // Mantener reset: no deben aparecer escrituras.
    cpu.limpiar_eventos();
    sc_start(50, SC_NS);
    std::cout << "Escrituras durante reset sostenido = " << cpu.escrituras.size() << "\n";
    comprobar("La instruccion en vuelo no escribe tras reset",
              cpu.escrituras.size() == 0, "");

    std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
