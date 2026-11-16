#include <systemc.h>
#include <iostream>
#include "cpu_goalkeeper.h"
#include "communication_mailbox.h"
#include "arbitro.h"
#include "striker_sim.h"
#include "goalkeeper_program.h"

// ============================================================
// Prueba de integracion de PENALTY-8 (lado GOALKEEPER-8)
//   - CPU_Goalkeeper ejecuta el programa del portero.
//   - CommunicationMailbox implementa REQ/ACK y mide reaccion.
//   - StrikerSim actua como STRIKER-8 a nivel de protocolo.
//   - Arbitro decide SAVE / GOAL / MISS.
// ============================================================

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);

    sc_signal<bool> reset;

    // Bus de E/S de la CPU (lado GOALKEEPER)
    sc_signal<bool> cpu_io_write;
    sc_signal<bool> cpu_io_read;
    sc_signal<sc_uint<8>> cpu_io_addr;
    sc_signal<sc_uint<8>> cpu_io_wdata;
    sc_signal<sc_uint<8>> cpu_io_rdata;
    sc_signal<bool> cpu_halted;

    // Lado STRIKER del mailbox
    sc_signal<bool> st_we;
    sc_signal<sc_uint<8>> st_addr;
    sc_signal<sc_uint<8>> st_wdata;
    sc_signal<bool> st_re;
    sc_signal<sc_uint<8>> st_rdata;

    // Estado del mailbox
    sc_signal<bool> req;
    sc_signal<bool> ack;
    sc_signal<sc_uint<8>> reaction;

    // Arbitro
    sc_signal<sc_uint<8>> a_tx, a_ty, a_power, a_flight;
    sc_signal<sc_uint<8>> a_dx, a_dy, a_reaction, a_rx, a_ry;
    sc_signal<sc_uint<2>> a_res;

    // ---------------- Instancias ----------------
    CPU_Goalkeeper cpu("cpu");
    cpu.clk(clk);
    cpu.reset(reset);
    cpu.io_write(cpu_io_write);
    cpu.io_read(cpu_io_read);
    cpu.io_addr(cpu_io_addr);
    cpu.io_wdata(cpu_io_wdata);
    cpu.io_rdata(cpu_io_rdata);
    cpu.halted(cpu_halted);

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

    // Cargar el programa del portero
    cpu.cargar_programa(goalkeeper_program::construir());

    // ---------------- Reset inicial ----------------
    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);

    // ---------------- Simulacion ----------------
    sc_start(5000, SC_NS);

    // ---------------- Verificacion ----------------
    int fallos = 0;
    std::cout << "\n===== RESULTADOS DE INTEGRACION =====" << std::endl;

    const unsigned int esperado[4] = { ARB_SAVE, ARB_GOAL, ARB_MISS, ARB_SAVE };
    const char* nombres[4] = { "SAVE", "GOAL(tarde)", "MISS(fuera)", "SAVE" };

    if (striker.resultados.size() != 4)
    {
        std::cout << "[FALLO] Se esperaban 4 respuestas, se obtuvieron "
                  << striker.resultados.size() << std::endl;
        fallos++;
    }

    for (unsigned int i = 0; i < striker.resultados.size() && i < 4; i++)
    {
        const StrikerSim::Resultado& r = striker.resultados[i];

        bool id_ok = (r.resp_id == r.id);
        bool arb_ok = (r.arbitro == esperado[i]);

        std::cout << (id_ok && arb_ok ? "[OK]   " : "[FALLO] ")
                  << "Disparo " << r.id
                  << " esperado=" << nombres[i]
                  << " obtenido=" << r.arbitro
                  << " resp_id=" << (unsigned)r.resp_id
                  << " reaction=" << (unsigned)r.reaction
                  << std::endl;

        if (!id_ok || !arb_ok) fallos++;
    }

    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
