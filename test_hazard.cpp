#include <systemc.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <map>
#include "cpu_goalkeeper.h"
#include "isa.h"

using namespace isa;

// Captura las escrituras del puerto de salida (flanco negativo).
SC_MODULE(MonitorSalida)
{
    sc_in<bool> clk;
    sc_in<bool> io_write;
    sc_in<sc_uint<8>> io_addr;
    sc_in<sc_uint<8>> io_wdata;

    std::map<int, int> escrituras;

    void capturar()
    {
        if (io_write.read())
            escrituras[io_addr.read().to_uint()] = io_wdata.read().to_uint();
    }

    SC_CTOR(MonitorSalida)
    {
        SC_METHOD(capturar);
        sensitive << clk.neg();
        dont_initialize();
    }
};

// Cuenta los ciclos en que la memoria de datos recibe una escritura.
SC_MODULE(MonitorMem)
{
    sc_in<bool> clk;
    sc_in<bool> write_enable;

    int escrituras;

    void capturar()
    {
        if (write_enable.read())
            escrituras++;
    }

    SC_CTOR(MonitorMem)
    {
        escrituras = 0;
        SC_METHOD(capturar);
        sensitive << clk.neg();
        dont_initialize();
    }
};

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const std::string& detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (!detalle.empty()) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

static std::string hex8(unsigned int v)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << v;
    return os.str();
}

static std::string desc(sc_uint<16> i)
{
    unsigned int op = i.range(15, 11).to_uint();
    unsigned int rd = i.range(10, 8).to_uint();
    unsigned int ra = i.range(7, 5).to_uint();
    unsigned int rb = i.range(4, 2).to_uint();
    unsigned int inm = i.range(7, 0).to_uint();
    unsigned int dir = i.range(10, 3).to_uint();

    std::ostringstream os;
    switch (op)
    {
        case NOP:  return "NOP";
        case LDI:  os << "LDI R" << rd << "," << inm; return os.str();
        case MOV:  os << "MOV R" << rd << ",R" << ra; return os.str();
        case ADD:  os << "ADD R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case SUB:  os << "SUB R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case AND:  os << "AND R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case OR:   os << "OR R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case CMP:  os << "CMP R" << ra << ",R" << rb; return os.str();
        case LD:   os << "LD R" << rd << "," << hex8(inm); return os.str();
        case ST:   os << "ST R" << rd << "," << hex8(inm); return os.str();
        case OUT:  os << "OUT R" << rd << "," << hex8(inm); return os.str();
        case IN:   os << "IN R" << rd << "," << hex8(inm); return os.str();
        case JEQ:  os << "JEQ " << dir; return os.str();
        case JNE:  os << "JNE " << dir; return os.str();
        case JGE:  os << "JGE " << dir; return os.str();
        case JLT:  os << "JLT " << dir; return os.str();
        case JGT:  os << "JGT " << dir; return os.str();
        case JLE:  os << "JLE " << dir; return os.str();
        case JMP:  os << "JMP " << dir; return os.str();
        case HALT: return "HALT";
        default:   os << hex8(i.to_uint()); return os.str();
    }
}

static std::string etapa(bool valida, sc_uint<16> i)
{
    return valida ? desc(i) : std::string("-");
}

static const char* fwd_nombre(unsigned int s)
{
    switch (s)
    {
        case CPU_Goalkeeper::FWD_EXMEM: return "EX/MEM";
        case CPU_Goalkeeper::FWD_MEMWB: return "MEM/WB";
        default:                        return "BANCO";
    }
}

static const char* flag_nombre(unsigned int s)
{
    switch (s)
    {
        case CPU_Goalkeeper::FLAG_EX:    return "EX";
        case CPU_Goalkeeper::FLAG_EXMEM: return "EX/MEM";
        case CPU_Goalkeeper::FLAG_MEMWB: return "MEM/WB";
        default:                         return "ARQ";
    }
}

static std::vector<sc_uint<16>> base()
{
    std::vector<sc_uint<16>> p;
    p.push_back(formato_i(LDI, 4, 5));   // R4 = 5
    p.push_back(formato_i(ST, 4, 0x10)); // mem[0x10] = 5
    p.push_back(formato_i(LDI, 3, 1));   // R3 = 1
    return p;
}

