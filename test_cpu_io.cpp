#include <systemc.h>
#include <iostream>
#include <vector>
#include "cpu_goalkeeper.h"
#include "communication_mailbox.h"
#include "isa.h"

using namespace isa;

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const char* detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (detalle && detalle[0]) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

// Carga los cinco campos de solicitud en el Mailbox usando su bus STRIKER.
SC_MODULE(StrikerLoader)
{
    sc_in<bool> clk;
    sc_out<bool> st_we;
    sc_out<sc_uint<8>> st_addr;
    sc_out<sc_uint<8>> st_wdata;
    sc_out<bool> listo;

    void escribir(unsigned int a, unsigned int v)
    {
        wait(clk.negedge_event());
        st_addr.write(a);
        st_wdata.write(v);
        st_we.write(true);
        wait(clk.posedge_event());
        wait(clk.negedge_event());
        st_we.write(false);
    }

    void cargar()
    {
        st_we.write(false);
        listo.write(false);
        wait(50, SC_NS);

        escribir(0x10, 0x11);
        escribir(0x11, 0x22);
        escribir(0x12, 0x33);
        escribir(0x13, 0x44);
        escribir(0x14, 0x55);

        listo.write(true);
    }

    SC_CTOR(StrikerLoader)
    {
        SC_THREAD(cargar);
    }
};

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);

    sc_signal<bool> cpu_reset;
    sc_signal<bool> mb_reset;

    // Bus CPU - Mailbox
    sc_signal<bool> cpu_io_write;
    sc_signal<bool> cpu_io_read;
    sc_signal<sc_uint<8>> cpu_io_addr;
    sc_signal<sc_uint<8>> cpu_io_wdata;
    sc_signal<sc_uint<8>> cpu_io_rdata;
    sc_signal<bool> cpu_halted;

    // Bus STRIKER - Mailbox
    sc_signal<bool> st_we;
    sc_signal<sc_uint<8>> st_addr;
    sc_signal<sc_uint<8>> st_wdata;
    sc_signal<bool> st_re;
    sc_signal<sc_uint<8>> st_rdata;
    sc_signal<bool> listo;

    sc_signal<bool> req;
    sc_signal<bool> ack;
    sc_signal<sc_uint<8>> reaction;

    CPU_Goalkeeper cpu("cpu");
    cpu.clk(clk);
    cpu.reset(cpu_reset);
    cpu.io_write(cpu_io_write);
    cpu.io_read(cpu_io_read);
    cpu.io_addr(cpu_io_addr);
    cpu.io_wdata(cpu_io_wdata);
    cpu.io_rdata(cpu_io_rdata);
    cpu.halted(cpu_halted);

    CommunicationMailbox mb("mb");
    mb.clk(clk);
    mb.reset(mb_reset);
    mb.st_we(st_we);
    mb.st_addr(st_addr);
    mb.st_wdata(st_wdata);
    mb.st_re(st_re);
    mb.st_rdata(st_rdata);
    mb.gk_we(cpu_io_write);
    mb.gk_addr(cpu_io_addr);
    mb.gk_wdata(cpu_io_wdata);
    mb.gk_re(cpu_io_read);
    mb.gk_rdata(cpu_io_rdata);
    mb.req(req);
    mb.ack(ack);
    mb.reaction_ticks(reaction);

    StrikerLoader loader("loader");
    loader.clk(clk);
    loader.st_we(st_we);
    loader.st_addr(st_addr);
    loader.st_wdata(st_wdata);
    loader.listo(listo);

    // B: leer los cinco campos de solicitud mediante IN.
    std::vector<sc_uint<16>> prog;
    prog.push_back(formato_i(IN, 0, 0x10));
    prog.push_back(formato_i(IN, 1, 0x11));
    prog.push_back(formato_i(IN, 2, 0x12));
    prog.push_back(formato_i(IN, 3, 0x13));
    prog.push_back(formato_i(IN, 4, 0x14));
    prog.push_back(formato_i(HALT, 0, 0));
    cpu.cargar_programa(prog);

    cpu_reset.write(true);
    mb_reset.write(true);
    sc_start(35, SC_NS);
    mb_reset.write(false);

    int guarda = 0;
    while (!listo.read() && guarda < 1000)
    {
        sc_start(1, SC_NS);
        guarda++;
    }

    comprobar("B. campos cargados en el Mailbox", listo.read(), "");

    cpu_reset.write(false);
    sc_start(300, SC_NS);

    unsigned int r0 = cpu.valor_registro(0).to_uint();
    unsigned int r1 = cpu.valor_registro(1).to_uint();
    unsigned int r2 = cpu.valor_registro(2).to_uint();
    unsigned int r3 = cpu.valor_registro(3).to_uint();
    unsigned int r4 = cpu.valor_registro(4).to_uint();

    std::cout << "  R0..R4 = " << r0 << "," << r1 << "," << r2
              << "," << r3 << "," << r4
              << ", halted=" << cpu_halted.read() << "\n";

    comprobar("B. IN lee shot_id por 0x10", r0 == 0x11, "");
    comprobar("B. IN lee target_x por 0x11", r1 == 0x22, "");
    comprobar("B. IN lee target_y por 0x12", r2 == 0x33, "");
    comprobar("B. IN lee power por 0x13", r3 == 0x44, "");
    comprobar("B. IN lee flight_ticks por 0x14", r4 == 0x55, "");
    comprobar("B. programa terminado con HALT", cpu_halted.read(), "");

    std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}
