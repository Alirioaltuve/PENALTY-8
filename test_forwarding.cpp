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

// Captura las escrituras reales del puerto de salida de la CPU.
// Se muestrea en el flanco negativo para que la logica combinacional
// de E/S ya este estable.
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

static const char* nombre_op(unsigned int op)
{
    switch (op)
    {
        case NOP:  return "NOP";
        case LDI:  return "LDI";
        case MOV:  return "MOV";
        case ADD:  return "ADD";
        case SUB:  return "SUB";
        case AND:  return "AND";
        case OR:   return "OR";
        case CMP:  return "CMP";
        case JMP:  return "JMP";
        case JEQ:  return "JEQ";
        case JNE:  return "JNE";
        case JGE:  return "JGE";
        case JLT:  return "JLT";
        case JGT:  return "JGT";
        case JLE:  return "JLE";
        case LD:   return "LD";
        case ST:   return "ST";
        case IN:   return "IN";
        case OUT:  return "OUT";
        case HALT: return "HALT";
        default:   return "?";
    }
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
        case ADD:
        case SUB:
        case AND:
        case OR:
            os << nombre_op(op) << " R" << rd << ",R" << ra << ",R" << rb;
            return os.str();
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

static void imprimir_cabecera(const std::string& nombre,
                              const std::vector<sc_uint<16>>& prog)
{
    std::cout << "\n================ " << nombre << " ================\n";
    for (unsigned int i = 0; i < prog.size(); i++)
        std::cout << "  [" << i << "] " << desc(prog[i]) << "\n";
}

static void imprimir_tabla(const CPU_Goalkeeper& cpu)
{
    std::cout << "Ciclo | PC | IF | ID | EX | MEM | WB\n";
    std::cout << "------+----+----+----+----+-----+----\n";
    for (const auto& c : cpu.traza)
    {
        std::cout << std::setw(5) << c.ciclo << " | "
                  << std::setw(2) << c.pc.to_uint() << " | "
                  << std::setw(16) << desc(c.if_instr) << " | "
                  << std::setw(16) << etapa(c.ifid_valid, c.ifid_instr) << " | "
                  << std::setw(16) << etapa(c.idex_valid, c.idex_instr) << " | "
                  << std::setw(16) << etapa(c.exmem_valid, c.exmem_instr) << " | "
                  << std::setw(16) << etapa(c.memwb_valid, c.memwb_instr) << "\n";
    }
}

static void imprimir_forwarding(const CPU_Goalkeeper& cpu)
{
    std::cout << "Forwarding de operandos observado en EX:\n";
    bool algo = false;
    for (const auto& c : cpu.traza)
    {
        if (c.fwd_a_sel.to_uint() != CPU_Goalkeeper::FWD_NINGUNO ||
            c.fwd_b_sel.to_uint() != CPU_Goalkeeper::FWD_NINGUNO)
        {
            algo = true;
            std::cout << "  Ciclo " << c.ciclo << ": EX " << desc(c.idex_instr)
                      << " | A: rs1=R" << c.idex_rs1
                      << " usa=" << c.idex_usa_rs1
                      << " orig=" << c.idex_a_orig.to_uint()
                      << " fwd=" << c.ex_a.to_uint()
                      << " [" << fwd_nombre(c.fwd_a_sel.to_uint()) << "]"
                      << " | B: rs2=R" << c.idex_rs2
                      << " usa=" << c.idex_usa_rs2
                      << " orig=" << c.idex_b_orig.to_uint()
                      << " fwd=" << c.ex_b.to_uint()
                      << " [" << fwd_nombre(c.fwd_b_sel.to_uint()) << "]\n";
        }
    }
    if (!algo) std::cout << "  (ninguno)\n";

    std::cout << "Forwarding de banderas observado en ID:\n";
    bool algaf = false;
    for (const auto& c : cpu.traza)
    {
        if (c.flag_src.to_uint() != CPU_Goalkeeper::FLAG_ARQ)
        {
            algaf = true;
            std::cout << "  Ciclo " << c.ciclo << ": ID "
                      << etapa(c.ifid_valid, c.ifid_instr)
                      << " C=" << c.flag_c_fwd << " Z=" << c.flag_z_fwd
                      << " [" << flag_nombre(c.flag_src.to_uint()) << "]"
                      << " salto=" << c.ctrl_salto << "\n";
        }
    }
    if (!algaf) std::cout << "  (ninguno)\n";
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

    imprimir_cabecera(nombre, prog);
    imprimir_tabla(cpu);
    imprimir_forwarding(cpu);
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

    MonitorSalida monitor("monitor");
    monitor.clk(clk);
    monitor.io_write(io_write);
    monitor.io_addr(io_addr);
    monitor.io_wdata(io_wdata);

    io_rdata.write(0);

    // =====================================================
    // A. EX/MEM -> EX
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 20));
        p.push_back(formato_r(ADD, 2, 1, 1));
        ejecutar(cpu, reset, "A. EX/MEM -> EX", p, 8);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 40)\n";
        comprobar("A. ADD recibe 20 en ambos operandos", r2 == 40, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->ex_a.to_uint() == 20 && f->ex_b.to_uint() == 20;
        comprobar("A. coincidencia detectada en EX/MEM", ev, "");
    }

    // =====================================================
    // B. MEM/WB -> EX
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 7));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_r(ADD, 2, 1, 1));
        ejecutar(cpu, reset, "B. MEM/WB -> EX", p, 8);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 14)\n";
        comprobar("B. ADD recibe 7 en ambos operandos", r2 == 14, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 7 && f->ex_b.to_uint() == 7;
        comprobar("B. coincidencia detectada en MEM/WB", ev, "");
    }

    // =====================================================
    // C. Prioridad EX/MEM sobre MEM/WB
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 1));
        p.push_back(formato_i(LDI, 1, 2));
        p.push_back(formato_r(ADD, 2, 1, 1));
        ejecutar(cpu, reset, "C. Prioridad EX/MEM sobre MEM/WB", p, 8);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 4)\n";
        comprobar("C. se usa el valor mas reciente (2)", r2 == 4, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->ex_a.to_uint() == 2 && f->ex_b.to_uint() == 2;
        comprobar("C. fuente EX/MEM tiene prioridad", ev, "");
    }

    // =====================================================
    // D. Forwarding de R0
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 20));
        p.push_back(formato_r(ADD, 1, 0, 0));
        ejecutar(cpu, reset, "D. Forwarding de R0", p, 8);

        unsigned int r1 = cpu.valor_registro(1).to_uint();
        std::cout << "Resultado: R1=" << r1 << " (esperado 40)\n";
        comprobar("D. R0 se forwardea como registro general", r1 == 40, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->ex_a.to_uint() == 20 && f->ex_b.to_uint() == 20;
        comprobar("D. R0 productor y consumidor de forwarding", ev, "");
    }

    // =====================================================
    // E. Ambos operandos con fuentes distintas
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 3));
        p.push_back(formato_i(LDI, 2, 4));
        p.push_back(formato_r(ADD, 3, 1, 2));
        ejecutar(cpu, reset, "E. Forwarding de ambos operandos", p, 8);

        unsigned int r3 = cpu.valor_registro(3).to_uint();
        std::cout << "Resultado: R3=" << r3 << " (esperado 7)\n";
        comprobar("E. ADD recibe 3 y 4", r3 == 7, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                  f->ex_a.to_uint() == 3 &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->ex_b.to_uint() == 4;
        comprobar("E. A desde MEM/WB y B desde EX/MEM", ev, "");
    }

    // =====================================================
    // F1. Banderas desde EX
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 5));
        p.push_back(formato_i(LDI, 2, 5));
        p.push_back(formato_r(CMP, 1, 1, 2));
        p.push_back(formato_j(JEQ, 0));
        ejecutar(cpu, reset, "F1. Banderas desde EX", p, 8);

        const auto* f = fila_id(cpu, JEQ);
        bool ev = f &&
                  f->flag_src.to_uint() == CPU_Goalkeeper::FLAG_EX &&
                  f->flag_c_fwd && f->flag_z_fwd && f->ctrl_salto;
        comprobar("F1. JEQ ve C/Z del CMP en EX", ev, "");
        comprobar("F1. banderas arquitectonicas actualizadas",
                  cpu.flag_c_arch.read() && cpu.flag_z_arch.read(), "");
    }

    // =====================================================
    // F2. Banderas desde EX/MEM
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 5));
        p.push_back(formato_i(LDI, 2, 5));
        p.push_back(formato_r(CMP, 1, 1, 2));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_j(JEQ, 0));
        ejecutar(cpu, reset, "F2. Banderas desde EX/MEM", p, 9);

        const auto* f = fila_id(cpu, JEQ);
        bool ev = f &&
                  f->flag_src.to_uint() == CPU_Goalkeeper::FLAG_EXMEM &&
                  f->flag_c_fwd && f->flag_z_fwd && f->ctrl_salto;
        comprobar("F2. JEQ ve C/Z del CMP en EX/MEM", ev, "");
    }

    // =====================================================
    // F3. Banderas desde MEM/WB
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 5));
        p.push_back(formato_i(LDI, 2, 5));
        p.push_back(formato_r(CMP, 1, 1, 2));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_j(JEQ, 0));
        ejecutar(cpu, reset, "F3. Banderas desde MEM/WB", p, 10);

        const auto* f = fila_id(cpu, JEQ);
        bool ev = f &&
                  f->flag_src.to_uint() == CPU_Goalkeeper::FLAG_MEMWB &&
                  f->flag_c_fwd && f->flag_z_fwd && f->ctrl_salto;
        comprobar("F3. JEQ ve C/Z del CMP en MEM/WB", ev, "");
    }

    // =====================================================
    // G. CMP no escribe registro y si actualiza banderas
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 3));
        p.push_back(formato_r(CMP, 1, 1, 1));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_i(NOP, 0, 0));
        ejecutar(cpu, reset, "G. CMP", p, 9);

        bool solo_ldi = cpu.escrituras.size() == 1 &&
                        cpu.escrituras[0].rd.to_uint() == 1 &&
                        cpu.escrituras[0].dato.to_uint() == 3;
        std::cout << "Escrituras observadas: " << cpu.escrituras.size() << "\n";
        comprobar("G. CMP no escribe registro general", solo_ldi, "");
        comprobar("G. CMP actualiza C/Z (3-3=0)",
                  cpu.flag_c_arch.read() && cpu.flag_z_arch.read(), "");
    }

    // =====================================================
    // H. ST: forwarding del dato almacenado
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 42));
        p.push_back(formato_i(ST, 1, 0x10));
        p.push_back(formato_i(LD, 2, 0x10));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_i(NOP, 0, 0));
        ejecutar(cpu, reset, "H. ST: forwarding del dato", p, 10);

        const auto* f = fila_ex(cpu, ST);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->ex_a.to_uint() == 42 &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_NINGUNO;
        comprobar("H. ST recibe 42 por forwarding (solo A)", ev, "");

        unsigned int mem = cpu.dmem.memoria[0x10].read().to_uint();
        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "mem[0x10]=" << mem << " (esperado 42), R2=" << r2
                  << " (esperado 42)\n";
        comprobar("H. ST almacena el dato forwardeado", mem == 42, "");
        comprobar("H. LD recupera el dato", r2 == 42, "");
    }

    // =====================================================
    // I. Dependencia a distancia 3 (bypass WB -> ID)
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 10));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_r(ADD, 2, 1, 1));
        ejecutar(cpu, reset, "I. Distancia 3: bypass WB -> ID", p, 9);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 20)\n";
        comprobar("I. consumidor a distancia 3 recibe el valor", r2 == 20, "");

        const auto* f = fila_ex(cpu, ADD);
        if (f)
        {
            std::cout << "  Nota: en EX no hay productor (fuente="
                      << fwd_nombre(f->fwd_a_sel.to_uint())
                      << "), el valor " << f->ex_a.to_uint()
                      << " llego por bypass WB->ID al capturar ID/EX.\n";
        }
        bool ev = f && f->ex_a.to_uint() == 10;
        comprobar("I. valor presente en el operando de EX", ev, "");
    }

    // =====================================================
    // J. MOV: forwarding de la fuente RA
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 20));
        p.push_back(formato_r(MOV, 2, 1, 0));
        ejecutar(cpu, reset, "J. MOV: forwarding de la fuente RA", p, 8);

        unsigned int r2 = cpu.valor_registro(2).to_uint();
        std::cout << "Resultado: R2=" << r2 << " (esperado 20)\n";
        comprobar("J. MOV escribe el valor forwardeado", r2 == 20, "");

        const auto* f = fila_ex(cpu, MOV);
        bool ev = f &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                  f->ex_a.to_uint() == 20 &&
                  f->fwd_b_sel.to_uint() == CPU_Goalkeeper::FWD_NINGUNO;
        comprobar("J. MOV recibe RA desde EX/MEM y no usa RB", ev, "");
    }

    // =====================================================
    // K. OUT: forwarding del dato de salida
    // =====================================================
    {
        monitor.escrituras.clear();
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 42));
        p.push_back(formato_i(OUT, 1, 0x40));
        ejecutar(cpu, reset, "K. OUT: forwarding del dato de salida", p, 8);

        const auto* f = fila_ex(cpu, OUT);
        bool ev_ex = f &&
                     f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_EXMEM &&
                     f->ex_a.to_uint() == 42;
        comprobar("K. OUT recibe 42 por forwarding", ev_ex, "");

        unsigned int puerto = monitor.escrituras.count(0x40) ?
                              monitor.escrituras[0x40] : -1;
        std::cout << "Puerto 0x40 = " << puerto << " (esperado 42)\n";
        comprobar("K. puerto de salida recibe 42", puerto == 42, "");
    }

    // =====================================================
    // L. Dos productores consecutivos de banderas
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 1, 5));
        p.push_back(formato_i(LDI, 2, 3));
        p.push_back(formato_r(CMP, 0, 1, 1)); // 5-5: C=1, Z=1
        p.push_back(formato_r(CMP, 0, 2, 1)); // 3-5: C=0, Z=0
        p.push_back(formato_j(JGE, 0));
        ejecutar(cpu, reset, "L. Dos productores consecutivos de banderas", p, 9);

        const auto* f = fila_id(cpu, JGE);
        bool ev = f &&
                  f->flag_src.to_uint() == CPU_Goalkeeper::FLAG_EX &&
                  !f->flag_c_fwd && !f->flag_z_fwd && !f->ctrl_salto;
        comprobar("L. consumidor ve banderas del productor mas reciente", ev, "");
        comprobar("L. banderas arquitectonicas finales del ultimo CMP",
                  !cpu.flag_c_arch.read() && !cpu.flag_z_arch.read(), "");
    }

    // =====================================================
    // M. WB -> ID con R0
    // =====================================================
    {
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 7));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_i(NOP, 0, 0));
        p.push_back(formato_r(ADD, 1, 0, 0));
        ejecutar(cpu, reset, "M. WB -> ID con R0", p, 9);

        unsigned int r0 = cpu.valor_registro(0).to_uint();
        unsigned int r1 = cpu.valor_registro(1).to_uint();
        std::cout << "R0=" << r0 << " (esperado 7), R1=" << r1
                  << " (esperado 14)\n";
        comprobar("M. R0 productor se escribe", r0 == 7, "");
        comprobar("M. consumidor recibe R0 por bypass WB -> ID", r1 == 14, "");

        const auto* f = fila_ex(cpu, ADD);
        bool ev = f && f->ex_a.to_uint() == 7 &&
                  f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_NINGUNO;
        comprobar("M. R0 no se excluye del bypass", ev, "");
    }

    // =====================================================
    // N. WB_MEM: el stall evita el valor antiguo; el LD llega
    //    a MEM/WB y se forwardea el dato correcto.
    // =====================================================
    {
        monitor.escrituras.clear();
        std::vector<sc_uint<16>> p;
        p.push_back(formato_i(LDI, 0, 0x11));
        p.push_back(formato_i(LDI, 2, 0x55));
        p.push_back(formato_i(ST, 2, 0x10));   // mem[0x10] = 0x55
        p.push_back(formato_i(LDI, 0, 0xAA));  // productor antiguo
        p.push_back(formato_i(LD, 0, 0x10));   // productor mas reciente, WB_MEM
        p.push_back(formato_i(OUT, 0, 0x40));  // consumidor inmediato (load-use)
        ejecutar(cpu, reset, "N. WB_MEM: stall + MEM/WB usan el dato del LD", p, 11);

        const auto* f = fila_ex(cpu, OUT);
        if (f)
        {
            std::cout << "  Ciclo " << f->ciclo << ": EX OUT R0,0x40 | "
                      << "ID/EX=" << f->idex_a_orig.to_uint()
                      << " fwd=" << f->ex_a.to_uint()
                      << " fuente=" << fwd_nombre(f->fwd_a_sel.to_uint())
                      << " (tras stall, LD en MEM/WB)\n";
        }
        bool ev_block = f &&
                        f->fwd_a_sel.to_uint() == CPU_Goalkeeper::FWD_MEMWB &&
                        f->ex_a.to_uint() == 0x55 &&
                        f->ex_a.to_uint() != 0xAA;
        comprobar("N. el stall entrega el dato del LD desde MEM/WB", ev_block, "");
        comprobar("N. se inserta exactamente un stall",
                  cpu.stalls_totales == 1, "");

        unsigned int puerto = monitor.escrituras.count(0x40) ?
                              monitor.escrituras[0x40] : -1;
        std::cout << "Puerto 0x40 = " << puerto
                  << " (esperado 0x55; NO 0xAA)\n";
        comprobar("N. no se usa el valor antiguo 0xAA de MEM/WB",
                  puerto == 0x55 && puerto != 0xAA, "");
        comprobar("N. mem[0x10] contiene el dato del ST",
                  cpu.dmem.memoria[0x10].read().to_uint() == 0x55, "");
    }

    std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
