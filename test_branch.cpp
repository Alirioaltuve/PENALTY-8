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

// Monitor de OUT que cuenta pulsos reales.
SC_MODULE(MonitorSalida)
{
    sc_in<bool> clk;
    sc_in<bool> io_write;
    sc_in<sc_uint<8>> io_addr;
    sc_in<sc_uint<8>> io_wdata;

    int pulsos;
    std::map<int, int> ultimo;

    void capturar()
    {
        if (io_write.read())
        {
            pulsos++;
            ultimo[io_addr.read().to_uint()] = io_wdata.read().to_uint();
        }
    }

    SC_CTOR(MonitorSalida)
    {
        pulsos = 0;
        SC_METHOD(capturar);
        sensitive << clk.neg();
        dont_initialize();
    }
};

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
        case IN:   os << "IN R" << rd << "," << hex8(inm); return os.str();
        case OUT:  os << "OUT R" << rd << "," << hex8(inm); return os.str();
        case JMP:  os << "JMP " << dir; return os.str();
        case JEQ:  os << "JEQ " << dir; return os.str();
        case JNE:  os << "JNE " << dir; return os.str();
        case JGE:  os << "JGE " << dir; return os.str();
        case JLT:  os << "JLT " << dir; return os.str();
        case JGT:  os << "JGT " << dir; return os.str();
        case JLE:  os << "JLE " << dir; return os.str();
        case HALT: return "HALT";
        default:   os << hex8(i.to_uint()); return os.str();
    }
}

static std::string etapa(bool valida, sc_uint<16> i)
{
    return valida ? desc(i) : std::string("-");
}

static bool decision_branch(unsigned int op, bool c, bool z)
{
    switch (op)
    {
        case JMP: return true;
        case JEQ: return z;
        case JNE: return !z;
        case JGE: return c;
        case JLT: return !c;
        case JGT: return c && !z;
        case JLE: return !c || z;
        default:  return false;
    }
}