static void imprimir_tabla(const CPU_Goalkeeper& cpu)
{
    std::cout << "Ciclo | PC | St | IF | ID | EX | MEM | WB | vI vE vM vW\n";
    std::cout << "------+----+----+----+----+----+-----+----+-------------\n";
    for (const auto& c : cpu.traza)
    {
        std::cout << std::setw(5) << c.ciclo << " | "
                  << std::setw(2) << c.pc.to_uint() << " | "
                  << std::setw(2) << (c.stall ? "SI" : "no") << " | "
                  << std::setw(16) << desc(c.if_instr) << " | "
                  << std::setw(16) << etapa(c.ifid_valid, c.ifid_instr) << " | "
                  << std::setw(16) << etapa(c.idex_valid, c.idex_instr) << " | "
                  << std::setw(16) << etapa(c.exmem_valid, c.exmem_instr) << " | "
                  << std::setw(16) << etapa(c.memwb_valid, c.memwb_instr) << " | "
                  << c.ifid_valid << "  " << c.idex_valid << "  "
                  << c.exmem_valid << "  " << c.memwb_valid << "\n";
    }
}

static void ejecutar(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                     const std::string& nombre,
                     const std::vector<sc_uint<16>>& prog, int ciclos)
{
    cpu.detener_traza();
    cpu.limpiar_eventos();
    cpu.cargar_programa(prog);

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);

    cpu.iniciar_traza();
    int guarda = 0;
    while ((int)cpu.traza.size() < ciclos && guarda < 2000)
    {
        sc_start(1, SC_NS);
        guarda++;
    }
    sc_start(1, SC_NS);
    cpu.detener_traza();

    std::cout << "\n================ " << nombre << " ================\n";
    for (unsigned int i = 0; i < prog.size(); i++)
        std::cout << "  [" << i << "] " << desc(prog[i]) << "\n";
    imprimir_tabla(cpu);
}

static const CPU_Goalkeeper::TrazaCiclo* fila_ex(const CPU_Goalkeeper& cpu,
                                                 unsigned int op)
{
    for (const auto& c : cpu.traza)
        if (c.idex_valid && c.idex_instr.range(15, 11).to_uint() == op)
            return &c;
    return nullptr;
}

static const CPU_Goalkeeper::TrazaCiclo* fila_id(const CPU_Goalkeeper& cpu,
                                                 unsigned int op)
{
    for (const auto& c : cpu.traza)
        if (c.ifid_valid && c.ifid_instr.range(15, 11).to_uint() == op)
            return &c;
    return nullptr;
}

static const CPU_Goalkeeper::TrazaCiclo* fila_st(const CPU_Goalkeeper& cpu,
                                                 unsigned int addr)
{
    for (const auto& c : cpu.traza)
        if (c.idex_valid && c.idex_instr.range(15, 11).to_uint() == isa::ST &&
            c.idex_instr.range(7, 0).to_uint() == addr)
            return &c;
    return nullptr;
}

static const CPU_Goalkeeper::TrazaCiclo* fila_stall(const CPU_Goalkeeper& cpu)
{
    for (const auto& c : cpu.traza)
        if (c.stall) return &c;
    return nullptr;
}

static int stalls_en_traza(const CPU_Goalkeeper& cpu)
{
    int n = 0;
    for (const auto& c : cpu.traza)
        if (c.stall) n++;
    return n;
}

