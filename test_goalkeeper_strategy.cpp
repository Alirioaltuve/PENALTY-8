#include <systemc.h>
#include <vector>
#include <iostream>
#include "cpu_goalkeeper.h"
#include "communication_mailbox.h"
#include "arbitro.h"
#include "striker_sim.h"
#include "goalkeeper_program.h"

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const char* detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (detalle && detalle[0]) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

static unsigned int absdif(unsigned int a, unsigned int b)
{
    return (a > b) ? (a - b) : (b - a);
}

static unsigned int arbitro_esperado(const StrikerSim::Disparo& d,
                                     const StrikerSim::Resultado& r)
{
    if (d.tx < 32 || d.tx > 223 || d.ty < 32 || d.ty > 223 || d.power == 0)
        return ARB_MISS;

    bool tiempo = (r.reaction <= d.flight);
    bool x = (absdif(d.tx, r.dive_x) <= r.reach_x);
    bool y = (absdif(d.ty, r.dive_y) <= r.reach_y);

    return (tiempo && x && y) ? ARB_SAVE : ARB_GOAL;
}

struct Esperado
{
    unsigned int dive_x;
    unsigned int dive_y;
    unsigned int reach;
};

static Esperado estrategia(const StrikerSim::Disparo& d)
{
    Esperado e;
    e.dive_x = (d.id & 1) ? (d.tx + 16) : (d.tx - 16);
    e.dive_y = (d.flight < 128) ? (d.ty + 16) : d.ty;
    e.reach  = 16 + ((d.power >= 128) ? 8 : 0) - ((d.flight < 128) ? 8 : 0);
    return e;
}

// Observa, en el ciclo en que el CPU ejecuta OUT, el registro fuente real
// que alimenta io_wdata. No calcula la estrategia: solo registra el valor
// arquitectonico leido del banco de registros y el operando usado por OUT.
struct MuestraRegistro
{
    unsigned int addr;
    unsigned int dato_out;
    unsigned int rd;
    unsigned int valor_registro;
    unsigned int operando_ex;
    unsigned int escrituras_antes;
};