static void imprimir_tabla(const CPU_Goalkeeper& cpu)
{
    std::cout << "Ciclo | PC | St | Fl | Br | HA | HS | HD | IF | ID | EX | MEM | WB | vI vE vM vW\n";
    std::cout << "------+----+----+----+----+----+----+----+----+----+----+-----+----+-------------\n";
    for (const auto& c : cpu.traza)
    {
        std::cout << std::setw(5) << c.ciclo << " | "
                  << std::setw(2) << c.pc.to_uint() << " | "
                  << std::setw(2) << (c.stall ? "SI" : "no") << " | "
                  << std::setw(2) << (c.flush ? "SI" : "no") << " | "
                  << std::setw(2) << (c.branch_taken ? "SI" : "no") << " | "
                  << std::setw(2) << (c.halt_ahora ? "SI" : "no") << " | "
                  << std::setw(2) << (c.halt_stop ? "SI" : "no") << " | "
                  << std::setw(2) << (c.halted_reg ? "SI" : "no") << " | "
                  << std::setw(14) << desc(c.if_instr) << " | "
                  << std::setw(14) << etapa(c.ifid_valid, c.ifid_instr) << " | "
                  << std::setw(14) << etapa(c.idex_valid, c.idex_instr) << " | "
                  << std::setw(14) << etapa(c.exmem_valid, c.exmem_instr) << " | "
                  << std::setw(14) << etapa(c.memwb_valid, c.memwb_instr) << " | "
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
    while ((int)cpu.traza.size() < ciclos && guarda < 3000)
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

static int idx_branch(const CPU_Goalkeeper& cpu, unsigned int op)
{
    for (unsigned int i = 0; i < cpu.traza.size(); i++)
    {
        const auto& c = cpu.traza[i];
        if (c.ifid_valid && c.ifid_instr.range(15, 11).to_uint() == op)
            return (int)i;
    }
    return -1;
}

static bool no_ejecutada(const CPU_Goalkeeper& cpu, sc_uint<16> instr)
{
    for (const auto& c : cpu.traza)
    {
        if ((c.ifid_valid && c.ifid_instr == instr) ||
            (c.idex_valid && c.idex_instr == instr) ||
            (c.exmem_valid && c.exmem_instr == instr) ||
            (c.memwb_valid && c.memwb_instr == instr))
            return false;
    }
    return true;
}

static void comprobar_taken(const CPU_Goalkeeper& cpu, unsigned int op,
                            unsigned int target,
                            const std::vector<sc_uint<16>>& skipped,
                            sc_uint<16> target_instr,
                            const std::string& nombre)
{
    int i = idx_branch(cpu, op);
    bool ok = i >= 0;
    if (ok)
    {
        const auto& b = cpu.traza[i];
        ok = b.branch_taken && b.flush && b.ifid_valid &&
             b.ctrl_salto && b.ctrl_dir_salto.to_uint() == target;

        if ((unsigned int)(i + 1) < cpu.traza.size())
        {
            const auto& n = cpu.traza[i + 1];
            ok = ok && n.pc.to_uint() == target && !n.ifid_valid &&
                 n.idex_valid &&
                 n.idex_instr.range(15, 11).to_uint() == op;
        }
        if ((unsigned int)(i + 2) < cpu.traza.size())
        {
            const auto& t = cpu.traza[i + 2];
            ok = ok && t.ifid_valid && t.ifid_instr == target_instr;
        }
        for (const auto& s : skipped)
            ok = ok && no_ejecutada(cpu, s);
    }
    comprobar(nombre.c_str(), ok, "");
}

static void comprobar_not(const CPU_Goalkeeper& cpu, unsigned int op,
                          sc_uint<16> sequential, sc_uint<16> target_sentinel,
                          const std::string& nombre)
{
    int i = idx_branch(cpu, op);
    bool ok = i >= 0;
    if (ok)
    {
        const auto& b = cpu.traza[i];
        ok = !b.branch_taken && !b.flush && b.ifid_valid && !b.ctrl_salto;
        if ((unsigned int)(i + 1) < cpu.traza.size())
        {
            const auto& n = cpu.traza[i + 1];
            ok = ok && n.ifid_valid && n.ifid_instr == sequential;
        }
        ok = ok && no_ejecutada(cpu, target_sentinel);
    }
    comprobar(nombre.c_str(), ok, "");
}

static void comprobar_halt(const CPU_Goalkeeper& cpu,
                           const std::vector<sc_uint<16>>& skipped,
                           const std::string& nombre)
{
    int i = -1;
    for (unsigned int k = 0; k < cpu.traza.size(); k++)
    {
        const auto& c = cpu.traza[k];
        if (c.ifid_valid && c.ifid_instr.range(15, 11).to_uint() == HALT &&
            c.halt_ahora)
        {
            i = (int)k;
            break;
        }
    }

    bool ok = i >= 0;
    if (ok)
    {
        const auto& b = cpu.traza[i];
        ok = b.flush && b.halt_ahora && b.pc_freeze && !b.branch_taken;

        if ((unsigned int)(i + 1) < cpu.traza.size())
        {
            const auto& n = cpu.traza[i + 1];
            ok = ok && !n.ifid_valid && n.halt_stop &&
                 n.idex_valid &&
                 n.idex_instr.range(15, 11).to_uint() == HALT &&
                 n.pc == b.pc;
        }

        bool halt_en_wb = false;
        for (const auto& c : cpu.traza)
            if (c.memwb_valid &&
                c.memwb_instr.range(15, 11).to_uint() == HALT)
                halt_en_wb = true;

        ok = ok && halt_en_wb && cpu.halted_reg.read();
        for (const auto& s : skipped)
            ok = ok && no_ejecutada(cpu, s);
    }
    comprobar(nombre.c_str(), ok, "");
}

static void comprobar_stall(const CPU_Goalkeeper& cpu, const std::string& nombre)
{
    int i = -1;
    for (unsigned int k = 0; k < cpu.traza.size(); k++)
        if (cpu.traza[k].stall) { i = (int)k; break; }

    bool ok = i >= 0;
    if (ok && (unsigned int)(i + 1) < cpu.traza.size())
    {
        const auto& b = cpu.traza[i];
        const auto& n = cpu.traza[i + 1];
        ok = n.pc == b.pc && n.ifid_instr == b.ifid_instr &&
             !n.idex_valid && n.exmem_valid &&
             n.exmem_instr.range(15, 11).to_uint() == LD;
    }
    comprobar(nombre.c_str(), ok, "");
}

// B-E, I: CMP seguido inmediatamente de un branch condicional.
static void probar_cond(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                        unsigned int op, unsigned int a, unsigned int b,
                        const std::string& nombre)
{
    bool c = (a >= b);
    bool z = (a == b);
    bool taken = decision_branch(op, c, z);

    std::vector<sc_uint<16>> p;
    p.push_back(formato_i(LDI, 1, a));
    p.push_back(formato_i(LDI, 2, b));
    p.push_back(formato_r(CMP, 0, 1, 2));
    p.push_back(formato_j((Opcode)op, 6));
    p.push_back(formato_i(LDI, 5, 0x99));
    p.push_back(formato_i(HALT, 0, 0));
    p.push_back(formato_i(LDI, 5, 0x77));
    p.push_back(formato_i(HALT, 0, 0));

    ejecutar(cpu, reset, nombre, p, 18);

    unsigned int r5 = cpu.valor_registro(5).to_uint();
    std::cout << "  Resultado R5=" << r5
              << ", branches=" << cpu.branches_tomados
              << ", halts=" << cpu.halts_detectados
              << ", flushes=" << cpu.flushes_totales
              << ", stalls=" << cpu.stalls_totales
              << ", escrituras=" << cpu.escrituras.size() << "\n";

    comprobar((nombre + ". R5 correcto").c_str(),
              r5 == (taken ? 0x77u : 0x99u), "");
    comprobar((nombre + ". branch tomado/no").c_str(),
              cpu.branches_tomados == (taken ? 1 : 0), "");
    comprobar((nombre + ". un HALT").c_str(), cpu.halts_detectados == 1, "");
    comprobar((nombre + ". flushes").c_str(),
              cpu.flushes_totales == (taken ? 2 : 1), "");
    comprobar((nombre + ". sin stalls").c_str(), cpu.stalls_totales == 0, "");
    comprobar((nombre + ". sin escrituras duplicadas").c_str(),
              cpu.escrituras.size() == 3, "");

    int i = idx_branch(cpu, op);
    const auto* f = (i >= 0) ? &cpu.traza[i] : nullptr;
    bool flags_ok = f &&
                    f->flag_src.to_uint() == CPU_Goalkeeper::FLAG_EX &&
                    f->flag_c_fwd == c && f->flag_z_fwd == z &&
                    f->ctrl_salto == taken;
    comprobar((nombre + ". flags del CMP en EX").c_str(), flags_ok, "");

    if (taken)
        comprobar_taken(cpu, op, 6, {formato_i(LDI, 5, 0x99)},
                        formato_i(LDI, 5, 0x77), nombre + ". flush/destino");
    else
        comprobar_not(cpu, op, formato_i(LDI, 5, 0x99),
                      formato_i(LDI, 5, 0x77), nombre + ". sin flush/sin destino");

    comprobar((nombre + ". halted sticky").c_str(),
              cpu.halted_reg.read(), "");
}

// J: SUB seguido inmediatamente de JGT/JLE.
static void probar_arith(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                         unsigned int op, unsigned int a, unsigned int b,
                         const std::string& nombre)
{
    bool c = (a >= b);
    bool z = (a == b);
    bool taken = decision_branch(op, c, z);

    std::vector<sc_uint<16>> p;
    p.push_back(formato_i(LDI, 1, a));
    p.push_back(formato_i(LDI, 2, b));
    p.push_back(formato_r(SUB, 3, 1, 2));
    p.push_back(formato_j((Opcode)op, 6));
    p.push_back(formato_i(LDI, 5, 0x99));
    p.push_back(formato_i(HALT, 0, 0));
    p.push_back(formato_i(LDI, 5, 0x77));
    p.push_back(formato_i(HALT, 0, 0));

    ejecutar(cpu, reset, nombre, p, 18);

    unsigned int r3 = cpu.valor_registro(3).to_uint();
    unsigned int r5 = cpu.valor_registro(5).to_uint();
    unsigned int esperado_r3 = (unsigned int)((a - b) & 0xFF);
    std::cout << "  Resultado R3=" << r3 << " R5=" << r5
              << ", branches=" << cpu.branches_tomados
              << ", flushes=" << cpu.flushes_totales << "\n";

    comprobar((nombre + ". R3=a-b").c_str(), r3 == esperado_r3, "");
    comprobar((nombre + ". R5 correcto").c_str(),
              r5 == (taken ? 0x77u : 0x99u), "");
    comprobar((nombre + ". branch tomado/no").c_str(),
              cpu.branches_tomados == (taken ? 1 : 0), "");
    comprobar((nombre + ". flushes").c_str(),
              cpu.flushes_totales == (taken ? 2 : 1), "");
    comprobar((nombre + ". escrituras RF").c_str(),
              cpu.escrituras.size() == 4, "");

    int i = idx_branch(cpu, op);
    const auto* f = (i >= 0) ? &cpu.traza[i] : nullptr;
    bool flags_ok = f &&
                    f->flag_src.to_uint() == CPU_Goalkeeper::FLAG_EX &&
                    f->flag_c_fwd == c && f->flag_z_fwd == z &&
                    f->ctrl_salto == taken;
    comprobar((nombre + ". flags del SUB en EX").c_str(), flags_ok, "");

    if (taken)
        comprobar_taken(cpu, op, 6, {formato_i(LDI, 5, 0x99)},
                        formato_i(LDI, 5, 0x77), nombre + ". flush/destino");
    else
        comprobar_not(cpu, op, formato_i(LDI, 5, 0x99),
                      formato_i(LDI, 5, 0x77), nombre + ". sin flush/sin destino");
}

static void test_A(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                   MonitorSalida&, MonitorMem&)
{
    auto p = std::vector<sc_uint<16>>{
        formato_j(JMP, 3),
        formato_i(LDI, 5, 0x99),
        formato_i(LDI, 6, 0x98),
        formato_i(LDI, 6, 0x07),
        formato_i(HALT, 0, 0)
    };
    ejecutar(cpu, reset, "A. JMP tomado", p, 16);

    unsigned int r5 = cpu.valor_registro(5).to_uint();
    unsigned int r6 = cpu.valor_registro(6).to_uint();
    std::cout << "  Resultado R5=" << r5 << " R6=" << r6
              << ", branches=" << cpu.branches_tomados
              << ", flushes=" << cpu.flushes_totales << "\n";
    comprobar("A. R5 no se escribe", r5 == 0, "");
    comprobar("A. R6=7 en destino", r6 == 7, "");
    comprobar("A. un branch tomado", cpu.branches_tomados == 1, "");
    comprobar("A. un flush de branch + HALT", cpu.flushes_totales == 2, "");
    comprobar("A. sin stalls", cpu.stalls_totales == 0, "");
    comprobar("A. una escritura RF (R6)", cpu.escrituras.size() == 1, "");
    comprobar_taken(cpu, JMP, 3,
                    {formato_i(LDI, 5, 0x99), formato_i(LDI, 6, 0x98)},
                    formato_i(LDI, 6, 0x07), "A. flush/destino");
    comprobar("A. halted sticky", cpu.halted_reg.read(), "");
}

static void test_A2(CPU_Goalkeeper& cpu, sc_signal<bool>& reset)
{
    // El JMP anula un JEQ y un HALT: ninguno debe redirigir ni detener.
    auto p = std::vector<sc_uint<16>>{
        formato_j(JMP, 3),
        formato_j(JEQ, 7),
        formato_i(HALT, 0, 0),
        formato_i(LDI, 6, 7),
        formato_i(HALT, 0, 0)
    };
    ejecutar(cpu, reset, "A2. JMP anula JEQ y HALT", p, 16);

    unsigned int r6 = cpu.valor_registro(6).to_uint();
    std::cout << "  Resultado R6=" << r6
              << ", branches=" << cpu.branches_tomados
              << ", halts=" << cpu.halts_detectados
              << ", flushes=" << cpu.flushes_totales << "\n";
    comprobar("A2. el JEQ/HALT anulados no afectan el flujo", r6 == 7, "");
    comprobar("A2. solo el JMP se toma", cpu.branches_tomados == 1, "");
    comprobar("A2. solo el HALT destino se detecta",
              cpu.halts_detectados == 1, "");
    comprobar("A2. branch + HALT, dos flushes", cpu.flushes_totales == 2, "");
    comprobar("A2. sin stalls", cpu.stalls_totales == 0, "");
    comprobar("A2. una escritura RF (R6)", cpu.escrituras.size() == 1, "");
    comprobar_taken(cpu, JMP, 3, {formato_j(JEQ, 7)},
                    formato_i(LDI, 6, 7), "A2. flush/destino");
    comprobar("A2. halted sticky", cpu.halted_reg.read(), "");
}

static void test_condicionales(CPU_Goalkeeper& cpu, sc_signal<bool>& reset)
{
    probar_cond(cpu, reset, JEQ, 5, 5, "B1. JEQ tomado");
    probar_cond(cpu, reset, JEQ, 5, 3, "B2. JEQ no tomado");
    probar_cond(cpu, reset, JNE, 5, 3, "C1. JNE tomado");
    probar_cond(cpu, reset, JNE, 5, 5, "C2. JNE no tomado");
    probar_cond(cpu, reset, JGE, 5, 3, "D1. JGE tomado");
    probar_cond(cpu, reset, JGE, 3, 5, "D2. JGE no tomado");
    probar_cond(cpu, reset, JLT, 3, 5, "D3. JLT tomado");
    probar_cond(cpu, reset, JLT, 5, 3, "D4. JLT no tomado");
    probar_cond(cpu, reset, JGT, 5, 5, "E1. JGT igualdad: no tomado");
    probar_cond(cpu, reset, JGT, 5, 3, "E2. JGT mayor: tomado");
    probar_cond(cpu, reset, JLE, 5, 5, "E3. JLE igualdad: tomado");
    probar_cond(cpu, reset, JLE, 3, 5, "E4. JLE menor: tomado");
    probar_cond(cpu, reset, JLE, 5, 3, "E5. JLE mayor: no tomado");
}

static void test_FGH(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                     MonitorSalida& monitor_out, MonitorMem& monitor_mem)
{
    // F: flush de LDI
    {
        monitor_mem.escrituras = 0;
        monitor_out.pulsos = 0;
        monitor_out.ultimo.clear();

        auto p = std::vector<sc_uint<16>>{
            formato_i(LDI, 1, 0x55),
            formato_i(LDI, 2, 5),
            formato_r(CMP, 0, 2, 2),
            formato_j(JEQ, 6),
            formato_i(LDI, 5, 0x99),
            formato_i(HALT, 0, 0),
            formato_i(LDI, 6, 7),
            formato_i(HALT, 0, 0)
        };
        ejecutar(cpu, reset, "F. Flush de LDI", p, 16);

        std::cout << "  R5=" << cpu.valor_registro(5).to_uint()
                  << " R6=" << cpu.valor_registro(6).to_uint()
                  << ", escrituras RF=" << cpu.escrituras.size() << "\n";
        comprobar("F. LDI anulada no escribe R5",
                  cpu.valor_registro(5).to_uint() == 0, "");
        comprobar("F. destino R6=7",
                  cpu.valor_registro(6).to_uint() == 7, "");
        comprobar("F. sin escrituras duplicadas",
                  cpu.escrituras.size() == 3, "");
        comprobar_taken(cpu, JEQ, 6, {formato_i(LDI, 5, 0x99)},
                        formato_i(LDI, 6, 7), "F. flush/destino");
    }

    // G: flush de ST
    {
        monitor_mem.escrituras = 0;
        monitor_out.pulsos = 0;
        monitor_out.ultimo.clear();

        auto p = std::vector<sc_uint<16>>{
            formato_i(LDI, 1, 0x55),
            formato_i(LDI, 2, 5),
            formato_r(CMP, 0, 2, 2),
            formato_j(JEQ, 6),
            formato_i(ST, 1, 0x20),
            formato_i(HALT, 0, 0),
            formato_i(LDI, 6, 7),
            formato_i(HALT, 0, 0)
        };
        ejecutar(cpu, reset, "G. Flush de ST", p, 16);

        std::cout << "  mem[0x20]=" << cpu.dmem.memoria[0x20].read().to_uint()
                  << ", escrituras MEM=" << monitor_mem.escrituras << "\n";
        comprobar("G. ST anulada no escribe memoria",
                  cpu.dmem.memoria[0x20].read().to_uint() == 0, "");
        comprobar("G. sin escritura MEM", monitor_mem.escrituras == 0, "");
        comprobar("G. destino R6=7",
                  cpu.valor_registro(6).to_uint() == 7, "");
        comprobar("G. tres escrituras RF (R1,R2,R6)",
                  cpu.escrituras.size() == 3, "");
        comprobar_taken(cpu, JEQ, 6, {formato_i(ST, 1, 0x20)},
                        formato_i(LDI, 6, 7), "G. flush/destino");
    }

    // H: flush de OUT
    {
        monitor_mem.escrituras = 0;
        monitor_out.pulsos = 0;
        monitor_out.ultimo.clear();

        auto p = std::vector<sc_uint<16>>{
            formato_i(LDI, 1, 0x55),
            formato_i(LDI, 2, 5),
            formato_r(CMP, 0, 2, 2),
            formato_j(JEQ, 6),
            formato_i(OUT, 1, 0x30),
            formato_i(HALT, 0, 0),
            formato_i(LDI, 6, 7),
            formato_i(HALT, 0, 0)
        };
        ejecutar(cpu, reset, "H. Flush de OUT", p, 16);

        std::cout << "  pulsos OUT=" << monitor_out.pulsos
                  << ", destino R6=" << cpu.valor_registro(6).to_uint() << "\n";
        comprobar("H. OUT anulada no genera pulso",
                  monitor_out.pulsos == 0, "");
        comprobar("H. destino R6=7",
                  cpu.valor_registro(6).to_uint() == 7, "");
        comprobar("H. tres escrituras RF (R1,R2,R6)",
                  cpu.escrituras.size() == 3, "");
        comprobar_taken(cpu, JEQ, 6, {formato_i(OUT, 1, 0x30)},
                        formato_i(LDI, 6, 7), "H. flush/destino");
    }
}

static void test_J(CPU_Goalkeeper& cpu, sc_signal<bool>& reset)
{
    probar_arith(cpu, reset, JGT, 5, 3, "J1. SUB -> JGT tomado");
    probar_arith(cpu, reset, JGT, 3, 5, "J2. SUB -> JGT no tomado");
    probar_arith(cpu, reset, JLE, 5, 3, "J3. SUB -> JLE no tomado");
    probar_arith(cpu, reset, JLE, 3, 5, "J4. SUB -> JLE tomado");
}

static void test_K(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                   MonitorSalida& monitor_out, MonitorMem& monitor_mem)
{
    monitor_mem.escrituras = 0;
    monitor_out.pulsos = 0;
    monitor_out.ultimo.clear();

    auto p = std::vector<sc_uint<16>>{
        formato_i(LDI, 4, 5),
        formato_i(ST, 4, 0x10),
        formato_i(LD, 1, 0x10),
        formato_r(CMP, 0, 1, 4),
        formato_j(JEQ, 7),
        formato_i(LDI, 3, 0x99),
        formato_i(HALT, 0, 0),
        formato_i(LDI, 5, 7),
        formato_i(HALT, 0, 0)
    };
    ejecutar(cpu, reset, "K. LD-use -> JEQ", p, 18);

    std::cout << "  R3=" << cpu.valor_registro(3).to_uint()
              << " R5=" << cpu.valor_registro(5).to_uint()
              << ", stalls=" << cpu.stalls_totales
              << ", branches=" << cpu.branches_tomados
              << ", flushes=" << cpu.flushes_totales
              << ", escrituras=" << cpu.escrituras.size() << "\n";
    comprobar("K. un stall load-use", cpu.stalls_totales == 1, "");
    comprobar("K. LDI anulada no escribe R3",
              cpu.valor_registro(3).to_uint() == 0, "");
    comprobar("K. destino R5=7",
              cpu.valor_registro(5).to_uint() == 7, "");
    comprobar("K. un branch + un HALT",
              cpu.branches_tomados == 1 && cpu.halts_detectados == 1, "");
    comprobar("K. dos flushes (branch+HALT)", cpu.flushes_totales == 2, "");
    comprobar("K. sin escrituras duplicadas", cpu.escrituras.size() == 3, "");
    comprobar_stall(cpu, "K. PC/IFID congelados; LD avanza");
    comprobar_taken(cpu, JEQ, 7, {formato_i(LDI, 3, 0x99)},
                    formato_i(LDI, 5, 7), "K. flush/destino");
}

static void test_O(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                   MonitorSalida& monitor_out, MonitorMem& monitor_mem)
{
    monitor_mem.escrituras = 0;
    monitor_out.pulsos = 0;
    monitor_out.ultimo.clear();

    auto p = std::vector<sc_uint<16>>{
        formato_i(LDI, 4, 5),
        formato_i(ST, 4, 0x10),
        formato_i(LD, 1, 0x10),
        formato_r(CMP, 0, 1, 4),
        formato_j(JEQ, 7),
        formato_i(ST, 1, 0x20),
        formato_i(OUT, 1, 0x30),
        formato_i(LDI, 5, 7),
        formato_i(HALT, 0, 0)
    };
    ejecutar(cpu, reset, "O. Stall + branch + flush", p, 18);

    std::cout << "  mem[0x20]=" << cpu.dmem.memoria[0x20].read().to_uint()
              << ", pulsos OUT=" << monitor_out.pulsos
              << ", R5=" << cpu.valor_registro(5).to_uint()
              << ", stalls=" << cpu.stalls_totales << "\n";
    comprobar("O. un stall load-use", cpu.stalls_totales == 1, "");
    comprobar("O. ST anulada no escribe",
              cpu.dmem.memoria[0x20].read().to_uint() == 0 &&
              monitor_mem.escrituras == 1, "");
    comprobar("O. OUT no genera pulso", monitor_out.pulsos == 0, "");
    comprobar("O. destino R5=7",
              cpu.valor_registro(5).to_uint() == 7, "");
    comprobar("O. tres escrituras RF (R4,R1,R5)",
              cpu.escrituras.size() == 3, "");
    comprobar("O. dos flushes (branch+HALT)", cpu.flushes_totales == 2, "");
    comprobar_stall(cpu, "O. congelacion por load-use");
    comprobar_taken(cpu, JEQ, 7,
                    {formato_i(ST, 1, 0x20), formato_i(OUT, 1, 0x30)},
                    formato_i(LDI, 5, 7), "O. flush/destino");
}

static void test_LM(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                    MonitorSalida& monitor_out, MonitorMem& monitor_mem)
{
    // L: HALT con instrucciones anteriores
    {
        monitor_mem.escrituras = 0;
        monitor_out.pulsos = 0;
        monitor_out.ultimo.clear();

        auto p = std::vector<sc_uint<16>>{
            formato_i(LDI, 1, 1),
            formato_i(LDI, 2, 2),
            formato_i(HALT, 0, 0),
            formato_i(LDI, 3, 0x99),
            formato_i(ST, 1, 0x20),
            formato_i(OUT, 2, 0x30)
        };
        ejecutar(cpu, reset, "L. HALT con anteriores", p, 16);

        std::cout << "  R1=" << cpu.valor_registro(1).to_uint()
                  << " R2=" << cpu.valor_registro(2).to_uint()
                  << " R3=" << cpu.valor_registro(3).to_uint()
                  << ", halted=" << cpu.halted_reg.read()
                  << ", halt_stop=" << cpu.halt_stop.read() << "\n";
        comprobar("L. anteriores completan R1=1 R2=2",
                  cpu.valor_registro(1).to_uint() == 1 &&
                  cpu.valor_registro(2).to_uint() == 2, "");
        comprobar("L. LDI posterior no escribe R3",
                  cpu.valor_registro(3).to_uint() == 0, "");
        comprobar("L. ST posterior sin efecto",
                  cpu.dmem.memoria[0x20].read().to_uint() == 0 &&
                  monitor_mem.escrituras == 0, "");
        comprobar("L. OUT posterior sin pulso", monitor_out.pulsos == 0, "");
        comprobar("L. un HALT y un flush",
                  cpu.halts_detectados == 1 && cpu.flushes_totales == 1, "");
        comprobar("L. halted sticky", cpu.halted_reg.read(), "");
        comprobar("L. fetch detenido", cpu.halt_stop.read(), "");
        comprobar_halt(cpu, {formato_i(LDI, 3, 0x99)},
                       "L. traza de HALT");
    }

    // M: HALT seguido de instrucciones
    {
        monitor_mem.escrituras = 0;
        monitor_out.pulsos = 0;
        monitor_out.ultimo.clear();

        auto p = std::vector<sc_uint<16>>{
            formato_i(HALT, 0, 0),
            formato_i(LDI, 3, 0x99),
            formato_i(ST, 1, 0x20),
            formato_i(OUT, 2, 0x30)
        };
        ejecutar(cpu, reset, "M. HALT con posteriores", p, 16);

        std::cout << "  R3=" << cpu.valor_registro(3).to_uint()
                  << ", halted=" << cpu.halted_reg.read()
                  << ", escrituras RF=" << cpu.escrituras.size() << "\n";
        comprobar("M. LDI posterior no escribe R3",
                  cpu.valor_registro(3).to_uint() == 0, "");
        comprobar("M. sin escrituras RF", cpu.escrituras.size() == 0, "");
        comprobar("M. ST posterior sin efecto",
                  cpu.dmem.memoria[0x20].read().to_uint() == 0 &&
                  monitor_mem.escrituras == 0, "");
        comprobar("M. OUT posterior sin pulso", monitor_out.pulsos == 0, "");
        comprobar("M. un HALT y un flush",
                  cpu.halts_detectados == 1 && cpu.flushes_totales == 1, "");
        comprobar("M. halted sticky", cpu.halted_reg.read(), "");
        comprobar_halt(cpu, {formato_i(LDI, 3, 0x99)},
                       "M. traza de HALT");
    }
}

static void test_P(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                   MonitorSalida& monitor_out, MonitorMem& monitor_mem)
{
    // HALT despues de un stall de load-use.
    monitor_mem.escrituras = 0;
    monitor_out.pulsos = 0;
    monitor_out.ultimo.clear();

    auto p = std::vector<sc_uint<16>>{
        formato_i(LDI, 4, 5),
        formato_i(ST, 4, 0x10),
        formato_i(LD, 1, 0x10),
        formato_r(CMP, 0, 1, 4),
        formato_i(HALT, 0, 0),
        formato_i(LDI, 3, 0x99),
        formato_i(ST, 1, 0x20),
        formato_i(OUT, 1, 0x30)
    };
    ejecutar(cpu, reset, "P. HALT despues de stall", p, 16);

    std::cout << "  R1=" << cpu.valor_registro(1).to_uint()
              << " R3=" << cpu.valor_registro(3).to_uint()
              << ", stalls=" << cpu.stalls_totales
              << ", halts=" << cpu.halts_detectados
              << ", flushes=" << cpu.flushes_totales << "\n";
    comprobar("P. un stall load-use", cpu.stalls_totales == 1, "");
    comprobar("P. sin branches", cpu.branches_tomados == 0, "");
    comprobar("P. un HALT y un flush",
              cpu.halts_detectados == 1 && cpu.flushes_totales == 1, "");
    comprobar("P. R1=5 y R3 no se escribe",
              cpu.valor_registro(1).to_uint() == 5 &&
              cpu.valor_registro(3).to_uint() == 0, "");
    comprobar("P. ST/OUT posteriores sin efecto",
              cpu.dmem.memoria[0x20].read().to_uint() == 0 &&
              monitor_mem.escrituras == 1 && monitor_out.pulsos == 0, "");
    comprobar("P. dos escrituras RF (R4, R1)", cpu.escrituras.size() == 2, "");
    comprobar("P. halted sticky", cpu.halted_reg.read(), "");
    comprobar_stall(cpu, "P. congelacion por load-use");
    comprobar_halt(cpu, {formato_i(LDI, 3, 0x99)}, "P. traza de HALT");
}

static void test_N(CPU_Goalkeeper& cpu, sc_signal<bool>& reset,
                   sc_signal<bool>& halted)
{
    auto p = std::vector<sc_uint<16>>{
        formato_i(LDI, 1, 1),
        formato_i(HALT, 0, 0),
        formato_i(LDI, 2, 2)
    };
    ejecutar(cpu, reset, "N. HALT antes de reset", p, 12);

    comprobar("N. halted antes de reset",
              cpu.halted_reg.read() && halted.read(), "");

    // Referencia acumulada de la primera ejecucion (no se reinicia).
    size_t ref_rf = cpu.escrituras.size();
    bool primera_ok = (ref_rf == 1) &&
                      (cpu.escrituras[0].rd.to_uint() == 1) &&
                      (cpu.escrituras[0].dato.to_uint() == 1);
    comprobar("N. primera ejecucion: una escritura R1=1", primera_ok, "");

    // Cargar el nuevo programa mientras el PC esta congelado (no en 0),
    // para que el reset a 0 fuerce la relectura combinacional de imem.
    std::vector<sc_uint<16>> p2;
    p2.push_back(formato_i(LDI, 1, 7));
    p2.push_back(formato_i(HALT, 0, 0));
    cpu.cargar_programa(p2);

    reset.write(true);
    sc_start(35, SC_NS);

    bool reset_ok = cpu.pc_addr.read().to_uint() == 0 &&
                    !cpu.halted_reg.read() &&
                    !cpu.halt_stop.read() &&
                    cpu.valor_registro(1).to_uint() == 0 &&
                    !cpu.ifid_valid.read();
    comprobar("N. reset limpia PC, HALT, RF y pipeline", reset_ok, "");

    reset.write(false);
    sc_start(150, SC_NS);

    std::cout << "  Tras reset+reinicio: R1="
              << cpu.valor_registro(1).to_uint()
              << ", halted=" << cpu.halted_reg.read() << "\n";
    comprobar("N. la CPU reinicia y ejecuta de nuevo",
              cpu.valor_registro(1).to_uint() == 7 &&
              cpu.halted_reg.read() && halted.read(), "");

    bool segunda_ok = (cpu.escrituras.size() == ref_rf + 1);
    if (segunda_ok)
        segunda_ok = (cpu.escrituras[ref_rf].rd.to_uint() == 1) &&
                     (cpu.escrituras[ref_rf].dato.to_uint() == 7);
    comprobar("N. segunda ejecucion: una escritura nueva R1=7",
              segunda_ok, "");
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

    test_A(cpu, reset, monitor_out, monitor_mem);
    test_A2(cpu, reset);
    test_condicionales(cpu, reset);
    test_FGH(cpu, reset, monitor_out, monitor_mem);
    test_J(cpu, reset);
    test_K(cpu, reset, monitor_out, monitor_mem);
    test_O(cpu, reset, monitor_out, monitor_mem);
    test_LM(cpu, reset, monitor_out, monitor_mem);
    test_P(cpu, reset, monitor_out, monitor_mem);
    test_N(cpu, reset, halted);

    std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
