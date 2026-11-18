#include <systemc.h>
#include <iostream>
#include "communication_mailbox.h"

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const char* detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (detalle && detalle[0]) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

// Driver de protocolo del lado STRIKER y del lado GOALKEEPER.
SC_MODULE(ProtocolDriver)
{
    sc_in<bool> clk;
    sc_out<bool> reset;

    sc_out<bool> st_we;
    sc_out<sc_uint<8>> st_addr;
    sc_out<sc_uint<8>> st_wdata;
    sc_out<bool> st_re;
    sc_in<sc_uint<8>> st_rdata;

    sc_out<bool> gk_we;
    sc_out<sc_uint<8>> gk_addr;
    sc_out<sc_uint<8>> gk_wdata;
    sc_out<bool> gk_re;
    sc_in<sc_uint<8>> gk_rdata;

    sc_in<bool> req;
    sc_in<bool> ack;
    sc_in<sc_uint<8>> reaction;

    void st_write(unsigned int a, unsigned int v)
    {
        wait(clk.negedge_event());
        st_addr.write(a);
        st_wdata.write(v);
        st_we.write(true);
        wait(clk.posedge_event());
        wait(clk.negedge_event());
        st_we.write(false);
    }

    unsigned int st_read(unsigned int a)
    {
        wait(clk.negedge_event());
        st_addr.write(a);
        st_re.write(true);
        wait(1, SC_NS);
        unsigned int v = st_rdata.read().to_uint();
        st_re.write(false);
        return v;
    }

    void gk_write(unsigned int a, unsigned int v)
    {
        wait(clk.negedge_event());
        gk_addr.write(a);
        gk_wdata.write(v);
        gk_we.write(true);
        wait(clk.posedge_event());
        wait(clk.negedge_event());
        gk_we.write(false);
    }

    unsigned int gk_read(unsigned int a)
    {
        wait(clk.negedge_event());
        gk_addr.write(a);
        gk_re.write(true);
        wait(1, SC_NS);
        unsigned int v = gk_rdata.read().to_uint();
        gk_re.write(false);
        return v;
    }

    void request(unsigned int id, unsigned int tx, unsigned int ty,
                 unsigned int pw, unsigned int ft)
    {
        st_write(0x10, id);
        st_write(0x11, tx);
        st_write(0x12, ty);
        st_write(0x13, pw);
        st_write(0x14, ft);
        st_write(0x15, 1);
    }

    void responder(unsigned int id, unsigned int dx, unsigned int dy,
                   unsigned int rx, unsigned int ry)
    {
        gk_write(0x20, id);
        gk_write(0x21, dx);
        gk_write(0x22, dy);
        gk_write(0x24, rx);
        gk_write(0x25, ry);
        gk_write(0x16, 1);
    }

    void cerrar()
    {
        st_write(0x15, 0);
        for (int i = 0; i < 3; i++)
            wait(clk.posedge_event());
    }

    void run()
    {
        // Estado inicial del bus
        st_we.write(false);
        st_re.write(false);
        gk_we.write(false);
        gk_re.write(false);
        reset.write(false);

        // Reset inicial
        reset.write(true);
        wait(20, SC_NS);
        reset.write(false);
        wait(clk.negedge_event());

        // =================================================
        // A. Escritura/lectura individual de los 5 campos
        // =================================================
        st_write(0x10, 0x11);
        st_write(0x11, 0x22);
        st_write(0x12, 0x33);
        st_write(0x13, 0x44);
        st_write(0x14, 0x55);

        bool a_ok = gk_read(0x10) == 0x11 &&
                    gk_read(0x11) == 0x22 &&
                    gk_read(0x12) == 0x33 &&
                    gk_read(0x13) == 0x44 &&
                    gk_read(0x14) == 0x55;
        comprobar("A. cinco campos de solicitud escriben/leen", a_ok, "");
        comprobar("A. REQ/ACK inactivos sin solicitud",
                  !req.read() && !ack.read(), "");

        // =================================================
        // D. Activacion/desactivacion de REQ y ACK
        // =================================================
        st_write(0x15, 1);
        bool d_req = (gk_read(0x15) == 1) && req.read() && !ack.read();
        comprobar("D. REQ se activa y ACK sigue bajo", d_req, "");

        responder(0x11, 0x22, 0x33, 0x40, 0x40);
        bool d_ack = ack.read() &&
                     st_read(0x16) == 1 &&
                     st_read(0x20) == 0x11 &&
                     st_read(0x21) == 0x22 &&
                     st_read(0x22) == 0x33 &&
                     st_read(0x24) == 0x40 &&
                     st_read(0x25) == 0x40;
        comprobar("D. ACK y respuesta visibles al STRIKER", d_ack, "");

        st_write(0x15, 0);
        bool d_fin = !req.read() && !ack.read() &&
                     gk_read(0x15) == 0 && st_read(0x16) == 0;
        comprobar("D. REQ=0 desactiva ACK y libera el protocolo", d_fin, "");

        // =================================================
        // E. Conservacion de datos durante solicitud pendiente
        // =================================================
        st_write(0x10, 0x09);
        st_write(0x11, 0x0A);
        st_write(0x15, 1);
        wait(clk.posedge_event());
        st_write(0x10, 0xEE);  // debe ignorarse
        st_write(0x11, 0xDD);  // debe ignorarse
        bool e_ok = gk_read(0x10) == 0x09 && gk_read(0x11) == 0x0A;
        comprobar("E. datos estables durante solicitud pendiente", e_ok, "");
        responder(0x09, 0x0A, 0x00, 0x40, 0x40);
        cerrar();

        // =================================================
        // F. Rechazo de segunda solicitud mientras hay una
        // =================================================
        st_write(0x10, 0x21);
        st_write(0x15, 1);
        wait(clk.posedge_event());
        wait(clk.posedge_event());
        unsigned int t1 = st_read(0x23);
        st_write(0x15, 1);  // segunda REQ: debe ignorarse
        unsigned int t2 = st_read(0x23);
        bool f_ok = req.read() && (t1 > 0) && (t2 >= t1);
        comprobar("F. segunda solicitud pendiente se rechaza", f_ok, "");
        responder(0x21, 0x00, 0x00, 0x40, 0x40);
        cerrar();

        // =================================================
        // G. Sin respuestas duplicadas
        // =================================================
        st_write(0x10, 0x31);
        st_write(0x15, 1);
        responder(0x31, 0x41, 0x51, 0x60, 0x61);
        bool g1 = ack.read() && st_read(0x20) == 0x31 &&
                  st_read(0x21) == 0x41;
        // Intentar sobrescribir tras ACK: debe ignorarse
        gk_write(0x20, 0x99);
        gk_write(0x21, 0x88);
        gk_write(0x16, 1);
        bool g2 = ack.read() && st_read(0x20) == 0x31 &&
                  st_read(0x21) == 0x41;
        comprobar("G. respuesta no se duplica ni sobrescribe", g1 && g2, "");
        cerrar();

        // =================================================
        // H. Medicion exacta de reaction_ticks
        // =================================================
        st_write(0x10, 0x41);
        st_write(0x15, 1);
        for (int i = 0; i < 4; i++)
            wait(clk.posedge_event());
        gk_write(0x16, 1);  // quinto flanco
        unsigned int h = st_read(0x23);
        comprobar("H. reaction_ticks = 5", h == 5, "");
        st_write(0x15, 0);
        for (int i = 0; i < 3; i++)
            wait(clk.posedge_event());

        // =================================================
        // I. Saturacion del contador en 255
        // =================================================
        st_write(0x10, 0x51);
        st_write(0x15, 1);
        for (int i = 0; i < 300; i++)
            wait(clk.posedge_event());
        unsigned int i1 = st_read(0x23);
        gk_write(0x16, 1);
        unsigned int i2 = st_read(0x23);
        comprobar("I. reaction_ticks satura en 255",
                  i1 == 255 && i2 == 255, "");
        cerrar();

        // =================================================
        // J. Reset con solicitud pendiente
        // =================================================
        st_write(0x10, 0x61);
        st_write(0x15, 1);
        wait(clk.posedge_event());
        reset.write(true);
        wait(20, SC_NS);
        reset.write(false);
        wait(clk.negedge_event());
        bool j_ok = !req.read() && !ack.read() &&
                    gk_read(0x15) == 0 && st_read(0x16) == 0 &&
                    st_read(0x23) == 0;
        comprobar("J. reset limpia solicitud, ACK y timer", j_ok, "");

        // =================================================
        // K. Varias solicitudes consecutivas
        // =================================================
        bool k_ok = true;
        for (unsigned int id = 1; id <= 3; id++)
        {
            request(id, 0x10 + id, 0x20 + id, 0x30 + id, 0x40 + id);
            wait(clk.posedge_event());
            wait(clk.posedge_event());
            unsigned int rt = st_read(0x23);
            responder(id, 0x10 + id, 0x20 + id, 0x40, 0x40);
            bool uno = ack.read() && st_read(0x20) == id &&
                       st_read(0x21) == 0x10 + id &&
                       st_read(0x22) == 0x20 + id &&
                       st_read(0x24) == 0x40 &&
                       st_read(0x25) == 0x40 &&
                       rt > 0;
            if (!uno) k_ok = false;
            cerrar();
        }
        comprobar("K. tres solicitudes consecutivas completas", k_ok, "");

        std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
        std::cout << "Fallos: " << fallos << std::endl;
        sc_stop();
    }

    SC_CTOR(ProtocolDriver)
    {
        SC_THREAD(run);
    }
};

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> reset;
    sc_signal<bool> st_we, st_re;
    sc_signal<sc_uint<8>> st_addr, st_wdata, st_rdata;
    sc_signal<bool> gk_we, gk_re;
    sc_signal<sc_uint<8>> gk_addr, gk_wdata, gk_rdata;
    sc_signal<bool> req, ack;
    sc_signal<sc_uint<8>> reaction;

    CommunicationMailbox mb("mb");
    mb.clk(clk);
    mb.reset(reset);
    mb.st_we(st_we);
    mb.st_addr(st_addr);
    mb.st_wdata(st_wdata);
    mb.st_re(st_re);
    mb.st_rdata(st_rdata);
    mb.gk_we(gk_we);
    mb.gk_addr(gk_addr);
    mb.gk_wdata(gk_wdata);
    mb.gk_re(gk_re);
    mb.gk_rdata(gk_rdata);
    mb.req(req);
    mb.ack(ack);
    mb.reaction_ticks(reaction);

    ProtocolDriver drv("drv");
    drv.clk(clk);
    drv.reset(reset);
    drv.st_we(st_we);
    drv.st_addr(st_addr);
    drv.st_wdata(st_wdata);
    drv.st_re(st_re);
    drv.st_rdata(st_rdata);
    drv.gk_we(gk_we);
    drv.gk_addr(gk_addr);
    drv.gk_wdata(gk_wdata);
    drv.gk_re(gk_re);
    drv.gk_rdata(gk_rdata);
    drv.req(req);
    drv.ack(ack);
    drv.reaction(reaction);

    sc_start();
    return fallos == 0 ? 0 : 1;
}