SC_MODULE(MonitorRegistros)
{
    sc_in<bool> clk;
    sc_in<bool> io_write;
    sc_in<sc_uint<8>> io_addr;
    sc_in<sc_uint<8>> io_wdata;

    CPU_Goalkeeper* cpu;
    std::vector<MuestraRegistro> muestras;

    void capturar()
    {
        if (!io_write.read() || cpu == nullptr)
            return;

        unsigned int a = io_addr.read().to_uint();
        if (a != goalkeeper_program::IO_OUT_ID &&
            a != goalkeeper_program::IO_DIVE_X &&
            a != goalkeeper_program::IO_DIVE_Y &&
            a != goalkeeper_program::IO_REACH_X &&
            a != goalkeeper_program::IO_REACH_Y)
            return;

        // En clk.neg() observamos la instruccion que esta en EX en este
        // ciclo y sus senales reales, sin depender de una copia del test.
        if (!cpu->idex_valid.read())
            return;

        sc_uint<16> instr = cpu->idex_instr.read();
        if (instr.range(15, 11).to_uint() != isa::OUT)
            return;

        MuestraRegistro m;
        m.addr = a;
        m.dato_out = io_wdata.read().to_uint();
        m.rd = instr.range(10, 8).to_uint();
        m.valor_registro = cpu->valor_registro(m.rd).to_uint();
        m.operando_ex = cpu->ex_a.read().to_uint();
        m.escrituras_antes = (unsigned int)cpu->escrituras.size();
        muestras.push_back(m);
    }

    SC_CTOR(MonitorRegistros)
        : cpu(nullptr)
    {
        SC_METHOD(capturar);
        sensitive << clk.neg();
        dont_initialize();
    }
};

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> reset;

    sc_signal<bool> cpu_io_write;
    sc_signal<bool> cpu_io_read;
    sc_signal<sc_uint<8>> cpu_io_addr;
    sc_signal<sc_uint<8>> cpu_io_wdata;
    sc_signal<sc_uint<8>> cpu_io_rdata;
    sc_signal<bool> cpu_halted;

    sc_signal<bool> st_we;
    sc_signal<sc_uint<8>> st_addr;
    sc_signal<sc_uint<8>> st_wdata;
    sc_signal<bool> st_re;
    sc_signal<sc_uint<8>> st_rdata;

    sc_signal<bool> req;
    sc_signal<bool> ack;
    sc_signal<sc_uint<8>> reaction;

    sc_signal<sc_uint<8>> a_tx, a_ty, a_power, a_flight;
    sc_signal<sc_uint<8>> a_dx, a_dy, a_reaction, a_rx, a_ry;
    sc_signal<sc_uint<2>> a_res;

    CPU_Goalkeeper cpu("cpu");
    cpu.clk(clk);
    cpu.reset(reset);
    cpu.io_write(cpu_io_write);
    cpu.io_read(cpu_io_read);
    cpu.io_addr(cpu_io_addr);
    cpu.io_wdata(cpu_io_wdata);
    cpu.io_rdata(cpu_io_rdata);
    cpu.halted(cpu_halted);

    MonitorRegistros monitor("monitor");
    monitor.clk(clk);
    monitor.io_write(cpu_io_write);
    monitor.io_addr(cpu_io_addr);
    monitor.io_wdata(cpu_io_wdata);
    monitor.cpu = &cpu;

    CommunicationMailbox mailbox("mailbox");
    mailbox.clk(clk);
    mailbox.reset(reset);
    mailbox.st_we(st_we);
    mailbox.st_addr(st_addr);
    mailbox.st_wdata(st_wdata);
    mailbox.st_re(st_re);
    mailbox.st_rdata(st_rdata);
    mailbox.gk_we(cpu_io_write);
    mailbox.gk_addr(cpu_io_addr);
    mailbox.gk_wdata(cpu_io_wdata);
    mailbox.gk_re(cpu_io_read);
    mailbox.gk_rdata(cpu_io_rdata);
    mailbox.req(req);
    mailbox.ack(ack);
    mailbox.reaction_ticks(reaction);

    Arbitro arbitro("arbitro");
    arbitro.target_x(a_tx);
    arbitro.target_y(a_ty);
    arbitro.power(a_power);
    arbitro.flight_ticks(a_flight);
    arbitro.dive_x(a_dx);
    arbitro.dive_y(a_dy);
    arbitro.reaction_ticks(a_reaction);
    arbitro.reach_x(a_rx);
    arbitro.reach_y(a_ry);
    arbitro.resultado(a_res);

    StrikerSim striker("striker");
    striker.clk(clk);
    striker.st_we(st_we);
    striker.st_addr(st_addr);
    striker.st_wdata(st_wdata);
    striker.st_re(st_re);
    striker.st_rdata(st_rdata);
    striker.ack(ack);
    striker.o_target_x(a_tx);
    striker.o_target_y(a_ty);
    striker.o_power(a_power);
    striker.o_flight(a_flight);
    striker.o_dive_x(a_dx);
    striker.o_dive_y(a_dy);
    striker.o_reaction(a_reaction);
    striker.o_reach_x(a_rx);
    striker.o_reach_y(a_ry);
    striker.arbitro_res(a_res);

    // Escenarios controlados (el constructor de StrikerSim ya puso 4).
    striker.disparos.clear();
    striker.disparos.push_back({1, 128, 128, 50, 200});  // base: SAVE
    striker.disparos.push_back({3, 100, 128, 50, 200});  // target distinto
    striker.disparos.push_back({1, 128, 128, 150, 200}); // power distinto
    striker.disparos.push_back({1, 128, 128, 50, 100});  // flight distinto
    striker.disparos.push_back({2, 180, 100, 50, 200});  // otro target/paridad

    // Fronteras exactas de power y flight_ticks.
    striker.disparos.push_back({1, 128, 128, 127, 200}); // power < 128
    striker.disparos.push_back({2, 100, 120, 128, 200}); // power = 128
    striker.disparos.push_back({3, 140, 130, 129, 200}); // power > 128
    striker.disparos.push_back({4, 160, 140, 50, 127});  // flight < 128
    striker.disparos.push_back({5, 180, 100, 50, 128});  // flight = 128
    striker.disparos.push_back({6, 200, 60, 50, 129});   // flight > 128

    cpu.cargar_programa(goalkeeper_program::construir());

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);
    sc_start(20000, SC_NS);

    std::cout << "\n===== RESULTADOS ESTRATEGIA =====" << std::endl;

    if (striker.resultados.size() != striker.disparos.size())
    {
        std::cout << "[FALLO] resultados=" << striker.resultados.size()
                  << " esperados=" << striker.disparos.size() << std::endl;
        fallos++;
    }

    for (unsigned int i = 0; i < striker.resultados.size(); i++)
    {
        const auto& d = striker.disparos[i];
        const auto& r = striker.resultados[i];
        Esperado e = estrategia(d);

        std::cout << "  id=" << d.id
                  << " req=(" << d.tx << "," << d.ty << ",pw=" << d.power
                  << ",ft=" << d.flight << ")"
                  << " resp=(" << r.dive_x << "," << r.dive_y
                  << ",rx=" << r.reach_x << ",ry=" << r.reach_y
                  << ",rt=" << r.reaction << ")"
                  << " arb=" << r.arbitro << "\n";

        comprobar("resp_id eco", r.resp_id == d.id, "");
        comprobar("dive_x calculado por la CPU", r.dive_x == e.dive_x, "");
        comprobar("dive_y calculado por la CPU", r.dive_y == e.dive_y, "");
        comprobar("reach_x calculado por la CPU", r.reach_x == e.reach, "");
        comprobar("reach_y calculado por la CPU", r.reach_y == e.reach, "");
        comprobar("reaction_ticks medido por el Mailbox", r.reaction > 0, "");
        comprobar("arbitraje coherente con solicitud+respuesta",
                  r.arbitro == arbitro_esperado(d, r), "");
    }

    // 1. Target distinto -> dive_x distinto.
    if (striker.resultados.size() >= 2)
        comprobar("target distinto produce dive_x distinto",
                  striker.resultados[0].dive_x != striker.resultados[1].dive_x, "");

    // 2. Power distinto -> reach distinto.
    if (striker.resultados.size() >= 3)
        comprobar("power distinto produce reach distinto",
                  striker.resultados[0].reach_x != striker.resultados[2].reach_x, "");

    // 3. Flight distinto -> dive_y/reach/arbitraje distintos.
    if (striker.resultados.size() >= 4)
    {
        comprobar("flight distinto produce dive_y distinto",
                  striker.resultados[0].dive_y != striker.resultados[3].dive_y, "");
        comprobar("flight distinto produce reach distinto",
                  striker.resultados[0].reach_x != striker.resultados[3].reach_x, "");
        comprobar("flight distinto puede cambiar el arbitraje",
                  striker.resultados[0].arbitro != striker.resultados[3].arbitro, "");
    }

    // 4. Fronteras exactas de power y flight_ticks.
    struct Frontera
    {
        unsigned int indice;
        unsigned int dive_x;
        unsigned int dive_y;
        unsigned int reach;
        const char* nombre;
    };

    const Frontera fronteras[] = {
        {5, 144, 128, 16, "power=127: dive_x/dive_y/reach exactos"},
        {6,  84, 120, 24, "power=128: dive_x/dive_y/reach exactos"},
        {7, 156, 130, 24, "power=129: dive_x/dive_y/reach exactos"},
        {8, 144, 156,  8, "flight=127: dive_x/dive_y/reach exactos"},
        {9, 196, 100, 16, "flight=128: dive_x/dive_y/reach exactos"},
        {10,184,  60, 16, "flight=129: dive_x/dive_y/reach exactos"}
    };

    for (const Frontera& f : fronteras)
    {
        bool ok = f.indice < striker.resultados.size();
        if (ok)
        {
            const auto& r = striker.resultados[f.indice];
            ok = r.dive_x == f.dive_x && r.dive_y == f.dive_y &&
                 r.reach_x == f.reach && r.reach_y == f.reach;
        }
        comprobar(f.nombre, ok, "");
    }

    // 5. Los valores de OUT deben salir del banco de registros real.
    const unsigned int muestras_por_solicitud = 5;
    if (monitor.muestras.size() !=
        striker.resultados.size() * muestras_por_solicitud)
    {
        std::cout << "[FALLO] muestras de registros=" << monitor.muestras.size()
                  << " esperadas="
                  << striker.resultados.size() * muestras_por_solicitud
                  << std::endl;
        fallos++;
    }

    for (unsigned int i = 0; i < striker.resultados.size(); i++)
    {
        const auto& r = striker.resultados[i];
        const unsigned int base = i * muestras_por_solicitud;
        if (base + muestras_por_solicitud > monitor.muestras.size())
            break;

        const unsigned int addrs[5] = {0x24, 0x25, 0x20, 0x21, 0x22};
        const unsigned int rds[5]   = {0, 0, 0, 5, 6};
        const unsigned int vals[5]  = {r.reach_x, r.reach_y, r.id,
                                       r.dive_x, r.dive_y};

        bool ok_solicitud = true;
        for (unsigned int k = 0; k < 5; k++)
        {
            const MuestraRegistro& m = monitor.muestras[base + k];
            bool escrito = false;
            for (unsigned int e = m.escrituras_antes;
                 e < cpu.escrituras.size(); e++)
            {
                if (cpu.escrituras[e].rd == m.rd &&
                    cpu.escrituras[e].dato.to_uint() == m.dato_out)
                {
                    escrito = true;
                    break;
                }
            }

            bool ok = m.addr == addrs[k] && m.rd == rds[k] &&
                      m.dato_out == vals[k] &&
                      m.operando_ex == m.dato_out &&
                      (m.valor_registro == m.dato_out || escrito);
            if (!ok)
            {
                ok_solicitud = false;
                std::cout << "  muestra i=" << i << " k=" << k
                          << " addr=0x" << std::hex << m.addr << std::dec
                          << " rd=" << m.rd
                          << " out=" << m.dato_out
                          << " reg=" << m.valor_registro
                          << " ex=" << m.operando_ex
                          << " escrito=" << escrito
                          << " esperado=" << vals[k] << std::endl;
            }
        }
        comprobar("OUT de respuesta coincide con registros reales",
                  ok_solicitud, "");
    }

    comprobar("todas las solicitudes consecutivas completas",
              striker.resultados.size() == striker.disparos.size(), "");

    std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