static void comprobar_congelacion(const CPU_Goalkeeper& cpu,
                                  const char* nombre)
{
    const auto* s = fila_stall(cpu);
    bool ok = false;
    if (s)
    {
        for (unsigned int i = 0; i + 1 < cpu.traza.size(); i++)
        {
            if (!cpu.traza[i].stall) continue;
            const auto& nxt = cpu.traza[i + 1];
            bool pc_igual = (nxt.pc == cpu.traza[i].pc);
            bool ifid_igual = (nxt.ifid_valid == cpu.traza[i].ifid_valid) &&
                              (nxt.ifid_instr == cpu.traza[i].ifid_instr);
            bool burbuja = !nxt.idex_valid;
            bool mem_avanza = nxt.exmem_valid &&
                (nxt.exmem_instr.range(15, 11).to_uint() == isa::LD);
            ok = pc_igual && ifid_igual && burbuja && mem_avanza;
            std::cout << "  stall en ciclo " << cpu.traza[i].ciclo
                      << ": PC=" << cpu.traza[i].pc.to_uint()
                      << " -> " << nxt.pc.to_uint()
                      << ", IF/ID=" << etapa(cpu.traza[i].ifid_valid, cpu.traza[i].ifid_instr)
                      << " -> " << etapa(nxt.ifid_valid, nxt.ifid_instr)
                      << ", ID/EX.v=" << nxt.idex_valid
                      << ", EX/MEM=" << etapa(nxt.exmem_valid, nxt.exmem_instr)
                      << "\n";
            if (i + 2 < cpu.traza.size())
            {
                const auto& w = cpu.traza[i + 2];
                bool wb_avanza = w.memwb_valid &&
                    (w.memwb_instr.range(15, 11).to_uint() == isa::LD);
                std::cout << "  ciclo " << w.ciclo << ": MEM/WB="
                          << etapa(w.memwb_valid, w.memwb_instr)
                          << " (avance del LD)\n";
                ok = ok && wb_avanza;
            }
            break;
        }
    }
    comprobar(nombre, ok, "PC/IFID congelados, burbuja ID/EX y avance de MEM");
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

    MonitorSalida monitor_out("monitor_out");
    monitor_out.clk(clk);
    monitor_out.io_write(io_write);
    monitor_out.io_addr(io_addr);
    monitor_out.io_wdata(io_wdata);

    MonitorMem monitor_mem("monitor_mem");
    monitor_mem.clk(clk);
    monitor_mem.write_enable(cpu.mem_write_enable);

    io_rdata.write(0);

    // =====================================================
    // A. LD -> ADD
    // =====================================================
    {
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_r(ADD, 2, 1, 3));
        ejecutar(cpu, reset, "A. LD -> ADD", p, 11);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 6), stalls="
                  << cpu.stalls_totales << ", escrituras="
                  << cpu.escrituras.size() << "\n";
        comprobar("A. R2=5+1", r2 == 6, "");
        comprobar("A. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("A. 1 stall en la traza", stalls_en_traza(cpu) == 1, "");
        comprobar("A. 4 escrituras RF (sin duplicados)", cpu.escrituras.size() == 4, "");
        comprobar_congelacion(cpu, "A. PC/IFID congelados y burbuja");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5 && f->ex_b.to_uint() == 1;
        comprobar("A. ADD recibe 5 desde MEM/WB y 1 del banco", ev, "");
    }

    // =====================================================
    // B. LD -> SUB
    // =====================================================
    {
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_r(SUB, 2, 1, 3));
        ejecutar(cpu, reset, "B. LD -> SUB", p, 11);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 4), stalls="
                  << cpu.stalls_totales << ", escrituras="
                  << cpu.escrituras.size() << "\n";
        comprobar("B. R2=5-1", r2 == 4, "");
        comprobar("B. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("B. 4 escrituras RF (sin duplicados)", cpu.escrituras.size() == 4, "");
        comprobar_congelacion(cpu, "B. PC/IFID congelados y burbuja");

        const auto* f = fila_ex(cpu, SUB);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5 && f->ex_b.to_uint() == 1;
        comprobar("B. SUB recibe 5 desde MEM/WB y 1 del banco", ev, "");
    }

    // =====================================================
    // C. LD -> CMP -> JEQ
    // =====================================================
    {
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_r(CMP, 0, 1, 4));
        p.push_back(formato_j(JEQ, 0));
        ejecutar(cpu, reset, "C. LD -> CMP -> JEQ", p, 11);

        std::cout << "Resultado: C=" << cpu.flag_c_arch.read()
                  << " Z=" << cpu.flag_z_arch.read()
                  << ", stalls=" << cpu.stalls_totales
                  << ", escrituras=" << cpu.escrituras.size() << "\n";
        comprobar("C. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("C. 3 escrituras RF (CMP no escribe)", cpu.escrituras.size() == 3, "");
        comprobar("C. banderas finales C=1 Z=1",
                  cpu.flag_c_arch.read() && cpu.flag_z_arch.read(), "");
        comprobar_congelacion(cpu, "C. PC/IFID congelados y burbuja");

        const auto* f = fila_ex(cpu, CMP);
        bool ev_cmp = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                      f->ex_a.to_uint() == 5 && f->ex_b.to_uint() == 5;
        comprobar("C. CMP recibe R1=5 desde MEM/WB", ev_cmp, "");

        const auto* j = fila_id(cpu, JEQ);
        if (j)
            std::cout << "  Ciclo " << j->ciclo << ": ID JEQ C=" << j->flag_c_fwd
                      << " Z=" << j->flag_z_fwd << " fuente="
                      << flag_nombre(j->flag_src.to_uint())
                      << " salto=" << j->ctrl_salto << "\n";
        bool ev_jeq = j && j->flag_src.to_uint() == CPU_Goalkeeper::FLAG_EX &&
                      j->flag_c_fwd && j->flag_z_fwd && j->ctrl_salto;
        comprobar("C. JEQ ve C/Z del CMP en EX (sin resolver PC)", ev_jeq, "");
    }

    // =====================================================
    // D. LD -> ST
    // =====================================================
    {
        monitor_mem.escrituras = 0;
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_i(ST, 1, 0x20));
        ejecutar(cpu, reset, "D. LD -> ST", p, 11);

        unsigned int mem20 = cpu.dmem.memoria[0x20].read().to_uint();
        std::cout << "Resultado: mem[0x20]=" << mem20
                  << " (esperado 5), stalls=" << cpu.stalls_totales
                  << ", escrituras MEM=" << monitor_mem.escrituras << "\n";
        comprobar("D. mem[0x20]=5", mem20 == 5, "");
        comprobar("D. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("D. dos ST en total (setup + prueba), sin duplicado",
                  monitor_mem.escrituras == 2, "");
        comprobar("D. 3 escrituras RF (LD si, ST no)",
                  cpu.escrituras.size() == 3, "");
        comprobar_congelacion(cpu, "D. PC/IFID congelados y burbuja");

        const auto* f = fila_st(cpu, 0x20);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5 &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_NINGUNO;
        comprobar("D. ST recibe el dato cargado desde MEM/WB (solo A)", ev, "");
    }

    // =====================================================
    // E. LD -> OUT
    // =====================================================
    {
        monitor_out.escrituras.clear();
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_i(OUT, 1, 0x30));
        ejecutar(cpu, reset, "E. LD -> OUT", p, 11);

        unsigned int puerto = monitor_out.escrituras.count(0x30) ?
                              monitor_out.escrituras[0x30] : -1;
        std::cout << "Resultado: puerto 0x30=" << puerto
                  << " (esperado 5), stalls=" << cpu.stalls_totales
                  << ", pulsos OUT=" << monitor_out.escrituras.size() << "\n";
        comprobar("E. puerto 0x30=5", puerto == 5, "");
        comprobar("E. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("E. un unico efecto OUT", monitor_out.escrituras.size() == 1, "");
        comprobar("E. 3 escrituras RF (OUT no escribe)", cpu.escrituras.size() == 3, "");
        comprobar_congelacion(cpu, "E. PC/IFID congelados y burbuja");

        const auto* f = fila_ex(cpu, OUT);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5;
        comprobar("E. OUT recibe 5 desde MEM/WB", ev, "");
    }

    // =====================================================
    // F. LD -> NOP -> ADD (sin stall)
    // =====================================================
    {
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_r(ADD, 2, 1, 3));
        ejecutar(cpu, reset, "F. LD -> NOP -> ADD (sin stall)", p, 11);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 6), stalls="
                  << cpu.stalls_totales << ", escrituras="
                  << cpu.escrituras.size() << "\n";
        comprobar("F. R2=5+1", r2 == 6, "");
        comprobar("F. cero stalls", cpu.stalls_totales == 0, "");
        comprobar("F. cero stalls en la traza", stalls_en_traza(cpu) == 0, "");
        comprobar("F. 4 escrituras RF (sin duplicados)", cpu.escrituras.size() == 4, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5 && f->ex_b.to_uint() == 1;
        comprobar("F. ADD recibe el LD desde MEM/WB sin stall", ev, "");
    }

    // =====================================================
    // G. LD -> ADD con el mismo registro en ambos operandos
    // =====================================================
    {
        auto p = base();
        p.push_back(formato_i(LD, 1, 0x10));
        p.push_back(formato_r(ADD, 2, 1, 1));
        ejecutar(cpu, reset, "G. LD -> ADD (R1 en A y B)", p, 11);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 10), stalls="
                  << cpu.stalls_totales << ", escrituras="
                  << cpu.escrituras.size() << "\n";
        comprobar("G. R2=5+5", r2 == 10, "");
        comprobar("G. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("G. 4 escrituras RF (sin duplicados)", cpu.escrituras.size() == 4, "");
        comprobar_congelacion(cpu, "G. PC/IFID congelados y burbuja");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5 && f->ex_b.to_uint() == 5;
        comprobar("G. A y B reciben 5 desde MEM/WB", ev, "");
    }

    // =====================================================
    // H. LD R0 -> ADD con R0 en ambos operandos
    // =====================================================
    {
        auto p = base();
        p.push_back(formato_i(LD, 0, 0x10));
        p.push_back(formato_r(ADD, 2, 0, 0));
        ejecutar(cpu, reset, "H. LD R0 -> ADD (R0 en A y B)", p, 11);

        unsigned int r0 = cpu.valor_registro(0).to_uint();
        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R0=" << r0 << " (esperado 5), R2=" << r2
                  << " (esperado 10), stalls=" << cpu.stalls_totales
                  << ", escrituras=" << cpu.escrituras.size() << "\n";
        comprobar("H. R0 cargado con 5", r0 == 5, "");
        comprobar("H. R2=5+5", r2 == 10, "");
        comprobar("H. exactamente 1 stall", cpu.stalls_totales == 1, "");
        comprobar("H. 4 escrituras RF (sin duplicados)", cpu.escrituras.size() == 4, "");
        comprobar_congelacion(cpu, "H. PC/IFID congelados y burbuja");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f && f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 5 && f->ex_b.to_uint() == 5;
        comprobar("H. R0 no se excluye: A y B desde MEM/WB", ev, "");
    }

    std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
